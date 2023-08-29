#include "audioFrame.h"
/*
#define PA_SAMPLE_TYPE paFloat32
constexpr auto PA_BUFFER_SIZE = 512;

static int NDIAudioCallback(const void* inputBuffer,
                            void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData)
{
    auto out = static_cast<float*>(outputBuffer);
    
    return paContinue;
}

inline void error_check(PaError err){if (err != paNoError) exit(EXIT_FAILURE);}

int main()
{
    std::filesystem::path inpath("D:/Music/Mahler Symphony No.2/Mahler- Symphony #2 In C Minor, 'Resurrection' - 5g. Mit Aufschwung Aber Nicht Eilen.wav");
    
    
    error_check(Pa_Initialize());
    PaStream* stream;
    error_check(Pa_OpenDefaultStream(&stream, 0, 2, PA_SAMPLE_TYPE, 44100, 2205, NDIAudioCallback, ));
    error_check(Pa_StartStream(stream));

    std::cout << "Playing..." << std::endl;
    std::cin.get();

    error_check(Pa_StopStream(stream));
    error_check(Pa_CloseStream(stream));
    error_check(Pa_Terminate());

    return 0;
}*/

int main()
{
    audioQueue<float> data(10);
    data.setChannelNum(2);
    data.setSampleRate(44100);

    float a[5] = { 1.2, 3.6, 4.8, 9.5, 56.871 };
    data.push(a, 5);

    float* out = new float[5];
    data.pop(out, 11);

    for (int i = 0; i < 5; i++)
        std::cout << out[i] << std::endl;
    return 0;

}