package com.example.aquacontrol;


import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class MainActivity extends AppCompatActivity {

    private NetworkAwareDiscovery discovery;
    private DeviceAdapter adapter;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        RecyclerView deviceList = findViewById(R.id.deviceList);
        adapter = new DeviceAdapter();
        deviceList.setLayoutManager(new LinearLayoutManager(this));
        deviceList.setAdapter(adapter);

        NetworkAwareDiscovery discovery = new NetworkAwareDiscovery(this, (device) -> {
            Log.d("DISCOVERY", "Znaleziono Pico W: " + device);
            runOnUiThread(() -> adapter.addDevice(device));
        });

        discovery.start();

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
