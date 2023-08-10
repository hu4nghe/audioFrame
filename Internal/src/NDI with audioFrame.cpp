#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <thread>
#include <queue>

#include "Processing.NDI.Lib.h" 
#include "audioFrame.h"

constexpr auto SAMPLE_RATE = 44100;

// System signal catch handler
static std::atomic<bool> exit_loop(false);
static void sigintHandler(int) { exit_loop = true; }

//Global variables for multi-thread communication.
std::mutex audioDataMtx;
std::mutex bufferSizeMtx;
std::condition_variable audioDataCondVar;
std::condition_variable bufferSizeCondVar;
std::queue<audioFrame<float>> audioBufferQueue;
bool bufferSizeChanged = false;
int bufferSize = 0;

void PAErrorCheck(PaError err)
{
	if (err != paNoError)
	{
		std::cout << std::format("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
		exit(EXIT_FAILURE);
	}
}

void NDIAudioTread()
{
	std::signal(SIGINT, sigintHandler);

	// Create a NDI finder and try to find a source NDI 
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

	//Create a NDI receiver if the NDI source is found.
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

	// NDI data capture loop

	int currentSampleNum = 0;

	NDIlib_audio_frame_v2_t audioInput;
	while (!exit_loop)
	{
		// Capture NDI data
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioInput, nullptr, 1000);

		if (NDI_frame_type == NDIlib_frame_type_audio)
		{
			// If the portaudio do not know the buffer size, pass it to portAudio output thread.
			//std::cout << std::format("NDIAudioThread : current sample number is {} and source sample number is {}\n", currentSampleNum, audioFrame.no_samples) << std::endl;
			if (currentSampleNum != audioInput.no_samples)
			{
				//std::cout << std::format("NDIAudioThread : Buffer size change mecanism triggered in NDI thread main !\n") << std::endl;
				currentSampleNum = audioInput.no_samples;
				std::unique_lock<std::mutex> lock(bufferSizeMtx);
				bufferSize = audioInput.no_samples;
				bufferSizeChanged = true;
				lock.unlock();
				bufferSizeCondVar.notify_one();
			}

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
			audio_frame_32bpp_interleaved.p_data = new float[audioInput.no_samples * audioInput.no_channels];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audio_frame_32bpp_interleaved);

			// Transfer the ownership of audio data to smart pointer and push it into the queue.
			audioFrame audioData(audioInput.sample_rate, audioInput.no_channels, audio_frame_32bpp_interleaved.p_data, audioInput.no_samples * audioInput.no_channels);
			std::lock_guard<std::mutex> lock(audioDataMtx);
			audioBufferQueue.push(std::move(audioData));
			audioDataCondVar.notify_one();
		}
	}
	// NDI clean up
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
}

static int audioCallback(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	//std::cout << std::format("PAThread : Buffer size is {}", framesPerBuffer) << std::endl;
	auto out = static_cast<float*>(outputBuffer);

	//Wait NDI thread push audio data into the queue
	std::unique_lock<std::mutex> lock(audioDataMtx);
	audioDataCondVar.wait(lock, [] { return !audioBufferQueue.empty(); });

	//Get data from the queue.
	auto audioSamples = std::move(audioBufferQueue.front());
	audioBufferQueue.pop();

	//Copy data to portaudio output stream.
	audioSamples.diffuse(out, framesPerBuffer);

	//std::cout << std::format("PAThread : Buffer size is {}, Callback normally called {} times.", framesPerBuffer, callbackCount) << std::endl;

	return paContinue;

}

void PAThread()
{
	//open a default stream at the start
	PaStream* stream;
	PAErrorCheck(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 0, nullptr, nullptr));
	std::cout << std::format("Steam set !") << std::endl;

	PAErrorCheck(Pa_StartStream(stream));
	std::cout << std::format("Stream started !") << std::endl;

	while (1)
	{
		if (Pa_IsStreamActive(stream))
		{
			//std::cout << std::format("PAThread : Buffer size change mecanism triggered in PA thread main") << std::endl;
			std::unique_lock<std::mutex> lock(bufferSizeMtx);
			bufferSizeCondVar.wait(lock, [] { return bufferSizeChanged; });
			bufferSizeChanged = false;
			//std::cout << "PAThread : buffer change status reset to false." << std::endl;
			int newBufferSize = bufferSize;
			lock.unlock();
			PAErrorCheck(Pa_StopStream(stream));
			//std::cout << std::format("PAThread : new buffer to setup stream is {}.",newBufferSize) << std::endl;
			PAErrorCheck(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, newBufferSize, audioCallback, nullptr));
			//std::cout << std::format("PAThread : Pa_Stream Reset") << std::endl;
			PAErrorCheck(Pa_StartStream(stream));
			//std::cout << std::format("PAThread : Pa_Stream Restart") << std::endl;
			//if (Pa_IsStreamActive(stream)) std::cout << "PAThread : restart sucess" << std::endl;
		}
	}
	PAErrorCheck(Pa_StopStream(stream));
	PAErrorCheck(Pa_CloseStream(stream));
}

int main()
{
	PAErrorCheck(Pa_Initialize());
	NDIlib_initialize();

	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(PAThread);

	ndiThread.detach();
	portaudio.join();

	PAErrorCheck(Pa_Terminate());
	return 0;
}