#include <iostream>
#include <list>
#include <print>
#include <vector>
#include "audioFrame.h"

audioQueue<float> queue(9);
audioQueue<float> out(9);

void thread1()
{
	std::vector<float> b{ 9,8,7,6,5,4,3,2,1 };
	queue.push(b.data(),b.size(), 2, 44100);

}
void thread2()
{
	float* ptr = new float[9];
	queue.pop(ptr, queue.size(), false);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	for (int i = 0; i < 9; i++)
	{
		std::print("{}\n", ptr[i]);
	}

	out.push(std::move(ptr), queue.size(), 2, 44100);

	float* ptr1 = new float[9];
	out.pop(ptr1, out.size(), false);

	for (int i = 0; i < 9; i++)
	{
		std::print("{}\n", ptr1[i]);
	}

	delete[] ptr1;
}


int main() 
{
	std::thread ndiThread(thread1);
	std::vector<float> a{ 1,2,3,4,5,6,7,8,9 };
	
	queue.push(a.data(), a.size(), 2, 44100);
	std::thread portaudio(thread2);


	ndiThread.detach();
	portaudio.join();
	return 0;
}






































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