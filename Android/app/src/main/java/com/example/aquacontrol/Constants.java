package com.example.aquacontrol;

public interface Constants {
    String TAG = "AquaControlDebug";

    int LOADER_DELAY = 1500;
    int MAX_AMOUNT_OF_RELAYS = 4;
    String MQTT_BROKER = "tcp://10.8.0.1:1883";
    String MQTT_CREDENTIALS = "aqua_mqtt_prefs";
    String MQTT_USER = "aqua_mqtt_user";
    String MQTT_PASS = "aqua_mqtt_pass";
    String AQUA_DEVICES_UPDATE = "discovered/devices";
    String AQUA_DEVICE_STATUS = "status-";
    String AQUA_DEVICE_TIME_SET = "time-";
    String AQUA_DEVICE_SWITCH_SET = "switch-";

    String dateHourStart = "dateHourStart";
    String dateHourEnd = "dateHourEnd";
    String isOn = "isOn";
}
