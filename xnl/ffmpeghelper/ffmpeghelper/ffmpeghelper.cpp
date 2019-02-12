// ffmpeghelper.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "ffmpeghelper.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern "C"
{
#include <libavformat/avformat.h>  
#include <libavutil/dict.h>
	//编码
#include <libavcodec/avcodec.h>
	//封装格式处理
	//像素处理
#include "libswscale/swscale.h"

#include "libavutil/imgutils.h"

#include <libswresample/swresample.h>
};


class XPlayContext{
private:
	const char * szMediaUri = 0;
	AVFormatContext *pFormatCtx = 0;
	AVCodec * codec[2] ;
	AVCodecContext * ccc[2];
	int v_codec_idx = -1, a_codec_idx = -1;
	int fps;
	AVPacket *packet = 0;
	AVFrame *pFrameDest = 0;
	uint8_t *video_out_buffer = 0;
	SwsContext * sws_ctx = 0;
	SwrContext * swr_ctx = 0;
	uint8_t  **dst_data = NULL;
	int  dst_linesize = 0;
	int dst_channel = 2;//双声道
	int dst_sample = 44100;	//采样率
	int dst_sample_fmt = AV_SAMPLE_FMT_S16; //采样格式
	AVFrame * pFrame = 0;
	AVCodecContext * current_codec = 0;
	AVPixelFormat destfmt = AV_PIX_FMT_BGRA;
	int frameProcessed = 0;

	int err_code = 0;

	int audiobuffer = 0, videobuffer = 0;


public:
	XPlayContext(){
		codec[0] = codec[1] = 0;
		ccc[0] = ccc[1] = 0;
	}

	bool openMedia(const char * uri){
		szMediaUri = uri;
	
		if (avformat_open_input(&pFormatCtx, uri, NULL, NULL) != 0){
			return false;//打不开
		}
		if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
			avformat_free_context(pFormatCtx);
			return false;//找不到媒体信息
		}

		for (int i = 0; i < pFormatCtx->nb_streams; i++){
			//流的类型
			AVStream *as = pFormatCtx->streams[i];

			if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
				as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
					v_codec_idx = i;	//视频流ID
					//获取FPS
					fps = (int)av_q2d(as->avg_frame_rate);
				} else
				if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
					a_codec_idx = i;	//音频流ID
				}

				//查找解码器
				codec[as->codecpar->codec_type] = avcodec_find_decoder(as->codecpar->codec_id);

				////分配解码上下文
				ccc[as->codecpar->codec_type] = avcodec_alloc_context3(codec[as->codecpar->codec_type]);

				//设置解码器参数
				avcodec_parameters_to_context(ccc[as->codecpar->codec_type], as->codecpar);
				//av_codec_set_pkt_timebase(ccc[as->codecpar->codec_type], as->time_base);
				//执行解码器
				avcodec_open2(ccc[as->codecpar->codec_type], codec[as->codecpar->codec_type], nullptr);
			}
		}

		return true;
	}

	//媒体格式信息
	const char * getFormatName(){
		return pFormatCtx->iformat->name;
	}

	//获取媒体时长
	const int64_t getDuartion(){
		return (pFormatCtx->duration) / 1000;
	}

	//获取视频媒体宽
	int getVideoWidth(){
		if (ccc[AVMEDIA_TYPE_VIDEO] != 0){
			return ccc[AVMEDIA_TYPE_VIDEO]->width;
		}
		return 0;
	}

	//获取视频媒体高
	int getVedeoHeight(){
		if (ccc[AVMEDIA_TYPE_VIDEO] != 0){
			return ccc[AVMEDIA_TYPE_VIDEO]->height;
		}
		return 0;
	}


	int getFps(){
		return fps;
	}

	//获取视频解码器名称
	const char * getCodecName(){
		if (ccc[AVMEDIA_TYPE_VIDEO] != 0){
			return codec[AVMEDIA_TYPE_VIDEO]->name;
		}
		return 0;
	}

	//是否包含视频信息
	bool hasVideo(){
		return codec[AVMEDIA_TYPE_VIDEO] != 0;
	}

	//是否包含音频信息
	bool hasAudio(){
		return codec[AVMEDIA_TYPE_AUDIO] != 0;
	}

	int getVideoBufferSize(){
		return videobuffer;
	}
	int getAudioBufferSize(){
		return audiobuffer;
	}
	//准备视频解码
	bool prepareVideo(){
		if (hasVideo()){
			pFrameDest = av_frame_alloc();
			//只有指定了AVFrame的像素格式、画面大小才能真正分配内存
			//缓冲区分配内存
			uint8_t *video_out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(destfmt, ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, 1));
			//初始化缓冲区
			err_code = av_image_fill_arrays(pFrameDest->data, pFrameDest->linesize, video_out_buffer, destfmt, ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, 1);

			if (err_code > 0){
				videobuffer = err_code;
				//用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
				sws_ctx = sws_getContext(ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, ccc[AVMEDIA_TYPE_VIDEO]->pix_fmt,
										 ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, destfmt,
										 SWS_FAST_BILINEAR, NULL, NULL, NULL);
			}
		}
		return sws_ctx != 0;
	}

	//准备音频解码
	bool prepareAudio(){
		swr_ctx = swr_alloc_set_opts(0,
									 av_get_default_channel_layout(dst_channel),
									 AV_SAMPLE_FMT_S16 /*位宽*/,
									 dst_sample/*采样率*/,
									 av_get_default_channel_layout(ccc[AVMEDIA_TYPE_AUDIO]->channels)/*原始位宽*/,
									 ccc[AVMEDIA_TYPE_AUDIO]->sample_fmt/*原始采样格式*/,
									 ccc[AVMEDIA_TYPE_AUDIO]->sample_rate/*原始采样率*/,
									 NULL,
									 NULL);

		err_code = swr_init(swr_ctx);

		if (err_code >= 0){
			err_code = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, av_get_default_channel_layout(dst_channel),
														  dst_sample, AV_SAMPLE_FMT_S16, 1);
			audiobuffer = err_code;
		}

		return err_code > 0;
	}

	bool beginDecode(){
		packet = (AVPacket*)av_malloc(sizeof(AVPacket));
		pFrame = av_frame_alloc();
		return true;
	}

	bool readStream(){

		current_codec = 0;

		err_code = av_read_frame(pFormatCtx, packet);

		if (err_code >= 0){
			if (packet->stream_index == v_codec_idx){
				current_codec = ccc[AVMEDIA_TYPE_VIDEO];
			} else
			if (packet->stream_index == a_codec_idx){
				current_codec = ccc[AVMEDIA_TYPE_AUDIO];
			}

			if (current_codec != 0){
				//7.解码一帧视频压缩数据，得到视频像素数据
				do{
					err_code = avcodec_send_packet(current_codec, packet);
				} while (EAGAIN == err_code);
			}
			//解码错误
			//assert(err_code >= 0);
			return (err_code == 0);
		}
		return false;
	}



	xint readMedia(xlong * out_lengths, xbyte * videoArray, xbyte * audioArray){

		if (current_codec == 0){
			return false;
		}

		if (avcodec_receive_frame(current_codec, pFrame) == 0){

			out_lengths[1] = pFrame->pts * av_q2d(pFormatCtx->streams[packet->stream_index]->time_base) * 1000;
			
			if (packet->stream_index == v_codec_idx){
				//AVFrame转为像素格式YUV420，宽高
				//2 6输入、输出数据
				//3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
				//4 输入数据第一列要转码的位置 从0开始
				//5 输入画面的高度
				
				
				int64_t timestamp = av_rescale_q(pFrame->pts, current_codec->time_base, pFormatCtx->streams[packet->stream_index]->time_base);
				err_code = sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, current_codec->height,
						  pFrameDest->data, pFrameDest->linesize);
				if (err_code > 0){
					int size = current_codec->width * current_codec->height * 4;
					out_lengths[0] = size;
					memcpy(videoArray, pFrameDest->data[0], size);
					frameProcessed++;
					return 1;
				}
			} else
			if (packet->stream_index == a_codec_idx){
				err_code = swr_convert(swr_ctx, dst_data, dst_linesize, (const uint8_t **)pFrame->data, pFrame->nb_samples);
				if (err_code >= 0){
					err_code = (err_code * dst_channel * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

					if (err_code > 0){
						//fwrite(*dst_data, result_size, 1, aofp);
						out_lengths[0] = err_code;
						memcpy(audioArray, *dst_data, err_code);
						return 2;
					}
				}
				/*int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, av_get_default_channel_layout(channel),
				ret, AV_SAMPLE_FMT_S16, 1);*/
				
			}
		} else{
			av_packet_unref(packet);
			current_codec = 0;
			return 0;
		}
		return -1;
	}

	bool seek(int64_t pos){
		if (av_seek_frame(pFormatCtx, -1, (pos / 1000) * AV_TIME_BASE, AVSEEK_FLAG_FRAME) >= 0){
			
			/*if (ccc[AVMEDIA_TYPE_VIDEO] != 0){
				avcodec_flush_buffers(ccc[AVMEDIA_TYPE_VIDEO]);
			}

			if (ccc[AVMEDIA_TYPE_AUDIO] != 0){
				avcodec_flush_buffers(ccc[AVMEDIA_TYPE_AUDIO]);
			}*/
			
			return true;
		}
		return false;
	}

	void closeStream(){
		av_frame_free(&pFrame);
	}


	void closeMedia(){
		swr_free(&swr_ctx);
		sws_freeContext(sws_ctx);

		if (ccc[AVMEDIA_TYPE_VIDEO] != 0){
			avcodec_close(ccc[AVMEDIA_TYPE_VIDEO]);
		}

		if (ccc[AVMEDIA_TYPE_AUDIO] != 0){
			avcodec_close(ccc[AVMEDIA_TYPE_AUDIO]);
		}

		avformat_free_context(pFormatCtx);
	}
};

/*
void main()
{
	//获取输入输出文件名
	const char *input = "d:\\2EF9253C0CC2AC6FA4334CF205CF615F.mp4";

	const char *output = "D:\\Cadaqs\\Desktop\\aa.argb";


	//封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
	AVFormatContext *pFormatCtx = 0;

	//2.打开输入视频文件
	if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0){
		printf("%s", "无法打开输入视频文件");
		return;
	}

	//3.获取视频文件信息
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
		printf("%s", "无法获取视频文件信息");
		return;
	}

	//获取视频流的索引位置
	//遍历所有类型的流（音频流、视频流、字幕流），找到视频流
	AVCodec * codec[2] = { 0, 0 };
	AVCodecContext * ccc[2] = { 0, 0 };

	int v_codec_idx = 0, a_codec_idx = 0;

	int i = 0;
	int fps = 0;
	//number of streams
	for (; i < pFormatCtx->nb_streams; i++)
	{
		//流的类型
		AVStream *as = pFormatCtx->streams[i];
		if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO || 
			as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
				v_codec_idx = i;
			}else
			if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
				a_codec_idx = i;
			}

			codec[as->codecpar->codec_type] = avcodec_find_decoder(as->codecpar->codec_id);
			ccc[as->codecpar->codec_type] = avcodec_alloc_context3(codec[as->codecpar->codec_type]);
			avcodec_parameters_to_context(ccc[as->codecpar->codec_type], as->codecpar);
			//av_codec_set_pkt_timebase(ccc[as->codecpar->codec_type], as->time_base);
			avcodec_open2(ccc[as->codecpar->codec_type], codec[as->codecpar->codec_type], nullptr);
			fps = (int)av_q2d(as->avg_frame_rate);
		}
	}

	//输出视频信息
	printf("视频的文件格式：%s\n", pFormatCtx->iformat->name);
	printf("视频时长：%d\n", (pFormatCtx->duration) / 1000000);
	printf("视频的宽高：%d,%d\n", ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height);
	printf("解码器的名称：%s\n", codec[AVMEDIA_TYPE_VIDEO]->name);

	//准备读取
	//AVPacket用于存储一帧一帧的压缩数据（H264）
	//缓冲区，开辟空间
	AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));

	//AVFrame用于存储解码后的像素数据(YUV)
	//内存分配
	//AVFrame *pFrame = av_frame_alloc();
	//YUV420
	//############################ VIDEO
	AVFrame *pFrameARGB = av_frame_alloc();
	//只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	//缓冲区分配内存
	uint8_t *video_out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(destfmt, ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, 1));
	//初始化缓冲区
	av_image_fill_arrays(pFrameARGB->data, pFrameARGB->linesize, video_out_buffer, destfmt, ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, 1);

	//用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	struct SwsContext *v_sws_ctx = sws_getContext(ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, ccc[AVMEDIA_TYPE_VIDEO]->pix_fmt,
												ccc[AVMEDIA_TYPE_VIDEO]->width, ccc[AVMEDIA_TYPE_VIDEO]->height, destfmt,
												SWS_BICUBIC, NULL, NULL, NULL);



	//////////////////////////////////// AUDIO
	
	int channel = 2;//双声道
	int sample = 44100;

	uint8_t  **dst_data = NULL;


	//用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	struct SwrContext *a_sws_ctx = swr_alloc_set_opts(0, 
													  av_get_default_channel_layout(channel),
													  AV_SAMPLE_FMT_S16 /*位宽* /,
													  sample/*采样率* /, 
													  av_get_default_channel_layout(ccc[AVMEDIA_TYPE_AUDIO]->channels)/*原始位宽* /,
													  ccc[AVMEDIA_TYPE_AUDIO]->sample_fmt/*原始采样格式* /,
													  ccc[AVMEDIA_TYPE_AUDIO]->sample_rate/*原始采样率* /,
													  NULL,
													  NULL);

	int ret = swr_init(a_sws_ctx);

	int  dst_linesize;

	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, av_get_default_channel_layout(channel),
											 sample, AV_SAMPLE_FMT_S16, 1);
	
	assert(a_sws_ctx != 0);

	int frame_count = 0;

	AVFrame * pFrame = av_frame_alloc();
	FILE * aofp = fopen("d:\\ssss.pcm", "wb");
	//6.一帧一帧的读取压缩数据
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		//只要视频压缩数据（根据流的索引位置判断）
		AVCodecContext * codec = 0;

		if (packet->stream_index == v_codec_idx){
			codec = ccc[AVMEDIA_TYPE_VIDEO];
		}else
		if (packet->stream_index == a_codec_idx){
			codec = ccc[AVMEDIA_TYPE_AUDIO];
		}
		
		//7.解码一帧视频压缩数据，得到视频像素数据
		do{
			ret = avcodec_send_packet(codec, packet);
		}while (EAGAIN == ret);

		if (ret != 0){
			printf("%s", "解码错误");
			return;
		}

		
		while (avcodec_receive_frame(codec, pFrame) == 0){
			if (packet->stream_index == v_codec_idx){
				//AVFrame转为像素格式YUV420，宽高
				//2 6输入、输出数据
				//3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
				//4 输入数据第一列要转码的位置 从0开始
				//5 输入画面的高度
				sws_scale(v_sws_ctx, pFrame->data, pFrame->linesize, 0, codec->height,
						  pFrameARGB->data, pFrameARGB->linesize);

				//输出到YUV文件
				//AVFrame像素帧写入文件
				//data解码后的图像像素数据（音频采样数据）
				//Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
				//U V 个数是Y的1/4
				int y_size = codec->width * codec->height;

				//fwrite(pFrameARGB->data[0], 1, y_size * 4, fp_yuv);
				/*fwrite(pFrameARGB->data[1], 1, y_size / 4, fp_yuv);
				fwrite(pFrameARGB->data[2], 1, y_size / 4, fp_yuv);*/
				//SaveRawbmp("d:\\sss.bmp", pFrameARGB->data[1], codec->width, codec->height);
				/*BYTE * p = (BYTE*)(pFrameARGB->data[0]);
				Gdiplus::Bitmap * b = new Gdiplus::Bitmap(codec->width, codec->height, PixelFormat32bppARGB);
				* /
				frame_count++;
				printf("解码第%d帧\n", frame_count);
			} else
			if (packet->stream_index == a_codec_idx){
				int ret = swr_convert(a_sws_ctx, dst_data, dst_linesize, (const uint8_t **)pFrame->data, pFrame->nb_samples);
	
				int result_size = (ret * channel * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

				if (ret < 0) {
					assert(0);
				}
				/*int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, av_get_default_channel_layout(channel),
														 ret, AV_SAMPLE_FMT_S16, 1);* /
				fwrite(*dst_data, result_size, 1, aofp);
			}
		}
		

		//释放资源
		av_packet_unref(packet);
	}

	fclose(aofp);

	av_frame_free(&pFrame);

	avcodec_close(ccc[AVMEDIA_TYPE_VIDEO]);

	avformat_free_context(pFormatCtx);
}
*/

XNLEXPORT xlong XI_STDCALL openMedia(xstring uri){
	XPlayContext * xpc = new XPlayContext();
	if (xpc->openMedia(uri)){
		return (xlong)xpc;
	}
	delete xpc;
	return 0;
}

XNLEXPORT xint XI_STDCALL prepareMedia(xlong hpc){
	xint ret = 0;
	XPlayContext * xpc = (XPlayContext*)hpc;
	if (xpc->prepareVideo()){
		ret |= 1;
	}
	if (xpc->prepareAudio()){
		ret |= 2;
	}
	return ret;
}

XNLEXPORT xbool XI_STDCALL beginDecode(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->beginDecode();
}


XNLEXPORT xbool XI_STDCALL readStream(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->readStream();
}

XNLEXPORT xint XI_STDCALL readMedia(xlong hpc, xlong * lengths, xbyte * video, xbyte * audio){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->readMedia(lengths, video, audio);
}


XNLEXPORT void XI_STDCALL closeStream(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	xpc->closeStream();
	delete xpc;
}


XNLEXPORT void XI_STDCALL closeMedia(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	xpc->closeMedia();
}

XNLEXPORT xlong XI_STDCALL getVideoBufferSize(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getVideoBufferSize();
}

XNLEXPORT xlong XI_STDCALL getAudioBufferSize(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getAudioBufferSize();
}

XNLEXPORT xint XI_STDCALL getVideoWidth(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getVideoWidth();
}

XNLEXPORT xint XI_STDCALL getVideoHeight(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getVedeoHeight();
}

XNLEXPORT xint XI_STDCALL getVideoFps(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getFps();
}

XNLEXPORT xbool XI_STDCALL seekMedia(xlong hpc, xlong position){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->seek(position);
}


XNLEXPORT xlong XI_STDCALL getDuration(xlong hpc){
	XPlayContext * xpc = (XPlayContext*)hpc;
	return xpc->getDuartion();
}