package com.wl.ffmpegplayer;

import android.content.pm.ActivityInfo;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

public class BaseActivity extends AppCompatActivity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);//始终竖屏
        setStatusBar();
    }

    private void setStatusBar() {
        StatusBarUtil.setTransparent(this);
//        StatusBarUtil.setStatusbarWhiteOrBlack(this, true);
    }
}
