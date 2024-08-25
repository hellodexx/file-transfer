package com.example.dexfiletransfer

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

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
                "Dex File Transfer Service",
                NotificationManager.IMPORTANCE_DEFAULT
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(notificationChannel)
        }

        val notification: Notification = NotificationCompat.Builder(this, "ForegroundServiceChannel")
            .setContentTitle("Dex File Transfer Service")
            .setContentText("Dex File Transfer service is running...")
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
