#include <iostream>
#include <format>

#include <queue>
#include <memory>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <thread>

#include "Processing.NDI.Lib.h" 
#include "portaudio.h"

constexpr auto SAMPLE_RATE = 44100;

// System signal catch handler
static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) { exit_loop = true; }

//Global variables for multi-thread communication.
std::mutex audioDataMtx;
std::mutex bufferSizeMtx;
std::condition_variable audioDataCondVar;
std::condition_variable bufferSizeCondVar;
std::queue<std::unique_ptr<float>> audioBufferQueue;
bool bufferSizeChanged = false;
int bufferSize = 0;

void PAErrorCheck(PaError err)
{
	if (err != paNoError)
	{
		std::print("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
		exit(EXIT_FAILURE);
	}
}

void NDIAudioTread()
{
	std::signal(SIGINT, sigIntHandler);

	NDIlib_initialize();

	// Create a NDI finder and try to find a source NDI 
	const NDIlib_find_create_t NDIFindCreateDesc;
	auto pNDIFind = NDIlib_find_create_v2(&NDIFindCreateDesc);
	if (pNDIFind == nullptr)
	{
		std::print("Error : Unable to create NDI finder.\n");
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
		std::print("Error : No source available!.\n");
		return;
	}

	//Create a NDI receiver if the NDI source is found.
	NDIlib_recv_create_v3_t NDIRecvCreateDesc;
	NDIRecvCreateDesc.source_to_connect_to = *pSources;
	NDIRecvCreateDesc.p_ndi_recv_name = "Audio Receiver";
	auto pNDI_recv = NDIlib_recv_create_v3(&NDIRecvCreateDesc);
	if (pNDI_recv == nullptr)
	{
		std::print("Error : Unable to create NDI receiver.\n");
		NDIlib_find_destroy(pNDIFind);
		return;
	}
	NDIlib_find_destroy(pNDIFind);

	// NDI data capture loop

	int currentSampleNum = 0;

	NDIlib_audio_frame_v2_t audioFrame;
	while (!exit_loop)
	{
		// Capture NDI data
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioFrame, nullptr, 1000);

		if (NDI_frame_type == NDIlib_frame_type_audio)
		{
			// If the portaudio do not know the buffer size, pass it to portAudio output thread.
			//std::print("NDIAudioThread : current sample number is {} and source sample number is {}\n", currentSampleNum, audioFrame.no_samples) << std::endl;
			if (currentSampleNum != audioFrame.no_samples)
			{
				//std::print("NDIAudioThread : Buffer size change mecanism triggered in NDI thread main !\n\n");
				currentSampleNum = audioFrame.no_samples;
				std::unique_lock<std::mutex> lock(bufferSizeMtx);
				bufferSize = audioFrame.no_samples;
				bufferSizeChanged = true;
				lock.unlock();
				bufferSizeCondVar.notify_one();
			}

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
			audio_frame_32bpp_interleaved.p_data = new float[audioFrame.no_samples * audioFrame.no_channels];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioFrame, &audio_frame_32bpp_interleaved);

			// Transfer the ownership of audio data to smart pointer and push it into the queue.
			std::unique_ptr<float> audioData(audio_frame_32bpp_interleaved.p_data);

			std::lock_guard<std::mutex> lock(audioDataMtx);
			audioBufferQueue.push(std::move(audioData));
			audioDataCondVar.notify_one();
		}
	}
	// NDI clean up
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
}

static int NDIAudioCallback(const void* inputBuffer, 
						 void* outputBuffer,
						 unsigned long framesPerBuffer,
						 const PaStreamCallbackTimeInfo* timeInfo,
						 PaStreamCallbackFlags statusFlags,
						 void* userData)
{
	//std::print("portAudioThread : Buffer size is {}", framesPerBuffer) << std::endl;
	auto out = static_cast<float*>(outputBuffer);

	//Wait NDI thread push audio data into the queue
	std::unique_lock<std::mutex> lock(audioDataMtx);
	audioDataCondVar.wait(lock, [] { return !audioBufferQueue.empty(); });

	//Get data from the queue.
	auto audioSamples = std::move(audioBufferQueue.front());
	audioBufferQueue.pop();

	//Copy data to portaudio output stream.
	std::copy(audioSamples.get(), audioSamples.get() + framesPerBuffer * 2, out);

	//std::print("portAudioThread : Buffer size is {}, Callback normally called {} times.", framesPerBuffer, callbackCount) << std::endl;

	return paContinue;

}

void portAudioThread()
{
	PAErrorCheck(Pa_Initialize());

	//open a default stream at the start
	PaStream* stream;
	PAErrorCheck(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 0, nullptr, nullptr));
	std::print("Steam set !\n");

	PAErrorCheck(Pa_StartStream(stream));
	std::print("Stream started !\n");

	while (1)
	{
		if (Pa_IsStreamActive(stream))
		{
			//std::print("portAudioThread : Buffer size change mecanism triggered in PA thread main\n");
			std::unique_lock<std::mutex> lock(bufferSizeMtx);
			bufferSizeCondVar.wait(lock, [] { return bufferSizeChanged; });
			bufferSizeChanged = false;
			//std::cout << "portAudioThread : buffer change status reset to false." << std::endl;
			int newBufferSize = bufferSize;
			lock.unlock();
			PAErrorCheck(Pa_StopStream(stream));
			//std::print("portAudioThread : new buffer to setup stream is {}.",newBufferSize) << std::endl;
			PAErrorCheck(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, newBufferSize, NDIAudioCallback, nullptr));
			//std::print("portAudioThread : Pa_Stream Reset\n");
			PAErrorCheck(Pa_StartStream(stream));
			//std::print("portAudioThread : Pa_Stream Restart\n");
			//if (Pa_IsStreamActive(stream)) std::cout << "portAudioThread : restart sucess" << std::endl;
		}
	}

	PAErrorCheck(Pa_StopStream(stream));
	PAErrorCheck(Pa_CloseStream(stream));
	PAErrorCheck(Pa_Terminate());
}

int main()
{
	//nothing here, only calls the thread
	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(portAudioThread);

	ndiThread.join();
	portaudio.join();

	return 0;
}