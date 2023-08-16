#include "audio.hpp"

template <class T> void SafeRelease(T * *ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

HRESULT ComInit() {
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (hr == S_OK) {
		std::cout << "COM library initialization success.\n";
	}
	else if (hr == S_FALSE) {
		std::cout << "COM Library already initialized.\n";
	}
	else if (hr == RPC_E_CHANGED_MODE) {
		std::cout << "Previous call specified different concurrency model.\n.";
	};
	return hr;
}

void CoUninit() {
	CoUninitialize();
}

int CreateAudioPlayerFromFile(LPCWSTR FileAname, Player** player) {
	*player = new Player(FileAname);
	//PLAYER
	if (*(*player)->GetState() != PlayerState::HASFILE) {
		delete(*player);
		*player = NULL;
		return -1;
	}

	return 1;
}
Player::Player(LPCWSTR FileName) {
	HRESULT hr = 0;
	PROPVARIANT pvarAttribute;
	Lock.lock();
	hr = MFCreateSourceReaderFromURL(FileName, NULL, &pSourceReader);
	if (FAILED(hr)) {
		std::cout << "Unable to open media file.\n";
		SafeRelease(&pSourceReader);
		Lock.unlock();
	}
	else {
		hr = pSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &pvarAttribute);
		Lock.unlock();
		if (FAILED(hr)) {
			std::cout << "Can't get presentation\n";
		}
		hr = PropVariantToInt64(pvarAttribute, &Duration);
		if (FAILED(hr)) {
			std::cout << "Can't convert prop variant\n";
		}
		std::cout << "File duration: " << Duration / 10000 << " milliseconds.\n";
		state = PlayerState::HASFILE;
	}


}

int Player::GetNativeFormat(IMFMediaType** mediaFormat) {
	HRESULT hr = 0;
	Lock.lock();
	hr = pSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mediaFormat);
	Lock.unlock();
	if (FAILED(hr)) {
		std::cout << "Get native media type fail\n";
		return -1;
	}
	return 1;
}
int Player::Initialize(IMFMediaType* mediaType) {
	HRESULT hr = 0;
	Lock.lock();
	if (state != PlayerState::HASFILE) {
		std::cout << "Wrong state\n";
		Lock.unlock();
		return -2;
	}
	hr = pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mediaType);
	Lock.unlock();
	if (FAILED(hr)) {
		std::cout << "Set native media type fail\n";
		return -1;
	}

	Lock.lock();
	SafeRelease(&mediaType);
	state = PlayerState::INITIALIZED;
	Lock.unlock();
	return 1;
}



int Player::Initialize() {
	IMFMediaType* mediaType = NULL;
	MFCreateMediaType(&mediaType);
	mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);//MFAudioFormat_Float
	mediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
	mediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
	mediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
	mediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
	mediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000);
	mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	mediaType->SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, TRUE);
	return  Player::Initialize(mediaType);
}
LONGLONG* Player::GetDuration() {
	Lock.lock();
	LONGLONG* DurationCopy = new LONGLONG(Duration);
	Lock.unlock();
	return DurationCopy;
}

LONGLONG* Player::GetCurrentProgress() {
	Lock.lock();
	LONGLONG* DurationCopy = new LONGLONG(llTimeStamp);
	Lock.unlock();
	return DurationCopy;
}

int  Player::SetProgress(const LONGLONG SetTime) {
	tagPROPVARIANT* Propvar = new tagPROPVARIANT{ 0 };
	HRESULT hr = InitPropVariantFromInt64(SetTime, Propvar);
	if (FAILED(hr)) {
		std::cout << "Convert time fail\n";
		return -1;
	}
	Lock.lock();
	hr = pSourceReader->SetCurrentPosition(GUID_NULL, *Propvar);
	Lock.unlock();
	if (FAILED(hr)) {
		std::cout << "Set position fail.\n";
		return -1;
	}
	return 0;
}
int Player::FetchSample(BYTE** Buffer, DWORD* Length, LONGLONG* DURATION) {
	DWORD MaxbufferLength = 0;
	DWORD length = 0;
	LONGLONG duration = 0;
	Lock.lock();
	SafeRelease(&pSample);
	SafeRelease(&mBuffer);

	HRESULT hr = pSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);

	if (FAILED(hr)) {
		Lock.unlock();
		if (hr == E_INVALIDARG) {
			if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
				std::cout << "End of stream\n";
				return -2;
			}
			std::cout << "Invalid Args\n";
			return -1;
		}
		else if (hr == MF_E_INVALIDREQUEST) std::cout << "Invalid request. ";
		else if (hr == MF_E_INVALIDSTREAMNUMBER) std::cout << "Invalid stream number. ";
		else if (hr == MF_E_NOTACCEPTING) std::cout << "Flush pending. ";
		std::cout << "Can't get samples\n";
		return -1;
	}

	if (SUCCEEDED(hr) && (flags == 0)) {
		if (pSample) {

			hr = pSample->GetSampleDuration(&duration);

			*DURATION = duration;
			//std::cout << "Duration: " << *DURATION << "\n";
			if (FAILED(hr)) {
				Lock.unlock();
				std::cout << " Can't get sample duration(not set).\n";
				return -1;
			}
			hr = pSample->GetBufferByIndex(0, &mBuffer);
			if (FAILED(hr)) {
				std::cout << " Can't get buffer. Out of index.\n";
				Lock.unlock();
				return -1;
			}
			hr = mBuffer->Lock(&Localbuffer, &MaxbufferLength, Length);
			if (hr == MF_E_INVALIDINDEX) {
				Lock.unlock();
				std::cout << "Can't get buffer now.\n";
				return 3;
			}
			*Buffer = new BYTE[*Length + 1];
			memcpy_s(*Buffer, static_cast<rsize_t>(*Length) + 1, Localbuffer, *Length);
			hr = mBuffer->Unlock();
			Lock.unlock();
			return 1;
		}
	}
	Lock.unlock();
	return 0;

}

int CreateRenderer(Renderer** renderer) {
	if (*renderer != NULL) {
		delete (*renderer);
	}
	*renderer = new Renderer();
	//PLAYER
	int error = (*renderer)->GetDefaultRenderDevice();
	if (error < 0) {
		delete(*renderer);
		*renderer = NULL;
		return -1;
	}
	return 1;
}
Renderer::~Renderer() {
	Lock.lock();
	SafeRelease(&AudioClient);
	state = AudioObjectState::NEW;
	Lock.unlock();
}
int Renderer::GetDefaultRenderDevice() {
	std::cout << "---------Get renderer device---------\n";
	HRESULT hr = 0;
	Lock.lock();
	state = AudioObjectState::NEW;
	SafeRelease(&AudioClient);
	Lock.unlock();

	// Enumerate deveices
	IMMDeviceEnumerator* deviceEnumerator = NULL;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)(&deviceEnumerator));
	if (FAILED(hr)) {
		std::cout << "Enumerate device fail.\n";
		return -1;
	}
	// Select the default audio render endpoint
	IMMDevice* audioDevice;
	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audioDevice);
	if (hr == E_NOTFOUND) {
		std::cout << "No available render endpoint.\n";
		return -1;
	}
	else if (FAILED(hr)) {
		std::cout << "Get render endpoint fail.\n";
		return -1;
	}
	deviceEnumerator->Release();// No longer needed.
	// Activate device.
	Lock.lock();
	hr = audioDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, nullptr, (LPVOID*)(&AudioClient));
	SafeRelease(&audioDevice);
	Lock.unlock();
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
		std::cout << "Device invalidated.\n";// Device removed. Start again!
		return -2;
	}
	else if (FAILED(hr)) {
		std::cout << "Can not activate audio device\n";
		return -1;
	}
	Lock.lock();
	state = AudioObjectState::HASDEVICE;
	Lock.unlock();
	return 1;
}
int  Renderer::Initialize() {
	std::cout << "------Initialize render device-------\n";
	Lock.lock();
	if (state != AudioObjectState::HASDEVICE) {
		Lock.unlock();
		std::cout << "Wrong state at \"Initialize\"\n";
		return -1;
	}
	Lock.unlock();
	WAVEFORMATEX* DefaultFormat = NULL;
	int error = this->GetFormat(&DefaultFormat);
	if (error < 0) {
		// Failed to get format
		return -2;
	}
	DefaultFormat->nChannels = (WORD)nCH;
	DefaultFormat->nSamplesPerSec = nSamplesPerSec;
	DefaultFormat->nBlockAlign = (WORD)numBitsPerSample / 8 * (WORD)nCH;
	DefaultFormat->wBitsPerSample = (WORD)numBitsPerSample;
	DefaultFormat->nAvgBytesPerSec = DefaultFormat->nSamplesPerSec * DefaultFormat->nBlockAlign;
	return this->Initialize(DefaultFormat);
}
int Renderer::GetFormat(WAVEFORMATEX** MixFormat) {
	HRESULT hr = 0;
	Lock.lock();
	if (AudioClient == NULL) {
		Lock.unlock();
		return -1;
	}
	if (state == AudioObjectState::HASDEVICE) {
		hr = AudioClient->GetMixFormat(MixFormat);
	}
	Lock.unlock();
	if (FAILED(hr)) {
		SafeRelease(&AudioClient);
		//if (!COMed)CoUninitialize();
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			state = AudioObjectState::NEW;
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			return -2;
		}
		else {
			std::cout << "Unable to get format.\n";
			return -1;
		}

	}
	return 1;
}
int Renderer::Initialize(WAVEFORMATEX* MixFormat) {
	Lock.lock();
	if (state != AudioObjectState::HASDEVICE) {
		Lock.unlock();
		std::cout << "Wrong state at \"Initialize\"\n";
		return -1;
	}
	Lock.unlock();
	WAVEFORMATEX* NeartestFormat = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));

	HRESULT hr = 0;

	Lock.lock();
	if (state == AudioObjectState::HASDEVICE) {
		hr = AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, MixFormat, &NeartestFormat);
	}
	Lock.unlock();

	if (hr == S_OK) {
		std::cout << "Format supported\n";
		CoTaskMemFree(NeartestFormat);
	}
	else if (hr == S_FALSE) {
		CoTaskMemFree(MixFormat);
		MixFormat = NeartestFormat;
		NeartestFormat = NULL;
	}
	else {
		Lock.lock();
		state = AudioObjectState::NEW;
		Lock.unlock();
		CoTaskMemFree(MixFormat);
		CoTaskMemFree(NeartestFormat);
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			return -2;
		}
		std::cout << "Format not supported\n";
		return -1;
	}

	Lock.lock();

	hr = AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		0,
		requestedSoundBufferDuration,
		0, MixFormat, NULL);
	Lock.unlock();

	if (hr == AUDCLNT_E_ALREADY_INITIALIZED)std::cout << "Already initialized\n";
	else if (hr == AUDCLNT_E_WRONG_ENDPOINT_TYPE) std::cout << "Wrong endpoint type\n";
	else if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)std::cout << "Buffer size not align\n";
	else if (hr == AUDCLNT_E_BUFFER_SIZE_ERROR)std::cout << "Wrong buffer size\n";
	else if (hr == AUDCLNT_E_CPUUSAGE_EXCEEDED)   std::cout << "CPU usage exceed\n";
	else if (hr == AUDCLNT_E_DEVICE_INVALIDATED)   std::cout << "Device invalidated\n";
	else if (hr == AUDCLNT_E_DEVICE_IN_USE)    std::cout << "Device in use\n";
	else if (hr == AUDCLNT_E_ENDPOINT_CREATE_FAILED)  std::cout << "Failed to create endpoint\n";
	else if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD)        std::cout << "Wrong device period\n";
	else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)         std::cout << "Unsupported format\n";
	else if (hr == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)  std::cout << "Excusive mode not allow\n";
	else if (hr == AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL)       std::cout << "hnsBufferDuration and hnsPeriodicity are not equal\n";
	else if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING) std::cout << "Windows audio service is not running\n";
	else if (hr == E_POINTER)  std::cout << "pformat is null\n";
	else if (hr == E_INVALIDARG)       std::cout << "pFormat points to an invalid format description\n";
	else if (hr == E_OUTOFMEMORY) std::cout << "out of memory\n";
	if (FAILED(hr)) {
		CoTaskMemFree(MixFormat);
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			//std::cout << "Device invalidated.\n"; //Device removed.Start again!
			return -2;
		}
		std::cout << "Failed to initialize client\n";
		return -1;
	}

	nCH = MixFormat->nChannels;
	nSamplesPerSec = MixFormat->nSamplesPerSec;
	numBitsPerSample = MixFormat->wBitsPerSample;
	Lock.lock();
	hr = AudioClient->GetService(__uuidof(IAudioRenderClient), (LPVOID*)(&audioRenderClient));
	Lock.unlock();
	if (FAILED(hr)) {
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			return -2;
		}
		std::cout << "Unable to get service\n";
		return -1;
	}
	//if (!COMed)CoUninitialize();
	Lock.lock();
	hr = AudioClient->GetBufferSize(&BufferSizeFrame);
	if (FAILED(hr)) {
		std::cout << "Can't get buffer size\n";
		Lock.unlock();
		return -1;
	}
	state = AudioObjectState::INITED;
	Lock.unlock();
	return 1;
}
int Renderer::Start() {
	Lock.lock();
	if (state != AudioObjectState::INITED) {
		Lock.unlock();
		std::cout << "Incorrect State at \"play\"\n";
		return -1;
	}
	HRESULT hr = AudioClient->Start();
	Lock.unlock();
	if (FAILED(hr))
		switch (hr)
		{
		case AUDCLNT_E_NOT_INITIALIZED:
		{
			std::cout << "Audio client has not been initialized.\n";
			Lock.lock();
			state = AudioObjectState::HASDEVICE;
			Lock.unlock();
			return -3;
		}
		case AUDCLNT_E_NOT_STOPPED:
		{
			std::cout << "Started called multiple times. \n";
			Lock.lock();
			state = AudioObjectState::STARTED;
			Lock.unlock();
			return -4;
		}
		case AUDCLNT_E_DEVICE_INVALIDATED:
		{
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			Lock.lock();
			state = AudioObjectState::NEW;
			Lock.unlock();
			return -2;
		}
		case AUDCLNT_E_EVENTHANDLE_NOT_SET: {
			std::cout << "IAudioClient Event handle not set.\n";
			[[fallthrough]];
		}
		case AUDCLNT_E_SERVICE_NOT_RUNNING: {
			std::cout << "Windows audio service not running.\n";
			[[fallthrough]];
		}
		default: {
			return -1;
		}
		}
	Lock.lock();
	state = AudioObjectState::STARTED;
	Lock.unlock();
	return 1;
}
int Renderer::PushBuffer(BYTE* Buffers, int bytes) {
	Lock.lock();
	if (state != AudioObjectState::STARTED) {
		Lock.unlock();
		std::cout << "Incorrect state at \"PushBuffer\"\n";
		return -1;
	}
	else if (bytes % BlockAlign != 0 && BlockAlign >= 4 && bytes > 0) {
		Lock.unlock();
		std::cout << "Input buffer should be a multiple of block align which is " << BlockAlign << ".\n";
		return -1;
	}

	HRESULT hr = 0;
	UINT32 bufferPadding = 0;
	hr = AudioClient->GetCurrentPadding(&bufferPadding);
	if (FAILED(hr)) {
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			Lock.unlock();
			return -2;
		}
		Lock.unlock();
		std::cout << "Get padding failed\n";
		return -1;
	}
	BYTE* Buffer = NULL;
	UINT32 FramesToWrite = BufferSizeFrame - bufferPadding;
	if (FramesToWrite > bytes / BlockAlign)FramesToWrite = bytes / BlockAlign;
	hr = audioRenderClient->GetBuffer(FramesToWrite, (BYTE**)(&Buffer));
	if (FAILED(hr)) {
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			state = AudioObjectState::NEW;
			Lock.unlock();
			return -2;
		}
		switch (hr)
		{
		case AUDCLNT_E_BUFFER_ERROR: {
			std::cout << "Failed to retreive buffer.\n";
			Lock.unlock();
			return -1;
		}
		case AUDCLNT_E_BUFFER_TOO_LARGE: {
			std::cout << "The NumFramesRequested value exceeds the available buffer space\n";
			Lock.unlock();
			return -1;
		}
		case AUDCLNT_E_BUFFER_SIZE_ERROR: {
			std::cout << "Attempted to get a packet that was not the size of the buffer\n";
			Lock.unlock();
			return -1;
		}
		case AUDCLNT_E_OUT_OF_ORDER: {
			std::cout << "Get and release out of order\n";
			Lock.unlock();
			return -1;
		}
		case AUDCLNT_E_BUFFER_OPERATION_PENDING: {
			std::cout << "Buffer cannot be accessed because a stream reset is in progress.\n";
			Lock.unlock();
			return -1;
		}
		case AUDCLNT_E_SERVICE_NOT_RUNNING: {
			std::cout << "The Windows audio service is not running.\n";
			Lock.unlock();
			return -1;
		}
		default: {
			Lock.unlock();
			return -1;
		}
		}
	}
	// cast to smaller type
	memcpy(Buffer, Buffers, (size_t)FramesToWrite * BlockAlign);
	DWORD Flags = 0;
	hr = audioRenderClient->ReleaseBuffer(FramesToWrite, Flags);
	if (FAILED(hr)) {
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			std::cout << "Device invalidated.\n"; //Device removed.Start again!
			state = AudioObjectState::NEW;
			Lock.unlock();
			return -2;
		}
		Lock.unlock();
		switch (hr)
		{
		case AUDCLNT_E_INVALID_SIZE: {
			std::cout << "The NumFramesWritten value exceeds the NumFramesRequested value specified.\n";
			[[fallthrough]];
		}
		case AUDCLNT_E_BUFFER_SIZE_ERROR: {
			std::cout << "client attempted to release a packet that was not the size of the buffer\n";
			[[fallthrough]];
		}
		case AUDCLNT_E_OUT_OF_ORDER: {
			std::cout << "Get and release out of order\n";
			[[fallthrough]];
		}
		case AUDCLNT_E_SERVICE_NOT_RUNNING: {
			std::cout << "The Windows audio service is not running\n";
			[[fallthrough]];
		}
		case E_INVALIDARG: {
			std::cout << "Parameter dwFlags is not a valid value..\n";
			[[fallthrough]];
		}
		default: {
			return -1;
		}
		}
	}
	Lock.unlock();
	return 1;
}

UINT32 Renderer::GetCurrentBuffer() {
	UINT32 CurrentBuffer = 0;
	Lock.lock();
	AudioClient->GetCurrentPadding(&CurrentBuffer);
	Lock.unlock();
	return CurrentBuffer;
}
