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
        : sampleRate(sRate), channelNum(cNum) {}

    // Copy constructor
    audioFrame(const audioFrame& other)
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(other.audioData), currentPos(other.currentPos) {}

    // Move Constructor
    audioFrame(audioFrame&& other) noexcept
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(std::move(other.audioData)), currentPos(other.currentPos) {}

    // Move constructor for C style array
    audioFrame(int sampleRate, int channelNum, float* data, size_t dataSize, int currentPos) noexcept
        : sampleRate(sampleRate), channelNum(channelNum), audioData(data, data + dataSize), currentPos(0) {}

    // constructor which takes a C style array as data, an interface with C libraries.
    audioFrame(const int sRate, const int cNum, const T *data, const size_t size)
        : sampleRate(sRate), channelNum(cNum), currentPos(0)
    {
        audioData.assign(data, data + size);
    }

    inline size_t getSize() { return this->audioData.size(); }
    inline int getRate() { return this->sampleRate; }

    void readSoundFile(const std::filesystem::path filePath)
    {
        SndfileHandle soundFile(filePath.string(), SFM_READ);
        sampleRate = soundFile.samplerate();
        channelNum = soundFile.channels();
        audioData.resize(soundFile.frames()* channelNum);
        soundFile.read(&audioData[0], int(soundFile.frames()* channelNum));
        currentPos = 0;
    }
    
    void resample(const int outputSampleRate)
    {
        if (outputSampleRate == sampleRate)
            return;
        else
        {
            sampleRate = outputSampleRate;
            SRC_STATE* state;
            SRC_DATA data;

            data.end_of_input = false;

            T* out = new T[audioData.size() * channelNum];

            data.input_frames = audioData.size() / 2;
            data.data_in = &audioData[0];
            data.data_out = out;
            data.src_ratio = static_cast<double>(outputSampleRate) / static_cast<double>(sampleRate);
            data.output_frames = audioData.size();

            state = src_new(SRC_LINEAR, channelNum, nullptr);
            if (state == nullptr)
            {
                std::cout << std::format("Error : Initialisation failed !") << std::endl;
                exit(EXIT_FAILURE);
            }

            src_process(state, &data);
            audioData.assign(out, out + (data.output_frames_gen)* channelNum);
            data.end_of_input = true;
            src_delete(state);    
        }
    }

    void diffuse(T* &out, const size_t framesPerBuffer)
    {
        auto posBegin = audioData.begin() + currentPos;
        size_t indexEnd = currentPos + (framesPerBuffer * channelNum);
        size_t endOfAudioLen = 0;

        // Extract current audio data slice
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
    }    
};

#endif// AUDIOFRAME_H