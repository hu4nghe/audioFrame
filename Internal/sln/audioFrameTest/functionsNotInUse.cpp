/* Future file reading function
bool readFile(std::filesystem::path audioPath)
{
    std::string filePath;
    std::cout << "Please enter the sound file path." << std::endl;
    std::getline(std::cin, filePath);
    if (std::filesystem::exists(filePath))
    {
        audioPath = filePath;
        return false;
    }
    else
    {
        std::cout << "The path is not valide !" << std::endl;
        return true;
    }
}
bool resample(size_t targetSampleRate)
    {
        SRC_STATE *srcState = src_new(SRC_SINC_BEST_QUALITY, this->channelNum, nullptr);

        const auto resempleRatio = static_cast<double>(targetSampleRate) / static_cast<double>(this->sampleRate);
        const auto newSize = static_cast<int>(audioData.size() * resempleRatio)+1;
        std::vector<T> resempledAudio(newSize);

        SRC_DATA srcData;
        srcData.data_in = audioData.data();
        srcData.data_out = resempledAudio.data();
        srcData.input_frames = static_cast<long>(audioData.size());
        srcData.output_frames = static_cast<long>(resempledAudio.size());
        srcData.src_ratio = resempleRatio;

        src_process(srcState, &srcData);

        sampleRate = targetSampleRate;
        return true;
    }

#include "portaudio.h"
#include "sndfile.hh"
#include "audioFrame.h"

#include <fstream>

#define PA_SAMPLE_TYPE paFloat32
constexpr auto PA_BUFFER_SIZE = 32;

static int audioCallback(const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    auto out = static_cast <float*>(outputBuffer);
    auto sndFile = static_cast<audioFrame<float>*>(userData);

    sndFile->diffuse(out, framesPerBuffer);
    if (sndFile->isEnd() == true) return paComplete;
    else return paContinue;
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
    std::filesystem::path inpath("D:/Music/Mahler Symphony No.6/Mahler- Symphony #6 In A Minor, 'Tragic' - 4. Finale- Allegro Moderato.wav");
    audioFrame<float> input;
    input.readSoundFile(inpath);

    // Set up PortAudio stream and start
    error_check(Pa_Initialize());
    PaStream* stream;
    error_check(Pa_OpenDefaultStream(&stream, 0, 2, PA_SAMPLE_TYPE, 48000, PA_BUFFER_SIZE, audioCallback, &input));
    error_check(Pa_StartStream(stream));

    std::cout << "Playing..." << std::endl;
    std::cin.get();

    // Stop
    error_check(Pa_StopStream(stream));
    error_check(Pa_CloseStream(stream));
    Pa_Terminate();

    return 0;
}
*/