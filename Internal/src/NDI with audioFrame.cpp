#include <csignal>
#include "Processing.NDI.Lib.h" 
#include "portaudio.h"
#include "audioFrame.h"
#include <print>

/**
 * @brief A system signal handler allows to quit with Crtl + C
 * 
 * Let exit_loop = true if a system signal is received.
 */
#pragma region Signal handler
static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) {exit_loop = true;}
#pragma endregion

/**
 * @brief Global variables definition
 * 
 * Constants and data queue.
 */
#pragma region Global definition
constexpr auto SAMPLE_RATE					= 48000;
constexpr auto PA_BUFFER_SIZE				= 128;
constexpr auto NDI_TIMEOUT					= 1000;
constexpr auto QUEUE_SIZE_MULTIPLIER		= 2;
audioQueue<float> NDIdata(0);
audioQueue<float> MicroInput(0);
#pragma endregion

/**
 * @brief Error checker for NDI and PortAudio library.
 * 
 * Exit with failure in case of error.
 */
#pragma region Error Handlers
template <typename T>
inline T*	NDIErrorCheck (T*	   ptr){if (!ptr){ std::print("NDI Error: No source is found.\n"); exit(EXIT_FAILURE); } else{ return ptr; }}
inline void  PAErrorCheck (PaError err){if ( err){ std::print("PortAudio error : {}.\n", Pa_GetErrorText(err)); exit(EXIT_FAILURE);}}
#pragma endregion

#pragma region NDI IO
void NDIAudioTread()
{
	std::signal(SIGINT, sigIntHandler);

	#pragma region NDI Initialization
	// Create a NDI finder and try to find a source NDI 
	const NDIlib_find_create_t NDIFindCreateDesc;
	auto pNDIFind = NDIErrorCheck(NDIlib_find_create_v2(&NDIFindCreateDesc));
	uint32_t noSources = 0;
	const NDIlib_source_t* pSources = nullptr;
	while (!exit_loop && !noSources)
	{
		NDIlib_find_wait_for_sources(pNDIFind, NDI_TIMEOUT);
		pSources = NDIlib_find_get_current_sources(pNDIFind, &noSources);
	}
	NDIErrorCheck(pSources);
	//Create a NDI receiver if the NDI source is found.
	NDIlib_recv_create_v3_t NDIRecvCreateDesc;
	if(pSources)NDIRecvCreateDesc.source_to_connect_to	= *pSources;
	NDIRecvCreateDesc.p_ndi_recv_name					= "Audio Receiver";

	auto pNDI_recv = NDIErrorCheck(NDIlib_recv_create_v3(&NDIRecvCreateDesc));
	NDIlib_find_destroy(pNDIFind);
	#pragma endregion
	
	#pragma region NDI Data capture
	NDIlib_audio_frame_v2_t audioInput;
	 
	while (!exit_loop)
	{
		// Capture NDI datae
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioInput, nullptr, NDI_TIMEOUT);
		if(NDI_frame_type == NDIlib_frame_type_audio)
		{
			const std::size_t dataSize = audioInput.no_samples * audioInput.no_channels;

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audioDataNDI;
			audioDataNDI.p_data = new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioDataNDI);
			NDIlib_recv_free_audio_v2(pNDI_recv, &audioInput);

			if(audioDataNDI.no_channels != NDIdata.channels  ()) NDIdata.setChannelNum(audioDataNDI.no_channels);
			if(audioDataNDI.sample_rate != NDIdata.sampleRate()) NDIdata.setSampleRate(audioDataNDI.sample_rate);
			NDIdata.setCapacity (static_cast<std::size_t>(dataSize * QUEUE_SIZE_MULTIPLIER));

			NDIdata.push(std::move(audioDataNDI.p_data), audioDataNDI.no_samples,2,SAMPLE_RATE);

			delete[] audioDataNDI.p_data;
		}
		
	}
	#pragma endregion
	
	#pragma region NDI Clean up
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
	#pragma endregion
}

static int portAudioOutputCallback(const void*					   inputBuffer, 
										 void*					   outputBuffer,
										 unsigned long             framesPerBuffer,
								   const PaStreamCallbackTimeInfo* timeInfo,
										 PaStreamCallbackFlags	   statusFlags,
										 void*					   UserData)
{
	auto out = static_cast<float*>(outputBuffer);
	memset(out, 0, framesPerBuffer * 2);
	//auto in = static_cast<const float*>(inputBuffer);
	//MicroInput.setCapacity(8192);
	//icroInput.setChannelNum(2);
	//MicroInput.push(in, framesPerBuffer);
	NDIdata.pop(out, framesPerBuffer, false);
	//MicroInput.pop(out, framesPerBuffer,true);
	return paContinue;
}

void portAudioOutputThread()
{
	std::signal(SIGINT, sigIntHandler);

#pragma region PA Initialization

	PaStream* streamOut;
	PAErrorCheck(Pa_OpenDefaultStream	(&streamOut,					// PaStream ptr
										 0,								// Input  channels
										 2,								// Output channels
										 paFloat32,						// Sample format
									     SAMPLE_RATE,					// 44100
										 PA_BUFFER_SIZE,				// 128
										 portAudioOutputCallback,		// Callback function called
										 nullptr));						// No user NDIdata passed
	PAErrorCheck(Pa_StartStream			(streamOut));
#pragma endregion

#pragma region PA Callback playing loop

	std::print("playing...\n");
	while (!exit_loop)
	{
		if (!NDIdata.size()) Pa_AbortStream(streamOut);
		if (NDIdata.size() && Pa_IsStreamStopped(streamOut)) Pa_StartStream(streamOut);
	}
#pragma endregion

#pragma region PA Clean up

	PAErrorCheck(Pa_StopStream	(streamOut));
	PAErrorCheck(Pa_CloseStream	(streamOut));
	
#pragma endregion
}
#pragma endregion

int main()
{
	NDIlib_initialize();
	PAErrorCheck(Pa_Initialize());
	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(portAudioOutputThread);


	ndiThread.detach();
	portaudio.join();
	PAErrorCheck(Pa_Terminate());
	return 0;
}
/*
#pragma region Microphone In
static int portAudioInputCallback(const void* inputBuffer,
	void* outputBuffer,
	unsigned long				framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags		statusFlags,
	void* UserData)
{
	auto in = static_cast<const float*>(inputBuffer);
	MicroInput.setCapacity(8192);
	MicroInput.setChannelNum(2);
	MicroInput.push(in, framesPerBuffer);
	return paContinue;
}
void portAudioInputThread()
{
	PaStream* streamIn;
	PAErrorCheck(Pa_OpenDefaultStream(&streamIn,						// PaStream ptr
		2,								// Input  channels
		0,								// Output channels
		paFloat32,						// Sample format
		44100,					// 44100
		28,					// 128
		portAudioInputCallback,			// Callback function called
		nullptr));						// No user NDIdata passed
	PAErrorCheck(Pa_StartStream(streamIn));
	while (!exit_loop) {}
	PAErrorCheck(Pa_StopStream(streamIn));
	PAErrorCheck(Pa_CloseStream(streamIn));
}
#pragma endregion*/