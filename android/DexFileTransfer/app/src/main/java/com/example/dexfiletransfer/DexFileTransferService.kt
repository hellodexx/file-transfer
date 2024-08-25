package com.example.dexfiletransfer

import android.app.ForegroundServiceStartNotAllowedException
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Process
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlin.concurrent.thread
import kotlin.math.log

class DexFileTransferService : Service() {
    override fun onBind(intent: Intent): IBinder? {
        TODO("Return the communication channel to the service.")
        println("DexLog xxxxxxx service onBind")
        return null
    }

    override fun onCreate() {
        super.onCreate()
        println("DexLog xxxxxxx service onCreate")

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationChannel = NotificationChannel(
                "ForegroundServiceChannel",
                "Foreground Service",
                NotificationManager.IMPORTANCE_DEFAULT
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(notificationChannel)
        }

        val notification: Notification = NotificationCompat.Builder(this, "ForegroundServiceChannel")
            .setContentTitle("Foreground Service")
            .setContentText("Service is running...")
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .build()

        startForeground(1, notification)

        Thread {
            runDexFileTransferServerJNI()
        }.start()

        println("DexLog xxxxxxx service onCreate end")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        println("DexLog xxxxxxx service onStartCommand")

//        runDexFileTransferServerJNI()
        return START_STICKY
    }

    override fun onDestroy() {
        println("DexLog xxxxxx service onDestroy")
        super.onDestroy()
    }

    init {
        System.loadLibrary("dexfiletransfer")
    }

    external fun runDexFileTransferServerJNI()
}
