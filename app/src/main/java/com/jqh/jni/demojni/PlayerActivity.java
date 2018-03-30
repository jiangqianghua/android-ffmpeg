package com.jqh.jni.demojni;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Spinner;

import com.jqh.jmedia.JMediaPlayer;

import java.util.ArrayList;
import java.util.List;

/**
 * 真实回放
 */
public class PlayerActivity extends AppCompatActivity implements SurfaceHolder.Callback{


    private SurfaceView surfaceView ;
    private Button start;
    private Button stop ;

    private JMediaPlayer mediaPlayer ;
    private int playStatus = 0 ; //0 停止   1播放  2暂停

    private String path = "http://v.xdfkoo.com/70873/liveVdesk60039339115274640_70873_0000.mp4";
    //private String path = "http://v.xdfkoo.com/126856/liveV421091964379749002_126856_0000.mp4";
    //"http://live.hkstv.hk.lxdns.com/live/hks/playlist.m3u8";
    // http://v.xdfkoo.com/70873/liveVdesk60039339115274640_70873_0000.mp4 ";
    //http://v.xdfkoo.com/126856/liveV421091964379749002_126856_0000.mp4
    private SeekBar seekBar ;
    private Spinner speedSpinner ;
    private ArrayAdapter<String> speed_adapter;
    private List<String> speed_list ;
    private float speed = 1.0f ;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_player);

        mediaPlayer = new JMediaPlayer(this);

        initView();

        initEvent();

        surfaceView.getHolder().addCallback(this);
        //mediaPlayer.setDisplay(surfaceView.getHolder());

    }

    private void initView(){
        surfaceView = (SurfaceView)findViewById(R.id.surfaceid);
        start = (Button)findViewById(R.id.start_play);
        stop = (Button)findViewById(R.id.stop_play);
        seekBar = (SeekBar)findViewById(R.id.progress);
        speedSpinner = (Spinner)findViewById(R.id.speedSpinner);


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



    }

    private void initEvent(){
        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(playStatus == 0 || playStatus == 2){
                    start.setText("暂停");
                    if(playStatus == 2)
                        mediaPlayer.start();
                    else
                        start();
                    playStatus = 1 ;
                }else if(playStatus == 1){
                    playStatus = 2;
                    start.setText("继续");
                    pause();
                }
            }
        });

        stop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playStatus = 0;
                start.setText("播放");
                stop();
            }
        });

        mediaPlayer.setOnPreparedListener(new JMediaPlayer.OnPreparedListener() {
            @Override
            public void onPrepared(JMediaPlayer mp) {
                mediaPlayer.start();

                // get duraction
                int  dur = (int)(mediaPlayer.getDuration()/(1000*1000));
                seekBar.setMax(dur);

            }
        });

        mediaPlayer.setOnSeekCompleteListener(new JMediaPlayer.OnSeekCompleteListener() {
            @Override
            public void onSeekComplete(JMediaPlayer mp) {

            }
        });

        mediaPlayer.setOnCompletionListener(new JMediaPlayer.OnCompletionListener() {
            @Override
            public void onCompletion(JMediaPlayer mp) {

            }
        });

        mediaPlayer.setOnErrorListener(new JMediaPlayer.OnErrorListener() {
            @Override
            public boolean onError(JMediaPlayer mp, int what, int extra) {
                return false;
            }
        });

        mediaPlayer.setOnInfoListener(new JMediaPlayer.OnInfoListener() {
            @Override
            public boolean onInfo(JMediaPlayer mp, int what, int extra) {
                return false;
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
                mediaPlayer.seekTo(seekBar.getProgress());
            }
        });

        speedSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                // 设置倍速
                speed = Float.parseFloat(speed_list.get(position)) ;
                mediaPlayer.setPlaybackSpeed(speed);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });

    }

    private void start(){
        mediaPlayer.setDataSource(path);
        mediaPlayer.setPlaybackSpeed(speed);
        try {
            mediaPlayer.prepareAsync();
        }catch (NullPointerException e){
            e.printStackTrace();
        }
    }

    private void stop(){
        mediaPlayer.stop();
    }

    private void pause(){
        mediaPlayer.pause();
    }


    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mediaPlayer.setDisplay(surfaceView.getHolder());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    @Override
    protected void onDestroy() {
        mediaPlayer.release();
        super.onDestroy();
    }
}
