package com.example.aquacontrol;


import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.Calendar;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "NetworkAwareDiscovery";

    private DeviceAdapter adapter;

    @SuppressLint("SourceLockedOrientationActivity")
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        RecyclerView deviceList = findViewById(R.id.deviceList);
        deviceList.addItemDecoration(
            new SubtleDividerDecoration(this, 1, Color.parseColor("#44000000"))
        );
        deviceList.setLayoutManager(new LinearLayoutManager(this));

        adapter = new DeviceAdapter(new DeviceAdapter.OnDeviceToggleListener() {
            @Override
            public void onToggle(NetworkAwareDiscovery.DeviceInfo device, int switchIndex, boolean isOn) {
                Log.d(TAG, "device:" + device + ": idx:" + switchIndex + "/" + (isOn ? "ON" : "OFF"));
            }
            @Override
            public void onTimeSetButton(NetworkAwareDiscovery.DeviceInfo device) {
                Log.d(TAG, "set time for:" + device.mac);

                TimeRangeDialog.show(MainActivity.this, (startMinutes, endMinutes) -> {

                    String range = String.format(Locale.ENGLISH, "%s - %s",
                                    TimeRangeDialog.formatTime(startMinutes), TimeRangeDialog.formatTime(endMinutes));
                    Log.d(TAG, "time set result: " + range);

                    DeviceAdapter.DeviceViewHolder holder = DeviceAdapter.getDeviceViewBy(device);
                    if(holder != null) {
                        holder.startTimeText.setText(TimeRangeDialog.formatTime(startMinutes));
                        holder.endTimeText.setText(TimeRangeDialog.formatTime(endMinutes));
                        long now = TimeRangeDialog.getTimeNowInMinutes();
                        SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };
                        for(int a = 0; a < device.switches; a++) {
                            holder.isOnFlags[a] = TimeRangeDialog.isTimeInRange(now, startMinutes, endMinutes);
                            DeviceAdapter.setSwitch(switches[a], holder.isOnFlags[a]);
                        }
                    }
                });

            }
        });

        deviceList.setAdapter(adapter);

        NetworkAwareDiscovery discovery = new NetworkAwareDiscovery(this, (device) -> {
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
