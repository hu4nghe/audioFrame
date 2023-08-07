#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <csignal>
#include <chrono>

// Non STL libs : NDI and PortAudio
#include "Processing.NDI.Lib.h" 
#include "portaudio.h"

#include "audioFrame.h"

void error_check(PaError err)
{
    if (err != paNoError)
    {
        // Print error info.
        std::cout << std::format("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
        exit(EXIT_FAILURE);
    }
}

constexpr auto PA_BUFFER_SIZE = 2205;
constexpr auto SAMPLE_RATE = 44100;
typedef float SAMPLE;

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int) { exit_loop = true; }

std::queue<audioFrame<float>> audioBufferQueue;
std::mutex bufferMutex;
std::condition_variable bufferCondVar;

void NDIAudioTread()
{
    std::signal(SIGINT, sigint_handler);


    NDIlib_initialize();

    const NDIlib_find_create_t NDI_find_create_desc;
    auto pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
    uint32_t no_sources = 0;
    const NDIlib_source_t* p_sources = nullptr;
    while (!exit_loop && !no_sources)
    {
        NDIlib_find_wait_for_sources(pNDI_find, 1000);
        p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
    }
    if (p_sources == nullptr)
    {
        std::cout << std::format("Error : No source available!.") << std::endl;
        return;
    }

    // Create a receiver
    NDIlib_recv_create_v3_t NDI_recv_create_desc;
    NDI_recv_create_desc.source_to_connect_to = p_sources[0];
    NDI_recv_create_desc.p_ndi_recv_name = "Audio Receiver";
    auto pNDI_recv = NDIlib_recv_create_v3(&NDI_recv_create_desc);
    if (pNDI_recv == nullptr)
    {
        // Print error info.
        std::cout << std::format("Error : Unable to create NDI receiver.") << std::endl;
        NDIlib_find_destroy(pNDI_find);
        return;
    }

    NDIlib_find_destroy(pNDI_find);

    NDIlib_audio_frame_v2_t audioIn;

    const auto start = std::chrono::steady_clock::now();
    while (!exit_loop && std::chrono::steady_clock::now() - start < std::chrono::minutes(30))
    {
        // Capture the data
        auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioIn, nullptr, 1000);

        if (NDI_frame_type == NDIlib_frame_type_audio)
        {
            auto numSamples = audioIn.no_samples * audioIn.no_channels;

            NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
            audio_frame_32bpp_interleaved.p_data = new float[numSamples];

            // Convert it
            NDIlib_util_audio_to_interleaved_32f_v2(&audioIn, &audio_frame_32bpp_interleaved);
            audioFrame<float> inputAudio(audioIn.no_samples, audioIn.no_channels, audioIn.p_data, numSamples);
            NDIlib_recv_free_audio_v2(pNDI_recv, &audioIn);

            std::lock_guard<std::mutex> lock(bufferMutex);
            audioBufferQueue.push(std::move(inputAudio));

            // debug output
            std::cout << std::format("NDI data pushed, there are {} elements in the queue.\n", audioBufferQueue.size());

            bufferCondVar.notify_one();
        }
    }

    //NDI clean up
    NDIlib_recv_destroy(pNDI_recv);
    NDIlib_destroy();
}

static int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    std::cout << std::format("PortAudio Callback Function called, there are {} elements in the queue.\n", audioBufferQueue.size());
    auto out = static_cast<SAMPLE*>(outputBuffer);

    std::unique_lock<std::mutex> lock(bufferMutex);
    bufferCondVar.wait(lock, [] { return !audioBufferQueue.empty(); });
    auto audioSamples = std::move(audioBufferQueue.front());
    audioBufferQueue.pop();

    audioSamples.diffuse(out, framesPerBuffer);

    /* debug outputs
    std::cout << "PortAudio OUTPUT:\n";
    std::cout << "Sample rate: "<< timeInfo->outputBufferDacTime / framesPerBuffer << std::endl;
    std::cout << "Frames Per Buffer: " << framesPerBuffer << std::endl;
    std::cout << "Number of Channels: 2" << "\n\n" << std::endl;
    */

    return paContinue;
}

int main()
{
    error_check(Pa_Initialize());

    std::thread ndiThread(NDIAudioTread);

    PaStream* stream;
    error_check(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, PA_BUFFER_SIZE, audioCallback, nullptr));
    error_check(Pa_StartStream(stream));

    std::cout << "Playing audio from NDI..." << std::endl;
    std::cin.get();

    error_check(Pa_StopStream(stream));

    //end the ndi thread
    ndiThread.join();

    error_check(Pa_CloseStream(stream));
    Pa_Terminate();

    return 0;
}