package com.example.aquacontrol;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;

public class DeviceAdapter extends RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder> {

    private final ArrayList<NetworkAwareDiscovery.DeviceInfo> devices = new ArrayList<>();

    public void addDevice(NetworkAwareDiscovery.DeviceInfo device) {
        for (NetworkAwareDiscovery.DeviceInfo d : devices) {
            if (d.mac.equals(device.mac)) return; // duplikat
        }
        devices.add(device);
        notifyItemInserted(devices.size() - 1);
    }

    @NonNull
    @Override
    public DeviceViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new DeviceViewHolder(LayoutInflater.from(parent.getContext())
                .inflate(android.R.layout.simple_list_item_2, parent, false));
    }

    @Override
    public void onBindViewHolder(@NonNull DeviceViewHolder holder, int position) {
        NetworkAwareDiscovery.DeviceInfo device = devices.get(position);
        holder.mac.setText(device.mac);
        holder.ip.setText(device.ip);
    }

    @Override
    public int getItemCount() {
        return devices.size();
    }

    static class DeviceViewHolder extends RecyclerView.ViewHolder {
        TextView mac, ip;

        public DeviceViewHolder(@NonNull View itemView) {
            super(itemView);
            mac = itemView.findViewById(android.R.id.text1);
            ip = itemView.findViewById(android.R.id.text2);
        }
    }
}
