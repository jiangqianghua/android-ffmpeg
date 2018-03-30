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

AVCodecContext *pCodecCtx = NULL;
AVPacket avpkt;
FILE * video_file ;
unsigned char * outbuf = NULL;
unsigned char * yuv420buf = NULL;
AVFrame * yuv420pframe = NULL;
static int outsize = 0 ; 
static int mwidth = 352 ; 
static int mheight = 288 ;
int count = 0 ;

// 主要是视频录制，利用ffmpeg编码采集数据
// 初始化参数
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordInit(JNIEnv *env,jobject jobject,
                                                            jbyteArray filename){

	AVCodec * pCodec = NULL;
	count = 0 ;
	avcodec_register_all();
	pCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
	if(pCodec == NULL){
		LOGE(" codec not found");
		return -1 ;
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if(pCodecCtx == NULL){
		LOGE("could not allocate video codec context");
		return -2 ;
	}

	pCodecCtx->bit_rate = 450000;
	pCodecCtx->width = mwidth ; 
	pCodecCtx->height = mheight;

	pCodecCtx->time_base = (AVRational){1,25};
	pCodecCtx->gop_size = 10 ; // emit one intra frame every ten frames
	pCodecCtx->max_b_frames = 1;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

	// open it
	if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
	{
		LOGE("could not open codec\n");
		return -3;
	}

	outsize = mwidth*mheight*2;
	outbuf = malloc(outsize*sizeof(char));
	jbyte * filedir = (jbyte *)(*env)->GetByteArrayElements(env,filename,0);
	LOGE("start open %s\n",filedir);
	if((video_file = fopen(filedir,"wb")) == NULL)
	{
		LOGE("open %s failed\n",filedir);
		return -4;
	}
	(*env)->ReleaseByteArrayElements(env,filename,filedir,0);

}
//摄像头采集数据
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordYuvData(JNIEnv *env,jobject jobject,
                                                           jbyteArray yuvdata){
	int frameFinished = 0 , size = 0 ;
	jbyte *ydata = (jbyte *)(*env)->GetByteArrayElements(env,yuvdata,0);

	yuv420pframe = NULL;
	av_init_packet(&avpkt);
	avpkt.data = NULL;
	avpkt.size = 0 ; 
	yuv420pframe = av_frame_alloc();
	int y_size = pCodecCtx->width * pCodecCtx->height;
	uint8_t* pictrue_buf ;
	int size1 = avpicture_get_size(pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height);
	pictrue_buf = (uint8_t*)av_malloc(y_size);
	if(!pictrue_buf)
	{
		av_free(yuv420pframe);
	}
	avpicture_fill((AVPicture*)yuv420pframe,pictrue_buf,pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height);
	yuv420pframe->pts = count;
	yuv420pframe->data[0] = ydata ; // PCM Data
	yuv420pframe->data[1] = ydata + y_size;  // U
	yuv420pframe->data[2] = ydata + y_size*5/4; // V
	size = avcodec_encode_video2(pCodecCtx,&avpkt,yuv420pframe,&frameFinished);
	count++;
	if(size < 0){
		LOGE("Error encoding frame");
		return -5;
	}

	if(frameFinished)
	{
		LOGD("write to file %d",count);
		fwrite(avpkt.data,1,avpkt.size,video_file);
	}

	av_free_packet(&avpkt);
	av_free(yuv420pframe);

	(*env)->ReleaseByteArrayElements(env,yuvdata,ydata,0);
	
}
// 结束
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordStop(JNIEnv *env,jobject jobject){

	fclose(video_file);
	// avcodec_close(pCodecCtx);
	// av_free(pCodecCtx);
	// av_free(&yuv420pframe->data[0]);
	// av_frame_free(&yuv420pframe);
	free(outbuf);	
}