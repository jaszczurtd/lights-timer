package com.example.aquacontrol;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.IOException;

import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Map;

public class HttpRequestHelper {

    public static String PARAM_DATE_HOUR_START = "dateHourStart";
    public static String PARAM_DATE_HOUR_END = "dateHourEnd";
    public static String PARAM_IS_ON = "isOn";

    public static String METHOD = "http://";

    public interface Callback {
        void onResponse(int status, String response);
        void onError(Exception e);
    }

    public static void get(String urlString, Callback callback) {
        new Thread(() -> {
            try {
                URL url = new URL(urlString);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("GET");

                int status = conn.getResponseCode();
                String response = readResponse(conn);
                callback.onResponse(status, response);
            } catch (Exception e) {
                callback.onError(e);
            }
        }).start();
    }

    public static void post(String urlString, Map<String, String> params, Callback callback) {
        new Thread(() -> {
            try {
                URL url = new URL(urlString);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setDoOutput(true);
                conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");

                String postData = buildPostData(params);

                try (OutputStream os = conn.getOutputStream()) {
                    os.write(postData.getBytes(StandardCharsets.UTF_8));
                }

                int status = conn.getResponseCode();
                String response = readResponse(conn);
                callback.onResponse(status, response);
            } catch (Exception e) {
                callback.onError(e);
            }
        }).start();
    }

    @SuppressWarnings("CharsetObjectCanBeUsed")
    private static String buildPostData(Map<String, String> params) throws IOException {
        StringBuilder result = new StringBuilder();
        boolean first = true;

        for (Map.Entry<String, String> entry : params.entrySet()) {
            if (!first) result.append("&");
            result.append(URLEncoder.encode(entry.getKey(), "UTF-8"));
            result.append("=");
            result.append(URLEncoder.encode(entry.getValue(), "UTF-8"));
            first = false;
        }

        return result.toString();
    }

    private static String readResponse(HttpURLConnection conn) throws IOException {
        int status = conn.getResponseCode();
        BufferedReader reader = new BufferedReader(new InputStreamReader(
                status >= 200 && status < 300 ? conn.getInputStream() : conn.getErrorStream()
        ));
        StringBuilder response = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) response.append(line);
        reader.close();
        return response.toString();
    }
}
