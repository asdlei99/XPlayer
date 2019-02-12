#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include "alsa/asoundlib.h"
#include "xnl/xnl.h"


struct AudioOutput{
    snd_pcm_uframes_t frames;
    snd_pcm_uframes_t periodsize;
    snd_pcm_t *playback_handle;//PCM设备句柄pcm.h
    snd_pcm_hw_params_t *hw_params;//硬件信息和PCM流配置

    char * period_buffer;
    size_t buffer_size;
    size_t buffer_used;
    size_t played_frame;
    int sample_rate,  bit_wide,  channel;
};


long bytes2ms(long data_bytelength, int sample_rate, int bit_wide, int channel){
    return (data_bytelength * 8 * 1000) / (sample_rate * bit_wide * channel);
}

long ms2bytes(long ms, int sample_rate, int bit_wide, int channel){
    return (ms * (sample_rate * bit_wide * channel)) / 8000;
}

long bytes2frame(long data_bytelength, int bit_wide, int channel){
    return data_bytelength / (bit_wide * channel / 8);
}

XNLEXPORT xlong XI_STDCALL audio_create(xint channel, xint sample, xint widebits, xint bufferms){
    AudioOutput * ao = new AudioOutput;
    ao->sample_rate = sample;
    ao->bit_wide = widebits;
    ao->channel = channel;

    int ret = snd_pcm_open(&ao->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0){
        delete ao;
        return 0;
    }

    ret = snd_pcm_hw_params_malloc(&ao->hw_params);
    if (ret < 0){
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    ret = snd_pcm_hw_params_any(ao->playback_handle, ao->hw_params);
    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    //4. 初始化访问权限
    ret = snd_pcm_hw_params_set_access(ao->playback_handle, ao->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    //5. 初始化采样格式SND_PCM_FORMAT_U8,8位
    ret = -1;
    switch(widebits){
        case 8:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_U8);
            break;
        case 80:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_S8);
            break;
        case 16:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_S16_LE);
            break;
        case 61:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_S16_BE);
            break;
        case 32:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_S32_LE);
            break;
        case 23:
            ret = snd_pcm_hw_params_set_format(ao->playback_handle, ao->hw_params, SND_PCM_FORMAT_S32_BE);
            break;
    }

    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    unsigned int val = sample;
    int dir = 0;
    ret = snd_pcm_hw_params_set_rate_near(ao->playback_handle, ao->hw_params, &val, &dir);
    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    //7. 设置通道数量
    ret = snd_pcm_hw_params_set_channels(ao->playback_handle, ao->hw_params, channel);
    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    ao->frames = 32;//bytes2frame(ms2bytes(bufferms, sample, widebits, channel), widebits, channel) / 3;//

    ret = snd_pcm_hw_params_set_period_size_near(ao->playback_handle, ao->hw_params, &ao->frames, 0);
    if (ret < 0){
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }


    ao->periodsize = ao->frames * channel * (widebits / 8);
    ret = snd_pcm_hw_params_set_buffer_size_near(ao->playback_handle, ao->hw_params, &ao->periodsize);
    if (ret < 0)
    {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }

    //8. 设置hw_params
    ret = snd_pcm_hw_params(ao->playback_handle, ao->hw_params);
    if (ret < 0) {
        snd_pcm_close(ao->playback_handle);
        delete ao;
        return 0;
    }
    snd_pcm_hw_params_get_period_size(ao->hw_params, &ao->frames, &dir);

    ao->buffer_size =  ao->frames * channel * (widebits / 8);
    ao->period_buffer = new char[ao->buffer_size ];
    ao->buffer_used = 0;
    return (xlong)ao;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
        if (err == -EPIPE) {    /* under-run */
                err = snd_pcm_prepare(handle);
                if (err < 0){
                	return err;
                }
                return 0;
        } else if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(handle)) == -EAGAIN)
                        sleep(1);       /* wait until the suspend flag is released */
                if (err < 0) {
                        err = snd_pcm_prepare(handle);
                        if (err < 0){
                        	return err;
                        }
                }
                return 0;
        }
        return err;
}

XNLEXPORT xint XI_STDCALL audio_writeData(xlong handle, char * buffer, xint size){
    //9. 写音频数据到PCM设备
    AudioOutput * ao = (AudioOutput*)handle;
    int ret = 0;

    char * pdata = buffer;
    size_t re_size = size;



    while (re_size > 0){
        size_t a_size = ao->buffer_size - ao->buffer_used;
        if (re_size < a_size){
            a_size = re_size;
        }
        memcpy(&ao->period_buffer[ao->buffer_used], &pdata[size - re_size], a_size);
        ao->buffer_used += a_size;
        re_size -= a_size;

        if (ao->buffer_size == ao->buffer_used){
            if((ret = snd_pcm_writei(ao->playback_handle, ao->period_buffer, ao->frames)) < 0){
                if (xrun_recovery(ao->playback_handle, ret) < 0) {
                    return -1;
                }
            }else{
                size_t writed = ret * (ao->bit_wide / 8) * ao->channel;
                ao->played_frame += ao->frames;
                ao->buffer_used -= writed;
                if (ao->buffer_used > 0){
                    for (size_t i =0; i < ao->buffer_used; i ++){
                        ao->period_buffer[i] = ao->period_buffer[writed + i];
                    }
                }
            }
        }else{
           break;
        }
    }
    return 0;
}

XNLEXPORT xbool XI_STDCALL audio_play(xlong handle){
    return true;
}

XNLEXPORT xbool XI_STDCALL audio_stop(xlong handle){
    AudioOutput * ao = (AudioOutput*)handle;
    if (ao->playback_handle != 0){
        snd_pcm_close(ao->playback_handle);
        ao->playback_handle = 0;
    }
    return true;
}

XNLEXPORT xbool XI_STDCALL audio_pause(xlong handle){
    return true;
}

XNLEXPORT void XI_STDCALL audio_cleanup(xlong handle){

}

XNLEXPORT void XI_STDCALL audio_destroy(xlong handle){
    AudioOutput * ao = (AudioOutput*)handle;
    if (ao->playback_handle != 0){
        snd_pcm_close(ao->playback_handle);
    }
    if (ao->period_buffer != 0){
        delete ao->period_buffer;
    }
    delete ao;
}

XNLEXPORT xint XI_STDCALL audio_getPosition(xlong handle){
     AudioOutput * ao = (AudioOutput*)handle;
    return ao->played_frame;
}
