/*
   bool resample(std::size_t targetSampleRate)
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


/*SNDFILE WITH MICROPHONE TEST*/
/*
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


/*DEBUG TOOL CODE DURATION CHECK*/
/*
	auto start = std::chrono::high_resolution_clock::now();
	std::cout << "Initialize time : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << " microseconds." << std::endl;
*/


/*FULL NDI ERROR HANDLE FUNCTION*/
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


/*SNDFILE READ*/
/*
bool readSoundFile(const fs::path filePath);
template<typename T, typename U>
inline bool audioQueue<T, U>::readSoundFile(const fs::path filePath)
{
	SndfileHandle soundFile(filePath.string(), SFM_READ);
	audioSampleRate = soundFile.samplerate();
	channelNum = soundFile.channels();
	queue.resize(soundFile.frames() * channelNum);
	queue.size() = queue.size();
	soundFile.read(queue.data(), int(soundFile.frames() * channelNum));
	return true;
}
*/


/*NDI METADATA*/
/*else if (NDI_frame_type == NDIlib_frame_type_metadata)
{
	NDIlib_recv_free_metadata(pNDI_recv, &metadata_frame);
	std::string meta(metadata_frame.p_data,metadata_frame.length);
	std::print("the metainfo is : {}",meta);
}
*/


//if (NDIdata.size())
		//{
			//auto *NDIout = new float [PA_BUFFER_SIZE];
			//NDIdata.pop(NDIout,PA_BUFFER_SIZE, false);
			//output.push(std::move(NDIout), PA_BUFFER_SIZE, 2, SAMPLE_RATE);
//if (!output.size()) Pa_AbortStream(streamOut);
//if (output.size() && Pa_IsStreamStopped(streamOut)) Pa_StartStream(streamOut);
//}