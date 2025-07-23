package com.example.aquacontrol;

import android.content.Context;
import android.util.Log;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttMessageListener;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;

@SuppressWarnings("CallToPrintStackTrace")
public class MQTTClient implements Constants {
    private MqttClient client;

    private final MQTTClient.MQTTStatusListener connectionCallback;
    private final IMqttMessageListener MQTTlistener;

    public interface MQTTStatusListener {
        void onConnected();
        void onDisconnected();
        void onConnectionFailed(String reason);
    }

    public MQTTClient(Context context, String broker, String username, String password,
                      IMqttMessageListener listener, MQTTStatusListener connectionListener) {

        connectionCallback = connectionListener;
        MQTTlistener = listener;

        try {
            String clientId = TAG + System.currentTimeMillis();
            Log.v(TAG, "clientID:" + clientId);

            client = new MqttClient(broker, clientId, null);

            MqttConnectOptions options = new MqttConnectOptions();
            options.setUserName(username);
            options.setPassword(password.toCharArray());
            options.setAutomaticReconnect(true);
            options.setCleanSession(false);

            client.setCallback(new MqttCallbackExtended() {
                @Override
                public void connectComplete(boolean reconnect, String serverURI) {
                    Log.v(TAG, "MQTT connected: " + serverURI + (reconnect ? " (again)" : ""));
                    if(connectionCallback != null) {
                        connectionCallback.onConnected();
                        subscribeTo(AQUA_DEVICES_UPDATE);
                    }
                }

                @Override
                public void connectionLost(Throwable cause) {
                    Log.v(TAG, "MQTT connection lost: " + cause.getMessage());
                    if(connectionCallback != null) {
                        connectionCallback.onDisconnected();
                    }
                }

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    Log.v(TAG, "MQTT message has been received: " + topic + " -> " + new String(message.getPayload()));
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {
                    Log.v(TAG, "MQTT message has been delivered");
                }
            });

            try {
                client.connect(options);
            } catch (MqttException e) {
                e.printStackTrace();
                String reason = e.getReasonCode() + " - " + e.getMessage();
                Log.e(TAG, "Error MQTT connection: " + reason);
                if (connectionCallback != null) {
                    connectionCallback.onConnectionFailed(reason);
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void subscribeTo(String topic) {
        Log.v(TAG, "subscribe to: " + topic);

        try {
            client.unsubscribe(topic);
            client.subscribe(topic, MQTTlistener);
        } catch (MqttException e) {
            e.printStackTrace();
            String reason = e.getReasonCode() + " - " + e.getMessage();
            Log.e(TAG, "Error MQTT subscription: " + reason);
        }
    }

    void stop() {
        if (isConnected()) {
            try {
                client.disconnect();
                client.close();
            } catch (MqttException e) {
                e.printStackTrace();
            }
        }
        client = null;
    }

    public void publish(String topic, String payload) {
        try {
            MqttMessage message = new MqttMessage(payload.getBytes());
            message.setQos(2);
            message.setRetained(true);
            client.publish(topic, message);
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    public boolean isConnected() {
        return client != null && client.isConnected();
    }
}
