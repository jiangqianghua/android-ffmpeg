package com.jqh.jni.demojni;

import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.jqh.jni.nativejni.MediaPlayerJni;
import com.jqh.jni.utils.LogUtil;

import java.util.Calendar;

/**
 * recode camera
 */
public class CameraRecordeActivity extends AppCompatActivity implements SurfaceHolder.Callback2, Camera.PictureCallback{

    SurfaceView sView;
    SurfaceHolder surfaceHolder;
    Button mStartButton, mStopButton;
    Camera camera;
    TextView filepathView;

    final int MSG_CHECK_PROESS = 10001;// "msg_check_proess";
    final int MSG_CHECK_TOUCH = 10002;// "msg_check_touch";
    final int MSG_WRITE_YUVDATA = 10003;
    boolean mIsStartPre;
    private MyHandler mHandler = null ;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setFormat(PixelFormat.TRANSLUCENT);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_camera_recode);
        initView();

        initEvent();

        initData();


    }

    private void initView(){
        mStartButton = (Button) findViewById(R.id.start);
        mStopButton = (Button)findViewById(R.id.stop);
        filepathView = (TextView)findViewById(R.id.videopath);

        sView = (SurfaceView)findViewById(R.id.surfaceid);

    }

    private void initEvent(){
        mStartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startRecord();
            }
        });

        mStopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopRecord();
            }
        });
    }

    private void initData(){
        LogUtil.d("MediaREcodeCameraJni v1.13444");
        mIsStartPre = false ;
        surfaceHolder = sView.getHolder();
        surfaceHolder.addCallback(this);
        surfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        mHandler = new MyHandler();
    }

    private void startRecord(){
        if(!mIsStartPre){
            mIsStartPre = true ;
            Calendar cc = Calendar.getInstance();
            cc.setTimeInMillis(System.currentTimeMillis());
            String filename = Environment.getExternalStorageDirectory().getAbsolutePath()  + "/"
                    + String.valueOf(cc.get(Calendar.YEAR))
                    + "-"
                    + String.valueOf(cc.get(Calendar.MONTH))
                    + "-"
                    + String.valueOf(cc.get(Calendar.DAY_OF_YEAR))
                    + "-"
                    + String.valueOf(cc.get(Calendar.HOUR_OF_DAY))
                    + "-"
                    + String.valueOf(cc.get(Calendar.MINUTE))
                    + "-"
                    + String.valueOf(cc.get(Calendar.SECOND))
                    + ".mp4";
            LogUtil.i("start open === "+filename);
            MediaPlayerJni.recordInit(filename);
        }
    }

    private void stopRecord(){
        if(mIsStartPre)
        {
            mIsStartPre = false ;
            MediaPlayerJni.recordStop();
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
        p.setPreviewSize(352,288);
        p.setPictureFormat(PixelFormat.JPEG);
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
                        MediaPlayerJni.recordYuvData(data);
                    break;
                case 2:

            }
        }
    }
}
