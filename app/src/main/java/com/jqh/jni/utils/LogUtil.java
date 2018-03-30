package com.jqh.jni.utils;

/**
 * Author: yangyueyul
 * Date: 2017/4/14 13:15
 * Description:
 */
public class LogUtil {
    public static boolean mIsLogEnable = true;

    public static void i(String info) {
        if (mIsLogEnable) {
            final StackTraceElement[] stack = new Throwable().getStackTrace();
            final StackTraceElement ste = stack[1];
            android.util.Log.i("playersdk", String.format("[%s][%s]%s[%s]",
                    ste.getFileName(), ste.getMethodName(), ste.getLineNumber(), info));
        }
    }

    public static void d(String info) {
        if (mIsLogEnable) {
            final StackTraceElement[] stack = new Throwable().getStackTrace();
            final StackTraceElement ste = stack[1];
            android.util.Log.d("playersdk", String.format("[%s][%s]%s[%s]",
                    ste.getFileName(), ste.getMethodName(), ste.getLineNumber(), info));
        }
    }
}
