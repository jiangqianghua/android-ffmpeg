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

#define LOG_TAG    "MediaREcodeCameraJni"
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

// 对摄像头进行录制保存
//http://blog.csdn.net/zh_ang_hua/article/details/47276357

AVCodecContext *pCodecCtx;
AVCodec *pCodec ;
AVPacket enc_pkt;
AVFrame *pFrameYUV ;
int yuv_width ; 
int yuv_height;
int y_length ; 
int uv_length ;
int width1 = 352;
int height1 = 288;
int fps1 = 15;
int count2 = 0 ;

FILE * video_file ;

// 主要是视频录制，利用ffmpeg编码采集数据,录制效果更好
// 初始化参数
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordInit(JNIEnv *env,jobject jobject,
                                                            jstring filename){

// 计算yuv数据的长度
	yuv_width = width1 ;
	yuv_height = height1 ;
	y_length = width1*height1;
	uv_length = width1*height1/4;

	av_register_all();
	//  初始参数初始化
	pCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4); //AV_CODEC_ID_H264
	if(!pCodec)
	{
		LOGE("can not find encoder MPEG4");
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
	pCodecCtx->width = width1 ; 
	pCodecCtx->height = height1 ;
	pCodecCtx->framerate = (AVRational){fps1,1}; // 1秒15帧
	// 帧率的基本单位，分数表示
	pCodecCtx->time_base = (AVRational){1,fps1};
	// 目标编码率，采样的码率，采样码越大，视频大小越大
	pCodecCtx->bit_rate = 400000;
	//所以GOP Size是连续的画面组大小的意思。
	pCodecCtx->gop_size = 50 ;

	//表示在压制，容易压制和难压制的场景，允许在0.0-1.0
	pCodecCtx->qcompress = 0.6;
	//最大和最小化系数
	pCodecCtx->qmin = 10;
	pCodecCtx->qmax = 51;
	// 两个非B帧之间云寻出现多少个B帧数
	//设置0表示不使用B帧
	//b帧越多，图片越少
	pCodecCtx->max_b_frames = 0 ;


	int r = 0 ;
	if((r = avcodec_open2(pCodecCtx,pCodec,NULL)) < 0)
	{
		LOGE("failed to open encoder %d",r);
		return -2 ;
	}
	
	//jbyte * filedir = (jbyte *)(*env)->GetByteArrayElements(env,filename,0);
	const char *out_path = (*env)->GetStringUTFChars(env,filename,0);
	LOGE("start open %s\n",out_path);
	if((video_file = fopen(out_path,"wb")) == NULL)
	{
		LOGE("open %s failed\n",out_path);
		return -4;
	}
	LOGE("start open ok %s\n",out_path);
	//(*env)->ReleaseByteArrayElements(env,out_path,filename,0);

}
//摄像头采集数据
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordYuvData(JNIEnv *env,jobject jobject,
                                                           jbyteArray buffer_){
	int ret = 0 ;
	LOGD("count3 %d",count2);
	jbyte *in = (*env)->GetByteArrayElements(env,buffer_,NULL);
	pFrameYUV = av_frame_alloc();
	int picture_size = av_image_get_buffer_size(pCodecCtx->pix_fmt,pCodecCtx->width,
		pCodecCtx->height,1);
	uint8_t *buffers = (uint8_t *)av_malloc(picture_size);

	// 将buffers的地址复制给AVFrame中图像数据，根据像素格式判断有几个数据指针
	av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize,buffers,pCodecCtx->pix_fmt,
		pCodecCtx->width,pCodecCtx->height,1);

	memcpy(pFrameYUV->data[0],in,y_length);
	pFrameYUV->pts = count2 ;
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
		enc_pkt.pts = count2;
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.pos = -1;
		//ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		fwrite(enc_pkt.data,1,enc_pkt.size,video_file);
		if(ret != 0)
		{
			LOGE("av_interleaved_write_frame failed");
		}
		count2++ ;
	}

	(*env)->ReleaseByteArrayElements(env,buffer_,in,0);
	
}
// 结束
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordStop(JNIEnv *env,jobject jobject){

	fclose(video_file);
	// avcodec_close(pCodecCtx);
	// av_free(pCodecCtx);
	// av_free(&yuv420pframe->data[0]);
	// av_frame_free(&yuv420pframe);
}