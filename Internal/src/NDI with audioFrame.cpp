#include <thread>
#include <csignal>

#include "Processing.NDI.Lib.h" 
#include "portaudio.h"
#include "audioFrame.h"

// System signal catch handler
static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) { exit_loop = true; }

constexpr auto SAMPLE_RATE = 44100;
constexpr auto PA_BUFFER_SIZE = 128;
constexpr auto QueueCapacity = 15000;
audioQueue<float> data(QueueCapacity);

inline void PAErrorCheck(PaError err){if (err != paNoError){ std::print("PortAudio error : {}.\n", Pa_GetErrorText(err)); exit(EXIT_FAILURE);}}

void NDIAudioTread()
{
	std::signal(SIGINT, sigIntHandler);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*Preparation phase*/

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

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*NDI data capture loop*/

	NDIlib_audio_frame_v2_t audioInput;
	while (!exit_loop)
	{
		// Capture NDI data
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioInput, nullptr, 1000);
		if (NDI_frame_type == NDIlib_frame_type_audio)
		{
			const size_t dataSize = audioInput.no_samples * audioInput.no_channels;

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audioDataNDI;
			audioDataNDI.p_data = new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioDataNDI);

			data.setChannelNum(audioInput.no_channels);
			data.setSampleRate(audioInput.sample_rate);
			data.push(audioDataNDI.p_data, audioInput.no_samples);

			delete[] audioDataNDI.p_data;

			std::print("							NDI : {} sample pushed,{} sample in the queue.\n", audioInput.no_samples, data.size());
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*NDI Clean up*/

	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
}

static int portAudioOutputCallback( const void* inputBuffer, 
									void* outputBuffer,
									unsigned long framesPerBuffer,
									const PaStreamCallbackTimeInfo* timeInfo,
									PaStreamCallbackFlags statusFlags,
									void* userData)
{
	auto out = static_cast<float*>(outputBuffer);
	data.pop(out, framesPerBuffer);
	std::print("PA out :{} sample poped, {} sample in the queue.\n", framesPerBuffer, data.size());
	return paContinue;
}

void portAudioOutputThread()
{
	std::signal(SIGINT, sigIntHandler);

	PAErrorCheck(Pa_Initialize());
	PaStream* streamOut;
	PAErrorCheck(Pa_OpenDefaultStream(&streamOut, 2, 2, paFloat32, SAMPLE_RATE, PA_BUFFER_SIZE, portAudioOutputCallback, nullptr));
	PAErrorCheck(Pa_StartStream(streamOut));
	std::print("playing...\n");
	while (!exit_loop){}
	PAErrorCheck(Pa_StopStream(streamOut));
	PAErrorCheck(Pa_CloseStream(streamOut));
	PAErrorCheck(Pa_Terminate());
}

int main()
{
	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(portAudioOutputThread);

	ndiThread.detach();
	portaudio.join();
	                 
	return 0;
}