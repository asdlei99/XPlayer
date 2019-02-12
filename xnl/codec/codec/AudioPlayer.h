#pragma once
#include <vector>
class AudioPlayer
{
public:
	AudioPlayer();
	~AudioPlayer();

	bool _bPlaying;
	HWAVEOUT m_hWaveOut;
	WAVEFORMATEX m_waveformat;

	SRWLOCK _lock;
	static void CALLBACK waveOutProc(
		HWAVEOUT hwo,
		UINT uMsg,
		DWORD_PTR dwInstance,
		DWORD_PTR dwParam1,
		DWORD_PTR dwParam2
		);

	void playBufferComplete(LPWAVEHDR WaveHdr);
	void lock(){
		AcquireSRWLockExclusive(&_lock);
	}
	void unlock(){
		ReleaseSRWLockExclusive(&_lock);
	}
	std::vector<LPWAVEHDR> freelist;
public:
	virtual bool create(int channel = 0, int sample = 0, int bits = 0, int millionSecBuffered = 40);
	virtual bool write(void* audioData, size_t bytesOfAudio);
	virtual bool play();
	virtual bool stop();
	virtual bool pause();
	void clean();
};

