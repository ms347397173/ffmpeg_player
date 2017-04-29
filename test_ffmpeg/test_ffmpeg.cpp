/* 学习使用ffmpeg
 * 功能：打开文件解码输出yuv文件
 *
*/

#include "stdafx.h"

#define __STDC_CONSTANT_MACROS
#define SDL_MAIN_HANDLED

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

extern "C"
{
	#include<libavcodec\avcodec.h>
	#include<libavformat\avformat.h>
	#include<libswscale\swscale.h>
	#include<SDL.h>
}

int thread_exit = 0;
int sfp_refresh_thread(void *opaque)
{
	thread_exit = 0;
	SDL_Event event;

	while (!thread_exit)
	{
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(40); //40ms
	}
	thread_exit= 0;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}

int main(int argc, char* argv[])
{
	//printf("%s\n", avcodec_configuration());
	char * filepath = ".\\test_vedio\\屌丝男士.mov";
	AVFormatContext *pFormatCtx=NULL;
	int i = 0, videoIndex=0;
	AVCodecContext * pCodecCtx;
	AVCodec *pCodec=NULL;
	AVFrame * pFrame, *pFrameYUV;
	uint8_t * outBuffer;
	AVPacket * pPacket;
	int y_size;
	int ret, gotPicture;
	SwsContext *pimgConvertCtx;
	
	//------------SDL----------------
	int screen_w, screen_h;
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;
	SDL_Thread *video_tid;
	SDL_Event event;

	FILE * fpYUV;
	int frameCount=0;

	//avcodec_register_all();
	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)   //打开文件
	{
		printf("open input stream failed \n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)  //寻找流信息
	{
		printf("Couldn't find stream infomation\n");
		return -1;
	}

	//寻找视频流在数组中
	videoIndex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; ++i)
	{
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoIndex = i;
			break;
		}
	}
	if (videoIndex==-1)
	{
		printf("Didn't find a vedio stream \n");
		return -1;
	}

	pCodecCtx = avcodec_alloc_context3(NULL);
	if (pCodecCtx == NULL)
	{
		printf("Could not allocate AVCodecContext\n");
		return -1;
	}

	avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoIndex]->codecpar);

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);  //寻找解码器
	if (pCodec == NULL)
	{
		printf("decoder not found\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Could not open codec \n");
		return -1;
	}

	printf("时长：%d\n", pFormatCtx->duration);
	printf("封装格式：%s\n", pFormatCtx->iformat->long_name);

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	outBuffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P,pCodecCtx->width, pCodecCtx->height));
	avpicture_fill((AVPicture *)pFrameYUV, outBuffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);


	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");

	//获取sws上下文
	pimgConvertCtx = sws_getContext(pCodecCtx->width,
		pCodecCtx->height, 
		pCodecCtx->pix_fmt, 
		pCodecCtx->width, 
		pCodecCtx->height, 
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, 
		NULL, 
		NULL, 
		NULL);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	
	screen = SDL_CreateWindow("ffmpeg player's", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL);
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}
	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));

	video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, NULL);

	for(;;)
	{
		SDL_WaitEvent(&event);
		if (event.type == SFM_REFRESH_EVENT)  //如果是REFRESH事件
		{
			if (av_read_frame(pFormatCtx, pPacket) >= 0)
			{
				if (pPacket->stream_index == videoIndex)
				{
					/*sprintf(filename, "yuv\\%d.yuv", frameCount);
					FILE * fp = fopen(filename, "wb+");*/

					ret = avcodec_decode_video2(pCodecCtx, pFrame, &gotPicture, pPacket);
					if (ret < 0)
					{
						printf("Decode Error.\n");
						return -1;
					}
					if (gotPicture)
					{
						sws_scale(pimgConvertCtx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
						printf("Decoded frame index: %d\n", frameCount);

						//输出YUV数据 来自pFrameYUV
						SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]); //更新纹理
						SDL_RenderClear(sdlRenderer);  //清空渲染器
						SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
						SDL_RenderPresent(sdlRenderer);


						frameCount++;
					}


					//fclose(fp);
				}
				av_free_packet(pPacket);  //每次调用read前调用该函数
			}
			else
			{
				thread_exit = 1;
			}
		}
		else if(event.type==SDL_QUIT)//不是refresh事件
		{
			thread_exit = 1;
		}
		else if (event.type == SFM_BREAK_EVENT)
		{
			break;
		}
		else
		{}


	}

	////读取帧
	//frameCount = 0;
	//FILE * fp = fopen("test.yuv", "wb");

	//while (av_read_frame(pFormatCtx, pPacket) >= 0)
	//{
	//	
	//	if (pPacket->stream_index == videoIndex)
	//	{
	//		/*sprintf(filename, "yuv\\%d.yuv", frameCount);
	//		FILE * fp = fopen(filename, "wb+");*/
	//		
	//		ret = avcodec_decode_video2(pCodecCtx, pFrame, &gotPicture, pPacket);
	//		if (ret < 0) {
	//			printf("Decode Error.\n");
	//			return -1;
	//		}
	//		if (gotPicture)
	//		{
	//			sws_scale(pimgConvertCtx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
	//			printf("Decoded frame index: %d\n", frameCount);

	//			//输出YUV数据 来自pFrameYUV
	//			int height = pCodecCtx -> height;
	//			fwrite(pFrameYUV->data[0], pFrameYUV->linesize[0]/* pCodecCtx->width*/, height ,fp);
	//			fwrite(pFrameYUV->data[1], pFrameYUV->linesize[1]>>1/* pCodecCtx->width * 5 >> 2*/, height,fp);
	//			fwrite(pFrameYUV->data[2], pFrameYUV->linesize[2]>>1  /* pCodecCtx->width * 5 >> 2*/, height,fp);

	//			frameCount++;
	//		}


	//		//fclose(fp);
	//	}
	//	av_free_packet(pPacket);  //每次调用read前调用该函数

	//}
	//fclose(fp);


	sws_freeContext(pimgConvertCtx);

	SDL_Quit();

	av_frame_free(&pFrame);
	av_frame_free(&pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);


    return 0;
}

