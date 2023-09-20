#include <csignal>
#include <iostream>
#include "Processing.NDI.Lib.h" 
#include "portaudio.h"
#include "audioFrame.h"
#include <print>

#pragma region System signal handler
/**
 * @brief A system signal handler allows to quit with Crtl + C
 * 
 * Let exit_loop = true if a system signal is received.
 */
static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) {exit_loop = true;}
#pragma endregion

#pragma region Global constants and variables
constexpr auto PA_SAMPLE_RATE				= 48000;
constexpr auto PA_BUFFER_SIZE				= 512;
constexpr auto PA_INPUT_CHANNELS			= 2;
constexpr auto PA_OUTPUT_CHANNELS			= 2;
constexpr auto PA_FORMAT					= paFloat32;
constexpr auto NDI_TIMEOUT					= 1000;
constexpr auto QUEUE_SIZE_MULTIPLIER		= PA_SAMPLE_RATE / 19200;

audioQueue<float> NDIdata(PA_SAMPLE_RATE, PA_INPUT_CHANNELS, 0);
audioQueue<float> SNDdata(PA_SAMPLE_RATE, PA_INPUT_CHANNELS, 0);
audioQueue<float> MICdata(PA_SAMPLE_RATE, PA_INPUT_CHANNELS, 0);
#pragma endregion

#pragma region Error Handlers
/**
 * @brief Error checker for NDI and PortAudio library.
 * 
 * Exit with failure in case of error.
 */
template <typename T>
inline T*	NDIErrorCheck (T*	   ptr){if (!ptr){ std::print("NDI Error: No source is found.\n"); exit(EXIT_FAILURE); } else{ return ptr; }}
inline void  PAErrorCheck (PaError err){if ( err){ std::print("PortAudio error : {}.\n", Pa_GetErrorText(err)); exit(EXIT_FAILURE);}}
#pragma endregion

#pragma region NDI Inout
void NDIAudioTread()
{
	NDIlib_initialize();
	std::signal(SIGINT, sigIntHandler);

	#pragma region NDI source search
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
	std::print("NDI sources list:\n");
	for (std::size_t i = 0; i < numSources; i++)
		std::print("Source {}\nName : {}\nIP   : {}\n\n", i , pSources[i].p_ndi_name, pSources[i].p_url_address);

	std::print("Please enter the IP of the source that you want to connect to.\n");

	NDIlib_recv_create_v3_t NDIRecvCreateDesc;
	bool NDIReady = false;
	std::string url;
	do 
	{
		std::cin >> url;
		for (std::size_t i = 0; i < numSources; i++) 
		{
			if (url == pSources[i].p_url_address) 
			{
				NDIRecvCreateDesc.source_to_connect_to = pSources[i];
				std::print("You are now connecting to {}.\n", pSources[i].p_ndi_name);
				NDIReady = true ;
				break;
			}
		}
		if (!NDIReady) std::print("No source matched! Please try again.\n");
	} while (!NDIReady);

	std::string name=NDIRecvCreateDesc.source_to_connect_to.p_ndi_name;
	name += " Receiver";
	NDIRecvCreateDesc.p_ndi_recv_name = name.c_str();

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
			const auto dataSize = static_cast<size_t>(audioInput.no_samples) * audioInput.no_channels;

			// Create a NDI audio object and convert it to interleaved float format.
			NDIlib_audio_frame_interleaved_32f_t audioDataNDI;
			audioDataNDI.p_data	= new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioDataNDI);
			NDIlib_recv_free_audio_v2(pNDI_recv, &audioInput);

			NDIdata.setCapacity (static_cast<std::size_t>(dataSize * QUEUE_SIZE_MULTIPLIER));
			NDIdata.push(audioDataNDI.p_data, audioDataNDI.no_samples,audioDataNDI.no_channels,audioDataNDI.sample_rate);

			delete[] audioDataNDI.p_data;
		}
		
	}
	#pragma endregion
	
	#pragma region NDI Clean up
	NDIlib_recv_destroy(pNDI_recv);
	NDIlib_destroy();
	#pragma endregion
}
#pragma endregion

#pragma region Sndfile Input
void sndfileRead()
{
	/*
	std::string filePathStr;
	while (true)
	{
		if(NDIReady.load())
		{ 
			std::print("Sndfile: Please enter the path of the sound file: ");
			std::getline(std::cin >> std::ws, filePathStr);
			break;
		}
	}*/

	SndfileHandle sndFile("C:/Users/Modulo/Desktop/Nouveau dossier/Music/Rachmaninov- Music For 2 Pianos, Vladimir Ashekenazy & André Previn/Rachmaninov- Music For 2 Pianos [Disc 1]/Rachmaninov- Suite #2 For 2 Pianos, Op. 17 - 3. Romance.wav");
	const size_t bufferSize = sndFile.frames() * sndFile.channels() + 100;

	float* temp = new float[bufferSize];

	SNDdata.setCapacity(bufferSize);

	sndFile.read(temp, bufferSize);
	SNDdata.push(temp, sndFile.frames(), sndFile.channels(), sndFile.samplerate());
	
	delete[] temp;
	return;
}
#pragma endregion


#pragma region PA output
static int portAudioOutputCallback(	const	void*						inputBuffer,
											void*						outputBuffer,
											unsigned long				framesPerBuffer,
									const	PaStreamCallbackTimeInfo*	timeInfo,
											PaStreamCallbackFlags		statusFlags,
											void*						UserData)
{
	auto in = static_cast<const float*>(inputBuffer);
	auto out = static_cast<float*>(outputBuffer);
	
	//auto isAllZeros = [](const float* ptr, int size) { for (int i = 0; i < size; ++i) { if (ptr[i] != 0) return false; } return true; };
	//MICdata.setCapacity(framesPerBuffer * PA_INPUT_CHANNELS);
	//if(!isAllZeros(in,framesPerBuffer)) 
		//MICdata.push(in, framesPerBuffer, PA_INPUT_CHANNELS, PA_SAMPLE_RATE);
	
	memset(out, 0, sizeof(out) * framesPerBuffer);
	if (!SNDdata.empty()) SNDdata.pop(out, framesPerBuffer, true);
	if (!NDIdata.empty()) NDIdata.pop(out, framesPerBuffer, true);
	//if (!MICdata.empty()) MICdata.pop(out, framesPerBuffer, true);
	for (uint32_t i = 0; i < framesPerBuffer * 2; i++)
	{
		out[i] += in[i];
	}
	return paContinue;
}

void portAudioOutputThread()
{
	std::signal(SIGINT, sigIntHandler);

	PaStream* streamOut;
	PAErrorCheck(Pa_OpenDefaultStream( &streamOut,						// PaStream ptr
										PA_INPUT_CHANNELS,				
										PA_OUTPUT_CHANNELS,				
										PA_FORMAT,						
										PA_SAMPLE_RATE,					
										PA_BUFFER_SIZE,					
										portAudioOutputCallback,		// Callback function called
										nullptr));						// No user NDIdata passed

	PAErrorCheck(Pa_StartStream(streamOut));
	while (!exit_loop){}

	PAErrorCheck(Pa_StopStream(streamOut));
	PAErrorCheck(Pa_CloseStream(streamOut));
}
#pragma endregion
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