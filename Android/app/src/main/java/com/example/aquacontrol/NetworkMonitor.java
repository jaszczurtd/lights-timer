package com.example.aquacontrol;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.util.Log;

import androidx.annotation.NonNull;

public class NetworkMonitor implements Constants{

    private final ConnectivityManager connectivityManager;
    private final ConnectivityManager.NetworkCallback networkCallback;

    private final NetworkStatusListener listener;

    public interface NetworkStatusListener {
        void onConnected();
        void onDisconnected();
    }

    public NetworkMonitor(Context context, NetworkStatusListener l) {
        connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        listener = l;

        networkCallback = new ConnectivityManager.NetworkCallback() {
            @Override
            public void onAvailable(@NonNull Network network) {
                Log.v(TAG, "Internet connection is available");
                listener.onConnected();
            }

            @Override
            public void onLost(@NonNull Network network) {
                Log.d(TAG, "Internet connection is not available");
                listener.onDisconnected();
            }
        };
    }

    public void startMonitoring() {
        NetworkRequest request = new NetworkRequest.Builder().build();
        connectivityManager.registerNetworkCallback(request, networkCallback);
    }

    /** @noinspection BooleanMethodIsAlwaysInverted*/
    public boolean isConnected() {
        NetworkCapabilities caps = connectivityManager.getNetworkCapabilities(connectivityManager.getActiveNetwork());
        if (caps != null) {
            return caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ||
                    caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR);
        }
        return false;
    }

    public void stopMonitoring() {
        connectivityManager.unregisterNetworkCallback(networkCallback);
    }
}
