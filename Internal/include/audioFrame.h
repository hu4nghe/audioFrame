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
    bool End;

public:
    audioFrame() = default;

    // constructor specify the file format without data.
    audioFrame(const int sRate, const int cNum)
        : sampleRate(sRate), channelNum(cNum) {}

    // Copy constructor
    audioFrame(const audioFrame& other)
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(other.audioData), currentPos(other.currentPos), End(other.End) {}

    // Move Constructor
    audioFrame(audioFrame&& other) noexcept
        : sampleRate(other.sampleRate), channelNum(other.channelNum),
        audioData(std::move(other.audioData)), currentPos(other.currentPos), End(other.End) {}

    // constructor which takes a C style array as data, an interface with C libraries.
    audioFrame(int sRate, int cNum, const T *data, size_t size)
    {
        sampleRate = sRate;
        channelNum = cNum;
        audioData.assign(data, data + size);
        currentPos = 0;
        End = false;
    }

    inline size_t getSize() { return this->audioData.size(); }

    // move data to C style array, an interface with C libraries.
    inline void moveToCArray(std::unique_ptr<T[]> &ptr){ std::copy(audioData.begin(), audioData.end(), ptr.get()); }

    void readSoundFile(const std::filesystem::path filePath)
    {
        SndfileHandle soundFile(filePath.string(), SFM_READ);
        sampleRate = soundFile.samplerate();
        channelNum = soundFile.channels();

        audioData.resize(soundFile.frames()*2);
        soundFile.read(&audioData[0], int(soundFile.frames()*2));
        currentPos = 0;
    }
    
    void diffuse(T* &out, size_t framesPerBuffer)
    {
        auto posBegin = audioData.begin() + currentPos;

        size_t indexEnd = currentPos + framesPerBuffer * 2;
        if (indexEnd > audioData.size())
        {
            std::copy(posBegin, audioData.end(), out);
            End = true;
            return;
        }
        else
        {
            std::cout << std::format("playing {} to {}, total {}.percentage : {}%", currentPos, indexEnd, audioData.size(), (static_cast<double>(indexEnd) / static_cast<double>(audioData.size()))*100) << std::endl;
            std::copy(posBegin, posBegin + (framesPerBuffer * 2), out);
        }
        currentPos = indexEnd;
    }
    
    bool resample(size_t targetSampleRate)
    {
        SRC_STATE *srcState = src_new(SRC_SINC_BEST_QUALITY, channelNum, nullptr);

        const auto resempleRatio = static_cast<double>(targetSampleRate) / static_cast<double>(this->sampleRate);
        const auto newSize = static_cast<int>(audioData.size() * resempleRatio)+1;
        
        auto out = new float[audioData.size()];

        float* a = &audioData[0];
        SRC_DATA srcData;
        srcData.data_in = a;
        srcData.data_out = out;
        srcData.src_ratio = resempleRatio;

        std::cout << src_strerror(src_process(srcState, &srcData)) << std::endl;
        std::vector<float> temp(out,out + audioData.size());
        audioData = temp;
        
        for (auto& i : audioData)
            std::cout << i << std::endl;

        src_delete(srcState);

        sampleRate = targetSampleRate;
        return true;
    }

    // debug functions
    inline void print()
    {
        for (auto &i : audioData)
        {
            std::cout << i << std::endl;
        }
    }
    inline bool isEnd() { return End; }
    inline std::vector<T> getVec() { return audioData; }

    
};

#endif