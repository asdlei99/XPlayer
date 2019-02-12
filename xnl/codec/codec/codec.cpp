// codec.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "xdef.h"
#include "AudioPlayer.h"

xlong XI_STDCALL audio_create(xint channel, xint sample, xint widebits, xint buffersize){
	AudioPlayer * player = new AudioPlayer();

	player->create(channel, sample, widebits, buffersize);

	return (xlong)player;
}

xint XI_STDCALL audio_writeData(xlong handle, xptr data, xint size){
	AudioPlayer * player = (AudioPlayer*)handle;
	return player->write(data, size);
}

xbool XI_STDCALL audio_play(xlong handle){
	AudioPlayer * player = (AudioPlayer*)handle;
	return player->play();
}

xbool XI_STDCALL audio_stop(xlong handle){
	AudioPlayer * player = (AudioPlayer*)handle;
	return player->stop();
}

xbool XI_STDCALL audio_pause(xlong handle){
	AudioPlayer * player = (AudioPlayer*)handle;
	return player->pause();
}

void XI_STDCALL audio_cleanup(xlong handle){
	AudioPlayer * player = (AudioPlayer*)handle;
	player->stop();
}

void XI_STDCALL audio_destroy(xlong handle){
	AudioPlayer * player = (AudioPlayer*)handle;
	player->stop();
	delete player;
}

int XI_STDCALL audio_getPosition(xlong handle){
	return 0;
}

