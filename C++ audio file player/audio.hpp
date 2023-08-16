#pragma once
#define WIN32_LEAN_AND_MEAN
#define STRICT

// Audio headers
#include <mmdeviceapi.h>
#include <audioclient.h>
// Audio MF
#include <strsafe.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <strmif.h>
#include <propidlbase.h>
#include <Propvarutil.h>
// General headers
#include <iostream>
#include <mutex>
// Media foundation

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "Propsys.lib")


#define REFTIMES_PER_SEC  100000000
#define REFTIMES_PER_MILLISEC  100000

template <class T> void SafeRelease(T** ppT);

HRESULT ComInit();
VOID CoUninit();

enum class PlayerState
{
	HASFILE,
	NOFILE,
	INITIALIZED,
	PLAYING
};
/// <summary>
/// Audio File reader using Media Foundation
/// </summary>
class Player {
public:
	Player(LPCWSTR FileName);
	~Player() {
		SafeRelease(&pSourceReader);
	}
	// Get format
	int  GetNativeFormat(IMFMediaType** mediaType);
	int  Initialize();
	int  Initialize(IMFMediaType* mediaType);
	/// <summary>
	/// Get Length
	/// </summary>
	/// <returns></returns>
	LONGLONG* GetDuration();
	LONGLONG* GetCurrentProgress();
	int SetProgress(const LONGLONG SetTime);
	/// <summary>
	/// 
	/// </summary>
	/// <param name="Buffer">Pointer to buffer</param>
	/// <param name="Length"> Length of buffer</param>
	/// <param name="DURATION">Duration of buffer in 100ns</param>
	/// <returns></returns>
	int  FetchSample(BYTE** Buffer, DWORD* Length, LONGLONG* DURATION);
	inline PlayerState* GetState() {
		Lock.lock();
		PlayerState* NewState = new PlayerState(state);
		Lock.unlock();
		return NewState;
	};
	std::mutex Lock;
private:
	IMFSourceReader* pSourceReader = NULL;
	PlayerState state = PlayerState::NOFILE;
	IMFSample* pSample = NULL;
	IMFMediaBuffer* mBuffer = NULL;
	BYTE* Localbuffer = NULL;
	DWORD streamIndex = 0;
	DWORD flags = 0;
	LONGLONG llTimeStamp = 0;
	LONGLONG Duration = 0;
	PROPVARIANT** pvarAttribute = NULL;
};

int CreateAudioPlayerFromFile(LPCWSTR FileAname, Player** player);

enum class AudioObjectState
{
	NEW,
	HASDEVICE,
	INITED,
	STARTED
};

class Renderer {
public:
	~Renderer();
	/// <summary>
	/// Get Default render device.
	/// </summary>
	/// <returns> Success is 0 or above. -1 means fatal error. -2 means retry again.</returns>
	int GetDefaultRenderDevice();
	// Use this one for simplicity
	int Initialize();
	/// <summary>
	/// Get audio format.
	/// </summary>
	/// <param name="MixFormat">Audio format for read and write. Follow this with the initialize below.</param>
	/// <returns></returns>
	int GetFormat(WAVEFORMATEX** MixFormat);
	int Initialize(WAVEFORMATEX* MixFormat);
	inline AudioObjectState* GetState() {
		Lock.lock();
		AudioObjectState* NewState = new AudioObjectState(state);
		Lock.unlock();
		return NewState;
	}
	/// <summary>
	/// This functions starts the renderer to consume data from buffer
	/// </summary>
	/// <returns>If return value is lesser than 0, it is ab error</returns>
	int Start();
	/// <summary>
	/// Push data into renderer buffer.
	/// </summary>
	/// <param name="Buffer"></param>
	/// <param name="bytes"></param>
	/// <returns></returns>
	int PushBuffer(BYTE* Buffer, int bytes);
	UINT32 GetCurrentBuffer();
private:

	unsigned long nCH = 2;	// Number of audio channels.
	unsigned long nSamplesPerSec = 44100; //Common number 44100
	unsigned long numBitsPerSample = 32; // Float32
	unsigned long BlockAlign = 8; // 32/8*2

	UINT32 BufferSizeFrame = 0;
	AudioObjectState state = AudioObjectState::NEW;
	IAudioClient2* AudioClient = NULL;
	IAudioRenderClient* audioRenderClient = NULL;

	REFERENCE_TIME requestedSoundBufferDuration = REFTIMES_PER_SEC;

	std::mutex Lock;
};
//Create renderer on a null pointer of renderer
int CreateRenderer(Renderer** renderer);
