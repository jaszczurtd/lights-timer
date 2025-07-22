package com.example.aquacontrol;

import androidx.annotation.NonNull;

public class NetworkAwareDiscovery implements Constants {

    public static class DeviceInfo {
        String mac;
        String ip;
        String hostName;
        int switches;

        DeviceInfo(String mac, String ip, String hostName, int switches) {
            this.mac = mac;
            this.ip = ip;
            this.hostName = hostName;
            this.switches = switches;
        }

        @NonNull
        @Override
        public String toString() {
            return mac + " (" + ip + ") " + hostName + " " + switches + " sw";
        }
    }


}
