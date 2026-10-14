package com.autoshieldx.dashboard

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.ViewModelProvider
import com.autoshieldx.dashboard.ui.dashboard.DashboardScreen
import com.autoshieldx.dashboard.ui.splash.FuturisticCarSplashScreen
import com.autoshieldx.dashboard.ui.theme.AutoShieldXTheme
import com.autoshieldx.dashboard.ui.theme.BackgroundLight
import com.autoshieldx.dashboard.viewmodel.DashboardViewModel

class MainActivity : ComponentActivity() {

    private lateinit var viewModel: DashboardViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        viewModel = ViewModelProvider(
            this,
            ViewModelProvider.AndroidViewModelFactory.getInstance(application)
        )[DashboardViewModel::class.java]

        setContent {
            AutoShieldXTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = BackgroundLight
                ) {
                    val uiState by viewModel.uiState.collectAsState()

                    Crossfade(
                        targetState = uiState.isIntroFinished,
                        animationSpec = tween(durationMillis = 600),
                        label = "splashToDashboardTransition"
                    ) { isFinished ->
                        if (!isFinished) {
                            FuturisticCarSplashScreen(
                                onSplashFinished = {
                                    viewModel.completeIntro()
                                }
                            )
                        } else {
                            DashboardScreen(
                                uiState = uiState,
                                onPortChanged = { newPort ->
                                    viewModel.updateListeningPort(newPort)
                                }
                            )
                        }
                    }
                }
            }
        }
    }
}
