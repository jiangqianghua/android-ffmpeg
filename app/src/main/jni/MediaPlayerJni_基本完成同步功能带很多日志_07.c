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

#define VIDEO_PICTURE_QUEUE_SIZE 1
#define AV_SYNC_THRESHOLD   0.01
#define AV_NOSYNC_THRESHOLD 10.0

int video_index = -1;
int audio_index = -1 ;
AVFormatContext *pFormatCtx = NULL;

static jclass mediaPlayerJniClass;

static jmethodID playMediaResult ;
static jmethodID callBackVideoData;
static jmethodID callBackAudioData ;
static jmethodID initAudio ;

static JavaVM *g_jvm = NULL;

// 一些状态控制
static bool isPlay = false;
static bool isResum = false ;
static int64_t seekpos = 0 ; 
static int64_t isseekpos = -1 ;
// 音频相关
static double audio_clock ;
unsigned int audio_buf_size;
unsigned int audio_buf_index;

// 视频相关
double frame_last_pts;
double frame_last_delay;
double frame_timer;

typedef struct VideoPicture
{
    AVFrame *rawdata;
    int width, height; /*source height & width*/
    int allocated;
    double pts;
} VideoPicture;

// 视频队列
static VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];

// 读取数据包的队列
typedef struct PacketQueue
{
    AVPacketList *first_ptk ; // 队头
    AVPacketList *last_pkt ;  //队尾
    int nb_packets ;          // 包的个数
    int size ;                // 占用空间的字节数   
    pthread_mutex_t mutex ;

}PacketQueue ;

PacketQueue audioq;
PacketQueue videoq;
// 工具函数
double nowTime();
double get_audio_clock();
// 线程函数
void *recv_thread(void *arg);
void *video_thread(void *arg);
void *audio_thread(void *arg);

// 回调给java的函数
void call_back_videodata(const char *pData, int width, int height,JNIEnv *env);
void call_back_aduiodata(AVFrame *frame,JNIEnv *env);

// 队列函数
void packet_queue_init(PacketQueue *q); // 初始化队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt); // 加入队列
int packet_queue_get(PacketQueue *q,AVPacket *pkt, int block);  // 获取队列数据，block 是否堵塞
void packet_queue_destory(PacketQueue *q);

void show_time(double duration);

// 初始化调用
void Java_com_jqh_jni_nativejni_MediaPlayerJni_init(JNIEnv *env, jclass cls,jobject jobject)
{
    LOGD("start init");
    av_register_all();
    //保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    LOGD("start init1 %p %p" ,g_jvm,cls);
    mediaPlayerJniClass = (jclass)((*env)->NewGlobalRef(env, cls));
    LOGD("start init2 %p" ,mediaPlayerJniClass);
    callBackVideoData = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "callBackVideoData", "([B)V");
    playMediaResult = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "playMediaResult", "(IIIJ)V");
    callBackAudioData = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "callBackAudioData", "([B)V");
    initAudio = (*env)->GetStaticMethodID(env, mediaPlayerJniClass,
                                "initAudio", "(IZZI)V");
}

//用户拖动进度
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_seekMedia(JNIEnv *env, jobject jobject,jlong pos)
{
    isseekpos = pos ;
    return 0 ;
}

// 继续
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_goOnMedia(JNIEnv *env, jobject jobject,jstring url)
{
    isResum = true ;
    return 0 ;
}

// 暂停
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_pauseMedia(JNIEnv *env, jobject jobject,jstring url)
{
    isResum = false ;
    return 0 ;
}

// 停止播放调用
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_stopMedia(JNIEnv *env, jobject jobject,jstring url)
{
    isPlay = false ;
    return 0 ;
}


// 开始播放调用
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_playeMedia(JNIEnv *env, jobject jobject,jstring url,jlong pos)
{
    LOGD("call_back_videodata create mediaPlayerJniClass %p",env);
    const char *pUrl = (*env)->GetStringUTFChars(env,url,NULL);
    LOGD("play url = %s",pUrl);
    pFormatCtx = avformat_alloc_context();
    seekpos = pos ;
    LOGD("seekpos = %llu",seekpos);

    // 设置参数
    AVDictionary *pAvDic = NULL;
    // 设置http请求头部信息
    av_dict_set(&pAvDic,"user_agent", "android", 0);
    // 设置缓冲大小
    av_dict_set(&pAvDic,"bufsize","655360",0);
    // 打开流
    if(avformat_open_input(&pFormatCtx,pUrl,NULL,&pAvDic))
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
        return -3 ;
    }
    pthread_detach(tid);
    return 0 ;
}
// 初始化队列
void packet_queue_init(PacketQueue *q) 
{
    memset(q,0,sizeof(PacketQueue));
    q->first_ptk = NULL; 
    q->last_pkt = NULL;
    pthread_mutex_init(&q->mutex , NULL) ; // 初始化锁
}

// 加入队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt) 
{
    /**
        typedef struct AVPacketList {
        AVPacket pkt;
        struct AVPacketList *next;
        } AVPacketList;
    **/
    AVPacketList *pkt1 = (AVPacketList *) av_malloc(sizeof(AVPacketList));
    if(!pkt1)
    {
        return -1 ;
    }

    //对互斥锁上锁
    if(pthread_mutex_lock(&q->mutex) != 0)
    {
        printf("packet_queue_put Thread lock failed! \n") ;
        return -1 ;
    }
    pkt1->pkt = *pkt ;
    pkt1->next = NULL; 

    if(!q->last_pkt)
    {
        q->first_ptk = pkt1 ;
    }
    else
    {
        q->last_pkt->next = pkt1 ;
    }

    q->last_pkt = pkt1 ;
    q->nb_packets++;
    q->size += pkt1->pkt.size ;
    //解锁
    pthread_mutex_unlock(&q->mutex) ;
    LOGD("[packet_queue_put] video packgets = %d",q->nb_packets);
}

// 获取队列数据，block 是否堵塞
int packet_queue_get(PacketQueue *q,AVPacket *pkt, int block)  
{
    AVPacketList *pkt1 = NULL;
    int ret = 0 ; 

    //对互斥锁上锁
    if(pthread_mutex_lock(&q->mutex) != 0)
    {
        printf("packet_queue_put Thread lock failed! \n") ;
        return -1 ;
    }
    for(;;)
    {
        pkt1 = q->first_ptk ;
        if(pkt1)
        {
            q->first_ptk = pkt1->next;
            if(!q->first_ptk)
            {
                q->last_pkt = NULL;
            }
            q->nb_packets-- ; 
            q->size -= pkt1->pkt.size ;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1 ; 
            LOGD("[packet_queue_get] video packgets = %d",q->nb_packets);
            break;
        }
        else if(!block)
        {
            ret = 0 ;
            LOGD("[packet_queue_get] !block");
            break;
        }
        else
        {
            //LOGD("[packet_queue_get] todo");
            // todo
            ret = -1 ;
            break;
        }
    }

     //解锁
    pthread_mutex_unlock(&q->mutex) ;
    return ret ;
}

void packet_queue_destory(PacketQueue *q)
{
    if(q != NULL)
        pthread_mutex_destroy(&q->mutex) ;
}

// 视频接受解码线程
void *video_thread(void *arg)
{
    AVPacket pkt1 , *packet = &pkt1 ;
    int frameFinished = 0;
    AVFrame *pFrame = NULL;
    AVStream *astream = pFormatCtx->streams[video_index] ;
    double sync_threshold ;
    double diff = 0 ;
    double pts ;
    double delay ;
    double actual_delay ;

    pFrame = av_frame_alloc();
    AVCodecContext *codecCtx = pFormatCtx->streams[video_index]->codec;
    JNIEnv *env = NULL;

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
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGD("AttachCurrentThread--() failed");
    }
    while(1)
    {
        if(!isPlay)
        {
            LOGD("video stop play");
            break;
        }
        if(!isResum)
        {   
            LOGD("pause ing ...");
            usleep(1000);
            continue;
        }
        LOGD("[video_thread] packet_queue_get");
        if(packet_queue_get(&videoq,packet,1) < 0)
        {
            LOGD("[video_thread] no packet");
            //usleep(200000);// 50ms   1ms = 1000us
        }

        LOGD("[video_thread] avcodec_decode_video2");
        avcodec_decode_video2(codecCtx,pFrame,&frameFinished,packet);
        //--------------以下是音视频同步相关
        pts = 0 ;
        if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque
            && *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE)
        {
            pts = *(uint64_t *) pFrame->opaque;
        }
        else if (packet->dts != AV_NOPTS_VALUE)
        {
            pts = packet->dts;
        }
        else
        {
            pts = 0;
        }
        LOGD("show time video_clock pts:%lf",pts);
        pts *= av_q2d(astream->time_base);
        LOGD("show time video_clock pts--:%lf",pts);
        double ref_clock = get_audio_clock() ;

        delay = pts - frame_last_pts;
        // 保存
        frame_last_delay = delay;
        frame_last_pts = pts;

        diff = pts - ref_clock;
        sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
        if (fabs(diff) < AV_NOSYNC_THRESHOLD)
        {
            if (diff <= -sync_threshold)
            {
                delay = 0;
            }
            else if (diff >= sync_threshold)
            {
                delay = 2 * delay;
            }
        }

        double dNow = nowTime();
        if ((frame_timer - dNow > 1000) || (dNow - frame_timer > 0.5))
        {
            //LOGE("show time ft=%lf dNow=%lf", frame_timer,dNow);
            frame_timer = dNow;
        }
        frame_timer += delay;
        //actual_delay = frame_timer - dNow;
        actual_delay = diff;
        if (actual_delay <= 0.010)
            actual_delay = 0.010; 
        if(diff >= 0.01) // 等待
            usleep(diff*1000*1000);
        else if(diff < -0.01)
             continue ; // 丢帧
        
        //sleep(0.2);
        //usleep(15000);
        LOGE("show time actual_delay3=%lf diff=%lf -- %lf", actual_delay,diff,diff*1000*1000);
        // 播放下一帧延迟
        //sleep(diff);
        //schedule_refresh((int) (actual_delay * 1000 + 0.5));
        //--------------end--------------

        if(frameFinished)
        {
            LOGD("[video_thread] call_back_videodata");
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0,
            codecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
            call_back_videodata(buffer, codecCtx->width, codecCtx->height,env);
        }

    }

    av_frame_free(&pFrameRGB);
    av_frame_free(&pFrame);

    packet_queue_destory(&videoq);
    LOGD("video thread exit");
    (*g_jvm)->DetachCurrentThread(g_jvm);
}

// 音频接受解码线程
void *audio_thread(void *arg)
{

    AVPacket pkt1 , *packet = &pkt1 ;
    AVCodecContext *acodeCtx = pFormatCtx->streams[audio_index]->codec;
    AVStream *astream = pFormatCtx->streams[audio_index] ;
    JNIEnv *env = NULL;
    AVFrame *pAFrame = av_frame_alloc();
    AVFrame *pAFrameCache = av_frame_alloc();
    SwrContext *pSwrCtx = NULL;
    int resampled_data_size ;

    LOGD("AttachCurrentThread-- %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGD("AttachCurrentThread--() failed");
    }

    while(1)
    {

        if(!isPlay)
        {
            LOGD("audio stop play");
            break;
        }
        if(!isResum)
        {   
            LOGD("pause ing ...");
            usleep(100);
            continue;
        }
        LOGD("[audio_thread] packet_queue_get");
        if(packet_queue_get(&audioq,packet,1) < 0)
        {
            LOGD("[audio_thread] no packet");
            //usleep(200000);// 50ms   1ms = 1000us
        }
        if(packet->pts != AV_NOPTS_VALUE)
        {
            audio_clock = av_q2d(astream->time_base)*packet->pts;
        }
        int framefinished = 0 ; // 判断是否读取完
        LOGD("recv_thread audio packet");
        avcodec_decode_audio4(acodeCtx,pAFrameCache,&framefinished,packet);
        if(framefinished)
        {
            // if is 16bit
            if(acodeCtx->sample_fmt == AV_SAMPLE_FMT_S16)
            {
                LOGD("is 16 bit");
                // render to java
                call_back_aduiodata(pAFrameCache,env);
            }
            else
            {

                int nb_sample = pAFrameCache->nb_samples;
                int out_channels = acodeCtx->channels;
                int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                int dst_buf_size = nb_sample * bytes_per_sample * out_channels;

                pAFrame->linesize[0] = pAFrameCache->linesize[0];
                pAFrame->data[0] = (uint8_t*) av_malloc(dst_buf_size);

                avcodec_fill_audio_frame(pAFrame, out_channels, AV_SAMPLE_FMT_S16, pAFrame->data[0], dst_buf_size, 0);
                    
                if (!pSwrCtx)
                {
                    uint64_t in_channel_layout = av_get_default_channel_layout(acodeCtx->channels);
                    uint64_t out_channel_layout = av_get_default_channel_layout(out_channels);
                    pSwrCtx = swr_alloc_set_opts(NULL, out_channel_layout, AV_SAMPLE_FMT_S16, acodeCtx->sample_rate,
                    in_channel_layout, acodeCtx->sample_fmt, acodeCtx->sample_rate, 0, NULL);
                    swr_init(pSwrCtx);
                }

                if(pSwrCtx)
                {
                    int out_count = dst_buf_size / out_channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                    int ret = swr_convert(pSwrCtx, pAFrame->data, out_count, (const uint8_t**)(pAFrameCache->data), nb_sample);
                    if (ret < 0)
                    {
                        //av_free(out_channels->data[0]);
                    }
                    else
                    {
                        resampled_data_size = ret * out_channels * bytes_per_sample;
                        // 更新基准时间
                        audio_clock += (double)resampled_data_size/(double)(2 * out_channels * acodeCtx->sample_rate);
                        LOGD("show time audio_clock:%lf",audio_clock);
                        pAFrameCache->linesize[0] = pAFrame->linesize[0] = ret * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * out_channels;
                    }
                    call_back_aduiodata(pAFrame,env);
                }


                LOGD("not 16 bit");
            }
        }
    }
    av_frame_free(&pAFrame);
    av_frame_free(&pAFrameCache);
    avformat_free_context(pFormatCtx);
    packet_queue_destory(&audioq);
    LOGD("audio thread exit");
    (*g_jvm)->DetachCurrentThread(g_jvm);
}

// 音视频接受数据线程
void *recv_thread(void *arg)
{
    isPlay = true ;
    isResum = true ;
    LOGD("recv_thread...");
    AVPacket pkt1, *packet = &pkt1;
    int result ; 
    //------------------------------------------
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;

    AVCodecContext *acodeCtx = NULL;
    AVCodec *acodec = NULL ;
    // 查找是否有解码器
    //LOGD("decode_video_stream index=%d",video_index);
    codecCtx = pFormatCtx->streams[video_index]->codec;
   // LOGD("decode_video_stream codecCtx=%p",codecCtx);
    codec = avcodec_find_decoder(codecCtx->codec_id);
    //LOGD("decode_video_stream codec=%p",codec);
    if(!codec)
    {
        LOGD("can not decode video,Unsupport codec id = %d",codecCtx->codec_id);
        return NULL ;
    }
    //LOGD("the  codec id = %d",codecCtx->codec_id);
    if(avcodec_open2(codecCtx,codec,NULL) < 0)
    {
        LOGD("video avcodec_open2 error");
        return NULL;
    }


    // 查找音频解码器
    acodeCtx = pFormatCtx->streams[audio_index]->codec;
    acodec = avcodec_find_decoder(acodeCtx->codec_id);
    if(!acodec)
    {
        LOGD("can not decode audio,Unsupport codec id = %d",acodeCtx->codec_id);
        return NULL ;
    }
    // 解码音频
    if(avcodec_open2(acodeCtx,acodec,NULL)<0)
    {
         LOGD("audio avcodec_open2 error");
         return NULL;
    }


    JNIEnv *env = NULL;
    LOGD("AttachCurrentThread-- %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGD("AttachCurrentThread--() failed");
    }
    (*env)->CallStaticVoidMethod(env, mediaPlayerJniClass, playMediaResult, 1,codecCtx->width,codecCtx->height,pFormatCtx->duration);
    
    AVFrame* pFrame = NULL;
    pFrame = av_frame_alloc();
    int i = 0 ;

    AVFrame *pAFrame = av_frame_alloc();
    AVFrame *pAFrameCache = av_frame_alloc();
    SwrContext *pSwrCtx = NULL;

    // initAudio
    (*env)->CallStaticVoidMethod(env, mediaPlayerJniClass, initAudio, acodeCtx->sample_rate,JNI_TRUE,acodeCtx->channels > 1,4096);
    if(seekpos > 0)
        av_seek_frame(pFormatCtx, -1 , seekpos * AV_TIME_BASE, AVSEEK_FLAG_ANY);

    packet_queue_init(&videoq);
    packet_queue_init(&audioq);

    pthread_t tid1 ;
    //调用音视频解码线程
    int result1 = pthread_create(&tid1,NULL,video_thread,NULL);
    pthread_detach(tid1);
    pthread_t tid2 ;
    int result2 = pthread_create(&tid2,NULL,audio_thread,NULL);
    pthread_detach(tid2);

    frame_timer = nowTime();
    frame_last_delay = 40e-3;
    frame_last_pts = 0 ;
    //------------------------------------------
    while(1){

        if(!isPlay)
        {
            LOGD("start stop play");
            break;
        }

        if(!isResum)
        {   
            LOGD("pause ing ...");
            usleep(100);
            continue;
        }

        //LOGD("recv_thread start av_read_frame pFormatCtx=%p",pFormatCtx);
        if(isseekpos != -1)
        {
            // 用户拖动进度条
            //AVSEEK_FLAG_BACKWARD：若你设置seek时间为1秒，但是只有0秒和2秒上才有I帧，则时间从0秒开始。
            //AVSEEK_FLAG_ANY：若你设置seek时间为1秒，但是只有0秒和2秒上才有I帧，则时间从2秒开始。
            //AVSEEK_FLAG_FRAME：若你设置seek时间为1秒，但是只有0秒和2秒上才有I帧，则时间从2秒开始。
            av_seek_frame(pFormatCtx, -1 , isseekpos * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
            isseekpos = -1 ;
        }
        result = av_read_frame(pFormatCtx,packet);
        //LOGD("recv_thread av_read_frame over");
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

        //LOGD("recv_thread set cur stream index = %d  and video index = %d",packet->stream_index,video_index);
        
        if(packet->stream_index == video_index)
        {
            LOGD("[packet_queue] video");
            packet_queue_put(&videoq,packet);
        }
        else if(packet->stream_index == audio_index)
        {
            packet_queue_put(&audioq,packet);
        }
        else
        {
            av_free_packet(packet);
        }
    }
    (*g_jvm)->DetachCurrentThread(g_jvm);
}

// 回调返回视频数据
void call_back_videodata(const char *pData, int width, int height,JNIEnv *env )
{
    int len = 2*width*height;
    if(!env)
    {
        LOGD("AttachCurrentThread %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,callBackVideoData);
        if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
        {
            LOGD("AttachCurrentThread() failed");
        }
    }

    LOGD("call_back_videodata create mediaPlayerJniClass1111 %p",env);
    jbyteArray carr = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env,carr,0,len,pData);
    LOGD("call_back_videodata call callBackVideoData");
    (*env)->CallStaticVoidMethod(env, mediaPlayerJniClass, callBackVideoData, carr);
    (*env)->DeleteLocalRef(env, carr);

}

// 回调返回音频数据
void call_back_aduiodata(AVFrame *frame,JNIEnv *env )
{
    if(!env)
    {
        LOGD("call_back_aduiodata  %p -- %p  -- %p -- %p",env,g_jvm,mediaPlayerJniClass,call_back_aduiodata);
        if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
        {
            LOGD("AttachCurrentThread() failed");
        }
    }
    jbyteArray carr = (*env)->NewByteArray(env, frame->linesize[0]);
    (*env)->SetByteArrayRegion(env,carr,0,frame->linesize[0],frame->data[0]);
    (*env)->CallStaticVoidMethod(env,mediaPlayerJniClass,callBackAudioData,carr);
    (*env)->DeleteLocalRef(env, carr);
}

//获取时间基准
double get_audio_clock()
{
    return audio_clock ;
}
// 返回当前时间，double类型
double nowTime()
{
    unsigned long long ullNow = av_gettime();
    double dNow = (double)ullNow / 1000000;
    return  dNow ;
}
// void getDuration()
// {
    // if(pFormatCtx->duration!=AV_NOPTS_VALUE){
    //    int hours,mins,secs,us;
    //    int64_t duration=pFormatCtx->duration+5000;
    //    secs=duration/AV_TIME_BASE;
    //    us=dyration%AV_TIME_BASE;//1000000
    //    mins=secs/60;
    //    secs%=60;
    //    hours=mins/60;
    //    mins%=60;
    //    printf("%02d:%02d:%02d.%02d\n",hours,mins,secs,(100*us)/AV_TIME_BASE);
//     }
// }

