#include "portaudio.h"
#include "sndfile.hh"
#include "audioFrame.h"

#define PA_SAMPLE_TYPE paFloat32
constexpr auto PA_BUFFER_SIZE = 64;

static int NDIAudioCallback(const void* inputBuffer,
                            void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData)
{
    auto out = static_cast<float*>(outputBuffer);
    auto sndFile = static_cast<audioFrame<float>*>(userData);      
    std::cout << "portaudio called;" << std::endl;
    auto vec(framesPerBuffer);
    std::vector<float> temp;
    const int size = sndFile->pop(temp , framesPerBuffer);
    auto tempPtr = std::make_unique<float>(size);
    std::copy(temp.begin(), temp.end(), tempPtr.get());
    out = tempPtr.get();
    return paContinue;
}

inline void error_check(PaError err){if (err != paNoError) exit(EXIT_FAILURE);}

int main()
{
    std::filesystem::path inpath("D:/Music/Mahler Symphony No.2/Mahler- Symphony #2 In C Minor, 'Resurrection' - 5g. Mit Aufschwung Aber Nicht Eilen.wav");
    audioFrame<float> input;
    input.readSoundFile(inpath);

    // Set up PortAudio stream and start
    error_check(Pa_Initialize());
    PaStream* stream;
    error_check(Pa_OpenDefaultStream(&stream, 0, 2, PA_SAMPLE_TYPE, 44100, PA_BUFFER_SIZE, NDIAudioCallback, &input));
    error_check(Pa_StartStream(stream));

    std::cout << "Playing..." << std::endl;
    std::cin.get();

    // Stop
    error_check(Pa_StopStream(stream));
    error_check(Pa_CloseStream(stream));
    Pa_Terminate();

    return 0;
}
