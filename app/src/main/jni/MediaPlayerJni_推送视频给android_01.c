#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <pthread.h>
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
# include "libswscale/swscale.h"

#define OPEN_LOG

#define LOG_TAG    "mediaplayerjni"
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

int video_index = -1;
int audio_index = -1 ;
AVFormatContext *pFormatCtx = NULL;
static jmethodID callBackVideoData;
static jclass mediaPlayerJniClass;
static jmethodID playMediaResult ;

static JavaVM *g_jvm = NULL;
static jobject g_obj = NULL;

void *video_decode_thread(void *arg) ;
void *recv_thread(void *arg);
void call_back_videodata(const char *pData, int width, int height);
// 初始化调用
void Java_com_jqh_jni_nativejni_MediaPlayerJni_init(JNIEnv *env, jclass cls,jobject jobject)
{
    LOGD("start init");
    av_register_all();
    //保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    LOGD("start init1 %p %p" ,g_jvm,cls);
    //不能直接赋值(g_obj = obj)
    mediaPlayerJniClass = (jclass)((*env)->NewGlobalRef(env, cls));
    LOGD("start init2 %p" ,mediaPlayerJniClass);
    callBackVideoData = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "callBackVideoData", "([B)V");
    playMediaResult = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "playMediaResult", "(III)V");
    LOGD("init over %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
}

// 开始播放调用
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_playeMedia(JNIEnv *env, jobject jobject,jstring url)
{
    LOGD("call_back_videodata create mediaPlayerJniClass %p",env);
    const char *pUrl = (*env)->GetStringUTFChars(env,url,NULL);
    LOGD("play url = %s",pUrl);
    pFormatCtx = avformat_alloc_context();
    // 打开流
    if(avformat_open_input(&pFormatCtx,pUrl,NULL,NULL))
    {
         LOGD("open error");
         return -1 ;
    }
    LOGD("open ok");
    // 查找流
    if(avformat_find_stream_info(pFormatCtx,NULL)<0)
    {
        LOGD("find error");
        return -2 ;
    }
    LOGD("find stream ok");

    // 获取流的个数
    int stream_num = pFormatCtx->nb_streams;
    LOGD("stream num=%d",stream_num);
    // 标识出每个流信息
    int i = 0 ;
    for(i = 0 ; i < stream_num; i++ )
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i ;
        }
        else if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_index = i ;
        }
    }

    LOGD("video stream index=%d",video_index);
    LOGD("audio stream index=%d",audio_index);

    // 解码视频流
    //decode_video_stream(video_index,pFormatCtx);
    // 解码音频流
    //decode_audio_stream(audio_index,pFormatCtx);

    // 接受音视频线程
    pthread_t tid ;
    // 创建一个接受线程
    int result = pthread_create(&tid,NULL,recv_thread,NULL);
    if(result != 0)
    {
        LOGD("recv thread created error %d",result);
    }
    return 0 ;
}

int decode_video_stream(int index,AVFormatContext *pFormatCtx)
{
    LOGD("decode_video_stream index=%d",index);
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;

    codecCtx = pFormatCtx->streams[index]->codec;
    LOGD("decode_video_stream codecCtx=%p",codecCtx);
    codec = avcodec_find_decoder(codecCtx->codec_id);
    LOGD("decode_video_stream codec=%p",codec);
    if(!codec)
    {
        LOGD("can not decode video,Unsupport codec id = %d",codecCtx->codec_id);
        return -1 ;
    }
    LOGD("the  codec id = %d",codecCtx->codec_id);
    if(avcodec_open2(codecCtx,codec,NULL) < 0)
    {
        LOGD("avcodec_open2 error");
        return -2;
    }
    if(codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        // get video info
        LOGD("video info width=%d,height=%d,pixfmt=%d",codecCtx->width,codecCtx->height,codecCtx->pix_fmt);
    }
    pthread_t tid ;
    // 创建一个接受线程
    int result = pthread_create(&tid,NULL,video_decode_thread,NULL);
    if(result != 0)
    {
        LOGD("video thread created error %d",result);
    }

}

int decode_audio_stream(int index,AVFormatContext *pFormatCtx)
{
    LOGD("decode_audio_stream");
}

void saveFrame(AVFrame* pFrame, int width, int height, int iFrame)
    {
        FILE *pFile;
        char szFilename[128];
        int  y;

        // Open file
        sprintf(szFilename, "mnt/sdcard/aaa/frame%d.ppm", iFrame);
        pFile = fopen(szFilename, "wb");
        if (pFile == NULL)
            return;

        // Write header
        fprintf(pFile, "P6\n%d %d\n255\n", width, height);

        // Write pixel data
        for (y = 0; y < height; y++)
            fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

        // Close file
        fclose(pFile);
    }

// 音视频接受数据线程
void *recv_thread(void *arg)
{
    LOGD("recv_thread...");
    AVPacket pkt1, *packet = &pkt1;
    int result ; 
    //------------------------------------------
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;

    LOGD("decode_video_stream index=%d",video_index);

    codecCtx = pFormatCtx->streams[video_index]->codec;
    LOGD("decode_video_stream codecCtx=%p",codecCtx);
    codec = avcodec_find_decoder(codecCtx->codec_id);
    LOGD("decode_video_stream codec=%p",codec);
    if(!codec)
    {
        LOGD("can not decode video,Unsupport codec id = %d",codecCtx->codec_id);
        return NULL ;
    }
    LOGD("the  codec id = %d",codecCtx->codec_id);
    if(avcodec_open2(codecCtx,codec,NULL) < 0)
    {
        LOGD("avcodec_open2 error");
        return NULL;
    }

     JNIEnv *env = NULL;
    LOGD("AttachCurrentThread-- %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGD("AttachCurrentThread--() failed");
    }
    (*env)->CallStaticVoidMethod(env, mediaPlayerJniClass, playMediaResult, 1,codecCtx->width,codecCtx->height);
    AVFrame* pFrameRGB = NULL;
    pFrameRGB = av_frame_alloc();
    // 使用的缓冲区的大小
    int numBytes = 0;
    uint8_t* buffer = NULL;

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB565, codecCtx->width, codecCtx->height);
    buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture*)pFrameRGB, buffer, AV_PIX_FMT_RGB565, codecCtx->width, codecCtx->height);
    
    struct SwsContext *sws_ctx = NULL;
    sws_ctx = sws_getContext(codecCtx->width,codecCtx->height,codecCtx->pix_fmt,
        codecCtx->width,codecCtx->height,AV_PIX_FMT_RGB565, SWS_BILINEAR,NULL,NULL,NULL);
    AVFrame* pFrame = NULL;
    pFrame = av_frame_alloc();
    int i = 0 ;
    //------------------------------------------
    while(1){   
        LOGD("recv_thread start av_read_frame pFormatCtx=%p",pFormatCtx);
        result = av_read_frame(pFormatCtx,packet);
        LOGD("recv_thread av_read_frame over");
        if(result != 0)
        {
            if(result == -EAGAIN)
            {
                LOGD("recv_thread continue");
                continue;
            }
            else
            {
                LOGD("recv_thread break");
                break;
            }
        }

        LOGD("recv_thread set cur stream index = %d  and video index = %d",packet->stream_index,video_index);
        
        if(packet->stream_index == video_index)
        {
               LOGD("recv_thread video packet");
               // 测试decode packet
               int framefinished = 0 ; // 判断是否读取完
               avcodec_decode_video2(codecCtx,pFrame,&framefinished,packet);
               if(framefinished)
               {
                    sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0,
                    codecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                    call_back_videodata(buffer, codecCtx->width, codecCtx->height);
               }
        }

    }

}

//视频解码线程
void *video_decode_thread(void *arg)
{
      JNIEnv *env;
     //Attach主线程
     if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
     {
         LOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
         return NULL;
     }
    AVPacket *packet = NULL ;
    int framefinished ; 
    AVFrame *pFrame = NULL;
    pFrame = av_frame_alloc();

    while(1)
    {
        usleep(50);
        //LOGD("video thread...");

    }
}

void call_back_videodata(const char *pData, int width, int height)
{
    int len = 2*width*height;
    JNIEnv *env = NULL;
    LOGD("AttachCurrentThread %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGD("AttachCurrentThread() failed");
    }

    LOGD("call_back_videodata create mediaPlayerJniClass1111 %p",env);
    jbyteArray carr = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env,carr,0,len,pData);
    LOGD("call_back_videodata call callBackVideoData");
    (*env)->CallStaticVoidMethod(env, mediaPlayerJniClass, callBackVideoData, carr);


}
