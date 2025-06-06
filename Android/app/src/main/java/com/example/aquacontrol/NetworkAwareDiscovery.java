package com.example.aquacontrol;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

import androidx.annotation.NonNull;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.util.Objects;


public class NetworkAwareDiscovery {

    private static final String TAG = "NetworkAwareDiscovery";

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

    public interface DeviceListener {
        void onDeviceDiscovered(DeviceInfo device);
    }

    private HandlerThread handlerThread;
    private Handler handler;
    private Runnable task;

    private final ConnectivityManager connectivityManager;
    private final DeviceListener listener;
    private boolean running = false;
    private boolean wifiAvailable;

    private final ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
        @Override
        public void onAvailable(@NonNull Network network) {
            wifiAvailable = true;
        }

        @Override
        public void onLost(@NonNull Network network) {
            wifiAvailable = false;
        }
    };

    public NetworkAwareDiscovery(Context context, DeviceListener listener) {
        this.listener = listener;
        this.connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    public void start() {
        NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build();
        connectivityManager.registerNetworkCallback(request, networkCallback);
        startDiscovery();
    }

    public void stop() {
        connectivityManager.unregisterNetworkCallback(networkCallback);
        stopDiscovery();
    }

    public int extractNumber(String input) {
        if (input == null || !input.contains(":")) return -1;
        try {
            return Integer.parseInt(input.split(":")[1]);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    private void startDiscovery() {
        if (running) return;

        handlerThread = new HandlerThread("DiscoveryThread");
        handlerThread.start();

        handler = new Handler(handlerThread.getLooper());

        running = true;

        task = new Runnable() {
            @Override
            public void run() {
                if (!running) return;

                try {
                    DatagramSocket socket = new DatagramSocket();
                    Objects.requireNonNull(connectivityManager.getActiveNetwork()).bindSocket(socket);

                    socket.setBroadcast(true);
                    socket.setSoTimeout(3000);

                    byte[] sendData = "AQUA_DISCOVER".getBytes();
                    DatagramPacket sendPacket = new DatagramPacket(
                            sendData, sendData.length,
                            InetAddress.getByName("255.255.255.255"), 12345);
                    socket.send(sendPacket);

                    byte[] recvBuf = new byte[256];
                    while (running) {
                        try {
                            DatagramPacket receivePacket = new DatagramPacket(recvBuf, recvBuf.length);
                            socket.receive(receivePacket);

                            String received = new String(receivePacket.getData(), 0, receivePacket.getLength());

                            if (received.startsWith("AQUA_FOUND|")) {
                                String[] parts = received.split("\\|");
                                if (parts.length >= 5) {
                                    String mac = parts[1];
                                    String ip = parts[2];
                                    String hostName = parts[3];
                                    int switches = extractNumber(parts[4]);
                                    DeviceInfo device = new DeviceInfo(mac, ip, hostName, switches);
                                    listener.onDeviceDiscovered(device);
                                }
                            }

                        } catch (SocketTimeoutException e) {
                            Log.i(TAG, Objects.requireNonNull(e.getMessage()) + " wifi:" + wifiAvailable);
                            break;
                        }
                    }
                    socket.close();

                } catch (Exception e) {
                    Log.e(TAG, "Discovery error: " + e.getMessage());
                }

                if (!running) return;
                handler.postDelayed(this, 1000);
            }
        };

        handler.post(task);
    }

    private void stopDiscovery() {
        running = false;

        if (handler != null && task != null) {
            handler.removeCallbacks(task);
        }

        if (handlerThread != null) {
            handlerThread.quit();
            handlerThread = null;
            handler = null;
        }
    }
}
