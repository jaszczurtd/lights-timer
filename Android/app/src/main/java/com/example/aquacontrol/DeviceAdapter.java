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
import java.util.Objects;

public class DeviceAdapter extends RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder> {
    private static final String TAG = "NetworkAwareDiscovery";

    private final ArrayList<NetworkAwareDiscovery.DeviceInfo> devices = new ArrayList<>();

    public boolean isOn1, isOn2, isOn3, isOn4;

    public interface OnDeviceToggleListener {
        void onToggle(NetworkAwareDiscovery.DeviceInfo device, boolean isOn);
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

                boolean isChecked = false;
                switch (index) {
                    case 0: isChecked = isOn1; break;
                    case 1: isChecked = isOn2; break;
                    case 2: isChecked = isOn3; break;
                    case 3: isChecked = isOn4; break;
                }

                switches[index].setChecked(isChecked);
                switches[index].setText(isChecked ? "ON" : "OFF");

                switches[index].setOnCheckedChangeListener((btn, checked) -> {
                    btn.setText(checked ? "ON" : "OFF");

                    boolean changed = false;
                    switch (index) {
                        case 0:
                            if (checked != isOn1) { isOn1 = checked; changed = true; }
                            break;
                        case 1:
                            if (checked != isOn2) { isOn2 = checked; changed = true; }
                            break;
                        case 2:
                            if (checked != isOn3) { isOn3 = checked; changed = true; }
                            break;
                        case 3:
                            if (checked != isOn4) { isOn4 = checked; changed = true; }
                            break;
                    }

                    if (changed) {
                        toggleListener.onToggle(d, checked);
                    }
                });

            } else {
                containers[index].setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onBindViewHolder(@NonNull DeviceViewHolder holder, int position) {
        NetworkAwareDiscovery.DeviceInfo device = devices.get(position);
        holder.label.setText(formatLabel(device.hostName));
        holder.isOn1.setChecked(false);
        holder.isOn2.setChecked(false);
        holder.isOn3.setChecked(false);
        holder.isOn4.setChecked(false);

        Log.i(TAG, "switches:" + device.switches);

        configureSwitches(holder, device);

        String initialQueryURL = "http://" + device.ip + "?";
        String query = HttpRequestHelper.PARAM_DATE_HOUR_START + "&" +
                        HttpRequestHelper.PARAM_DATE_HOUR_END + "&" +
                        HttpRequestHelper.PARAM_IS_ON + "1";
        if(device.switches > 1) {
            query += HttpRequestHelper.PARAM_IS_ON + "&2";
        }

        String fullUrl = initialQueryURL + query;
        Log.i(TAG, "full url:" + fullUrl);

        HttpRequestHelper.get(fullUrl, new HttpRequestHelper.Callback() {
            @Override
            public void onResponse(String response) {
                Log.d(TAG, "response json:" + response);

                Handler mainHandler = new Handler(Looper.getMainLooper());

                try {
                    JSONObject obj = new JSONObject(response);
                    final long start = Long.parseLong(obj.getString(HttpRequestHelper.PARAM_DATE_HOUR_START));
                    final long end = Long.parseLong(obj.getString(HttpRequestHelper.PARAM_DATE_HOUR_END));
                    isOn1 = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + "1");
                    if (device.switches > 1) {
                        isOn2 = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + "2");
                    } else {
                        isOn2 = false;
                    }
                    if (device.switches > 2) {
                        isOn3 = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + "3");
                    } else {
                        isOn3 = false;
                    }
                    if (device.switches > 4) {
                        isOn4 = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + "4");
                    } else {
                        isOn4 = false;
                    }

                    mainHandler.post(() -> {
                        holder.isOn1.setText(isOn1 ? "ON" : "OFF");
                        holder.isOn1.setChecked(isOn1);

                        if (device.switches > 1) {
                            holder.isOn2.setText(isOn2 ? "ON" : "OFF");
                            holder.isOn2.setChecked(isOn2);
                        }
                    });

                    Log.i(TAG, "parsed values: start:" + start +
                            " end:" + end +
                            " isOn1:" + isOn1 +
                            " isOn2:" + isOn2 +
                            " isOn3:" + isOn3 +
                            " isOn4:" + isOn4);

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

    static class DeviceViewHolder extends RecyclerView.ViewHolder {
        TextView label;
        SwitchCompat isOn1, isOn2, isOn3, isOn4;
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
            time = itemView.findViewById(R.id.timeButton);
        }
    }
}
