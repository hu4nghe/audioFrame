﻿#include <iostream>
#include <format>

#include <queue>
#include <memory>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <csignal>

#include "Processing.NDI.Lib.h" 
#include "portaudio.h"

void PAErrorCheck(PaError err)
{
	if (err != paNoError)
	{
		std::cout << std::format("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
		exit(EXIT_FAILURE);
	}
}

constexpr auto SAMPLE_RATE = 44100;
typedef float SAMPLE;

static std::atomic<bool> exit_loop(false);
static void sigintHandler(int) { exit_loop = true; }

std::mutex bufferMutex;
std::mutex bufferSizeMtx;
std::condition_variable bufferCondVar;
std::condition_variable bufferSizeCondVar;

std::queue<std::unique_ptr<SAMPLE>> audioBufferQueue;
bool bufferSizeReady = false;
bool bufferSizeDefined = false;
int bufferSize = 0;

void NDIAudioTread()
{
	std::signal(SIGINT, sigintHandler);

	/**
	 * @brief Create a NDI finder and try to find a source NDI 
	 */
	const NDIlib_find_create_t NDIFindCreateDesc;
	auto pNDIFind = NDIlib_find_create_v2(&NDIFindCreateDesc);
	if (pNDIFind == nullptr)
	{
		std::cout << std::format("Error : Unable to create NDI finder.") << std::endl;
		return;
	}
	unsigned int noSources = 0;
	const NDIlib_source_t* pSources = nullptr;
	while (!exit_loop && !noSources)
	{
		NDIlib_find_wait_for_sources(pNDIFind, 1000);
		pSources = NDIlib_find_get_current_sources(pNDIFind, &noSources);
	}
	if (pSources == nullptr)
	{
		std::cout << std::format("Error : No source available!.") << std::endl;
		return;
	}

	/**
	 * @brief Create a NDI receiver if the NDI source is found.
	 */
	NDIlib_recv_create_v3_t NDIRecvCreateDesc;
	NDIRecvCreateDesc.source_to_connect_to = *pSources;
	NDIRecvCreateDesc.p_ndi_recv_name = "Audio Receiver";
	auto pNDI_recv = NDIlib_recv_create_v3(&NDIRecvCreateDesc);
	if (pNDI_recv == nullptr)
	{
		std::cout << std::format("Error : Unable to create NDI receiver.") << std::endl;
		NDIlib_find_destroy(pNDIFind);
		return;
	}
	NDIlib_find_destroy(pNDIFind);

	/**
	 * @brief Capture NDI data
	 */
	NDIlib_audio_frame_v2_t audioFrame;
	while (!exit_loop)
	{
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioFrame, nullptr, 1000);

		if (NDI_frame_type == NDIlib_frame_type_audio)
		{
			// If the portaudio do not know the buffer size, pass it to portAudio output thread.
			if (!bufferSizeDefined)
			{
				std::unique_lock<std::mutex> lock(bufferSizeMtx);
				bufferSize = audioFrame.no_samples;
				bufferSizeReady = true;
				bufferSizeDefined = true;
				lock.unlock();
				bufferSizeCondVar.notify_one();
			}
			
			//Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
			audio_frame_32bpp_interleaved.p_data = new float[audioFrame.no_samples * audioFrame.no_channels];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioFrame, &audio_frame_32bpp_interleaved);
			
			//Transfer the ownership of audio data to smart pointer and push it into the queue.
			std::unique_ptr<float> audioData(audio_frame_32bpp_interleaved.p_data);
			std::lock_guard<std::mutex> lock(bufferMutex);
			audioBufferQueue.push(std::move(audioData));
			bufferCondVar.notify_one();
		}
	}
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
}

static int audioCallback(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	auto out = static_cast<float*>(outputBuffer);

	//Wait NDI thread push audio data into the queue
	std::unique_lock<std::mutex> lock(bufferMutex);
	bufferCondVar.wait(lock, [] { return !audioBufferQueue.empty(); });

	//Get data from the queue.
	auto audioSamples = std::move(audioBufferQueue.front());
	audioBufferQueue.pop();

	//Copy data to portaudio output stream.
	std::copy(audioSamples.get(), audioSamples.get() + framesPerBuffer * 2, out);

	return paContinue;
}

void PAThread()
{
	if (!bufferSizeDefined)
	{
		std::unique_lock<std::mutex> lock(bufferSizeMtx);
		bufferSizeCondVar.wait(lock, [] { return bufferSizeReady; });
	}

	PaStream* stream;
	PAErrorCheck(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, bufferSize, audioCallback, nullptr));
	PAErrorCheck(Pa_StartStream(stream));

	std::cout << "Playing audio from NDI..." << std::endl;
	std::cin.get();

	PAErrorCheck(Pa_StopStream(stream));
	PAErrorCheck(Pa_CloseStream(stream));
}

int main()
{
	PAErrorCheck(Pa_Initialize());
	NDIlib_initialize();

	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(PAThread);

	ndiThread.join();
	portaudio.join();

	PAErrorCheck(Pa_Terminate());
	return 0;
}