package com.example.aquacontrol;

import android.util.Log;

import androidx.annotation.NonNull;

public class DeviceInfo implements Constants {
    String mac;
    String ip;
    String hostName;
    int switches;

    DeviceInfo(String mac, String ip, String hostName, int switches) {
        this.mac = mac;
        this.ip = ip;
        this.hostName = hostName;
        this.switches = switches;

        Log.v(TAG, "MAC: " + mac + ", IP: " + ip + ", Host: " + hostName + ", Switches: " + switches);
    }

    @NonNull
    @Override
    public String toString() {
        return mac + " (" + ip + ") " + hostName + " " + switches + " sw";
    }
}

