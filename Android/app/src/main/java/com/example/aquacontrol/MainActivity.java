package com.example.aquacontrol;


import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.json.JSONObject;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

public class MainActivity extends AppCompatActivity {

    FrameLayout loader;
    TextView emptyView;

    private static final String TAG = "NetworkAwareDiscovery";
    private DeviceAdapter adapter;
    private NetworkAwareDiscovery discovery;
    private final Handler loaderHandler = new Handler(Looper.getMainLooper());
    private Runnable showLoaderRunnable;
    private boolean loaderPending = false;
    private AlertDialog alert;

    public static boolean isConnectedViaWifi(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return false;

        Network activeNetwork = cm.getActiveNetwork();
        if (activeNetwork == null) return false;

        NetworkCapabilities nc = cm.getNetworkCapabilities(activeNetwork);
        return nc != null && nc.hasTransport(NetworkCapabilities.TRANSPORT_WIFI);
    }

    private void checkWifiConnection() {
        if (isConnectedViaWifi(getApplicationContext())) {
            Log.d(TAG, "Wi-Fi connectivity");
        } else {
            Log.d(TAG, "No Wi-Fi");
            if(alert == null) {
                alert = new AlertDialog.Builder(this)
                        .setTitle(getString(R.string.error))
                        .setMessage(getString(R.string.no_wifi_error))
                        .setPositiveButton(getString(R.string.open_settings), (dialog, which) -> {
                            startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
                            alert = null;
                        })
                        .setNegativeButton(getString(R.string.cancel), (dialog, which) -> alert = null)
                        .show();
            }
        }
    }
    private void updateEmptyView() {
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            boolean isEmpty = adapter.getItemCount() == 0;
            emptyView.setVisibility(isEmpty ? View.VISIBLE : View.GONE);
            findViewById(R.id.deviceList).setVisibility(isEmpty ? View.GONE : View.VISIBLE);
        }, 500);
    }

    public void showLoaderDelayed(long delayMillis) {
        if (loaderPending) return;

        loaderPending = true;
        showLoaderRunnable = () -> {
            loader.setVisibility(View.VISIBLE);
            loaderPending = false;
        };
        loaderHandler.postDelayed(showLoaderRunnable, delayMillis);
    }

    public void hideLoader() {
        if (loaderPending) {
            loaderHandler.removeCallbacks(showLoaderRunnable);
            loaderPending = false;
        }
        loader.setVisibility(View.GONE);
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
                DeviceAdapter.DeviceViewHolder holder = DeviceAdapter.getDeviceViewBy(device);
                if(holder != null) {
                    showLoaderDelayed(1500);
                    SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };

                    Map<String, String> params = new HashMap<>();

                    holder.isOnFlags[switchIndex] = isOn;
                    DeviceAdapter.setSwitch(switches[switchIndex], holder.isOnFlags[switchIndex]);
                    params.put(HttpRequestHelper.PARAM_IS_ON + (switchIndex + 1), String.valueOf(holder.isOnFlags[switchIndex]));

                    String fullUrl = HttpRequestHelper.METHOD + device.ip;

                    HttpRequestHelper.post(fullUrl, params, new HttpRequestHelper.Callback() {
                        @Override
                        public void onResponse(int status, String response) {
                            new Handler(Looper.getMainLooper()).post(MainActivity.this::hideLoader);
                            Log.i(TAG, "status:" + status + " post response:" + response);

                            try {
                                JSONObject obj = new JSONObject(response);
                                boolean isOn = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + (switchIndex + 1));
                                if(holder.isOnFlags[switchIndex] == isOn) {
                                    Log.i(TAG, "Action went successfully");
                                } else {
                                    new Handler(Looper.getMainLooper()).post(() -> {
                                        Toast.makeText(ContextProvider.getContext(),
                                                getText(R.string.error_setting), Toast.LENGTH_LONG).show();
                                    });
                                }

                            } catch (Exception e) {
                                Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                            }

                        }

                        @Override
                        public void onError(Exception e) {
                            new Handler(Looper.getMainLooper()).post(() -> {
                                hideLoader();
                                Toast.makeText(ContextProvider.getContext(),
                                        getText(R.string.error_connecting), Toast.LENGTH_LONG).show();
                            });
                            Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                        }
                    });
                }
            }
            @Override
            public void onTimeSetButton(NetworkAwareDiscovery.DeviceInfo device) {
                Log.d(TAG, "set time for:" + device.mac);

                DeviceAdapter.DeviceViewHolder holder = DeviceAdapter.getDeviceViewBy(device);
                if(holder != null) {
                    TimeRangeDialog.show(MainActivity.this, holder.start, holder.end, (startMinutes, endMinutes) -> {

                        String range = String.format(Locale.ENGLISH, "%s - %s",
                                TimeRangeDialog.formatTime(startMinutes), TimeRangeDialog.formatTime(endMinutes));
                        Log.d(TAG, "time set result: " + range);

                        loader.setVisibility(View.VISIBLE);

                        holder.startTimeText.setText(TimeRangeDialog.formatTime(startMinutes));
                        holder.endTimeText.setText(TimeRangeDialog.formatTime(endMinutes));
                        long now = TimeRangeDialog.getTimeNowInMinutes();
                        SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };

                        Map<String, String> params = new HashMap<>();
                        params.put(HttpRequestHelper.PARAM_DATE_HOUR_START, String.valueOf(startMinutes));
                        params.put(HttpRequestHelper.PARAM_DATE_HOUR_END, String.valueOf(endMinutes));

                        int a = 0;
                        holder.isOnFlags[a] = TimeRangeDialog.isTimeInRange(now, startMinutes, endMinutes);
                        DeviceAdapter.setSwitch(switches[a], holder.isOnFlags[a]);
                        params.put(HttpRequestHelper.PARAM_IS_ON + (a + 1), String.valueOf(holder.isOnFlags[a]));

                        String fullUrl = HttpRequestHelper.METHOD + device.ip;

                        HttpRequestHelper.post(fullUrl, params, new HttpRequestHelper.Callback() {
                            @Override
                            public void onResponse(int status, String response) {
                                new Handler(Looper.getMainLooper()).post(MainActivity.this::hideLoader);
                                Log.i(TAG, "status:" + status + " post response:" + response);

                                try {
                                    JSONObject obj = new JSONObject(response);
                                    boolean isOn = obj.getBoolean(HttpRequestHelper.PARAM_IS_ON + (a + 1));
                                    long start = obj.getLong(HttpRequestHelper.PARAM_DATE_HOUR_START);
                                    long end = obj.getLong(HttpRequestHelper.PARAM_DATE_HOUR_END);

                                    if(holder.isOnFlags[a] == isOn && start == startMinutes && end == endMinutes) {
                                        Log.i(TAG, "Action went successfully");
                                    } else {
                                        new Handler(Looper.getMainLooper()).post(() -> {
                                            Toast.makeText(ContextProvider.getContext(),
                                                    getText(R.string.error_setting), Toast.LENGTH_LONG).show();
                                        });
                                    }

                                } catch (Exception e) {
                                    Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                                }
                            }

                            @Override
                            public void onError(Exception e) {
                                new Handler(Looper.getMainLooper()).post(() -> {
                                    hideLoader();
                                    Toast.makeText(ContextProvider.getContext(),
                                            getText(R.string.error_connecting), Toast.LENGTH_LONG).show();
                                });
                                Log.e(TAG, Objects.requireNonNull(e.getMessage()));
                            }
                        });
                    });
                } else {
                    Log.e(TAG, "cannot get holder object from the list");
                }
            }
        });

        deviceList.setAdapter(adapter);

        updateEmptyView();
        checkWifiConnection();

        discovery = new NetworkAwareDiscovery(this, (device) -> {
            Log.d(TAG, "Found Pico W: " + device);
            runOnUiThread(() -> {
                adapter.addDevice(device);
                updateEmptyView();
            });
        });

        discovery.start();
    }

    @Override
    protected void onResume() {
        super.onResume();
        new Handler(Looper.getMainLooper()).postDelayed(this::checkWifiConnection, 3000);
        discovery.start();
        runOnUiThread(this::updateEmptyView);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if(alert != null) {
            alert.dismiss();
            alert = null;
        }
        discovery.stop();
        adapter.clearDevices();
    }

    @Override
    protected void onDestroy() {
        discovery.stop();
        adapter.clearDevices();

        super.onDestroy();
    }
}
