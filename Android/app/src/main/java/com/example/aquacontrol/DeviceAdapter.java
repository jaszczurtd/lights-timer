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

import java.util.ArrayList;

public class DeviceAdapter extends RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder> {
    private static final String TAG = "NetworkAwareDiscovery";

    private final ArrayList<NetworkAwareDiscovery.DeviceInfo> devices = new ArrayList<>();

    public interface OnDeviceToggleListener {
        void onToggleMain(NetworkAwareDiscovery.DeviceInfo device, boolean isOn);
        void onToggleSecondary(NetworkAwareDiscovery.DeviceInfo device, boolean isOn);
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

    @Override
    public void onBindViewHolder(@NonNull DeviceViewHolder holder, int position) {
        NetworkAwareDiscovery.DeviceInfo device = devices.get(position);
        holder.label.setText(formatLabel(device.hostName));
        holder.switchMain.setChecked(false);
        holder.switchSecondary.setChecked(false);

        holder.switchMain.setOnCheckedChangeListener((buttonView, isChecked) -> {
            holder.switchMain.setText(isChecked ? "ON" : "OFF");
            toggleListener.onToggleMain(device, isChecked);
        });

        Log.i(TAG, "switches:" + device.switches);

        if(device.switches > 1) {
            holder.secondaryContainer.setVisibility(View.VISIBLE);

            holder.switchSecondary.setOnCheckedChangeListener((buttonView, isChecked) -> {
                holder.switchSecondary.setText(isChecked ? "ON" : "OFF");
                toggleListener.onToggleSecondary(device, isChecked);
            });
        } else {
            holder.switchSecondary.setVisibility(View.GONE);
        }

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
        SwitchCompat switchMain, switchSecondary;
        LinearLayout secondaryContainer;
        ImageButton time;

        public DeviceViewHolder(@NonNull View itemView) {
            super(itemView);
            label = itemView.findViewById(R.id.deviceName);
            switchMain = itemView.findViewById(R.id.switchMain);
            switchSecondary = itemView.findViewById(R.id.switchSecondary);
            secondaryContainer = itemView.findViewById(R.id.secondaryContainer);
            time = itemView.findViewById(R.id.timeButton);
        }
    }
}
