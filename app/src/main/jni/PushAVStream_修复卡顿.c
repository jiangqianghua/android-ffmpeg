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

#define PLAY_STATUS_STOP 0; 
#define PLAY_STATUS_PLAY 1;

#define OPEN_LOG

#define LOG_TAG    "PushAVStream"
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
// 视频一些参数对象
typedef struct VideoStatus
{
	AVStream *video_st;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec ;
	AVFrame *pFrameYUV ;
	int yuv_width ; 
	int yuv_height;
	int y_length ; 
	int uv_length ;

} VideoStatus;

// 音频一些参数对象
typedef struct AudioStatus
{
	AVStream* mic_audio_st;  
	AVCodecContext *mic_pCodecCtx;
	AVCodec *mic_codec ;
	AVFrame *mic_frame;
	uint8_t* mic_frame_buf;
	AVPacket mic_pkt;
} AudioStatus;

// 公共的一些参数对象
typedef struct CommonStatus
{
	AVFormatContext *ofmt_ctx;
	AVPacket enc_pkt;
    int play_status ;
    int width;
	int height;
	int fps;
	int v_count;
	int a_count ;
	int64_t start_time;
} CommonStatus;

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

   PacketQueue avq;
// 队列相关
void packet_queue_init1(PacketQueue *q); // 初始化队列
int packet_queue_put1(PacketQueue *q, AVPacket *pkt); // 加入队列
int packet_queue_get1(PacketQueue *q,AVPacket *pkt, int block);  // 获取队列数据，block 是否堵塞
void packet_queue_destory1(PacketQueue *q);
void packet_queue_clean1(PacketQueue *q);
void *wirte_thread(void *arg);
VideoStatus *videostatus = NULL;
AudioStatus *audiostatus = NULL;
CommonStatus *commonStatus = NULL;

// 参考地址 https://www.jianshu.com/p/462e489b7ce0
// 推送本地摄像头

/**
 * 自定义日志输出回调接口,可以获取到ffmpeg系统的一些日志
 */
void mylog2(void* p, int v, const char* str, va_list lis){

	va_list vl2;
	char line[1024];
	static int print_prefix = 1;


	va_copy(vl2, lis);
	av_log_format_line(p, v, str, vl2, line, sizeof(line), &print_prefix);
	va_end(vl2);
	LOGE("%s",line);
}

/**
	初始化数据
**/
void initAVCommonData()
{
	videostatus = av_malloc(sizeof(VideoStatus));
	audiostatus = av_malloc(sizeof(AudioStatus));
	commonStatus = av_malloc(sizeof(CommonStatus));
	commonStatus->play_status = PLAY_STATUS_STOP;
	commonStatus->v_count = 0;
	commonStatus->a_count = 0;
	commonStatus->fps = 15; // 15 or 20

}

// 初始化视频参数
int initVideoParams()
{
	videostatus->yuv_width = commonStatus->width ;
	videostatus->yuv_height = commonStatus->height ;
	videostatus->y_length = commonStatus->width*commonStatus->height;
	videostatus->uv_length = commonStatus->width*commonStatus->height/4;

	videostatus->pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	//videostatus->pCodec = avcodec_find_encoder_by_name("h264_nvenc");
	if(!videostatus->pCodec)
	{
		LOGE("can not find encoder h264");
		return -1 ;
	}
	videostatus->pCodecCtx = avcodec_alloc_context3(videostatus->pCodec);
	// 设置编码器id  h264的id
	videostatus->pCodecCtx->codec_id = videostatus->pCodec->id;
	// 设置像素格式，采取什么样的色彩空间来表示一个像素点
	videostatus->pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	// 设置编码器的数据类型
	videostatus->pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	// 编码目标的帧大小，像素为单位
	videostatus->pCodecCtx->width = commonStatus->width ; 
	videostatus->pCodecCtx->height = commonStatus->height ;
	videostatus->pCodecCtx->framerate = (AVRational){commonStatus->fps,1}; // 1秒15帧
	// 帧率的基本单位，分数表示
	videostatus->pCodecCtx->time_base = (AVRational){1,commonStatus->fps};
	// 目标编码率，采样的码率，采样码越大，视频大小越大
	videostatus->pCodecCtx->bit_rate = 400000;
	//所以GOP Size是连续的画面组大小的意思。
	videostatus->pCodecCtx->gop_size = 15 ;

	if(commonStatus->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		videostatus->pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	//表示在压制，容易压制和难压制的场景，允许在0.0-1.0
	videostatus->pCodecCtx->qcompress = 0.6;
	//最大和最小化系数
	videostatus->pCodecCtx->qmin = 10;
	videostatus->pCodecCtx->qmax = 51;
	// 两个非B帧之间云寻出现多少个B帧数
	//设置0表示不使用B帧
	//b帧越多，图片越少
	videostatus->pCodecCtx->max_b_frames = 0 ;
	videostatus->pCodecCtx->b_frame_strategy = 0;
	// 设置h264编码参数
	AVDictionary *param = 0 ;
	if(videostatus->pCodecCtx->codec_id == AV_CODEC_ID_H264){
		/**
         * 这个非常重要，如果不设置延时非常的大
         * ultrafast,superfast, veryfast, faster, fast, medium
         * slow, slower, veryslow, placebo.　这是x264编码速度的选项
       */
         LOGD("av_dict_set ----");
		//av_dict_set(&param, "preset", "superfast", 0);
        //av_dict_set(&param, "tune", "zerolatency", 0);
        //av_opt_set(videostatus->pCodecCtx->priv_data, "preset", "superfast", 0);
         //参数参考https://segmentfault.com/a/1190000002502526
        av_opt_set(videostatus->pCodecCtx->priv_data, "crf", "40", 0);
        av_opt_set(videostatus->pCodecCtx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(videostatus->pCodecCtx->priv_data, "tune", "zerolatency", 0);
	}

	int r = 0 ;
	if((r = avcodec_open2(videostatus->pCodecCtx,videostatus->pCodec,&param)) < 0)
	{
		LOGE("failed to open encoder %d",r);
		return -2 ;
	}
	// 添加一个新的流给输出url
	videostatus->video_st = avformat_new_stream(commonStatus->ofmt_ctx,videostatus->pCodec);
	if(videostatus->video_st == NULL)
	{
		LOGE("av new stream error");
		return -3;
	}
	videostatus->video_st->time_base.num = 1 ; 
	videostatus->video_st->time_base.den = commonStatus->fps;
	videostatus->video_st->codec = videostatus->pCodecCtx ;

	videostatus->pFrameYUV = av_frame_alloc();
	int picture_size = av_image_get_buffer_size(videostatus->pCodecCtx->pix_fmt,videostatus->pCodecCtx->width,
	videostatus->pCodecCtx->height,1);
	uint8_t *buffers = (uint8_t *)av_malloc(picture_size);

	// 将buffers的地址复制给AVFrame中图像数据，根据像素格式判断有几个数据指针
	av_image_fill_arrays(videostatus->pFrameYUV->data,videostatus->pFrameYUV->linesize,buffers,videostatus->pCodecCtx->pix_fmt,
	videostatus->pCodecCtx->width,videostatus->pCodecCtx->height,1);
	//commonStatus->start_time = av_gettime();
	LOGD("video init over");
	return 0 ;
}

// 初始化音频参数
int initAudioParams()
{
	audiostatus->mic_audio_st = avformat_new_stream(commonStatus->ofmt_ctx,0);
	if(audiostatus->mic_audio_st == NULL)
	{
		LOGD("mic_audio_st is NULL");
		return -2 ;
	}   
	audiostatus->mic_audio_st->time_base.num = 1 ; 
	audiostatus->mic_audio_st->time_base.den = commonStatus->fps;

	LOGD("555");
	audiostatus->mic_pCodecCtx = audiostatus->mic_audio_st->codec;
	audiostatus->mic_pCodecCtx->codec_id = commonStatus->ofmt_ctx->oformat->audio_codec;
	LOGD("555-1 codecid %d",audiostatus->mic_pCodecCtx->codec_id);
	audiostatus->mic_pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16 ;
	audiostatus->mic_pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO; 
	audiostatus->mic_pCodecCtx->sample_rate = 22050;// flv
	audiostatus->mic_pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	audiostatus->mic_pCodecCtx->channels = av_get_channel_layout_nb_channels(audiostatus->mic_pCodecCtx->channel_layout); 
	audiostatus->mic_pCodecCtx->bit_rate = 64000;
	audiostatus->mic_pCodecCtx->framerate = (AVRational){commonStatus->fps,1}; // 1秒15帧
	// 帧率的基本单位，分数表示
	audiostatus->mic_pCodecCtx->time_base = (AVRational){1,commonStatus->fps};
	LOGD("666 channels=%d sample_fmt=%d",audiostatus->mic_pCodecCtx->channels,audiostatus->mic_pCodecCtx->sample_fmt);
	audiostatus->mic_codec = avcodec_find_encoder(audiostatus->mic_pCodecCtx->codec_id); 
	if(!audiostatus->mic_codec){
		LOGD("avcodec_find_encoder error %d",audiostatus->mic_pCodecCtx->codec_id);
		return -1 ;
	}
	audiostatus->mic_pCodecCtx->width = commonStatus->width;
	audiostatus->mic_pCodecCtx->height = commonStatus->height;
	//LOGD("888 %d %d",mic_pCodecCtx-codec_type,mic_codec->type);
	int r = 0 ;
	if((r = avcodec_open2(audiostatus->mic_pCodecCtx,audiostatus->mic_codec,NULL)) < 0)
	{
		LOGD("avcodec_open2 error888-  (%d)",r);// -22
		return -1 ;
	}
	LOGD("999");
	audiostatus->mic_frame = av_frame_alloc();
	audiostatus->mic_frame->nb_samples = audiostatus->mic_pCodecCtx->frame_size;
	audiostatus->mic_frame->format = audiostatus->mic_pCodecCtx->sample_fmt;
	LOGD("1111-1");
	int size = av_samples_get_buffer_size(NULL,audiostatus->mic_pCodecCtx->channels,audiostatus->mic_pCodecCtx->frame_size,audiostatus->mic_pCodecCtx->sample_fmt,1);
	audiostatus->mic_frame_buf = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(audiostatus->mic_frame,audiostatus->mic_pCodecCtx->channels,audiostatus->mic_pCodecCtx->sample_fmt,(const uint8_t *)audiostatus->mic_frame_buf,size,1);
	LOGD("audio init over");
	return 0;
}

// 初始化参数
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_publicStreamInit(JNIEnv *env,jobject jobject,
                                                            jstring url_,jint w , jint h)
{
	const char *out_path = (*env)->GetStringUTFChars(env,url_,0);
	LOGD("output url : %s" , out_path);
	av_log_set_callback(mylog2);
	av_log_set_level(AV_LOG_INFO);
	av_register_all();
	avformat_network_init();

	int result = 0 ;
	initAVCommonData();
	commonStatus->width = w ; 
	commonStatus->height = h ;

	packet_queue_init1(&avq);

	//  初始参数初始化
	avformat_alloc_output_context2(&commonStatus->ofmt_ctx,NULL,"flv",out_path);

	result = initVideoParams();
	if(result < 0)
		return result;
	result = initAudioParams();
	if(result < 0)
		return result ;


	if(avio_open(&commonStatus->ofmt_ctx->pb,out_path,AVIO_FLAG_READ_WRITE) < 0)
	{
		LOGE("failed to open output file!\n");
		return -4;
	}

	avformat_write_header(commonStatus->ofmt_ctx,NULL);
	
	commonStatus->play_status = PLAY_STATUS_PLAY;

	commonStatus->start_time = av_gettime();

	// pthread_t tid1 ;
 //    int result1 = pthread_create(&tid1,NULL,wirte_thread,NULL);
 //    pthread_detach(tid1);

	return 0 ;
}

// 返回当前时间，double类型
double nowTime1()
{
    unsigned long long ullNow = av_gettime();
    double dNow = (double)ullNow / 1000000;
    return  dNow ;
}

// 开始刷视频
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_flushStreamData(JNIEnv *env,jobject jobject,
                                                            jbyteArray buffer_,jint ji)
{	

	int ret = 0 ;

	LOGD("count3 %d %d",commonStatus->a_count,commonStatus->v_count);
	jbyte *in = (*env)->GetByteArrayElements(env,buffer_,NULL);
	AVFrame *pFrame = NULL;
	int stream_index = 0 ;
	AVRational time_base1 ;
	AVCodecContext *pCodecCtx;
	AVStream *av_stream ;
	if(!commonStatus->play_status)
	{
		(*env)->ReleaseByteArrayElements(env,buffer_,in,0);
		return ;
	}

	if(ji == 1)
	{
		LOGD("push --- video ");
		// 视频
		memcpy(videostatus->pFrameYUV->data[0],in,videostatus->y_length);
		videostatus->pFrameYUV->pts = commonStatus->v_count ;
		int i = 0 ;
		for(i = 0 ; i < videostatus->uv_length; i++)
		{
			//将v数据存到第三个平面
			*(videostatus->pFrameYUV->data[2] + i) = *(in + videostatus->y_length + i*2);
			//将U数据存到第二个平面
			*(videostatus->pFrameYUV->data[1] + i) = *(in + videostatus->y_length + i*2 + 1);
		}
		videostatus->pFrameYUV->format = AV_PIX_FMT_YUV420P;
		videostatus->pFrameYUV->width = videostatus->yuv_width ; 
		videostatus->pFrameYUV->height = videostatus->yuv_height;
		pFrame = videostatus->pFrameYUV;
		stream_index = videostatus->video_st->index;
		time_base1 = videostatus->video_st->time_base;
		pCodecCtx = videostatus->pCodecCtx;
		av_stream = videostatus->video_st ;

	}
	else if(ji == 0)
	{
		// 音频
		LOGD("push --- audio ");
		audiostatus->mic_frame->data[0] = in ;
		audiostatus->mic_frame->pts = commonStatus->a_count ;
		pFrame = audiostatus->mic_frame;
		stream_index = audiostatus->mic_audio_st->index;
		time_base1 = audiostatus->mic_audio_st->time_base;
		pCodecCtx = audiostatus->mic_pCodecCtx;
		av_stream = audiostatus->mic_audio_st ;
	}
	//初始化packet
	commonStatus->enc_pkt.data = NULL;
	commonStatus->enc_pkt.size = 0 ;
	av_init_packet(&commonStatus->enc_pkt);

	//LOGD("test1");
	int frameFinished = 0 ;
	// 先计算时间戳

	// ---- end ---
	double time1 = nowTime1();
	int size = avcodec_encode_video2(pCodecCtx,&commonStatus->enc_pkt,pFrame,&frameFinished);
	double time2 = nowTime1();
	LOGD("push --- time1=%f time2=%f  diff = %f",time1,time2,time2-time1);
	//LOGD("test2");
	//av_frame_free(&pFrame);
	if(size < 0){
		LOGE("avcodec_encode_video2 error");
		return -5;
	}
	LOGD("test3");
	LOGD("avcodec_encode_video2 result=%d",frameFinished);
	//video stream index=0  | audio stream index=1
	if(frameFinished == 1)
	{

		
		commonStatus->enc_pkt.stream_index = stream_index ; 
		AVRational time_base = commonStatus->ofmt_ctx->streams[0]->time_base;//{1,1000}
		AVRational time_base_q = { 1, AV_TIME_BASE };
		/**
		double pts = commonStatus->v_count *(time_base1.den)/((time_base1.num)*commonStatus->fps);;
		double dts = pts;
		double duration = (time_base1.den)/((time_base1.num)*commonStatus->fps);
		commonStatus->enc_pkt.pts = pts;
		commonStatus->enc_pkt.dts = dts;
		commonStatus->enc_pkt.duration = duration ;
		LOGD("index:%d,pts:%lld,dts:%lld,duration:%lld,time_base:%d,%d",commonStatus->v_count,
	                        (long long) commonStatus->enc_pkt.pts,
	                        (long long) commonStatus->enc_pkt.dts,
	                        (long long) commonStatus->enc_pkt.duration,
	                        time_base1.num, time_base1.den);
		commonStatus->v_count++ ;
		**/
		// if(ji == 0)
		// {
		// 	int64_t pts_time = av_rescale_q(commonStatus->enc_pkt.dts, time_base1, time_base_q);  
	 //        int64_t now_time = av_gettime() - commonStatus->start_time;  
	 //        if (pts_time > now_time)  
	 //        {
	 //        	LOGD("index:sleep = %lld",pts_time - now_time);
	 //            av_usleep(pts_time - now_time);
	 //        }
		// }
	    

		//  以下是新的计算方法

		double frame_size = av_stream->codec->frame_size;

		int64_t calc_duration = 0 ;
		if(ji == 1)
		{
			double pts = commonStatus->v_count *(time_base1.den)/((time_base1.num)*commonStatus->fps);;
			double dts = pts;
			double duration = (time_base1.den)/((time_base1.num)*commonStatus->fps);
			commonStatus->enc_pkt.pts = pts;
			commonStatus->enc_pkt.dts = dts;
			commonStatus->enc_pkt.duration = duration ;
			commonStatus->v_count++ ;
        	
		}
		else
		{
			double pts = commonStatus->v_count *(time_base1.den)/((time_base1.num)*commonStatus->fps);;
			double dts = pts;
			double duration = (time_base1.den)/((time_base1.num)*commonStatus->fps);
			commonStatus->enc_pkt.pts = pts;
			commonStatus->enc_pkt.dts = dts;
			commonStatus->enc_pkt.duration = duration ;
			commonStatus->v_count++ ;

		}

				LOGD("index:%d,ji:%d,pts:%lld,dts:%lld,duration:%lld,time_base:%d,%d cal:%lld,time_base_q:%d,%d fsize:%f",
							commonStatus->v_count,ji,
	                        (long long) commonStatus->enc_pkt.pts,
	                        (long long) commonStatus->enc_pkt.dts,
	                        (long long) commonStatus->enc_pkt.duration,
	                        time_base1.num, time_base1.den,
	                        calc_duration,
	                        time_base_q.num,time_base_q.den,
	                        frame_size);

		//		LOGD("index:%lld",calc_duration);

		//packet_queue_put1(&avq,&commonStatus->enc_pkt);
		ret = av_interleaved_write_frame(commonStatus->ofmt_ctx, &commonStatus->enc_pkt);
		if(ret != 0)
		{
			LOGE("av_interleaved_write_frame failed");
		}
	}

	(*env)->ReleaseByteArrayElements(env,buffer_,in,0);
	return 0 ; 

}

//停止推流
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_stopStream(JNIEnv *env,jobject jobject)
{ 
	commonStatus->play_status = PLAY_STATUS_STOP ;
    av_write_trailer(commonStatus->ofmt_ctx);
	if (videostatus->video_st)
        avcodec_close(videostatus->video_st->codec);
    if(audiostatus->mic_audio_st)
    	avcodec_close(audiostatus->mic_audio_st->codec);
    if (commonStatus->ofmt_ctx) {
        avio_close(commonStatus->ofmt_ctx->pb);
        avformat_free_context(commonStatus->ofmt_ctx);
        av_free(videostatus->pFrameYUV);  
        av_free(audiostatus->mic_frame); 
        commonStatus->ofmt_ctx = NULL;
    }
    return 0;
}

// 新增队列 废弃
void *wirte_thread(void *arg)
{
	LOGD("start wirte thread 1 --- ");
	sleep(10);
	LOGD("start wirte thread 2 --- ");
	AVPacket pkt1 , *packet = &pkt1 ;
	int ret ;
	int i = 0 ;
	while(1){
		packet_queue_get1(&avq,packet,1);
		LOGD("start wirte thread  --- %d",i);
		i++;
		if(packet == NULL)
		{
			usleep(0.01*1000*1000);
		}
		ret = av_interleaved_write_frame(commonStatus->ofmt_ctx, packet);
		if(ret != 0)
		{
			LOGE("av_interleaved_write_frame failed");
		}
	}
}

// 增加队列
// 初始化队列
void packet_queue_init1(PacketQueue *q) 
{
    memset(q,0,sizeof(PacketQueue));
    q->first_ptk = NULL; 
    q->last_pkt = NULL;
    packet_queue_clean(q);
    pthread_mutex_init(&q->mutex , NULL) ; // 初始化锁
}

// 加入队列
int packet_queue_put1(PacketQueue *q, AVPacket *pkt) 
{

    AVPacketList *pkt1 = (AVPacketList *) av_malloc(sizeof(AVPacketList));
    if(!pkt1)
    {
        return -1 ;
    }
    if(q == NULL)
        return -1 ;
    //对互斥锁上锁
    if(pthread_mutex_lock(&q->mutex) != 0)
    {
        printf("packet_queue_put Thread lock failed! \n") ;
        return -1 ;
    }
    pkt1->pkt = *pkt ;
    pkt1->next = NULL; 

    if(q == NULL)
        return -1 ;
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
int packet_queue_get1(PacketQueue *q,AVPacket *pkt, int block)  
{
    AVPacketList *pkt1 = NULL;
    int ret = 0 ; 

    if(q == NULL)
        return -1 ;
    //对互斥锁上锁
    if(pthread_mutex_lock(&q->mutex) != 0)
    {
        printf("packet_queue_get Thread lock failed! \n") ;
        return -1 ;
    }

    if(q == NULL)
        return -1 ;
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
    pthread_mutex_unlock(&q->mutex);
    return ret ;
}

void packet_queue_destory1(PacketQueue *q)
{
    if(q != NULL)
        pthread_mutex_destroy(&q->mutex) ;
}

void packet_queue_clean1(PacketQueue *q)
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
