#include "audio.hpp"
#include <Windows.h>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <cmath>

int main() {
	HRESULT hr = ComInit();
	// initialise winsock
	if (hr != S_OK) {
		std::cout << "Failed to init com interface.\n";
		return -1;
	}

	hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	if (hr != S_OK) {
		std::cout << "Failed to init media foundation.\n";
		return -1;
	}

	Renderer renderer;
	WAVEFORMATEX* rendererFormat = NULL;

	int error = renderer.GetDefaultRenderDevice();

	if (error < 0) {
		CoUninit();
		return -1;
	}
	std::cout << "Got default\n";
	error = renderer.GetFormat(&rendererFormat);
	if (rendererFormat == NULL) {
		std::cout << "No format\n";
	}
	if (error < 0 || rendererFormat == NULL) {
		CoUninit();
		return -1;
	}
	std::cout << "Got Format\n";
	rendererFormat->nSamplesPerSec = 44100;
	rendererFormat->nBlockAlign = 8;
	rendererFormat->nAvgBytesPerSec = 44100 * 8;
	rendererFormat->wBitsPerSample = 32;

	error = renderer.Initialize(rendererFormat);
	if (error < 0) {
		CoUninit();
		return -1;
	}
	else std::cout << "Device initialized\n";

	Player* player = NULL;
	std::string Filename;
	error = -1;
	while (error < 0) {
		std::cout << "Please input file name:\n";
		std::cin >> Filename;
		std::wstring stemp = std::wstring(Filename.begin(), Filename.end());
		LPCWSTR sw = stemp.c_str();
		error = CreateAudioPlayerFromFile(sw, &player);
	}

	error = player->Initialize();
	if (error < 0) {
		std::cout << "Initialize fail\n";
		MFShutdown();
		CoUninit();

		return -1;
	}
	std::cout << "Player Started\n";
	error = renderer.Start();
	if (error < 0) {
		CoUninit();
		return -1;
	}
	else {
		std::cout << "Start Play\n";
	}

	BYTE* buffer = NULL;
	DWORD Length = 0;
	LONGLONG DURATION = 0;

	while (error > 0) {
		// Fetch Sample
		error = player->FetchSample(&buffer, &Length, &DURATION);
		if (error < 0) {
			std::cout << "fetch fail\n";
			MFShutdown();
			CoUninit();
			return -1;
		}
		else if (error == 1 && buffer != NULL) {
			// Push Buffer
			renderer.PushBuffer((BYTE*)(buffer), Length);
		}
		delete(buffer);
		buffer = NULL;
		UINT32 BUFF = renderer.GetCurrentBuffer();
		Sleep(BUFF / 441000 * 1000 / 2);
	}

	hr = MFShutdown();
	std::cout << "Program end\n";
	return 0;
}

