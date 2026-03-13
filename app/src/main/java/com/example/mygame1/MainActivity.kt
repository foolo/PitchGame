package com.example.mygame1

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import androidx.annotation.Keep
import androidx.core.app.ActivityCompat
import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {
    private lateinit var debugTextView: TextView
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    
    // Values updated from C++
    private var lastRms: Float = 0f
    private var lastPitch: Float = 0f

    companion object {
        private const val REQUEST_RECORD_AUDIO_PERMISSION = 200
        init {
            System.loadLibrary("mygame1")
        }
    }

    private external fun analyzeAudio(buffer: ShortArray, size: Int)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        debugTextView = TextView(this).apply {
            setTextColor(Color.YELLOW)
            textSize = 18f
            setBackgroundColor(Color.argb(100, 0, 0, 0))
            setPadding(16, 16, 16, 16)
            text = "..."
        }
        val params = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.START
        ).apply {
            setMargins(50, 50, 0, 0)
        }
        addContentView(debugTextView, params)

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_RECORD_AUDIO_PERMISSION)
        } else {
            startAudioCapture()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_RECORD_AUDIO_PERMISSION && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startAudioCapture()
        }
    }

    private fun startAudioCapture() {
        val sampleRate = 44100
        val channelConfig = AudioFormat.CHANNEL_IN_MONO
        val audioFormat = AudioFormat.ENCODING_PCM_16BIT
        val bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            return
        }
        
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            sampleRate,
            channelConfig,
            audioFormat,
            bufferSize
        )

        audioRecord?.startRecording()
        isRecording = true

        Thread {
            val buffer = ShortArray(bufferSize)
            while (isRecording) {
                val read = audioRecord?.read(buffer, 0, bufferSize) ?: 0
                if (read > 0) {
                    analyzeAudio(buffer, read)
                }
            }
        }.start()
    }

    override fun onStop() {
        super.onStop()
        isRecording = false
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUi()
        }
    }

    @Suppress("DEPRECATION") // TODO: fix deprecation warnings
    private fun hideSystemUi() {
        val decorView = window.decorView
        decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN)
    }

    @Keep
    fun updateDebugInfo(x: Float, y: Float, rms: Float, pitch: Float) {
        runOnUiThread {
            lastRms = rms
            lastPitch = pitch
            debugTextView.text = String.format("Player Pos: (%.2f, %.2f)\nVU-meter: %.0f\nPitch: %.0fHz", x, y, lastRms, lastPitch)
        }
    }
}