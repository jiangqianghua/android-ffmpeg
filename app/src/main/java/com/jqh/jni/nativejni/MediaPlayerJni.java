package com.jqh.jni.nativejni;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.jqh.jni.utils.LogUtil;

/**
 * Created by user on 2017/11/23.
 */
public class MediaPlayerJni {

    private static MediaPlayerJni instance ;
    public static Handler handler ;
    public static int width ;
    public static int height ;
    public static long duration ;// 单位是秒  0 秒一般代表是直播
    // 播放相关
    public static native int playeMedia(String url,long seekTime);
    public static native int stopMedia();
    public static native int pauseMedia();
    public static native int goOnMedia();
    public static native int seekMedia(long seekpos);
    public static native void init();
    public static native void setSpeed(double speed);

    // 推流相关
    public static native void publishLocalVideo(String inputUrl,String outputURL);

    // 录制相关
    public static native  int recordInit(String filename);
    public static native  int recordYuvData(byte[] yuvdata);
    public static native  int recordStop();

    // 推摄像头相关
    public static native int pushCameraInit(String url,int w,int h);
    public static native int pushCameraOnFrame(byte[] data);
    public static native int closePushCamera();

    // 录制音频相关
    public static native int recordMicInit(String filename);
    public static native  int recordAduioPCMData(byte[] data,int len);
    public static native  int recordMicStop();

    //public video and audio stream
    public static native int publicStreamInit(String url,int w,int h);
    public static native  int flushStreamData(byte[] data,int isVideo);
    public static native  int stopStream();


    public static JSurfaceView surfaceView ;

    public static Sonic sonic ;
    static float speed = 2.0f;
    static float pitch = 1.0f;
    static float rate = 2.5f;
    static float volume = 1.0f;
    static boolean emulateChordPitch = false;
    static int quality = 0;
    static int sampleRate ;
    static int numChannels ;


//    static {
//        System.loadLibrary("ffmpegjni");
//    }

    /**
     * 播放结果返回
     * @param code
     * @param w
     * @param h
     */
    public static void playMediaResult(int code , int w, int h,long _duration)
    {
        width = w ;
        height = h ;
        duration = _duration/1000000 ;

        Bundle bundle = new Bundle();
        bundle.putLong("duration",duration);
        bundle.putInt("width",width);
        bundle.putInt("height",height);
        Message msg = new Message();
        msg.what = 2 ;
        msg.obj = null ;
        msg.setData(bundle);
        handler.sendMessage(msg);

        surfaceView.height = height ;
        surfaceView.width = width ;
    }

    public static void initAudio(int sampleRate, boolean is16Bit, boolean isStereo, int desiredFrames)
    {
        JAudioPlayer.audioInit(sampleRate,is16Bit,isStereo ,desiredFrames);
        sonic = new Sonic(sampleRate, numChannels);
        sonic.setSpeed(speed);
        sonic.setPitch(pitch);
        sonic.setRate(rate);
        sonic.setVolume(volume);
        sonic.setChordPitch(emulateChordPitch);
        sonic.setQuality(quality);

    }
    /**
     * 视频数据返回
     * @param data
     */
    public static void callBackVideoData(byte[] data)
    {
        Log.d("callBackVideoData","length="+data.length);
        surfaceView.flushVideoData(data);
    }

    /**
     * 音频数据返回
     * @param data
     */
    public static void callBackAudioData(byte[] data)
    {
        Log.d("callBackAudioData",data.length+"");
        // 经过Sonic处理
//        sonic.writeBytesToStream(data, data.length);
//        byte[] outBuffer = new byte[data.length];
//        int numWritten = sonic.readBytesFromStream(outBuffer, data.length);
        JAudioPlayer.audioWriteByteBuffer(data);
    }

    public synchronized  static void flushStreamDataToJni(final byte[] data,final int isVideo)
    {
        flushStreamData(data,isVideo);
    }


}
