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
#include "libavutil/log.h"


#define OPEN_LOG

#define LOG_TAG    "MediaRecodeMicJni"
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

// 对摄像头进行录制保存AAC编码
//http://blog.csdn.net/zh_ang_hua/article/details/47276357
// 解决问题
//http://blog.csdn.net/peter327447/article/details/53215731

AVFormatContext* mic_pFormatCtx;
AVOutputFormat* mic_ofmt;  
AVStream* mic_audio_st;  

AVFrame *mic_frame;
AVCodec *mic_codec = NULL;
AVPacket mic_packet;
AVCodecContext *mic_pCodecCtx;
uint8_t* mic_frame_buf;
int size = 0 ;
int mic_index ; 
int mic_got_frame ;
int mic_ret ;
AVPacket mic_pkt;

//
/**
 * 自定义日志输出回调接口,可以获取到ffmpeg系统的一些日志
 */
void mylog(void* p, int v, const char* str, va_list lis){

	va_list vl2;
	char line[1024];
	static int print_prefix = 1;


	va_copy(vl2, lis);
	av_log_format_line(p, v, str, vl2, line, sizeof(line), &print_prefix);
	va_end(vl2);
	LOGE("%s",line);
}

/**
 * 用于输出编码器中剩余的AVPacket。
 */
int flush_all_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret = 0;
			break;
		}
		LOGE("Flush %d Encoder: Succeed to encode 1 frame!\tsize:%5d\n", stream_index,enc_pkt.size);
		/* mux encoded frame */

		enc_pkt.stream_index = stream_index;
		ret = av_interleaved_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

// 参考地址 
//http://blog.csdn.net/u011913612/article/details/53838675

// 主要是麦克风录制，利用ffmpeg编码采集数据
// 初始化参数
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordMicInit(JNIEnv *env,jobject jobject,
                                                            jstring filename){
	const char *out_file = (*env)->GetStringUTFChars(env,filename,0);
	LOGD("recordMicInit %s",out_file);
	av_log_set_callback(mylog);
	av_log_set_level(AV_LOG_INFO);
	av_register_all();
	avformat_network_init();
	mic_index = 0 ;
	LOGD("111");
	mic_pFormatCtx = avformat_alloc_context();
	//mic_ofmt = av_guess_format(NULL, out_file, NULL);
	mic_ofmt = av_guess_format(NULL, "file.aac", NULL);
	LOGD("222 %p",mic_ofmt);
	mic_pFormatCtx->oformat = mic_ofmt;
	LOGD("333");
	if(avio_open(&mic_pFormatCtx->pb,out_file,AVIO_FLAG_READ_WRITE) < 0)
	{
		LOGD("avio_open error");
		return -1 ;
	}
	LOGD("444");
	mic_audio_st = avformat_new_stream(mic_pFormatCtx,0);
	if(mic_audio_st == NULL)
	{
		LOGD("mic_audio_st is NULL");
		return -2 ;
	}   
	LOGD("555");
	mic_pCodecCtx = mic_audio_st->codec;
	mic_pCodecCtx->codec_id = mic_ofmt->audio_codec;
	LOGD("555-1 codecid %d",mic_pCodecCtx->codec_id);
	mic_pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16 ;
	mic_pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO; 
	mic_pCodecCtx->sample_rate = 16000;
	mic_pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	mic_pCodecCtx->channels = av_get_channel_layout_nb_channels(mic_pCodecCtx->channel_layout); 
	mic_pCodecCtx->bit_rate = 64000;
	LOGD("666 channels=%d sample_fmt=%d",mic_pCodecCtx->channels,mic_pCodecCtx->sample_fmt);
	av_dump_format(mic_pFormatCtx,0,out_file,1);
	LOGD("777");
	mic_codec = avcodec_find_encoder(mic_pCodecCtx->codec_id); 
	if(!mic_codec){
		LOGD("avcodec_find_encoder error AV_CODEC_ID_SPEEX");
		return -1 ;
	}
	//LOGD("888 %d %d",mic_pCodecCtx-codec_type,mic_codec->type);
	int r = 0 ;
	if((r = avcodec_open2(mic_pCodecCtx,mic_codec,NULL)) < 0)
	{
		LOGD("avcodec_open2 error888-  (%d)",r);// -22
		return -1 ;
	}
	LOGD("999");
	mic_frame = av_frame_alloc();
	mic_frame->nb_samples = mic_pCodecCtx->frame_size;
	mic_frame->format = mic_pCodecCtx->sample_fmt;
	LOGD("1111-1");
	size = av_samples_get_buffer_size(NULL,mic_pCodecCtx->channels,mic_pCodecCtx->frame_size,mic_pCodecCtx->sample_fmt,1);
	mic_frame_buf = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(mic_frame,mic_pCodecCtx->channels,mic_pCodecCtx->sample_fmt,(const uint8_t *)mic_frame_buf,size,1);
	LOGD("1111-2");
	avformat_write_header(mic_pFormatCtx,NULL);
	LOGD("size %d",size);



}
//摄像头采集数据
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordAduioPCMData(JNIEnv *env,jobject jobject,
                                                           jbyteArray buffer_,jint len){
	jbyte *in = (*env)->GetByteArrayElements(env,buffer_,NULL);

	mic_frame->data[0] = in ;
	mic_frame->pts = mic_index * 100 ;
	mic_got_frame = 0 ; 
	mic_ret = avcodec_encode_audio2(mic_pCodecCtx,&mic_pkt,mic_frame,&mic_got_frame);
	if(mic_ret < 0)
	{
		LOGD("failed to encode");
		return -1;
	}
	if(mic_got_frame == 1)
	{
		 mic_index++ ;
		 LOGD("Succeed to encode %d frame! \tsize:%5d\n",mic_index,mic_pkt.size); 
		 mic_pkt.stream_index = mic_audio_st->index;
		 //av_interleaved_write_frame可以写多个流进来，只有单一流av_write_frame，av_interleaved_write_frame都函数都可以用。
		 mic_ret = av_write_frame(mic_pFormatCtx,&mic_pkt); 
		 //mic_ret = av_interleaved_write_frame(mic_pFormatCtx,&mic_pkt); // 写入本地文件和流文件
		 LOGD("av_write_frame result %d \n",mic_ret); 
		 av_free_packet(&mic_pkt); 
	}

	(*env)->ReleaseByteArrayElements(env,buffer_,in,0);
	
}
// 结束
JNIEXPORT int Java_com_jqh_jni_nativejni_MediaPlayerJni_recordMicStop(JNIEnv *env,jobject jobject){

	//  将编码剩余输出
	flush_all_encoder(mic_pFormatCtx,0);
	av_write_trailer(mic_pFormatCtx);
	if (mic_audio_st){  
        avcodec_close(mic_audio_st->codec);  
        av_free(mic_frame);  
        av_free(mic_frame_buf);  
    }  
    avio_close(mic_pFormatCtx->pb);  
    avformat_free_context(mic_pFormatCtx);   
}