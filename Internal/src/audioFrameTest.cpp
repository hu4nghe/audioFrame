/*
#include <print>
#include "audioFrame.h"
#include "sndfile.hh"

int main() 
{

}




*/





















/*

	std::print("start\n");
	
	audioQueue<float> a;
	audioQueue<float> b;

	a.sndfileRead("C:/Users/Modulo/Desktop/Nouveau dossier/Music/Rachmaninov- Music For 2 Pianos, Vladimir Ashekenazy & André Previn/Rachmaninov- Music For 2 Pianos [Disc 1]/Rachmaninov- Suite #1 For 2 Pianos, Op. 5, 'Fantaisie-Tableaux' - 1. Bacarolle- Allegretto.wav");
	return 0;
	*/









/* 

static std::atomic<bool> exit_loop(false);
static void sigIntHandler(int) { exit_loop = true; }

inline void  PAErrorCheck(PaError err) { if (err) { std::print("PortAudio error : {}.\n", Pa_GetErrorText(err)); exit(EXIT_FAILURE); } }
audioQueue<float> MicroInput(0);
constexpr auto QUEUE_SIZE_MULTIPLIER = 1.8;

static int portAudioInputCallback(const void*						inputBuffer,
										void*						outputBuffer,
										unsigned long				framesPerBuffer,
								  const PaStreamCallbackTimeInfo*	timeInfo,
										PaStreamCallbackFlags		statusFlags,
										void* UserData)
{
	auto in = static_cast<const float*>(inputBuffer);
	MicroInput.setCapacity(8192);
	MicroInput.setChannelNum(2);
	MicroInput.push(in, framesPerBuffer);
	return paContinue;
}

static int portAudioOutputCallback(const void* inputBuffer,
	void* outputBuffer,
	unsigned long				framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags		statusFlags,
	void* UserData)
{
	auto out = static_cast<float*>(outputBuffer);
	MicroInput.pop(out, framesPerBuffer,true);
	std::print("out : poped\n");
	return paContinue;
}


void portAudioInputThread()
{
	PaStream* streamIn;
	PAErrorCheck(Pa_OpenDefaultStream( &streamIn,						// PaStream ptr
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


void paOut()
{
	PaStream* streamIn;
	PAErrorCheck(Pa_OpenDefaultStream(&streamIn,						// PaStream ptr
										0,								// Input  channels
										2,								// Output channels
										paFloat32,						// Sample format
										44100,					// 44100
										128,					// 128
										portAudioOutputCallback,			// Callback function called
										nullptr));						// No user NDIdata passed
	PAErrorCheck(Pa_StartStream(streamIn));
	while (!exit_loop) 
	{
		if (!MicroInput.size()) Pa_AbortStream(streamIn);
		if (MicroInput.size() && Pa_IsStreamStopped(streamIn)) Pa_StartStream(streamIn);
	}
	PAErrorCheck(Pa_StopStream(streamIn));
	PAErrorCheck(Pa_CloseStream(streamIn));
}

int main()
{
	Pa_Initialize();
	std::thread portaudioIn(portAudioInputThread);
	std::thread portaudioOUt(paOut);
	portaudioIn.join();
	portaudioOUt.join();
	PAErrorCheck(Pa_Terminate());
}*/