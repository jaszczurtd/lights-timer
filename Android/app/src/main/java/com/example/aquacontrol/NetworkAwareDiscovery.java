package com.example.aquacontrol;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;


public class NetworkAwareDiscovery {

    public static class DeviceInfo {
        String mac;
        String ip;

        DeviceInfo(String mac, String ip) {
            this.mac = mac;
            this.ip = ip;
        }

        @NonNull
        @Override
        public String toString() {
            return mac + " (" + ip + ")";
        }
    }

    public interface DeviceListener {
        void onDeviceDiscovered(DeviceInfo device);
    }

    private final ConnectivityManager connectivityManager;
    private final DeviceListener listener;
    private DatagramSocket socket;
    private boolean running = false;

    private final ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
        @Override
        public void onAvailable(@NonNull Network network) {
            startDiscovery();
        }

        @Override
        public void onLost(@NonNull Network network) {
            stopDiscovery();
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

    private void startDiscovery() {
        if (running) return;

        running = true;
        new Thread(() -> {
            try {
                socket = new DatagramSocket();
                socket.setBroadcast(true);
                socket.setSoTimeout(2000);

                // Wyślij zapytanie broadcastowe
                String message = "PICO_DISCOVER";
                byte[] sendData = message.getBytes();
                DatagramPacket sendPacket = new DatagramPacket(
                        sendData,
                        sendData.length,
                        InetAddress.getByName("255.255.255.255"),
                        12345
                );
                socket.send(sendPacket);

                byte[] recvBuf = new byte[256];
                while (running) {
                    try {
                        DatagramPacket receivePacket = new DatagramPacket(recvBuf, recvBuf.length);
                        socket.receive(receivePacket);

                        String received = new String(receivePacket.getData(), 0, receivePacket.getLength());
                        if (received.startsWith("PICO_FOUND|")) {
                            String[] parts = received.split("\\|");
                            if (parts.length >= 3) {
                                String mac = parts[1];
                                String ip = parts[2];

                                DeviceInfo device = new DeviceInfo(
                                        mac, // MAC
                                        ip // IP
                                );

                                listener.onDeviceDiscovered(device);
                            }
                        }
                    } catch (SocketTimeoutException ignored) {
                        break;
                    }
                }

            } catch (IOException e) {
                Log.e("NetworkAwareDiscovery", "Discovery error: " + e.getMessage());
            } finally {
                if (socket != null) {
                    socket.close();
                    socket = null;
                }
                running = false;
            }
        }).start();
    }

    private void stopDiscovery() {
        running = false;
        if (socket != null) {
            socket.close();
            socket = null;
        }
    }
}
