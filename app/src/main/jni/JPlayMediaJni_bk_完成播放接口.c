#include "header.h"
#include "sonic.h"

#define OPEN_LOG

#define LOG_TAG    "JPlayMediaJni"
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
#define RESUME_WAITINH_TIME 5*1000

/****************回调函数相关*****************/
static jobject jPlayMediaJniObj;

static jmethodID jni_perpare ;
static jmethodID jni_flush_video_data;
static jmethodID jni_flush_audio_short_data ;
static jmethodID jni_flush_audio_byte_data ;
static jmethodID jni_play_completion ;
static jmethodID jni_seek_complete ;
static jmethodID jni_error;
static jmethodID jni_info ;

static JavaVM *g_jvm = NULL;

/****************音视频队列相关*****************/
// 读取数据包的队列
typedef struct PacketQueue
{
    AVPacketList *first_ptk ; // 队头
    AVPacketList *last_pkt ;  //队尾
    int nb_packets ;          // 包的个数
    int size ;                // 占用空间的字节数   
    pthread_mutex_t mutex ;
}PacketQueue ;

/****************一些结构体*****************/
typedef struct AVFFmpegCtx
{   
    AVFormatContext *pFormatCtx ;
} AVFFmpegCtx;

typedef struct PlayStatus
{
    // 一些状态控制
    bool isPlay ;
    bool isReadStop ;
    bool isResum  ;
    int64_t seekpos  ; 
    int64_t isseekpos  ;
    // 倍速控制
    double speed  ;
    // 音频相关
    double audio_clock ;

    // 视频相关
    double frame_last_pts;
    double frame_last_delay;
    double frame_timer;
    const char *url ;
    PacketQueue audioq;
    PacketQueue videoq;
    // 初始化部分：先创建一个流  
    sonicStream tempoStream_;  

    int video_index ;
    int audio_index ;

    int videoWdith ; 
    int videoHeight ; 
    long duration ;

    int sample_rate ;
    int isStereo ; 
    int is16Bit;
    int desiredFrames ;

} PlayStatus;


/****************结构体对象相关*****************/
PacketQueue *packetQueue ;
PlayStatus *playStatus ;
AVFFmpegCtx *avFFmpegCtx ;


/****************函数声明*****************/
void init_ffmpeg();
void init_params();
void myffmpeglog2(void* p, int v, const char* str, va_list lis);
int hasVideo();
int hasAudio();
void decode_video_stream();
void decode_audio_stream();
// 队列相关
void packet_queue_init(PacketQueue *q); // 初始化队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt); // 加入队列
int packet_queue_get(PacketQueue *q,AVPacket *pkt, int block);  // 获取队列数据，block 是否堵塞
void packet_queue_destory(PacketQueue *q);
void packet_queue_clean(PacketQueue *q);

// 线程相关
void *prepareAsync(void *arg);
/****************java调用的函数*****************/

// 初始化调用
void Java_com_jqh_jmedia_JMediaPlayer__1initNative(JNIEnv *env,jobject jobj)
{
    init_ffmpeg();

//保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    LOGD("init1 - %p %p" ,g_jvm,jobj);
    //com/jqh/jmedia/JMediaPlayer
    jclass cls = (*env)->FindClass(env,"com/jqh/jmedia/JMediaPlayer");
    if(cls == NULL)
    {
        LOGD("Can not find cls by  com/jqh/jmedia/JMediaPlayer" );
    }
    LOGD("init2 %p" ,cls);
    jPlayMediaJniObj = (jobject)((*env)->NewGlobalRef(env, jobj));
    LOGD("init3 %p" ,jPlayMediaJniObj);

    jni_perpare = (*env)->GetMethodID(env, cls,
                                "jni_perpare", "()V");
    jni_flush_video_data = (*env)->GetMethodID(env, cls,
                                "jni_flush_video_data", "([B)V");
    jni_flush_audio_byte_data = (*env)->GetMethodID(env, cls,
                                "jni_flush_audio_byte_data", "([B)V");
}

// 设置源数据
void Java_com_jqh_jmedia_JMediaPlayer__1setDataSource(JNIEnv *env, jobject jobject,jstring url,jobjectArray keys,jobjectArray values)
{
    init_params();

    playStatus->url = (*env)->GetStringUTFChars(env,url,NULL);

    (*env)->DeleteLocalRef(env,url);
}

// 准备播放，异步操作，准备完成回调给java上层
void Java_com_jqh_jmedia_JMediaPlayer_prepareAsync(JNIEnv *env, jobject jobject)
{
    // 开辟线程执行
    pthread_t tid ;
    // 创建一个接受线程
    int result = pthread_create(&tid,NULL,prepareAsync,NULL);
    if(result != 0)
    {
        LOGD("prepareAsync thread created error %d",result);
    }
    pthread_detach(tid);
}

void Java_com_jqh_jmedia_JMediaPlayer__1start(JNIEnv *env, jobject jobject)
{
    
}

jint Java_com_jqh_jmedia_JMediaPlayer_getVideoWidth(JNIEnv *env, jobject jobject)
{
    return playStatus->videoWdith;
}

jint Java_com_jqh_jmedia_JMediaPlayer_getVideoHeight(JNIEnv *env, jobject jobject)
{
    return playStatus->videoHeight;
}

jlong Java_com_jqh_jmedia_JMediaPlayer_getDuration(JNIEnv *env, jobject jobject)
{
    return playStatus->duration;
}

jint Java_com_jqh_jmedia_JMediaPlayer_getSampleRate(JNIEnv *env, jobject jobject)
{
    return playStatus->sample_rate;
}

jint Java_com_jqh_jmedia_JMediaPlayer_getDesiredFrames(JNIEnv *env, jobject jobject)
{
    return playStatus->desiredFrames;
}

jboolean Java_com_jqh_jmedia_JMediaPlayer_is16Bit(JNIEnv *env, jobject jobject)
{
    return playStatus->is16Bit;
}

jboolean Java_com_jqh_jmedia_JMediaPlayer_isStereo(JNIEnv *env, jobject jobject)
{
    return playStatus->isStereo;
}
/**


 * 初始化ffmpeg参数
 */
void init_ffmpeg()
{
    av_log_set_callback(myffmpeglog2);
    av_log_set_level(AV_LOG_INFO);
    av_register_all();
    avformat_network_init(); 
}

/**
 *  初始化音视频相关参数
 */
void init_params()
{
    packetQueue = malloc(sizeof(PacketQueue));
    playStatus = malloc(sizeof(PlayStatus));
    avFFmpegCtx = malloc(sizeof(AVFFmpegCtx));

    packet_queue_init(&playStatus->videoq);
    packet_queue_init(&playStatus->audioq);

    playStatus->isPlay = false;
    playStatus->isReadStop;
    playStatus->isResum = false;
    playStatus->seekpos = 0 ; 
    playStatus->isseekpos = -1;
    playStatus->speed = 1.0;
    playStatus->audio_clock = 0 ; 
    playStatus->frame_last_delay = 0.0 ; 
    playStatus->frame_last_pts = 0.0 ;
    playStatus->frame_timer = 0.0;  

    playStatus->video_index = -1;
    playStatus->audio_index = -1; 
    playStatus->videoWdith = 0 ; 
    playStatus->videoHeight = 0 ; 
    playStatus->duration = 0 ;

    playStatus->sample_rate = 0 ;
    playStatus->isStereo = 0 ; 
    playStatus->is16Bit = 0;
    playStatus->desiredFrames = 0;

    avFFmpegCtx->pFormatCtx = NULL;
}

/**
 * 自定义日志输出回调接口,可以获取到ffmpeg系统的一些日志
 */
void myffmpeglog2(void* p, int v, const char* str, va_list lis){

    va_list vl2;
    char line[1024];
    static int print_prefix = 1;


    va_copy(vl2, lis);
    av_log_format_line(p, v, str, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
    LOGE("%s",line);
}


// 初始化队列
void packet_queue_init(PacketQueue *q) 
{
    memset(q,0,sizeof(PacketQueue));
    q->first_ptk = NULL; 
    q->last_pkt = NULL;
    packet_queue_clean(q);
    pthread_mutex_init(&q->mutex , NULL) ; // 初始化锁
}

// 加入队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt) 
{

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
}

// 获取队列数据，block 是否堵塞
int packet_queue_get(PacketQueue *q,AVPacket *pkt, int block)  
{
    AVPacketList *pkt1 = NULL;
    int ret = 0 ; 

    //对互斥锁上锁
    // if(pthread_mutex_lock(&q->mutex) != 0)
    // {
    //     printf("packet_queue_get Thread lock failed! \n") ;
    //     return -1 ;
    // }

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
            ret = -1 ;
            break;
        }
    }

     //解锁
    //pthread_mutex_unlock(&q->mutex);
    return ret ;
}

void packet_queue_destory(PacketQueue *q)
{
    if(q != NULL)
        pthread_mutex_destroy(&q->mutex) ;
}

void packet_queue_clean(PacketQueue *q)
{
    if(q == NULL)
        return ;
    //对互斥锁上锁
    if(pthread_mutex_lock(&q->mutex) != 0)
    {
        printf("packet_queue_clean Thread lock failed! \n") ;
        return  ;
    }

    //清空数据
    q->first_ptk = NULL; 
    q->last_pkt = NULL;
    q->size = 0 ; 
    q->nb_packets = 0 ;
    //解锁
    pthread_mutex_unlock(&q->mutex) ;

}

// 准备解码
void *prepareAsync(void *arg)
{

    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;

    AVCodecContext *acodeCtx = NULL;
    AVCodec *acodec = NULL ;

    JNIEnv *env = NULL;

    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
    {
        LOGE("AttachCurrentThread--() failed");
    }
    avFFmpegCtx->pFormatCtx = avformat_alloc_context();
    // 设置参数
    AVDictionary *pAvDic = NULL;
    // 设置http请求头部信息
    av_dict_set(&pAvDic,"user_agent", "android", 0);

    if(avformat_open_input(&avFFmpegCtx->pFormatCtx,playStatus->url,NULL,&pAvDic))
    {
         LOGE("open error");
    }

    LOGD("avformat_open_input ok");
    // 查找流
    if(avformat_find_stream_info(avFFmpegCtx->pFormatCtx,NULL)<0)
    {
        LOGE("find error");
    }

    LOGD("find stream ok");
    // 获取流的个数
    int stream_num = avFFmpegCtx->pFormatCtx->nb_streams;
    LOGD("stream num=%d",stream_num);
    // 标识出每个流信息
    int i = 0 ;
    for(i = 0 ; i < stream_num; i++ )
    {
        if(avFFmpegCtx->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            playStatus->video_index = i ;
        }
        else if(avFFmpegCtx->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            playStatus->audio_index = i ;
        }
    }

    LOGD("video stream index=%d",playStatus->video_index);
    LOGD("audio stream index=%d",playStatus->audio_index);

    if(hasVideo)
        decode_video_stream(); // 解码视频流
    if(hasAudio)
        decode_audio_stream(); // 解码音频流

    (*env)->CallVoidMethod(env,jPlayMediaJniObj, jni_perpare);
    // 回调上层
    (*g_jvm)->DetachCurrentThread(g_jvm);
}


void decode_video_stream()
{
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    codecCtx = avFFmpegCtx->pFormatCtx->streams[playStatus->video_index]->codec;
    codec = avcodec_find_decoder(codecCtx->codec_id);
    if(!codec)
    {
        LOGE("can not decode video,Unsupport codec id = %d",codecCtx->codec_id);
    }

    LOGD("the video codec id = %d",codecCtx->codec_id);
    if(avcodec_open2(codecCtx,codec,NULL) < 0)
    {
        LOGE("video avcodec_open2 error");
    }

    playStatus->videoWdith = codecCtx->width ; 
    playStatus->videoHeight = codecCtx->height ;
    playStatus->duration = avFFmpegCtx->pFormatCtx->duration ;

}

void decode_audio_stream()
{
    AVCodecContext *acodeCtx = NULL;
    AVCodec *acodec = NULL ;

    // 查找音频解码器
    acodeCtx = avFFmpegCtx->pFormatCtx->streams[playStatus->audio_index]->codec;
    acodec = avcodec_find_decoder(acodeCtx->codec_id);
    if(!acodec)
    {
        LOGE("can not decode audio,Unsupport codec id = %d",acodeCtx->codec_id);
    }
    // 解码音频
    if(avcodec_open2(acodeCtx,acodec,NULL)<0)
    {
        LOGE("audio avcodec_open2 error");
    }

    int audioSampleRate = (int)(acodeCtx->sample_rate * playStatus->speed) ;
    LOGD("InitAudio --- %d %d %lld channels=%d ",acodeCtx->sample_rate, audioSampleRate,acodeCtx->channel_layout,acodeCtx->channels);
    // 参数为采样率和声道数  
    playStatus->tempoStream_ = sonicCreateStream(acodeCtx->sample_rate,acodeCtx->channels);
    // 设置速率
    sonicSetSpeed(playStatus->tempoStream_, playStatus->speed);  
    sonicSetPitch(playStatus->tempoStream_, 1.0);
    sonicSetRate(playStatus->tempoStream_, 1.0/playStatus->speed);   

    playStatus->sample_rate = acodeCtx->sample_rate;
    playStatus->is16Bit = 1;
    playStatus->isStereo = acodeCtx->channels > 1;
    playStatus->desiredFrames = 4096;


}

int hasVideo()
{
    if(playStatus->video_index >= 0)
    {
        return 1 ;
    }
    return 0 ;
}

int hasAudio()
{
    if(playStatus->audio_index >= 0)
    {
        return 1 ;
    }
    return 0 ;
}