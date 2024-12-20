package com.example.dexfiletransfer

import android.Manifest
import android.content.Context
import android.content.Intent
import android.media.MediaScannerConnection
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.example.dexfiletransfer.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import android.net.wifi.WifiManager
import android.text.format.Formatter
import android.widget.Switch
import android.widget.Toast
import java.io.File

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
//        binding.sampleText.text = "Local IP Address: $localIpAddress"

        requestPermissions(arrayOf(
            Manifest.permission.READ_MEDIA_IMAGES,
            Manifest.permission.READ_MEDIA_VIDEO,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.ACCESS_MEDIA_LOCATION,
            Manifest.permission.FOREGROUND_SERVICE,
            Manifest.permission.FOREGROUND_SERVICE_SPECIAL_USE,
            Manifest.permission.POST_NOTIFICATIONS,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
            ),
            0) // Add checking

//        GlobalScope.launch(Dispatchers.IO) {
//            runDexFileTransferServerJNI()
//        }

        intent = Intent(this, DexFileTransferService::class.java)

        val switchButton: Switch = findViewById(R.id.ftServerSwitch)
        switchButton.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                // Action when switched ON
                Toast.makeText(this, "DexFT Server is ON", Toast.LENGTH_SHORT).show()
                binding.sampleText.text = "Server local IP: $localIpAddress"
                // Start the service when the switch is ON
                startForegroundService(intent)
            } else {
                // Action when switched OFF
                Toast.makeText(this, "DexFT Server is OFF", Toast.LENGTH_SHORT).show()
                binding.sampleText.text = "Server is OFF"
                // Stop the service when the switch is OFF
                stopService(intent)
            }
        }
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

        // Function to scan all files in a directory
        fun scanDirectory(context: Context, directoryPath: String) {
            val directory = File(directoryPath)
            if (directory.exists() && directory.isDirectory) {
                val files = directory.listFiles()
                files?.forEach { file ->
                    if (file.isFile) {
                        scanFile(context, file.absolutePath)
                    }
                }
            } else {
                println("DexLog Directory does not exist or is not a directory.")
            }
        }

        // Helper function to scan a single file
        private fun scanFile(context: Context, path: String) {
            MediaScannerConnection.scanFile(
                context, arrayOf(path), null
            ) { _, uri ->
                // Scanning completed
                println("DexLog Scanned $path -> uri=$uri")
            }
        }
    }
}