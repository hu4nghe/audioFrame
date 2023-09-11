#include <iostream>
#include <list>
#include <print>

int main() {
	std::list<int> a = { 1, 2, 3, 4, 5, 6, 7, 8, 9 ,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36 };
	int x = 6; 
	int y = 2; 

	int copyCount			= std::min(x, y);
	int modificationCount	= std::abs(x - y);
	

	if (x < y)
	{
		for (auto iter = std::next(a.begin(), copyCount);iter != a.end();std::advance(iter, copyCount+ modificationCount))
		{
			a.insert(iter, modificationCount, 0);
		}
		a.insert(a.end(), modificationCount, 0);
	}
	else //Convert audio from mono/stereo to a multi-channel.
	{
		auto naut = a.end();
		for (auto iter = std::next(a.begin(), copyCount); iter != a.end(); std::advance(iter, copyCount))
		{
			std::print("current iter pos : {}\n", *iter);
			for (auto i = 0; i < modificationCount; i++)
			{
				if (std::next(iter, 1) != a.end())
					iter = a.erase(iter);
				else
				{
					a.erase(iter);
					goto x;
				}

			}		
		}
	}
	x:
	for (const auto& element : a) 
	{
		std::cout << element << " ";
	}
	std::cout << std::endl;

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