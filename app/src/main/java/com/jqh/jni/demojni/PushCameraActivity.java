package com.jqh.jni.demojni;

import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

import com.jqh.jni.nativejni.MediaPlayerJni;
import com.jqh.jni.utils.LogUtil;

import java.io.File;
import java.util.Calendar;

//push camera stream
public class PushCameraActivity extends AppCompatActivity implements SurfaceHolder.Callback2, Camera.PictureCallback{

    SurfaceView sView;
    SurfaceHolder surfaceHolder;
    Button mStartButton, mStopButton;
    Camera camera;
    TextView filepathView;

    final int MSG_WRITE_YUVDATA = 10004;
    boolean mIsStartPre;
    private MyHandler mHandler = null ;

    private String filePath ;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setFormat(PixelFormat.TRANSLUCENT);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_push_camera);
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

    }

    private void initEvent(){
        mStartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startLive();
            }
        });

        mStopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopLive();
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



    private void startLive(){
        if(!mIsStartPre){
            mIsStartPre = true ;

            filePath = getSDPath()+"/outVideo.h264";
            File file = new File(filePath);
            if (file.exists())
            file.delete();
            //filePath = "rtmp://livemediabspb.xdfkoo.com/mediaserver/livecam334422113";
            MediaPlayerJni.pushCameraInit(filePath,sView.getWidth(),sView.getHeight());
        }
    }

    private void stopLive(){
        if(mIsStartPre)
        {
            mIsStartPre = false ;
            MediaPlayerJni.closePushCamera();
        }
    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder holder) {

    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        camera = Camera.open();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Camera.Parameters p = camera.getParameters();
        p.setPreviewSize(480,320);
       // p.setPictureSize(480,320);
       // p.setPictureFormat(PixelFormat.JPEG);
        p.setPreviewFormat(PixelFormat.YCbCr_420_SP);
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
                }
            }
        });
        camera.setParameters(p);
        try {
            camera.setPreviewDisplay(surfaceHolder);
        }catch(Exception e){
            e.printStackTrace();
        }
        camera.startPreview();
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
                        MediaPlayerJni.pushCameraOnFrame(data);
                    break;
                case 2:

            }
        }
    }

}
