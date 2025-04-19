package com.example.aquacontrol;


import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "NetworkAwareDiscovery";

    private NetworkAwareDiscovery discovery;
    private DeviceAdapter adapter;

    @SuppressLint("SourceLockedOrientationActivity")
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        RecyclerView deviceList = findViewById(R.id.deviceList);
        deviceList.setLayoutManager(new LinearLayoutManager(this));

        adapter = new DeviceAdapter(new DeviceAdapter.OnDeviceToggleListener() {
            @Override
            public void onToggleMain(NetworkAwareDiscovery.DeviceInfo device, boolean isOn) {
                Log.d(TAG, "switch 1:" + device.mac + ": " + (isOn ? "ON" : "OFF"));
            }

            @Override
            public void onToggleSecondary(NetworkAwareDiscovery.DeviceInfo device, boolean isOn) {
                Log.d(TAG, "switch 2:" + device.mac + ": " + (isOn ? "ON" : "OFF"));
            }

            @Override
            public void onTimeSetButton(NetworkAwareDiscovery.DeviceInfo device) {
                Log.d(TAG, "set time for:" + device.mac);
            }
        });

        deviceList.setAdapter(adapter);

        discovery = new NetworkAwareDiscovery(this, (device) -> {
            Log.d(TAG, "Znaleziono Pico W: " + device);
            runOnUiThread(() -> adapter.addDevice(device));
        });

        discovery.start();

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
