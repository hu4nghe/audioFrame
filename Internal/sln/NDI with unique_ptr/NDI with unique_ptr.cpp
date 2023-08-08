#include <iostream>
#include <format>

#include <queue>
#include <memory>

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <csignal>
#include <chrono>

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


std::mutex bufferSizeMtx;
std::condition_variable bufferSizeCondVar;
bool bufferSizeReady = false;
bool bufferSizeDefined = false;
int bufferSize = 0;




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
            auto numSamples = audioFrame.no_samples * audioFrame.no_channels;

            NDIlib_audio_frame_interleaved_32f_t audio_frame_32bpp_interleaved;
            audio_frame_32bpp_interleaved.p_data = new float[numSamples];

            // Convert it
            NDIlib_util_audio_to_interleaved_32f_v2(&audioFrame, &audio_frame_32bpp_interleaved);
            std::cout << std::format("number of sample converted in NDI structure : {}", audioFrame.no_samples) << std::endl;
            
            // Pass buffer size to portAudio output thread.
            if (!bufferSizeDefined)
            {
                std::unique_lock<std::mutex> lock(bufferSizeMtx);
                bufferSize = audioFrame.no_samples;
                bufferSizeReady = true;
                bufferSizeDefined = true;
                lock.unlock();
                bufferSizeCondVar.notify_one();

            }

            auto sampleData = std::make_unique<SAMPLE[]>(numSamples);
            std::copy(audio_frame_32bpp_interleaved.p_data, audio_frame_32bpp_interleaved.p_data + (numSamples), sampleData.get());
            std::cout << std::format("number of sample in audioFrame object : {}", numSamples) << std::endl;
            NDIlib_recv_free_audio_v2(pNDI_recv, &audioFrame);

            std::lock_guard<std::mutex> lock(bufferMutex);
            audioBufferQueue.push(std::move(sampleData));

            // debug output
            //std::cout << std::format("NDI data pushed, there are {} elements in the queue.\n", audioBufferQueue.size());

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
    std::cout <<std::format("Frames per buffer passed to call back funtion is : {}.", framesPerBuffer)  << std::endl;

    //std::cout << std::format("PortAudio Callback Function called, there are {} elements in the queue.\n", audioBufferQueue.size());
    
    auto out = static_cast<SAMPLE*>(outputBuffer);

    std::unique_lock<std::mutex> lock(bufferMutex);

    bufferCondVar.wait(lock, [] { return !audioBufferQueue.empty(); });

    auto audioSamples = std::move(audioBufferQueue.front());

    audioBufferQueue.pop();
    // debug output
    //std::cout << std::format("NDI data poped, there are {} elements in the queue.", audioBufferQueue.size()) << std::endl;;

    std::copy(audioSamples.get(), audioSamples.get() + framesPerBuffer * 2, out);

    std::cout << std::format("number of sample played in portAudio : {}\n\n\n", framesPerBuffer) << std::endl;

    return paContinue;
}

void runPA()
{
    if (!bufferSizeDefined)
    {
        std::unique_lock<std::mutex> lock(bufferSizeMtx);
        bufferSizeCondVar.wait(lock, [] { return bufferSizeReady; });
    }

    PaStream* stream;
    PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, bufferSize, audioCallback, nullptr);
    handlePaError(err);

    err = Pa_StartStream(stream);

    std::cout << "Playing audio from NDI..." << std::endl;
    std::cin.get();

    err = Pa_StopStream(stream);
    handlePaError(err);
    Pa_CloseStream(stream);

}

int main()
{
    PaError err = Pa_Initialize();
    handlePaError(err);

    std::thread ndiThread(NDIAudioTread);

    std::thread portaudio(runPA);

    

    //end the ndi thread
    ndiThread.join();
    portaudio.join();

    //clean up
    
    Pa_Terminate();

    return 0;
}