package com.example.aquacontrol;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public class MainActivity extends AppCompatActivity implements Constants {
    FrameLayout loader;
    TextView emptyView;

    private DeviceAdapter adapter;
    private final Handler loaderHandler = new Handler(Looper.getMainLooper());
    private Runnable showLoaderRunnable;
    private boolean loaderPending = false;
    private AlertDialog alert;
    MQTTClient mqttClient;
    NetworkMonitor networkMonitor;
    SharedPreferences prefs;

    private void handleNoNetwork(Runnable onExitConfirmed) {
        alert = new AlertDialog.Builder(this)
                .setTitle(getString(R.string.error))
                .setMessage(getString(R.string.no_internet_error))
                .setPositiveButton(getString(R.string.exit), (dialog, which) -> {
                    alert.dismiss();
                    new Handler(Looper.getMainLooper()).postDelayed(onExitConfirmed, 100);
                })
                .show();
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
            public void onToggle(DeviceInfo device, int switchIndex, boolean isOn) {
                Log.d(TAG, "device:" + device + ": idx:" + switchIndex + "/" +
                        (isOn ? getString(R.string.on) : getString(R.string.off)));
                DeviceAdapter.DeviceViewHolder holder = DeviceAdapter.getDeviceViewBy(device);
                if(holder != null) {
                    showLoaderDelayed(1500);
                    SwitchCompat[] switches = { holder.isOn1, holder.isOn2, holder.isOn3, holder.isOn4 };

                    Map<String, String> params = new HashMap<>();

                    holder.isOnFlags[switchIndex] = isOn;
                    DeviceAdapter.setSwitch(switches[switchIndex], holder.isOnFlags[switchIndex]);

                    //TODO:

                }
            }
            @Override
            public void onTimeSetButton(DeviceInfo device) {
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

                        String start = String.valueOf(startMinutes);
                        String end = String.valueOf(endMinutes);

                        int a = 0;
                        holder.isOnFlags[a] = TimeRangeDialog.isTimeInRange(now, startMinutes, endMinutes);
                        DeviceAdapter.setSwitch(switches[a], holder.isOnFlags[a]);

                        //TODO:

                    });
                } else {
                    Log.e(TAG, "cannot get holder object from the list");
                }
            }
        });

        deviceList.setAdapter(adapter);
        updateEmptyView();

        networkMonitor = new NetworkMonitor(this, new NetworkMonitor.NetworkStatusListener() {
            @Override
            public void onConnected() {
                Log.v(TAG, "Internet is connected");
            }

            @Override
            public void onDisconnected() {
                Log.v(TAG, "Internet has been disconnected");
            }
        });

        networkMonitor.startMonitoring();
        if (!networkMonitor.isConnected()) {
            handleNoNetwork(() -> android.os.Process.killProcess(android.os.Process.myPid()));
        }

        prefs = getSharedPreferences(MQTT_CREDENTIALS, MODE_PRIVATE);
        String user = prefs.getString(MQTT_USER, null);
        String pass = prefs.getString(MQTT_PASS, null);
        if (user != null && pass != null) {
            setupMQTT(user, pass);
        } else {
            askForCredentials();
        }
    }

    void askForCredentials() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(getString(R.string.mqtt_login));

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);

        final EditText inputUser = new EditText(this);
        inputUser.setHint(getString(R.string.user));
        layout.addView(inputUser);

        final EditText inputPass = new EditText(this);
        inputPass.setHint(getString(R.string.pass));
        inputPass.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        layout.addView(inputPass);

        builder.setView(layout);

        builder.setPositiveButton(getString(R.string.ok), (dialog, which) -> {
            String user = inputUser.getText().toString();
            String pass = inputPass.getText().toString();
            prefs.edit()
                    .putString(MQTT_USER, user)
                    .putString(MQTT_PASS, pass)
                    .apply();

            setupMQTT(user, pass);
        });

        builder.setCancelable(false);
        builder.show();
    }

    @Override
    protected void onDestroy() {
        adapter.clearDevices();
        destroyMQTT();

        super.onDestroy();
    }

    void setupMQTT(String user, String pass) {
        if((mqttClient != null && mqttClient.isConnected())) {
            Log.v(TAG, "MQTT client already connected and active");
            return;
        }

        mqttClient = new MQTTClient(
            this,
            MQTT_BROKER,
            user, pass,
            (topic, message) -> runOnUiThread(() -> {
                Log.v(TAG, "update from broker:" + topic + "/" + message);

                if(topic.equalsIgnoreCase(AQUA_DEVICES_UPDATE)) {
                    adapter.updateDevicesFrom(new String(message.getPayload()));
                    updateEmptyView();
                }

                //TODO: others?

            }),
            new MQTTClient.MQTTStatusListener() {
                @Override
                public void onConnected() {

                }

                @Override
                public void onDisconnected() {
                    runOnUiThread(() -> {
                        Toast.makeText(MainActivity.this, getString(R.string.mqttt_connection_end), Toast.LENGTH_SHORT).show();
                    });
                }

                @Override
                public void onConnectionFailed(String reason) {
                    Toast.makeText(MainActivity.this, reason, Toast.LENGTH_SHORT).show();
                }
            });
    }

    void destroyMQTT() {
        Log.v(TAG, "destroy MQTT client");

        if(mqttClient != null) {
            mqttClient.stop();
            mqttClient = null;
        }
    }

}
