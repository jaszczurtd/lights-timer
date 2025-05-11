package com.example.aquacontrol;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class DeviceAdapter extends RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder> {
    private static final String TAG = "NetworkAwareDiscovery";

    private final ArrayList<NetworkAwareDiscovery.DeviceInfo> devices = new ArrayList<>();

    public interface OnDeviceToggleListener {
        void onToggle(NetworkAwareDiscovery.DeviceInfo device, int switchIndex, boolean isOn);
        void onTimeSetButton(NetworkAwareDiscovery.DeviceInfo device);
    }

    private final OnDeviceToggleListener toggleListener;

    public DeviceAdapter(OnDeviceToggleListener listener) {
        this.toggleListener = listener;
    }

    public void addDevice(NetworkAwareDiscovery.DeviceInfo device) {
        for (NetworkAwareDiscovery.DeviceInfo d : devices) {
            if (d.mac.equals(device.mac)) return;
        }
        devices.add(device);
        notifyItemInserted(devices.size() - 1);
    }

    @NonNull
    @Override
    public DeviceViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_device, parent, false);
        return new DeviceViewHolder(view);
    }

    public static String formatLabel(String raw) {
        if (raw == null || raw.isEmpty()) return "";

        raw = raw.replace('_', ' ');
        return Character.toUpperCase(raw.charAt(0)) + raw.substring(1);
    }

    void configureSwitches(DeviceViewHolder h, NetworkAwareDiscovery.DeviceInfo d) {
        View[] containers = { h.sw1Container, h.sw2Container, h.sw3Container, h.sw4Container };
        SwitchCompat[] switches = { h.isOn1, h.isOn2, h.isOn3, h.isOn4 };

        for (int i = 0; i < 4; i++) {
            final int index = i;

            if (d.switches > index) {
                containers[index].setVisibility(View.VISIBLE);

                boolean isChecked = h.isOnFlags[index];

                setSwitch(switches[index], isChecked);

                switches[index].setOnCheckedChangeListener((btn, checked) -> {
                    btn.setText(checked ?
                            ContextProvider.getContext().getString(R.string.on) :
                            ContextProvider.getContext().getString(R.string.off));

                    if(checked != h.isOnFlags[index]) {
                        h.isOnFlags[index] = checked;
                        toggleListener.onToggle(d, index, checked);
                    }
                });

            } else {
                containers[index].setVisibility(View.GONE);
            }
        }
    }

    static void setSwitch(SwitchCompat sw, boolean state) {
        sw.setText(state ?
                ContextProvider.getContext().getString(R.string.on) :
                ContextProvider.getContext().getString(R.string.off));
        sw.setChecked(state);
    }

    private static final Map<NetworkAwareDiscovery.DeviceInfo, DeviceViewHolder> activeHolders = new HashMap<>();

    public static DeviceViewHolder getDeviceViewBy(NetworkAwareDiscovery.DeviceInfo device) {
        return activeHolders.get(device);
    }

    @Override
    public void onViewRecycled(@NonNull DeviceViewHolder holder) {
        super.onViewRecycled(holder);
        activeHolders.values().remove(holder);
    }

    @Override
    public void onBindViewHolder(@NonNull DeviceViewHolder holder, int position) {
        NetworkAwareDiscovery.DeviceInfo device = devices.get(position);
        activeHolders.put(device, holder);

        holder.label.setText(formatLabel(device.hostName));

        Log.i(TAG, "switches:" + device.switches);

        configureSwitches(holder, device);

        StringBuilder query = new StringBuilder();
        query.append("dateHourStart&dateHourEnd");

        for (int i = 1; i <= device.switches; i++) {
            query.append("&isOn").append(i);
        }

        String fullUrl = HttpRequestHelper.METHOD + device.ip + "/?" + query;
        Log.i(TAG, "full url:" + fullUrl);

        HttpRequestHelper.get(fullUrl, new HttpRequestHelper.Callback() {
            @Override
            public void onResponse(int status, String response) {
                Log.d(TAG, "status:" + status + " response json:" + response);

                Handler mainHandler = new Handler(Looper.getMainLooper());

                try {
                    JSONObject obj = new JSONObject(response);
                    holder.start = Long.parseLong(obj.getString(HttpRequestHelper.PARAM_DATE_HOUR_START));
                    holder.end = Long.parseLong(obj.getString(HttpRequestHelper.PARAM_DATE_HOUR_END));

                    SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };
                    String paramPrefix = HttpRequestHelper.PARAM_IS_ON;

                    StringBuilder b = new StringBuilder();
                    for (int i = 0; i < device.switches; i++) {
                        holder.isOnFlags[i] = obj.optBoolean(paramPrefix + (i + 1), false);
                        b.append("isOn").append(i).append(":").append(holder.isOnFlags[i]).append(" ");
                    }

                    mainHandler.post(() -> {
                        holder.startTimeText.setText(TimeRangeDialog.formatTime(holder.start));
                        holder.endTimeText.setText(TimeRangeDialog.formatTime(holder.end));

                        for (int i = 0; i < device.switches; i++) {
                            setSwitch(switches[i], holder.isOnFlags[i]);
                        }
                    });

                    Log.i(TAG, "parsed values: start:" + holder.start +
                            " end:" + holder.end + " " + b);

                } catch (JSONException e) {
                    Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                }
            }

            @Override
            public void onError(Exception e) {
                Log.e(TAG, "Error", e);
            }
        });

        holder.time.setOnClickListener((buttonView) -> {
            toggleListener.onTimeSetButton(device);
        });
    }

    @Override
    public int getItemCount() {
        return devices.size();
    }

    public static class DeviceViewHolder extends RecyclerView.ViewHolder {
        TextView label, startTimeText, endTimeText;

        SwitchCompat isOn1, isOn2, isOn3, isOn4;
        public boolean[] isOnFlags = new boolean[4];
        long start, end;

        LinearLayout sw1Container, sw2Container, sw3Container, sw4Container;
        ImageButton time;

        public DeviceViewHolder(@NonNull View itemView) {
            super(itemView);
            label = itemView.findViewById(R.id.deviceName);
            isOn1 = itemView.findViewById(R.id.isOn1);
            isOn2 = itemView.findViewById(R.id.isOn2);
            isOn3 = itemView.findViewById(R.id.isOn3);
            isOn4 = itemView.findViewById(R.id.isOn4);

            sw1Container = itemView.findViewById(R.id.sw1Container);
            sw2Container = itemView.findViewById(R.id.sw2Container);
            sw3Container = itemView.findViewById(R.id.sw3Container);
            sw4Container = itemView.findViewById(R.id.sw4Container);

            startTimeText = itemView.findViewById(R.id.startTimeText);
            endTimeText = itemView.findViewById(R.id.endTimeText);

            time = itemView.findViewById(R.id.timeButton);
        }

    }

    public void clearDevices() {
        int size = devices.size();
        if (size > 0) {
            devices.clear();
            notifyItemRangeRemoved(0, size);
        }
    }

}
