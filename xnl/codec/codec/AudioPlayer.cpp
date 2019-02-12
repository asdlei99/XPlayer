#include "stdafx.h"
#include "AudioPlayer.h"


AudioPlayer::AudioPlayer()
{
	_bPlaying = false;
	InitializeSRWLock(&_lock);
}


AudioPlayer::~AudioPlayer()
{
}

void CALLBACK AudioPlayer::waveOutProc(
	HWAVEOUT hwo,
	UINT uMsg,
	DWORD_PTR dwInstance,
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2
	)
{
	AudioPlayer * player = (AudioPlayer*)dwInstance;
	LPWAVEHDR WaveHdr = (LPWAVEHDR)dwParam1;
	switch (uMsg)
	{
	case WOM_CLOSE:
		break;
	case WOM_OPEN:
		break;
	case WOM_DONE:
		player->playBufferComplete(WaveHdr);
		break;
	default:
		break;
	}
}

bool AudioPlayer::create(int channel, int sample, int bits,int millionSecBuffered){

	if (channel == 0){
		if (::waveOutOpen(0, 0, &m_waveformat, 0, 0, WAVE_FORMAT_QUERY)){
			return false;
		}
	}else{
		m_waveformat.wFormatTag = WAVE_FORMAT_PCM;
		m_waveformat.nChannels = channel;
		m_waveformat.nSamplesPerSec = sample;
		m_waveformat.nAvgBytesPerSec = channel * sample * bits / 8;
		m_waveformat.nBlockAlign = channel * (bits / 8);
		m_waveformat.wBitsPerSample = bits;
		m_waveformat.cbSize = 0;
	}
	MMRESULT mmReturn = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &m_waveformat, (DWORD_PTR)waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if (mmReturn != MMSYSERR_NOERROR){
		return false;
	}
	return true;
}

void AudioPlayer::clean(){
	std::vector<LPWAVEHDR> _freelist;
	lock();
	_freelist.swap(freelist);
	unlock();
	for (LPWAVEHDR lph : _freelist){
		delete []lph->lpData;
		delete lph;
	}
}

void AudioPlayer::playBufferComplete(LPWAVEHDR WaveHdr){
	lock();
	freelist.push_back(WaveHdr);
	unlock();
}


bool AudioPlayer::write(void* buffer, size_t bytesOfBuffer)
{
	clean();
	WAVEHDR * header = new WAVEHDR;
	ZeroMemory(header, sizeof(WAVEHDR));
	header->dwBufferLength = bytesOfBuffer;
	header->lpData = new char[bytesOfBuffer];
	memcpy(header->lpData, buffer, bytesOfBuffer);
	MMRESULT mmResult = waveOutPrepareHeader(m_hWaveOut, header, sizeof(WAVEHDR));
	mmResult = waveOutWrite(m_hWaveOut, header, sizeof(WAVEHDR));
	return true;
}

bool AudioPlayer::play()
{
	_bPlaying = true;
	waveOutRestart(m_hWaveOut);
	return true;
}


bool AudioPlayer::stop()
{
	if (m_hWaveOut != 0){
		waveOutReset(m_hWaveOut);
		waveOutClose(m_hWaveOut);
		m_hWaveOut = 0;
	}
	_bPlaying = false;
	clean();
	return true;
}


bool AudioPlayer::pause()
{
	if (_bPlaying){
		_bPlaying = false;
		waveOutPause(m_hWaveOut);
		return true;
	}
	return false;
}
