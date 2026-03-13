package com.example.mygame1

import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import androidx.annotation.Keep
import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {
    private lateinit var debugTextView: TextView

    companion object {
        init {
            System.loadLibrary("mygame1")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        debugTextView = TextView(this).apply {
            setTextColor(Color.YELLOW)
            textSize = 18f
            setBackgroundColor(Color.argb(100, 0, 0, 0))
            setPadding(16, 16, 16, 16)
            text = "Ball Pos: (0.00, 0.00)"
        }
        val params = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.START
        ).apply {
            setMargins(50, 50, 0, 0)
        }
        addContentView(debugTextView, params)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUi()
        }
    }

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
    fun updateDebugInfo(x: Float, y: Float) {
        runOnUiThread {
            debugTextView.text = String.format("Ball Pos: (%.2f, %.2f)", x, y)
        }
    }
}