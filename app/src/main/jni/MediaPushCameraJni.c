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

#define LOG_TAG    "MediaPushCameraJni"
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

AVFormatContext *ofmt_ctx;
AVStream *video_st;
AVCodecContext *pCodecCtx;
AVCodec *pCodec ;
AVPacket enc_pkt;
AVFrame *pFrameYUV ;
int yuv_width ; 
int yuv_height;
int y_length ; 
int uv_length ;
int width = 480;
int height = 320;
int fps = 15;
int count1 = 0 ;



// 参考地址 https://www.jianshu.com/p/462e489b7ce0
// 推送本地摄像头

/**
 * 自定义日志输出回调接口,可以获取到ffmpeg系统的一些日志
 */
void mylog1(void* p, int v, const char* str, va_list lis){

	va_list vl2;
	char line[1024];
	static int print_prefix = 1;


	va_copy(vl2, lis);
	av_log_format_line(p, v, str, vl2, line, sizeof(line), &print_prefix);
	va_end(vl2);
	LOGE("%s",line);
}


// 初始化参数
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_pushCameraInit(JNIEnv *env,jobject jobject,
                                                            jstring url_)
{
	const char *out_path = (*env)->GetStringUTFChars(env,url_,0);
	LOGD("output334 url : %s" , out_path);

	// 计算yuv数据的长度
	yuv_width = width ;
	yuv_height = height ;
	y_length = width*height;
	uv_length = width*height/4;
	av_log_set_callback(mylog1);
	av_log_set_level(AV_LOG_INFO);
	av_register_all();
	avformat_network_init();
	//  初始参数初始化
	avformat_alloc_output_context2(&ofmt_ctx,NULL,"flv",out_path);
	pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if(!pCodec)
	{
		LOGE("can not find encoder h264");
		return -1 ;
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);
	// 设置编码器id  h264的id
	pCodecCtx->codec_id = pCodec->id;
	// 设置像素格式，采取什么样的色彩空间来表示一个像素点
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	// 设置编码器的数据类型
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	// 编码目标的帧大小，像素为单位
	pCodecCtx->width = width ; 
	pCodecCtx->height = height ;
	pCodecCtx->framerate = (AVRational){fps,1}; // 1秒15帧
	// 帧率的基本单位，分数表示
	pCodecCtx->time_base = (AVRational){1,fps};
	// 目标编码率，采样的码率，采样码越大，视频大小越大
	pCodecCtx->bit_rate = 400000;
	//所以GOP Size是连续的画面组大小的意思。
	pCodecCtx->gop_size = 50 ;

	if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	//表示在压制，容易压制和难压制的场景，允许在0.0-1.0
	pCodecCtx->qcompress = 0.6;
	//最大和最小化系数
	pCodecCtx->qmin = 10;
	pCodecCtx->qmax = 51;
	// 两个非B帧之间云寻出现多少个B帧数
	//设置0表示不使用B帧
	//b帧越多，图片越少
	pCodecCtx->max_b_frames = 0 ;
	// 设置h264编码参数
	AVDictionary *param = 0 ;
	if(pCodecCtx->codec_id == AV_CODEC_ID_H264){
		/**
         * 这个非常重要，如果不设置延时非常的大
         * ultrafast,superfast, veryfast, faster, fast, medium
         * slow, slower, veryslow, placebo.　这是x264编码速度的选项
       */
		av_dict_set(&param, "preset", "superfast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
        av_opt_set(pCodecCtx->priv_data, "tune", "zerolatency", 0);
        av_opt_set(pCodecCtx->priv_data, "preset", "superfast", 0);
	}

	int r = 0 ;
	if((r = avcodec_open2(pCodecCtx,pCodec,&param)) < 0)
	{
		LOGE("failed to open encoder %d",r);
		return -2 ;
	}
	// 添加一个新的流给输出url
	video_st = avformat_new_stream(ofmt_ctx,pCodec);
	if(video_st == NULL)
	{
		LOGE("av new stream error");
		return -3;
	}
	video_st->time_base.num = 1 ; 
	video_st->time_base.den = fps;
	video_st->codec = pCodecCtx ;

	//video_st->codecpar->codec_tag = 0 ; 

	//avcodec_parameters_from_context(video_st->codecpar,pCodecCtx);

	if(avio_open(&ofmt_ctx->pb,out_path,AVIO_FLAG_READ_WRITE) < 0)
	{
		LOGE("failed to open output file!\n");
		return -4;
	}

	avformat_write_header(ofmt_ctx,NULL);
	return 0 ;
}

// 开始刷视频
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_pushCameraOnFrame(JNIEnv *env,jobject jobject,
                                                            jbyteArray buffer_)
{
	int ret = 0 ;
	LOGD("count3 %d",count1);
	jbyte *in = (*env)->GetByteArrayElements(env,buffer_,NULL);
	pFrameYUV = av_frame_alloc();
	int picture_size = av_image_get_buffer_size(pCodecCtx->pix_fmt,pCodecCtx->width,
		pCodecCtx->height,1);
	uint8_t *buffers = (uint8_t *)av_malloc(picture_size);

	// 将buffers的地址复制给AVFrame中图像数据，根据像素格式判断有几个数据指针
	av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize,buffers,pCodecCtx->pix_fmt,
		pCodecCtx->width,pCodecCtx->height,1);

	memcpy(pFrameYUV->data[0],in,y_length);
	pFrameYUV->pts = count1 ;
	int i = 0 ;
	for(i = 0 ; i < uv_length; i++)
	{
		//将v数据存到第三个平面
		*(pFrameYUV->data[2] + i) = *(in + y_length + i*2);
		//将U数据存到第二个平面
		*(pFrameYUV->data[1] + i) = *(in + y_length + i*2 + 1);
	}
	pFrameYUV->format = AV_PIX_FMT_YUV420P;
	pFrameYUV->width = yuv_width ; 
	pFrameYUV->height = yuv_height;

	//初始化packet
	enc_pkt.data = NULL;
	enc_pkt.size = 0 ;
	av_init_packet(&enc_pkt);
 
	//对yuv进行编码
	// ret = avcodec_send_frame(pCodecCtx,pFrameYUV);
	// if(ret != 0)
	// {
	// 	LOGE("avcodec send frame error\n");
	// 	return -2;
	// }

	// // 获取编码后的数据
	// ret = avcodec_receive_packet(pCodecCtx,&enc_pkt);

	// if(ret != 0 || enc_pkt.size <= 0)
	// {
	// 	LOGE("avcodec_receive_packet error");
	// 	return -3;
	// }

	LOGD("test1");
	int frameFinished = 0 ;
	int size = avcodec_encode_video2(pCodecCtx,&enc_pkt,pFrameYUV,&frameFinished);
	LOGD("test2");
	av_frame_free(&pFrameYUV);
	if(size < 0){
		LOGE("avcodec_encode_video2 error");
		return -5;
	}
	LOGD("test3");
	if(frameFinished == 1)
	{
		enc_pkt.stream_index = video_st->index ; 
		AVRational time_base = ofmt_ctx->streams[0]->time_base;//{1,1000}
		enc_pkt.pts = count1 *(video_st->time_base.den)/((video_st->time_base.num)*fps);
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.duration = (video_st->time_base.den)/((video_st->time_base.num)*fps);
		LOGD("index:%d,pts:%lld,dts:%lld,duration:%lld,time_base:%d,%d",count1,
	                        (long long) enc_pkt.pts,
	                        (long long) enc_pkt.dts,
	                        (long long) enc_pkt.duration,
	                        time_base.num, time_base.den);
		enc_pkt.pos = -1;
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		if(ret != 0)
		{
			LOGE("av_interleaved_write_frame failed");
		}
		count1++ ;
	}

	(*env)->ReleaseByteArrayElements(env,buffer_,in,0);
	return 0 ; 

}

//停止推流
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_closePushCamera(JNIEnv *env,jobject jobject)
{
	if (video_st)
        avcodec_close(video_st->codec);
    if (ofmt_ctx) {
        avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
        ofmt_ctx = NULL;
    }
    return 0;
}

