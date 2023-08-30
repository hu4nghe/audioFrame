/*
   bool resample(size_t targetSampleRate)
   {
       SRC_STATE *srcState = src_new(SRC_SINC_BEST_QUALITY, channelNum, nullptr);

       const auto resempleRatio = static_cast<double>(targetSampleRate) / static_cast<double>(this->sampleRate);
       const auto newSize = static_cast<int>(audioData.size() * resempleRatio)+1;

       auto out = new float[audioData.size()];

       float* a = &audioData[0];
       SRC_DATA srcData;
       srcData.data_in = a;
       srcData.data_out = out;
       srcData.src_ratio = resempleRatio;

       std::cout << src_strerror(src_process(srcState, &srcData)) << std::endl;
       std::vector<float> temp(out,out + audioData.size());
       audioData = temp;

       for (auto& i : audioData)
           std::cout << i << std::endl;

       src_delete(srcState);

       sampleRate = targetSampleRate;
       return true;
   }
   */



//mic, sndfile

/* test : sndfile and microphone input
static int microphoneAudioCallback( const void* inputBuffer,
									void* outputBuffer,
									unsigned long framesPerBuffer,
									const PaStreamCallbackTimeInfo* timeInfo,
									PaStreamCallbackFlags statusFlags,
									void* userData)
{
	const auto in = static_cast<const float*>(inputBuffer);
	auto out = static_cast<float*>(outputBuffer);

	for (int i = 0; i < framesPerBuffer * 2; i++)
	{
		out[i] = in[i] * 3;
	}

	return paContinue;
}

static int sndfileAudioCallback(const void* inputBuffer,
								void* outputBuffer,
								unsigned long framesPerBuffer,
								const PaStreamCallbackTimeInfo* timeInfo,
								PaStreamCallbackFlags statusFlags,
								void* userData)
{
	auto out = static_cast<float*>(outputBuffer);
	SndfileHandle* sndFile = (SndfileHandle*)userData;

	sf_count_t numFramesRead = sndFile->read(out, static_cast<sf_count_t>(framesPerBuffer) * 2);
	return paContinue;
}
*/
//in pa thread
/* test : sndfile and microphone input
	SndfileHandle sndFile("D:/Music/Mahler Symphony No.2/Mahler- Symphony #2 In C Minor, 'Resurrection' - 5g. Mit Aufschwung Aber Nicht Eilen.wav");
	PaStream* sndfileStream;
	PAErrorCheck(Pa_OpenDefaultStream(&sndfileStream, 0, 2, paFloat32, SAMPLE_RATE, 128, sndfileAudioCallback, &sndFile));
	PAErrorCheck(Pa_StartStream(sndfileStream));


	PaStream* microStream;
	PAErrorCheck(Pa_OpenDefaultStream(&microStream, 2, 2, paFloat32, SAMPLE_RATE, 128, microphoneAudioCallback, nullptr));
	PAErrorCheck(Pa_StartStream(microStream));
	
	...

	 test : sndfile and microphone input
	PAErrorCheck(Pa_StopStream(sndfileStream));
	PAErrorCheck(Pa_StopStream(microStream));
	PAErrorCheck(Pa_CloseStream(sndfileStream));
	PAErrorCheck(Pa_CloseStream(microStream));
	
	
	*/



//duration check
	/*auto start = std::chrono::high_resolution_clock::now();
				std::cout << "Initialize time : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << " microseconds." << std::endl;*/

/*template <typename T>
void NDIErrorCheck(T* ptr)
{

}*/
/*
template <typename T>
auto NDIErrorCheck(T* err) -> decltype(err)
{
	if constexpr (std::is_same<T, NDIlib_find_instance_type>::value)
	{
		if (err == nullptr)
		{
			std::print("Error : Unable to create NDI finder.\n");
			exit(EXIT_FAILURE);
		}
		else return err;

	}
	else if constexpr (std::is_same<T, NDIlib_source_t>::value)
	{
		if (err == nullptr)
		{
			std::print("Error : No source available!.\n");
			exit(EXIT_FAILURE);
		}
		else return err;
	}
	else if constexpr (std::is_same<T, NDIlib_recv_instance_t>::value)
	{
		if (err == nullptr)
		{
			std::print("Error : Unable to create NDI receiver.\n");
			exit(EXIT_FAILURE);
		}
		else return err;
	}
	else
		exit(EXIT_FAILURE);
}*/