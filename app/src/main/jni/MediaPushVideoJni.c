#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/version.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#define OPEN_LOG

#define LOG_TAG    "MediaPushVideoJni"
#undef LOG
#ifdef  OPEN_LOG
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,LOG_TAG,__VA_ARGS__)
#else
#define LOGD(...)
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#define LOGF(...)
#endif
// 主要是推视频文件流，也可以推在线的文件，以及rtmpt流
// 开始推本地视频
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_publishLocalVideo(JNIEnv *env,jobject jobject,
                                                            jstring input_jstr,jstring output_jstr)
{
    LOGD("publishLocalVideo init");
    AVOutputFormat *ofmt = NULL;

    AVFormatContext * ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt ; 

    int ret , i ;

    char input_str[500] = {0};
    char output_str[500] = {0};

    char info[1000] = {0};
    sprintf(input_str,"%s",(*env)->GetStringUTFChars(env,input_jstr,NULL));
    sprintf(output_str,"%s",(*env)->GetStringUTFChars(env,output_jstr,NULL));

    av_register_all();

    avformat_network_init();

    if((ret == avformat_open_input(&ifmt_ctx,input_str,0,0)) < 0)
    {
        LOGE("Could not open intput file");
        return -1;
    }

    if((ret == avformat_find_stream_info(ifmt_ctx,0)) < 0)
    {
        LOGE("Could not retrieve input stream information");
        return -2;
    }

    int videoindex = -1;

    for(i = 0 ; i < ifmt_ctx->nb_streams ; i++)
    {
        if(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }

    // output  -- rtmpt
    avformat_alloc_output_context2(&ofmt_ctx,NULL,"flv",output_str);

    if(!ofmt_ctx)
    {
        LOGE("Could con create output context");
        return -3;
    }

    ofmt = ofmt_ctx->oformat ;
    for(i = 0 ; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream * in_stream = ifmt_ctx->streams[i];
        AVStream * out_stream = avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
        if(!out_stream){
            LOGE("failed allocation output stream!");
            return -4;
        }

        ret = avcodec_copy_context(out_stream->codec,in_stream->codec);
        if(ret < 0)
        {
            LOGE("failed to copy context from input to output stream codec context");
            return -5;
        }

        out_stream->codec->codec_tag = 0 ;

        if(ofmt_ctx->oformat->flags & AVFMT_NOFILE)
        {
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    // open out url
    if(!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb,output_str,AVIO_FLAG_WRITE);
        if(ret < 0)
        {
            LOGE("could not open output url '%s'",output_str);
            return -6;
        }
    }

    // write file header
    ret = avformat_write_header(ofmt_ctx,NULL);
    if(ret < 0)
    {
        LOGE("Error occurred when opening output URL\n");
        return -7;
    }

    int frame_index = 0 ;

    int64_t start_time = av_gettime();

    while(1)
    {
        AVStream *in_stream , *out_stream;
        ret = av_read_frame(ifmt_ctx,&pkt);
        if(ret < 0)
            break;
        if(pkt.pts == AV_NOPTS_VALUE)
        {
            AVRational time_base1 = ifmt_ctx->streams[videoindex]->time_base;
            int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
            pkt.pts = (double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
        }

        if(pkt.stream_index == videoindex)
        {
            AVRational time_base = ifmt_ctx->streams[videoindex]->time_base;
            AVRational time_base_q = {1,AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(pkt.dts,time_base,time_base_q);
            int64_t now_time = av_gettime()-start_time;
            if(pts_time > now_time)
                av_usleep(pts_time - now_time);
        }

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        pkt.pts = av_rescale_q_rnd(pkt.pts,in_stream->time_base,out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.dts = av_rescale_q_rnd(pkt.dts,in_stream->time_base,out_stream->time_base,AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration,in_stream->time_base,out_stream->time_base);
        pkt.pos = -1;
        // print to screen
        if(pkt.stream_index == videoindex)
        {
            LOGD("Send %8d video frames to output URL\n",frame_index);
            frame_index++;
        }

        ret = av_interleaved_write_frame(ofmt_ctx,&pkt);
        if(ret < 0)
        {
            LOGE("Error muxing packet\n");
            break;
        }
        av_free_packet(&pkt);
    }

    av_write_trailer(ofmt_ctx);
    avformat_close_input(&ifmt_ctx);
    if(ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    return 0 ;

}
