package com.example.dexfiletransfer

import android.Manifest
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.example.dexfiletransfer.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
//        binding.sampleText.text = stringFromJNI()
        binding.sampleText.text = "Hello World!"

        requestPermissions(arrayOf(Manifest.permission.READ_MEDIA_IMAGES,
            Manifest.permission.READ_MEDIA_VIDEO,
            Manifest.permission.READ_EXTERNAL_STORAGE), 0) // Add checking

        GlobalScope.launch(Dispatchers.IO) {
            runDexFileTransferServerJNI()
        }
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