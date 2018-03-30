package com.jqh.jni.demojni;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

import com.jqh.jmedia.JMediaPushStream;
import com.jqh.jni.nativejni.MediaPlayerJni;
import com.jqh.jni.utils.LogUtil;

import java.util.ArrayList;

public class PushAVStreamActivity extends AppCompatActivity implements SurfaceHolder.Callback2, Camera.PictureCallback {

    SurfaceView sView;
    SurfaceHolder surfaceHolder;
    Button mStartButton, mStopButton;
    Camera camera;
    TextView filepathView;
    final int MSG_WRITE_YUVDATA = 10004;
    boolean mIsStartPre;
    private MyHandler mHandler = null ;
    private String filePath ;

    private final int REQUEST_PREMISSION=2;
    PcmRecordThread thread;

    private int viewH , viewW ;

    private JMediaPushStream mJMediaPushStream ;

    private boolean isBackCamera = true ;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);


        getWindow().setFormat(PixelFormat.TRANSLUCENT);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_push_avstream);
        initView();

        initEvent();

        initData();
    }

    private void initView(){
        LogUtil.i("pushCamera V1.0111333");
        mStartButton = (Button) findViewById(R.id.start);
        mStopButton = (Button)findViewById(R.id.stop);
        filepathView = (TextView)findViewById(R.id.videopath);

        sView = (SurfaceView)findViewById(R.id.surfaceid);

        mJMediaPushStream = new JMediaPushStream();

    }

    private void initEvent(){
        mStartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // ask av premission
                checkPremission();
            }
        });

        mStopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopLive();
            }
        });

        sView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                doAutoFocus();
            }
        });
    }

    private void initData(){
        mIsStartPre = false ;
        surfaceHolder = sView.getHolder();
        surfaceHolder.addCallback(this);
        surfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        mHandler = new MyHandler();
    }

    public static String getSDPath() {
        String str = "";
        if (Environment.getExternalStorageState().equals("mounted"))
            str = Environment.getExternalStorageDirectory().toString();
        return str;
    }

    private  void checkPremission(){
        if (Build.VERSION.SDK_INT >= 23) {
            Activity activity =this;
            if (activity != null) {
                if (activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                    askForPermissons();
                }else{
                    startLive();
                }
            }
        }else{
            startLive();
        }
    }

    private void askForPermissons() {
        Activity activity =this;
        if (activity == null) {
            return;
        }
        ArrayList<String> permissons = new ArrayList<>();

        if (activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            permissons.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            permissons.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            permissons.add(Manifest.permission.RECORD_AUDIO);
        }
        String[] items = permissons.toArray(new String[permissons.size()]);
        activity.requestPermissions(items, REQUEST_PREMISSION);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if(grantResults[0]==0)
            startLive();
    }

    private void startLive(){
        if(!mIsStartPre){
            mIsStartPre = true ;
//            filePath = getSDPath()+"/outVideo.h264";
//            File file = new File(filePath);
//            if (file.exists())
//                file.delete();
            filePath = "rtmp://livemediabspb.xdfkoo.com/mediaserver/live334422113";
            mJMediaPushStream.publicStreamInit(filePath,viewW,viewH);
            thread=new PcmRecordThread();
            thread.start();
        }
    }

    private void stopLive(){
        if(mIsStartPre)
        {
            mIsStartPre = false ;
            mJMediaPushStream.stopStream();
            thread.stopRecord();
        }
    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder holder) {

    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        camera = Camera.open(isBackCamera?0:1);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

        mJMediaPushStream.setDisplay(holder);


        Camera.Parameters p = camera.getParameters();

        if (Integer.parseInt(Build.VERSION.SDK) >= 8) {
            camera.setDisplayOrientation(90);
        } else {
            if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
                p.set("orientation", "portrait");
                p.set("rotation", 90);
            }
            if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
                p.set("orientation", "landscape");
                p.set("rotation", 90);
            }
        }

        viewW = 480 ;
        viewH = 320 ;



        MyCamPara.showSupportedPreviewFpsRange(p);  //1000 - 35000
        Camera.Size pictureSize,previewSize;
        pictureSize= MyCamPara.getInstance().getPictureSize(p.getSupportedPictureSizes(), 800);
        //预览大小
        previewSize=MyCamPara.getInstance().getPreviewSize(p.getSupportedPreviewSizes(), sView.getHeight());
        if(previewSize!=null) {
            viewW = 960;//960;//previewSize.width ;
            viewH = 544;//544;//previewSize.height ;
            mJMediaPushStream.setVideoSize(viewW,viewH);
            p.setPreviewSize(viewW, viewH);
        }
        if(pictureSize!=null) {
            p.setPictureSize(pictureSize.width, pictureSize.height);
        }
        p.set("jpeg-quality", 100);
       // p.setPictureSize(width,height);
        // p.setPictureFormat(PixelFormat.JPEG);
       // p.setPreviewFormat(PixelFormat.YCbCr_420_SP);
        //p.setPreviewFormat(PixelFormat.YCbCr_420_SP);
        p.setPreviewFormat(ImageFormat.NV21);
        //p.setPreviewFpsRange(25000,35000);
        camera.setPreviewCallback(new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                if(mIsStartPre){
                    Message msg = new Message();
                    Bundle bundle = new Bundle();
                    bundle.putByteArray("messageyuvdata",data);
                    msg.setData(bundle);
                    msg.what = MSG_WRITE_YUVDATA;
                    mHandler.sendMessage(msg);
//                    mJMediaPushStream.flushStreamDataToJni(data,1);
//
                }
            }
        });
        camera.setParameters(p);
//        camera.autoFocus(null);

        // 自己绘制图片
        try {
            camera.setPreviewDisplay(surfaceHolder);
        }catch(Exception e){
            e.printStackTrace();
        }
        camera.startPreview();


        // 实现自动对焦
        camera.autoFocus(new Camera.AutoFocusCallback() {
            @Override
            public void onAutoFocus(boolean success, Camera camera) {
                if (success) {
                    camera.cancelAutoFocus();// 只有加上了这一句，才会自动对焦
                    doAutoFocus();
                }
            }
        });


    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

        if(camera != null)
        {
            camera.setPreviewCallback(null);
            camera.stopPreview();
            camera.release();
            camera = null ;
        }
    }

    @Override
    public void onPictureTaken(byte[] data, Camera camera) {

    }


    class MyHandler extends Handler
    {
        @Override
        public void dispatchMessage(Message msg) {

            switch (msg.what){
                case MSG_WRITE_YUVDATA:
                    byte[] data = msg.getData().getByteArray("messageyuvdata");
                    if(data != null)
                        mJMediaPushStream.flushStreamDataToJni(data,1);
                    break;
                case 2:

            }
        }
    }

    private class PcmRecordThread extends Thread {
        private int sampleRate = 22050;
        private AudioRecord audioRecord;
        private int minBufferSize = 0;
        private boolean isRecording = false;

        public PcmRecordThread() {
            minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);
            minBufferSize = 4096;
            audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT, minBufferSize);

        }

        @Override
        public synchronized void start() {
            audioRecord.startRecording();
            isRecording = true;
            super.start();
        }

        @Override
        public void run() {
            while (isRecording == true) {
                byte[] bytes = new byte[minBufferSize];
                if (audioRecord == null)
                    return;
                int res = audioRecord.read(bytes, 0, minBufferSize);
                if (res > 0 && isRecording == true) {
                    mJMediaPushStream.flushStreamDataToJni(bytes,0);
                }
            }
        }

        public void stopRecord() {
            isRecording = false;
            audioRecord.stop();
            audioRecord.release();
            audioRecord = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mJMediaPushStream = null;
    }



    // 自动对焦
    // handle button auto focus
    private void doAutoFocus() {
        if(!isBackCamera)
            return ;
        final Camera.Parameters parameters1 = camera.getParameters();
        parameters1.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
        camera.setParameters(parameters1);
        camera.autoFocus(new Camera.AutoFocusCallback() {
            @Override
            public void onAutoFocus(boolean success, Camera camera) {
                if (success) {
                    camera.cancelAutoFocus();// 只有加上了这一句，才会自动对焦。
                    if (!Build.MODEL.equals("KORIDY H30")) {
                        Camera.Parameters parameters = camera.getParameters();
                        parameters = camera.getParameters();
                        parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);// 1连续对焦
                        camera.setParameters(parameters);
                    }else{
                        Camera.Parameters parameters = camera.getParameters();
                        parameters = camera.getParameters();
                        parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
                        camera.setParameters(parameters);
                    }
                }
            }
        });
    }

}

