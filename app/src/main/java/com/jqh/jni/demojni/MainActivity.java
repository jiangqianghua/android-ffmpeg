package com.jqh.jni.demojni;

import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import com.jqh.jni.nativejni.JSurfaceView;
import com.jqh.jni.nativejni.MediaPlayerJni;
import com.jqh.jni.utils.LogUtil;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    private TextView mediaUrl;
    private Button playerAndStopBtn ;
    private Button pauseAndGoOnBtn ;
    private ImageView image;
    private MyHandler handler ;
    private SeekBar seekBar ;
    private boolean isStop = true ;
    private boolean isPause = false ;
    private JSurfaceView surfaceView ;
    // 下拉列表相关
    private Spinner spinner ;
    private List<String> data_list;
    private List<String> speed_list ;
    private ArrayAdapter<String> arr_adapter;
    private String curUrl ;
    private int pos = 0 ;
    private Spinner speedSpinner ;
    private ArrayAdapter<String> speed_adapter;
    private int speedPos = 0 ;

    private Button gotoPushVideoStreamActivity;
    private Button gotoRecordCameraActivity ;
    private Button gotoPushCameraActivity ;
    private Button gotoRecordMicActivity ;
    private Button gotoPublicAVStream ;
    private Button gotoPlayerActivity ;
    private Button gotoLiveActivity ;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mediaUrl = (TextView)findViewById(R.id.mediaurl);
        playerAndStopBtn = (Button)findViewById(R.id.playeAndStopBtn);
        pauseAndGoOnBtn = (Button)findViewById(R.id.pauseAndGoOnBtn);
        seekBar = (SeekBar)findViewById(R.id.progress);

        surfaceView = (JSurfaceView) findViewById(R.id.surfaceViewid);
        spinner = (Spinner) findViewById(R.id.spinner);
        speedSpinner = (Spinner)findViewById(R.id.speedSpinner);
        curUrl = mediaUrl.getText().toString() ;

        gotoPushVideoStreamActivity = (Button)findViewById(R.id.gotoPushStreamActivity);
        gotoRecordCameraActivity = (Button)findViewById(R.id.gotoRecordCameraActivity);
        gotoPushCameraActivity = (Button)findViewById(R.id.gotoPushCameraActivity);
        gotoRecordMicActivity = (Button)findViewById(R.id.gotoRecordMicActivity);
        gotoPublicAVStream = (Button)findViewById(R.id.gotoPublicAVStream);
        gotoPlayerActivity = (Button)findViewById(R.id.gotoPlayerActivity);
        gotoLiveActivity = (Button)findViewById(R.id.gotoLiveActivity);
        //rtmp://live.hkstv.hk.lxdns.com/live/hks
        //http://10.155.32.236:8080/zatan.mp4
       // http://10.155.32.236:8080/live1.mp4
       // https://v.xdfkoo.com/126856/liveV421091964379749002_126856_0000.mp4

        playerAndStopBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(isStop) {
                    if(!"".equals(mediaUrl.getText()))
                        curUrl = mediaUrl.getText().toString() ;
                    else
                        curUrl = data_list.get(pos);
                    double speed = Double.parseDouble(speed_list.get(speedPos));
                    MediaPlayerJni.setSpeed(speed);
                    LogUtil.i("url:"+curUrl);
                    int result = MediaPlayerJni.playeMedia(curUrl,0);
                    if(result >= 0) {
                        playerAndStopBtn.setText("关闭");
                    }
                    LogUtil.d("play result " + result + "  url=" + curUrl);
                    isStop = false ;
                }
                else
                {
                    int result = MediaPlayerJni.stopMedia();
                    playerAndStopBtn.setText("播放");
                    pauseAndGoOnBtn.setText("暂停");
                    isStop = true ;

                }
            }
        });
        pauseAndGoOnBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MediaPlayerJni.pauseMedia();
                if(!isStop)
                {
                    if(!isPause)
                    {
                        MediaPlayerJni.pauseMedia();
                        pauseAndGoOnBtn.setText("继续");
                        isPause = true ;
                    }
                    else
                    {
                        MediaPlayerJni.goOnMedia();
                        pauseAndGoOnBtn.setText("暂停");
                        isPause = false ;
                    }
                }
            }
        });

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                MediaPlayerJni.seekMedia(seekBar.getProgress());
            }
        });

        gotoPushVideoStreamActivity.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this,PushStreamVideoActivity.class);
                startActivity(intent);
            }
        });

        gotoRecordCameraActivity.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this,CameraRecordeActivity.class);
                startActivity(intent);
            }
        });

        gotoPushCameraActivity.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this,PushCameraActivity.class);
                startActivity(intent);
            }
        });

        gotoRecordMicActivity.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                gotoActivity(MicRecordeActivity.class);
            }
        });

        gotoPublicAVStream.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                gotoActivity(PushAVStreamActivity.class);
            }
        });
        gotoPlayerActivity.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                gotoActivity(PlayerActivity.class);
            }
        });
        gotoLiveActivity.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                gotoActivity(JPlayFullActivity.class);
            }
        });
        handler = new MyHandler();
        MediaPlayerJni.handler = handler;
        image = (ImageView)findViewById(R.id.imageView);

        MediaPlayerJni.surfaceView = surfaceView ;
        //MediaPlayerJni.init();

        initData();
        initEvent();
    }

    private void gotoActivity(Class clazz)
    {
        Intent intent = new Intent(MainActivity.this,clazz);
        startActivity(intent);
    }

    private void initData(){
        //数据
        data_list = new ArrayList<String>();
        data_list.add("http://v.xdfkoo.com/126856/liveV421091964379749002_126856_0000.mp4");
        data_list.add("rtmp://live.hkstv.hk.lxdns.com/live/hks");
        data_list.add("http://10.155.32.236:8080/zatan.mp4");
        data_list.add("http://10.155.32.236:8080/live1.mp4");
        data_list.add("rtmp://livemediaxl.xdfkoo.com/mediaserver/liveV422090395176822002");
        data_list.add("/storage/emulated/0/outAudio.aac");
        data_list.add("/storage/emulated/0/outVideo.h264");
        data_list.add("/storage/emulated/0/liveV426050471086090002_142287_0000.mp4");
        data_list.add("rtmp://livemediabspl.xdfkoo.com/mediaserver/livemic334422113");
        data_list.add("rtmp://livemediabspl.xdfkoo.com/mediaserver/live334422113");


        LogUtil.i("v5V00000055555555555");
        //适配器
        arr_adapter= new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, data_list);
        //设置样式
        arr_adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        //加载适配器
        spinner.setAdapter(arr_adapter);


        speed_list = new ArrayList<String>();
        speed_list.add("1.0");
        speed_list.add("0.5");
        speed_list.add("0.8");
        speed_list.add("1.25");
        speed_list.add("1.5");
        speed_list.add("1.75");
        speed_list.add("2");

        //适配器
        speed_adapter= new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, speed_list);
        //设置样式
        speed_adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        //加载适配器
        speedSpinner.setAdapter(speed_adapter);

        pos = 0 ;
    }

    private void initEvent(){
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                pos = position ;
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });

        speedSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                speedPos = position ;
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });
    }

    class MyHandler extends Handler
    {
        @Override
        public void dispatchMessage(Message msg) {

            switch (msg.what){
                case 1:
                    byte[] imagebytes = msg.getData().getByteArray("data");
//                    Bitmap stitchBmp = Bitmap.createBitmap(MediaPlayerJni.width, MediaPlayerJni.height, Bitmap.Config.RGB_565);
//                    stitchBmp.copyPixelsFromBuffer(ByteBuffer.wrap(imagebytes));
//                    image.setImageBitmap(stitchBmp);
                    break;
                case 2:
                    long duration = msg.getData().getLong("duration");
                    int width = msg.getData().getInt("width");
                    int height = msg.getData().getInt("height");
                    seekBar.setMax((int)duration);
            }
        }
    }
}
