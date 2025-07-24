package com.example.aquacontrol;

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

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

public class DeviceAdapter extends RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder> implements Constants {

    private final ArrayList<DeviceInfo> devices = new ArrayList<>();

    public interface OnDeviceToggleListener {
        void onToggle(DeviceInfo device, int switchIndex, boolean isOn);
        void onTimeSetButton(DeviceInfo device);
        void onDeviceJustAppearOnList(DeviceInfo device);
    }

    private final OnDeviceToggleListener toggleListener;

    public DeviceAdapter(OnDeviceToggleListener listener) {
        this.toggleListener = listener;
    }

    public void createListOfDevicesFromJSONString(String list) {
        try {
            JSONObject root = new JSONObject(list);
            JSONArray devs = root.getJSONArray("devices");

            ArrayList<DeviceInfo> newDevices = new ArrayList<>();
            for (int i = 0; i < devs.length(); i++) {
                JSONObject device = devs.getJSONObject(i);

                String mac = device.getString("mac");
                String ip = device.getString("ip");
                String hostName = device.getString("hostName");
                int switches = device.getInt("switches");

                newDevices.add(new DeviceInfo(mac, ip, hostName, switches));
            }

            //remove non-active devices
            for (int i = devices.size() - 1; i >= 0; i--) {
                DeviceInfo d = devices.get(i);
                boolean found = false;
                for (DeviceInfo newDevice : newDevices) {
                    if (d.mac.equalsIgnoreCase(newDevice.mac)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    devices.remove(i);
                    notifyItemRemoved(i);
                }
            }

            //add new devices
            for (DeviceInfo newDevice : newDevices) {
                boolean found = false;
                for (DeviceInfo d : devices) {
                    if (d.mac.equalsIgnoreCase(newDevice.mac)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    devices.add(newDevice);
                    notifyItemInserted(devices.size() - 1);
                }
            }

        } catch (Exception e) {
            Log.e(TAG, "JSON parse error", e);
        }
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

    void configureSwitches(DeviceViewHolder h, DeviceInfo d) {
        View[] containers = { h.sw1Container, h.sw2Container, h.sw3Container, h.sw4Container };
        SwitchCompat[] switches = { h.isOn1, h.isOn2, h.isOn3, h.isOn4 };

        for (int i = 0; i < MAX_AMOUNT_OF_RELAYS; i++) {
            final int index = i;

            if (d.switches > index) {
                containers[index].setVisibility(View.VISIBLE);

                boolean isChecked = d.isOnFlags[index];

                setSwitch(switches[index], isChecked);

                switches[index].setOnCheckedChangeListener((btn, checked) -> {
                    btn.setText(checked ?
                            ContextProvider.getContext().getString(R.string.on) :
                            ContextProvider.getContext().getString(R.string.off));

                    if(checked != d.isOnFlags[index]) {
                        d.isOnFlags[index] = checked;
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

    void consumeBrokerUpdate(String topic, String update) {
        if(topic == null || update == null){
            Log.e(TAG, "Invalid data provided:" + topic + "/" + update);
            return;
        }
        if(!topic.startsWith(AQUA_DEVICE_STATUS)) { //update not for adapter
            return;
        }
        DeviceViewHolder holder = null;
        DeviceInfo device = null;
        for(DeviceInfo d : devices) {  //find device hostname embed in topic
            if(Objects.equals(d.hostName, topic.substring(AQUA_DEVICE_STATUS.length()))) {
                holder = getDeviceViewBy(d);
                device = d;
                break;
            }
        }
        if(holder == null) {
            Log.v(TAG, "holder for topic " + topic + " not found.");
            return;
        }

        try {
            JSONObject root = new JSONObject(update);
            String status = root.getString("status");
            if(!status.equals("ok")) {
                Log.e(TAG, "error response from device " + device.hostName + ": " + status);
                return;
            }

            device.start = root.getLong(dateHourStart);
            device.end = root.getLong(dateHourEnd);
            for(int a = 0; a < MAX_AMOUNT_OF_RELAYS; a++) {
                device.isOnFlags[a] = root.getBoolean(isOn +  (a + 1));
            }

            String range = String.format(Locale.ENGLISH, "%s - %s",
                    TimeRangeDialog.formatTime(device.start), TimeRangeDialog.formatTime(device.end));
            Log.v(TAG, "refreshHolder: time set for: " + holder.label.getText() + " is: " + range);

            holder.startTimeText.setText(TimeRangeDialog.formatTime(device.start));
            holder.endTimeText.setText(TimeRangeDialog.formatTime(device.end));
            SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };

            for(int a = 0; a < MAX_AMOUNT_OF_RELAYS; a++) {
                DeviceAdapter.setSwitch(switches[a], device.isOnFlags[a]);
            }

        } catch (Exception e) {
            Log.e(TAG, "topic broker response parse error:" + e);
        }
    }

    private final Map<DeviceInfo, DeviceViewHolder> activeHolders = new HashMap<>();

    public DeviceViewHolder getDeviceViewBy(DeviceInfo device) {
        return activeHolders.get(device);
    }

    @Override
    public void onViewRecycled(@NonNull DeviceViewHolder holder) {
        super.onViewRecycled(holder);
        activeHolders.values().remove(holder);
    }

    @Override
    public void onBindViewHolder(@NonNull DeviceViewHolder holder, int position) {
        DeviceInfo device = devices.get(position);
        activeHolders.put(device, holder);

        holder.label.setText(formatLabel(device.hostName));

        configureSwitches(holder, device);

        if(toggleListener != null) {
            toggleListener.onDeviceJustAppearOnList(device);

            holder.time.setOnClickListener((buttonView) -> {
                toggleListener.onTimeSetButton(device);
            });
        }
    }

    @Override
    public int getItemCount() {
        return devices.size();
    }

    public static class DeviceViewHolder extends RecyclerView.ViewHolder {
        TextView label, startTimeText, endTimeText;

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

            startTimeText = itemView.findViewById(R.id.startTimeText);
            endTimeText = itemView.findViewById(R.id.endTimeText);

            time = itemView.findViewById(R.id.timeButton);
        }

    }

    public int indexOf(DeviceInfo d) {
        return devices.indexOf(d);
    }

    public void clearDevices() {
        int size = devices.size();
        if (size > 0) {
            devices.clear();
            notifyItemRangeRemoved(0, size);
            activeHolders.clear();
        }
    }

}
