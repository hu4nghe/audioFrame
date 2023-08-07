#include <iostream>
#include <fstream>
#include <format>

#include <queue>
#include <memory>

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <csignal>
#include <chrono>

// Non STL libs : NDI and PortAudio
#include "Processing.NDI.Lib.h" 
#include "portaudio.h"

/**
 * @brief A PortAudio error handler.
 *
 * @param err PortAudio error code.
 */
void handlePaError(PaError err)
{
    if (err != paNoError)
    {
        // Print error info.
        std::cout << std::format("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Define audio parameters.
 */
constexpr auto PA_BUFFER_SIZE = 4096;
constexpr auto SAMPLE_RATE = 44100;
typedef float SAMPLE;

/**
 * @brief Define signal handler allows to exit by Ctrl + C.
 */
static std::atomic<bool> exit_loop(false);
static void sigint_handler(int) { exit_loop = true; }

/**
 * @brief Create the audio data queue and use mutex to avoid access conflicts.
 */
std::queue<std::unique_ptr<SAMPLE[]>> audioBufferQueue;
std::mutex bufferMutex;
std::condition_variable bufferCondVar;

/**
 * @brief NDI audio reveive function, runs on a individual thread.
 */
void NDIAudioTread()
{
    // Initialize NDI lib.
    if (!NDIlib_initialize())
    {
        // Print error info.
        std::cout << std::format("Error : Unable to run NDI.") << std::endl;
        exit(EXIT_FAILURE);
    }

    // Try to catch a exit signal.
    std::signal(SIGINT, sigint_handler);


    // Create a NDI finder
    const NDIlib_find_create_t NDI_find_create_desc;
    auto pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
    if (pNDI_find == nullptr)
    {
        std::cout << std::format("Error : Unable to create NDI finder.") << std::endl;
        return;
    }

    // We wait until there is at least one source on the network
    uint32_t no_sources = 0;
    const NDIlib_source_t* p_sources = nullptr;
    while (!exit_loop && !no_sources)
    {
        NDIlib_find_wait_for_sources(pNDI_find, 1000);
        p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
    }
    // No source
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

    NDIlib_audio_frame_v2_t audioFrame;

    const auto start = std::chrono::steady_clock::now();
    while (!exit_loop && std::chrono::steady_clock::now() - start < std::chrono::minutes(30))
    {
        // Capture the data
        auto NDI_frame_type = NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audioFrame, nullptr, 1000);

        if (NDI_frame_type == NDIlib_frame_type_audio)
        {
            /* debug outputs
            std::cout << "NDI INPUT :\n";
            std::cout << "Audio data received:" << std::endl;
            std::cout << "Sample Rate: " << audioFrame.sample_rate << std::endl;
            std::cout << "Number of Channels: " << audioFrame.no_channels << std::endl;
            std::cout << "Number of Samples: " << audioFrame.no_samples << std::endl;
            */

            auto numSamples = audioFrame.no_samples * audioFrame.no_channels;

            NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
            audio_frame_32bpp_interleaved.p_data = new float[numSamples];

            // Convert it
            NDIlib_util_audio_to_interleaved_32f_v2(&audioFrame, &audio_frame_32bpp_interleaved);

            auto sampleData = std::make_unique<SAMPLE[]>(numSamples);
            std::copy(audio_frame_32bpp_interleaved.p_data, audio_frame_32bpp_interleaved.p_data + (numSamples), sampleData.get());
            NDIlib_recv_free_audio_v2(pNDI_recv, &audioFrame);

            std::lock_guard<std::mutex> lock(bufferMutex);
            audioBufferQueue.push(std::move(sampleData));

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
    // debug output
    std::cout << std::format("NDI data poped, there are {} elements in the queue.\n\n\n", audioBufferQueue.size());

    std::copy(audioSamples.get(), audioSamples.get() + framesPerBuffer * 2, out);

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
    PaError err = Pa_Initialize();
    handlePaError(err);

    std::thread ndiThread(NDIAudioTread);

    PaStream* stream;
    err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, 44100, PA_BUFFER_SIZE, audioCallback, nullptr);
    handlePaError(err);

    err = Pa_StartStream(stream);

    std::cout << "Playing audio from NDI..." << std::endl;
    std::cin.get();


    err = Pa_StopStream(stream);
    handlePaError(err);

    //end the ndi thread
    ndiThread.join();

    //clean up
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}
/*
SRC_DATA srcData;
srcData.data_in = audioSamples.get();
srcData.input_frames = framesPerBuffer;
srcData.data_out = outTemp;
srcData.output_frames = framesPerBuffer;
srcData.src_ratio = static_cast<double>(160) / static_cast <double>(147);


std::cout << "before resample : " << *(audioSamples.get()) << std::endl;
src_simple(&srcData, SRC_SINC_BEST_QUALITY, 2);
std::cout << "after resample : " << *outTemp << std::endl;
*/