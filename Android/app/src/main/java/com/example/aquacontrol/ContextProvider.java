package com.example.aquacontrol;

import android.app.Application;
import android.content.Context;

public class ContextProvider extends Application {
    private static ContextProvider instance;

    public void onCreate() {
        super.onCreate();
        instance = this;
    }

    public static Context getContext() {
        return instance.getApplicationContext();
    }
}
