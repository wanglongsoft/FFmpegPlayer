package com.wl.ffmpegplayer;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.HashMap;

public class FFmpegPlayer implements SurfaceHolder.Callback {
    final String TAG = "FFmpegPlayer";

    public SurfaceView mSurfaceView;
    public String mDataSource;
    public OnPreparedListener listener;

    public HashMap<Integer, String> mErrorCode;
    static {
        System.loadLibrary("ffmpeg-player");
    }

    public FFmpegPlayer() {
        mErrorCode = new HashMap<>();
        mErrorCode.put(1, "");
    }


    public void setSurfaceView(SurfaceView surfaceView) {
        mSurfaceView = surfaceView;
        mSurfaceView.getHolder().removeCallback(this);
        mSurfaceView.getHolder().addCallback(this);
    }

    public void setPreparedListener(OnPreparedListener listener) {
        this.listener = listener;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated: ");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "surfaceChanged: ");
        setFFmpegSurface(holder.getSurface());
        Log.d(TAG, "surfaceChanged: width : " + mSurfaceView.getWidth() + " height: " + mSurfaceView.getHeight());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed: ");
    }

    public void setDataSource(String path) {
        mDataSource = path;
    }

    public void prepare() {
        Log.d(TAG, "prepare: ");
        prepareFFmpegPlayer(mDataSource);
    }

    public void onPrepared() {
        Log.d(TAG, "onPrepared: ");
        if(null != listener) {
            listener.onPrepared();
        }
    }

    public void onError(int errorCode) {
        Log.d(TAG, "onError errorCode : " + errorCode);
        if(null != listener) {
            listener.onError("");
        }
    }

    public interface OnPreparedListener {
        void onPrepared();
        void onError(String errorText);
    }

    // native method

    public native void prepareFFmpegPlayer(String dataSource);
    public native void startFFmpegPlayer();
    public native void pauseFFmpegPlayer();
    public native void stopFFmpegPlayer();
    public native void releaseFFmpegPlayer();
    public native void setFFmpegSurface(Surface surface);
}
