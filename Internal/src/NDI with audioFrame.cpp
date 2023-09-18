#include <csignal>
#include <iostream>
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
constexpr auto SAMPLE_RATE					= 44100;
constexpr auto PA_BUFFER_SIZE				= 128;
constexpr auto NDI_TIMEOUT					= 1000;
constexpr auto QUEUE_SIZE_MULTIPLIER		= 200;
audioQueue<float> NDIdata(0);
audioQueue<float> SNDdata(0);
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
	NDIlib_initialize();
	std::signal(SIGINT, sigIntHandler);

	#pragma region NDI Initialization
	// Create a NDI finder and try to find a source NDI 
	const NDIlib_find_create_t NDIFindCreateDesc;
	auto pNDIFind = NDIErrorCheck(NDIlib_find_create_v2(&NDIFindCreateDesc)); 
	uint32_t numSources = 0;
	const NDIlib_source_t* pSources = nullptr;
	while (!numSources)
	{
		NDIlib_find_wait_for_sources(pNDIFind, NDI_TIMEOUT);
		pSources = NDIlib_find_get_current_sources(pNDIFind, &numSources);
	}
	NDIErrorCheck(pSources);
	
	for (int i = 0; i < numSources; i++)
		std::print("Source {}\nName : {}\nIP   : {}\n\n", i , pSources[i].p_ndi_name, pSources[i].p_url_address);

	std::print("Please enter the URL of the source that you want to connect to.\n");

	NDIlib_recv_create_v3_t NDIRecvCreateDesc;
	std::string url;
	bool found = false;
	do 
	{
		std::cin >> url;
		for (int i = 0; i < numSources; i++) 
		{
			if (url == pSources[i].p_url_address) 
			{
				NDIRecvCreateDesc.source_to_connect_to = pSources[i];
				std::print("You are now connecting to {},IP :{}\n", pSources[i].p_ndi_name,pSources[i].p_url_address);
				found = true;
				break;
			}
		}
		if (!found) std::print("No source matched! Please try again.\n");
	} while (!found);
	
	
	std::string name=NDIRecvCreateDesc.source_to_connect_to.p_ndi_name;
	name += " Receiver";
	NDIRecvCreateDesc.p_ndi_recv_name = name.c_str();

	auto pNDI_recv = NDIErrorCheck(NDIlib_recv_create_v3(&NDIRecvCreateDesc));
	std::print("playing...\n");
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
			const auto dataSize = static_cast<size_t>(audioInput.no_samples) * audioInput.no_channels;

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audioDataNDI;
			audioDataNDI.p_data	= new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioDataNDI);
			NDIlib_recv_free_audio_v2(pNDI_recv, &audioInput);

			if(audioDataNDI.no_channels != NDIdata.channels  ()) NDIdata.setChannelNum(audioDataNDI.no_channels);
			if(audioDataNDI.sample_rate != NDIdata.sampleRate()) NDIdata.setSampleRate(audioDataNDI.sample_rate);
			NDIdata.setCapacity (static_cast<std::size_t>(dataSize * QUEUE_SIZE_MULTIPLIER));

			NDIdata.push(std::move(audioDataNDI.p_data), audioDataNDI.no_samples,2,SAMPLE_RATE);
			//NDIlib_audio_frame_interleaved_32f_t.p_data's ownership is moved to audioQueue, no need to delete[] explicitly.
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
	NDIdata.pop(out, framesPerBuffer, false);
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
									     SAMPLE_RATE,					// Sample rate
										 PA_BUFFER_SIZE,				// 128
										 portAudioOutputCallback,		// Callback function called
										 nullptr));						// No user NDIdata passed
	PAErrorCheck(Pa_StartStream			(streamOut));
#pragma endregion

#pragma region PA Callback playing loop

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

void sndfileRead()
{
	SndfileHandle sndFile("C:/Users/Modulo/Desktop/Nouveau dossier/Music/Rachmaninov- Music For 2 Pianos, Vladimir Ashekenazy & André Previn/Rachmaninov- Music For 2 Pianos [Disc 1]/Rachmaninov- Suite #1 For 2 Pianos, Op. 5, 'Fantaisie-Tableaux' - 1. Bacarolle- Allegretto.wav");
	
	const size_t bufferSize = 2048 * sndFile.channels();
	SNDdata.setCapacity(bufferSize);
	SNDdata.setChannelNum(sndFile.channels());
	SNDdata.setSampleRate(sndFile.samplerate());

	while (!exit_loop)
	{
		float* temp = new float[bufferSize];
		sndFile.read(temp, bufferSize);
		for (int i = 0; i < bufferSize; i++)
		{
			std::print("{}\n", temp[i]);
		}
		//SNDdata.push(std::move(temp), sndFile.frames(), 2,44100);
	}
	return;
}
int main()
{
	
	PAErrorCheck(Pa_Initialize());
	std::thread ndiThread(NDIAudioTread);
	std::thread sndfile(sndfileRead);
	std::thread portaudio(portAudioOutputThread);
	


	ndiThread.detach();
	sndfile.detach();
	portaudio.join();
	PAErrorCheck(Pa_Terminate());
	return 0;
}