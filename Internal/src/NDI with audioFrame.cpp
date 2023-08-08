#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <csignal>
#include <chrono>

#include "Processing.NDI.Lib.h" 
#include "portaudio.h"
#include "audioFrame.h"
#include <future>

void error_check(PaError err)
{
    if (err != paNoError)
    {
        // Print error info.
        std::cout << std::format("PortAudio error : {}.", Pa_GetErrorText(err)) << std::endl;
        exit(EXIT_FAILURE);
    }
}

typedef float SAMPLE;

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int) { exit_loop = true; }

std::queue<audioFrame<float>> audioFrameBufferQueue;
std::mutex bufferMutex;
std::condition_variable bufferCondVar;

std::mutex bufferSizeMtx;
std::condition_variable bufferSizeCondVar;
bool bufferSizeReady = false;
bool bufferSizeDefined = false;
int bufferSize = 0;


void NDIAudioTread()
{
    std::signal(SIGINT, sigint_handler);


    NDIlib_initialize();

    const NDIlib_find_create_t NDIFindCreateDesc;
    auto pNDIFind = NDIlib_find_create_v2(&NDIFindCreateDesc);
    uint32_t noSources = 0;
    const NDIlib_source_t* pSources = nullptr;
    while (!exit_loop && !noSources)
    {
        NDIlib_find_wait_for_sources(pNDIFind, 1000);
        pSources = NDIlib_find_get_current_sources(pNDIFind, &noSources);
    }
    if (pSources == nullptr)
    {
        std::cout << std::format("Error : No source available!.") << std::endl;
        return;
    }

    // Create a receiver
    NDIlib_recv_create_v3_t NDIRecvCreateDesc;
    NDIRecvCreateDesc.source_to_connect_to = pSources[0];
    NDIRecvCreateDesc.p_ndi_recv_name = "Audio Receiver";
    auto pNDIRecv = NDIlib_recv_create_v3(&NDIRecvCreateDesc);
    if (pNDIRecv == nullptr)
    {
        // Print error info.
        std::cout << std::format("Error : Unable to create NDI receiver.") << std::endl;
        return;
    }

    NDIlib_audio_frame_v2_t audioIn;

    const auto start = std::chrono::steady_clock::now();
    while (!exit_loop && std::chrono::steady_clock::now() - start < std::chrono::minutes(30))
    {

        

        // Capture the data
        auto NDI_frame_type = NDIlib_recv_capture_v2(pNDIRecv, nullptr, &audioIn, nullptr, 1000);

        if (NDI_frame_type == NDIlib_frame_type_audio)
        {
            auto numSamples = audioIn.no_samples * audioIn.no_channels;

            NDIlib_audio_frame_interleaved_32f_t audioFrame32bppInterleaved;
            audioFrame32bppInterleaved.p_data = new float[numSamples];
            NDIlib_util_audio_to_interleaved_32f_v2(&audioIn, &audioFrame32bppInterleaved);

            // Pass buffer size to portAudio output thread.
            if (!bufferSizeDefined)
            {
                std::unique_lock<std::mutex> lock(bufferSizeMtx);
                bufferSize = audioIn.no_samples;
                bufferSizeReady = true;
                bufferSizeDefined = true;
                lock.unlock();
                bufferSizeCondVar.notify_one();

            }

            std::cout << std::format("number of sample converted in NDI structure : {}", audioIn.no_samples) << std::endl;

            audioFrame<float> inputAudio(audioIn.sample_rate, audioIn.no_channels, audioIn.p_data, numSamples);


            std::lock_guard<std::mutex> lock(bufferMutex);
            audioFrameBufferQueue.push(std::move(inputAudio));

            bufferCondVar.notify_one();
            delete[] audioFrame32bppInterleaved.p_data;
            NDIlib_recv_free_audio_v2(pNDIRecv, &audioIn);
        }
    }

    NDIlib_recv_destroy(pNDIRecv);
    NDIlib_destroy();
}

static int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    //debug output
    std::cout << std::format("PortAudio Callback Function called, there are {} elements in the queue.\n", audioFrameBufferQueue.size());
    
    auto out = static_cast<SAMPLE*>(outputBuffer);

    std::unique_lock<std::mutex> lock(bufferMutex);
    bufferCondVar.wait(lock, [] { return !audioFrameBufferQueue.empty(); });
    auto audioSamples = std::move(audioFrameBufferQueue.front());
    audioFrameBufferQueue.pop();

    std::cout << std::format("number of sample played in portAudio : {}", framesPerBuffer) << std::endl;

    audioSamples.diffuse(out, framesPerBuffer*2);

    return paContinue;
}

void portAudioOutputThread()
{
    if (!bufferSizeDefined)
    {
        std::unique_lock<std::mutex> lock(bufferSizeMtx);
        bufferSizeCondVar.wait(lock, [] { return bufferSizeReady; });
    }

    PaStream* stream;
    error_check(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, 44100, bufferSize, audioCallback, nullptr));
    error_check(Pa_StartStream(stream));

    std::cout << "Playing audio from NDI..." << std::endl;
    std::cin.get();

    error_check(Pa_StopStream(stream));
    error_check(Pa_CloseStream(stream));
    
}

int main()
{
    error_check(Pa_Initialize());
    std::thread ndiThread (NDIAudioTread);
    std::thread portAudioThread(portAudioOutputThread);
    

    ndiThread.join();
    portAudioThread.join();
   
    Pa_Terminate();

    return 0;
}

