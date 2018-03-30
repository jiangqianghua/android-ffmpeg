package com.jqh.jni.demojni;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import com.jqh.jni.nativejni.MediaPlayerJni;

public class PushStreamVideoActivity extends AppCompatActivity {

    private Button pushStreamBtn;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_push_stream);

        pushStreamBtn = (Button)findViewById(R.id.pushStreamBtn);
        final String inputUrl = "/storage/emulated/0/liveV426050471086090002_142287_0000.mp4";
        final String outputUrl = "rtmp://livemediabspb.xdfkoo.com/mediaserver/live33442211";
        pushStreamBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MediaPlayerJni.publishLocalVideo(inputUrl,outputUrl);
            }
        });
    }
}
