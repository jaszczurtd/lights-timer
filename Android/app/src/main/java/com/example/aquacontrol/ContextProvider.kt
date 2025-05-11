package com.example.aquacontrol

import android.app.Application
import android.content.Context

class ContextProvider : Application() {
    override fun onCreate() {
        super.onCreate()
        instance = this
    }

    companion object {
        private var instance: ContextProvider? = null

        @JvmStatic
        val context: Context
            get() = instance!!.applicationContext
    }
}
