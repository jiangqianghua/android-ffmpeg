package com.jqh.jni.demojni;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;

import com.jqh.jmedia.JMediaPlayer;

public class JPlayFullActivity extends AppCompatActivity  implements SurfaceHolder.Callback{

    private SurfaceView surfaceView ;
    private Button playBtn ;
    private JMediaPlayer mediaPlayer ;
    private boolean isPlay = false ;
    private String playUrl = "rtmp://livemediabspl.xdfkoo.com/mediaserver/live334422113";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_jplay_full);

        surfaceView = (SurfaceView)findViewById(R.id.surfaceViewid);
        playBtn = (Button)findViewById(R.id.playBtn);


        mediaPlayer = new JMediaPlayer(this);


        surfaceView.getHolder().addCallback(this);
        playBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!isPlay){
                    playBtn.setText("停止");
                    start();
                    isPlay = true ;
                }else{
                    playBtn.setText("播放");
                    stop();
                    isPlay = false ;
                }
            }
        });


        mediaPlayer.setOnPreparedListener(new JMediaPlayer.OnPreparedListener() {
            @Override
            public void onPrepared(JMediaPlayer mp) {
                mediaPlayer.start();

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
    }

    private void stop(){
        mediaPlayer.stop();
    }

    private void start(){
        mediaPlayer.setDataSource(playUrl);
        try {
            mediaPlayer.prepareAsync();
        }catch (NullPointerException e){
            e.printStackTrace();
        }
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
