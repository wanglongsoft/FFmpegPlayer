package com.wl.ffmpegplayer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.widget.ImageView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends BaseActivity {
    private final String TAG = "FFmpegPlayer";

    //private final static String PATH = Environment.getExternalStorageDirectory() + File.separator + "filefilm" + File.separator + "mediatest2.mp4";
    //private final static String PATH = "rtmp://58.200.131.2:1935/livetv/gxtv";
    private final static String PATH = "rtmp://58.200.131.2:1935/livetv/hunantv";
    private SurfaceView mSurfaceView = null;
    private FFmpegPlayer mFFmpegPlayer = null;

    private ImageView mPlayPause;
    private boolean isPlayState;
    private MyRubnable runnable;
    private Handler mHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mHandler = new Handler();
        isPlayState = false;
        runnable = new MyRubnable();
        mPlayPause = findViewById(R.id.play_pause_icon);

        mPlayPause.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d(TAG, "onClick: play");
                if(!isPlayState) {
                    isPlayState = true;
                    mFFmpegPlayer.startFFmpegPlayer();
                    mPlayPause.setImageResource(R.drawable.ic_view_array_white_36dp);
                } else {
                    isPlayState = false;
                    mFFmpegPlayer.pauseFFmpegPlayer();
                    mPlayPause.setImageResource(R.drawable.ic_change_history_white_36dp);
                }
                setDelayButtonInvisible();
            }
        });


        requestMyPermissions();
        mSurfaceView = findViewById(R.id.view_player);
        mFFmpegPlayer = new FFmpegPlayer();
        mFFmpegPlayer.setSurfaceView(mSurfaceView);
        mFFmpegPlayer.setDataSource(PATH);
        mFFmpegPlayer.setPreparedListener(new FFmpegPlayer.OnPreparedListener() {
            @Override
            public void onPrepared() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "Video is onPrepared", Toast.LENGTH_SHORT).show();
                    }
                });
            }

            @Override
            public void onError(String errorText) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "Video is onError", Toast.LENGTH_SHORT).show();
                    }
                });
            }
        });
    }

    private void requestMyPermissions() {

        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            //没有授权，编写申请权限代码
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 100);
        } else {
            Log.d(TAG, "requestMyPermissions: 有写SD权限");
        }
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            //没有授权，编写申请权限代码
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 100);
        } else {
            Log.d(TAG, "requestMyPermissions: 有读SD权限");
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mFFmpegPlayer.prepare();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mFFmpegPlayer.stopFFmpegPlayer();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mFFmpegPlayer.releaseFFmpegPlayer();
    }

    private void setDelayButtonInvisible() {
        if(mHandler.hasCallbacks(runnable)) {
            mHandler.removeCallbacks(runnable);
        }
        mHandler.postDelayed(runnable, 5000);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if(ev.getAction() == MotionEvent.ACTION_DOWN) {
            if(mPlayPause.getVisibility() != View.VISIBLE) {
                mPlayPause.setVisibility(View.VISIBLE);
                mPlayPause.setEnabled(true);
                setDelayButtonInvisible();
            }
        }
        return super.dispatchTouchEvent(ev);
    }

    class MyRubnable implements Runnable {

        @Override
        public void run() {
            mPlayPause.setVisibility(View.INVISIBLE);
            mPlayPause.setEnabled(false);
        }
    }
}
