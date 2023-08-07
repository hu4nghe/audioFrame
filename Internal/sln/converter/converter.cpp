#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <filesystem>
#include <memory>

#include "sndfile.hh"
#include "samplerate.h"
#include "portaudio.h"

namespace fs = std::filesystem;

#define PA_SAMPLE_TYPE paFloat32
constexpr auto PA_SAMLERATE = 48000;
constexpr auto PA_BUFFER_SIZE = 128;

static int audioCallback(const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    float* out = static_cast <float*>(outputBuffer);

    SndfileHandle* sndFile = (SndfileHandle*)userData;

    float *tempIn = new float[framesPerBuffer * sndFile->channels()];
    


    sf_count_t numFramesRead = sndFile->read(tempIn, static_cast<sf_count_t>(framesPerBuffer) * 2);

    double ratio = static_cast<double>(48000) / static_cast<double>(44100);
    int outputFrames = static_cast<int>(static_cast<double>(numFramesRead) * ratio)+1;
    std::cout << std::format("the frames before : {}, the frames after : {}",numFramesRead,outputFrames) << std::endl;

    float* tempOut = new float[framesPerBuffer * ratio * sndFile->channels()];

    SRC_DATA convertData;
    convertData.data_in = tempIn;
    convertData.data_out = tempOut;
    convertData.input_frames = numFramesRead;
    convertData.output_frames = outputFrames;

    src_simple(&convertData, SRC_SINC_BEST_QUALITY, sndFile->channels());
    
    std::copy(tempOut, tempOut + static_cast<int>(outputFrames)+1, out);
    



    return 0;
}

void error_check(PaError err)
{
    if (err != paNoError)
    {
        std::cout << std::format("Portaudio error : {} \n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}


int main()
{

    PaError err = Pa_Initialize();
    error_check(err);
    fs::path musicInput("D:\\Music\\Mahler Symphony No.5\\Mahler- Symphony #5 In C Sharp Minor - 1. Trauermarsch.wav");
    SndfileHandle sndFile(musicInput.string().c_str());

    // Set up PortAudio stream
    PaStream* stream;
    Pa_OpenDefaultStream(&stream, 0, 2, PA_SAMPLE_TYPE, PA_SAMLERATE,PA_BUFFER_SIZE, audioCallback, &sndFile);
    Pa_StartStream(stream);
    std::cout << "Playing..." << std::endl;
    std::cin.get();
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}
