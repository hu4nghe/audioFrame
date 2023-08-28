#ifndef AUDIOFRAME_H
#define AUDIOFRAME_H

#include <filesystem>
#include <iostream>

#include <vector>
#include <string>

#include <format>
#include <memory>
#include <type_traits>

#include "portaudio.h"
#include "sndfile.hh"
#include "samplerate.h"

template <typename T,
          typename = std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, short>>>
class audioFrame
{
private:
    int sampleRate;
    int channelNum; // number of channels
    std::vector<T> audioData;
    size_t currentPos;
    //debug members

public:
    audioFrame() = default;

    // constructor specify the file format without data.
    audioFrame(const int sRate, const int cNum)
        : sampleRate(sRate), channelNum(cNum), currentPos(0), audioData() {}

    // Copy constructor
    audioFrame(const audioFrame& other)
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(other.audioData), currentPos(other.currentPos) {}

    // Move Constructor
    audioFrame(audioFrame&& other) noexcept
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(std::move(other.audioData)), currentPos(other.currentPos) {}
    audioFrame(int sampleRate, int channelNum, std::vector<T>&& data) noexcept
        : sampleRate(sampleRate), channelNum(channelNum), audioData(data), currentPos(0) {}

    // constructor which takes a C style array as data, an interface with C libraries.
    audioFrame(const int sRate, const int cNum, const T *data, const size_t size)
        : sampleRate(sRate), channelNum(cNum), currentPos(0), audioData(data, data + size) {}

    inline size_t getSize() { return audioData.size(); }

    void readSoundFile(const std::filesystem::path filePath)
    {
        SndfileHandle soundFile(filePath.string(), SFM_READ);
        sampleRate = soundFile.samplerate();
        channelNum = soundFile.channels();
        audioData.resize(soundFile.frames() * channelNum);
        soundFile.read(audioData.data(), int(soundFile.frames() * channelNum));
        currentPos = 0;
    }
    
    int resample(const int outputSampleRate)
    {
        if (outputSampleRate == sampleRate)
            return audioData.size();
        else
        {
            auto start = std::chrono::high_resolution_clock::now();

            const auto ratio = static_cast<double>(outputSampleRate) / static_cast<double>(sampleRate);
            const auto outputSize = static_cast<int>(audioData.size() * ratio);
            T* out = new T[outputSize];
            
            SRC_STATE* state;
            SRC_DATA data;
            
            data.end_of_input = false;
            data.input_frames = audioData.size() / channelNum;
            data.data_in = std::move(audioData.data());
            data.data_out = out;
            data.src_ratio = ratio;
            data.output_frames = outputSize;

            state = src_new(SRC_SINC_BEST_QUALITY, channelNum, nullptr);
            
            if (state == nullptr)
            {
                std::cout << std::format("Error : Initialisation failed !") << std::endl;
                exit(EXIT_FAILURE);
            }
            std::cout << "Preparation time : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << " microseconds." << std::endl;

            auto start1 = std::chrono::high_resolution_clock::now();
            src_process(state, &data);
            std::cout << "Resample time    : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start1).count() << " microseconds." << std::endl;
            
            auto start2 = std::chrono::high_resolution_clock::now();
            audioData.assign(out, out + (data.output_frames_gen)* channelNum);
            std::cout << "Data Copy time   : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start2).count() << " microseconds.\n" << std::endl;
            
            src_delete(state);
            
            
            sampleRate = outputSampleRate;
            return data.output_frames_gen;
        }
    }

    void diffuse(T* &out, const size_t framesPerBuffer)
    {
        auto start = std::chrono::high_resolution_clock::now();

        auto posBegin = audioData.begin() + currentPos;
        size_t indexEnd = currentPos + (framesPerBuffer * channelNum);
        size_t endOfAudioLen = 0;
        // Check if it is the end of audio
        if (indexEnd > audioData.size())
        {
            endOfAudioLen = audioData.end() - posBegin;
            std::copy(posBegin, audioData.end(), out);                   
        }
        else
        {
            std::copy(posBegin, posBegin + (framesPerBuffer * channelNum), out);
        }
        currentPos = indexEnd;

        std::cout << "Difusion time    : " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << " microseconds.\n" << std::endl;
    }    
};

#endif// AUDIOFRAME_H