package com.wl.ffmpegplayer;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.WindowManager;

public class StatusBarUtil {
    public static void setTransparent(Activity activity) {
        transparentStatusBar(activity);
    }

    private static void transparentStatusBar(Activity activity) {
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        //需要设置这个flag contentView才能延伸到状态栏
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        //状态栏覆盖在contentView上面，设置透明使contentView的背景透出来
        activity.getWindow().setStatusBarColor(Color.TRANSPARENT);
    }

    public static void setStatusbarWhiteOrBlack(Activity activity, boolean isBlack) {
        if(isBlack) {
            activity.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
        }
    }
}
