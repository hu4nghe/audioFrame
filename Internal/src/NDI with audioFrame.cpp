#include <csignal>

#include "Processing.NDI.Lib.h" 
#include "portaudio.h"
#include "audioFrame.h"
#include <conio.h>

// System signal catch handler
static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) {exit_loop = true;}

constexpr auto SAMPLE_RATE					= 44100;
constexpr auto PA_BUFFER_SIZE				= 128;
constexpr auto NDI_TIMEOUT					= 1000;
constexpr auto QUEUE_SIZE_MULTIPLIER		= 1.3;

audioQueue<float> data(0);

template <typename T>
inline T*	NDIErrorCheck (T*	   ptr){if (!ptr){ std::print("NDI Error: No source is found.\n"); exit(EXIT_FAILURE); } else{ return ptr; }}
inline void  PAErrorCheck (PaError err){if ( err){ std::print("PortAudio error : {}.\n", Pa_GetErrorText(err)); exit(EXIT_FAILURE);}}

void NDIAudioTread()
{
	std::signal(SIGINT, sigIntHandler);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*Preparation phase*/

	NDIlib_initialize();
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

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*NDI data capture loop*/

	NDIlib_audio_frame_v2_t audioInput;
	 
	while (!exit_loop)
	{
		// Capture NDI datae
		auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioInput, nullptr, NDI_TIMEOUT);
		if (NDI_frame_type == NDIlib_frame_type_audio)
		{
			const size_t dataSize = audioInput.no_samples * audioInput.no_channels;

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audioDataNDI;
			audioDataNDI.p_data = new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioDataNDI);
			
			if(audioInput.no_channels != data.channels  ())	data.setChannelNum	(audioInput.no_channels);
			if(audioInput.sample_rate != data.sampleRate()) data.setSampleRate	(audioInput.sample_rate);
			data.setCapacity (static_cast<size_t>(dataSize * QUEUE_SIZE_MULTIPLIER));

			data.push(audioDataNDI.p_data, audioInput.no_samples);

			delete[] audioDataNDI.p_data;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*NDI Clean up*/

	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
}

static int portAudioOutputCallback(const void* inputBuffer, 
								   void* outputBuffer,
								   unsigned long framesPerBuffer,
								   const PaStreamCallbackTimeInfo* timeInfo,
								   PaStreamCallbackFlags statusFlags,
								   void* userData)
{
	auto in = static_cast<const float*>(inputBuffer);
	auto out = static_cast<float*>(outputBuffer);

	data.pop(out, framesPerBuffer);
	
	for (auto i = 0; i < framesPerBuffer * 2; i++)
	{
		out[i] += in[i]*3;
	}
	return paContinue;
}

void portAudioOutputThread()
{
	std::signal(SIGINT, sigIntHandler);

	PaStream* streamOut;

	PAErrorCheck(Pa_Initialize			());
	PAErrorCheck(Pa_OpenDefaultStream	(&streamOut,					// PaStream ptr
										 2,								// Input  channels
										 2,								// Output channels
										 paFloat32,						// Sample format
									     SAMPLE_RATE,					// 44100
										 PA_BUFFER_SIZE,				// 128
										 portAudioOutputCallback,		// Callback function called
										 nullptr));						// No user data passed

	PAErrorCheck(Pa_StartStream			(streamOut));

	std::print("playing...\n");
	while (!exit_loop)
	{
		if (!data.size()) Pa_AbortStream(streamOut);
		if (data.size() && Pa_IsStreamStopped(streamOut)) Pa_StartStream(streamOut);
	}

	PAErrorCheck(Pa_StopStream	(streamOut));
	PAErrorCheck(Pa_CloseStream	(streamOut));
	PAErrorCheck(Pa_Terminate	());
}

int main()
{
	std::thread ndiThread(NDIAudioTread);
	std::thread portaudio(portAudioOutputThread);

	ndiThread.detach();
	portaudio.join();
	                 
	return 0;
}