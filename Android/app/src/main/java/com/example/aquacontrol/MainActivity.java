package com.example.aquacontrol;


import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

public class MainActivity extends AppCompatActivity {

    FrameLayout loader;
    TextView emptyView;

    private static final String TAG = "NetworkAwareDiscovery";

    private DeviceAdapter adapter;

    private void updateEmptyView() {
        boolean isEmpty = adapter.getItemCount() == 0;
        emptyView.setVisibility(isEmpty ? View.VISIBLE : View.GONE);
        findViewById(R.id.deviceList).setVisibility(isEmpty ? View.GONE : View.VISIBLE);
    }

    @SuppressLint("SourceLockedOrientationActivity")
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        loader = findViewById(R.id.fullscreenLoader);
        emptyView = findViewById(R.id.emptyView);

        RecyclerView deviceList = findViewById(R.id.deviceList);
        deviceList.addItemDecoration(
            new SubtleDividerDecoration(this, 1, Color.parseColor("#44000000"))
        );
        deviceList.setLayoutManager(new LinearLayoutManager(this));

        adapter = new DeviceAdapter(new DeviceAdapter.OnDeviceToggleListener() {
            @Override
            public void onToggle(NetworkAwareDiscovery.DeviceInfo device, int switchIndex, boolean isOn) {
                Log.d(TAG, "device:" + device + ": idx:" + switchIndex + "/" +
                        (isOn ? getString(R.string.on) : getString(R.string.off)));
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
                        loader.setVisibility(View.VISIBLE);

                        holder.startTimeText.setText(TimeRangeDialog.formatTime(startMinutes));
                        holder.endTimeText.setText(TimeRangeDialog.formatTime(endMinutes));
                        long now = TimeRangeDialog.getTimeNowInMinutes();
                        SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };

                        Map<String, String> params = new HashMap<>();
                        params.put(HttpRequestHelper.PARAM_DATE_HOUR_START, String.valueOf(startMinutes));
                        params.put(HttpRequestHelper.PARAM_DATE_HOUR_END, String.valueOf(endMinutes));

                        for(int a = 0; a < device.switches; a++) {
                            holder.isOnFlags[a] = TimeRangeDialog.isTimeInRange(now, startMinutes, endMinutes);
                            DeviceAdapter.setSwitch(switches[a], holder.isOnFlags[a]);
                            params.put(HttpRequestHelper.PARAM_IS_ON + (a + 1), String.valueOf(holder.isOnFlags[a]));
                        }
                        String fullUrl = HttpRequestHelper.METHOD + device.ip;

                        HttpRequestHelper.post(fullUrl, params, new HttpRequestHelper.Callback() {
                            @Override
                            public void onResponse(int status, String response) {
                                new Handler(Looper.getMainLooper()).post(() -> {
                                    loader.setVisibility(View.GONE);
                                });

                                Log.i(TAG, "status:" + status + " post response:" + response);

                            }

                            @Override
                            public void onError(Exception e) {
                                new Handler(Looper.getMainLooper()).post(() -> {
                                    loader.setVisibility(View.GONE);
                                });

                                Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                            }
                        });
                    }
                });

            }
        });

        deviceList.setAdapter(adapter);

        updateEmptyView();

        NetworkAwareDiscovery discovery = new NetworkAwareDiscovery(this, (device) -> {
            Log.d(TAG, "Found Pico W: " + device);
            runOnUiThread(() -> {
                adapter.addDevice(device);
                updateEmptyView();
            });
        });

        discovery.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
