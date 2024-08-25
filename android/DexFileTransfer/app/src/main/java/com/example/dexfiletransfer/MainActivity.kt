package com.example.dexfiletransfer

import android.Manifest
import android.content.Context
import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.example.dexfiletransfer.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import android.net.wifi.WifiManager
import android.text.format.Formatter

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        println("DexLog onCreate")
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
//        binding.sampleText.text = stringFromJNI()
        val localIpAddress = getLocalIpAddress(this)
        binding.sampleText.text = "Local IP Address: $localIpAddress"

        requestPermissions(arrayOf(
            Manifest.permission.READ_MEDIA_IMAGES,
            Manifest.permission.READ_MEDIA_VIDEO,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.ACCESS_MEDIA_LOCATION,
            Manifest.permission.FOREGROUND_SERVICE,
            Manifest.permission.FOREGROUND_SERVICE_SPECIAL_USE
            ),
            0) // Add checking

//        GlobalScope.launch(Dispatchers.IO) {
//            runDexFileTransferServerJNI()
//        }

        intent = Intent(this, DexFileTransferService::class.java)
        startForegroundService(intent)
    }

    fun getLocalIpAddress(context: Context): String {
        val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val ipAddress = wifiManager.connectionInfo.ipAddress
        return Formatter.formatIpAddress(ipAddress)
    }

    override fun onDestroy() {
        super.onDestroy()
        stopService(intent)
        println("DexLog onDestroy")
    }
    /**
     * A native method that is implemented by the 'dexfiletransfer' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String
    external fun runDexFileTransferServerJNI(): String

    companion object {
        // Used to load the 'dexfiletransfer' library on application startup.
        init {
            System.loadLibrary("dexfiletransfer")
        }
    }
}