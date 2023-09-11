#ifndef audioQueue_H
#define audioQueue_H

#include <algorithm>
#include <atomic>
#include <print>
#include <thread>
#include <type_traits>
#include <vector>
#include <list>

#include "samplerate.h"

template <typename T, typename = std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, short>>>
class audioQueue 
{
    private : //Class members
              std::vector<T> queue;

                std::size_t  audioSampleRate;
                std::size_t  channelNum;
                std::size_t  inputDelay;
                std::size_t  outputDelay;
   std::atomic <std::size_t> head;
   std::atomic <std::size_t> tail;                
   std::atomic <std::size_t> elementCount;

               std::uint8_t  lowerThreshold;
               std::uint8_t  upperThreshold;
//             std::uint8_t  volume; 
   std::atomic<std::uint8_t> usage;

    public : //Public member functions
                             audioQueue         () = default;
                             audioQueue         (const  std:: size_t    initialCapacity);

                       void  push               (                 T*  &&ptr, 
                                                 const  std:: size_t    frames,
                                                 const  std:: size_t    outputChannelNum,
                                                 const  std:: size_t    outputSampleRate);
                       void  pop                (                 T*   &ptr, 
                                                 const  std:: size_t    frames,
                                                 const          bool    mode);                

    inline             void  setSampleRate      (const  std:: size_t    sRate)               { audioSampleRate = sRate; }
    inline             void  setChannelNum      (const  std:: size_t    cNum )               { channelNum = cNum; }
                       void  setCapacity        (const  std:: size_t    newCapacity);
//                     void  setVolume          (const  std::uint8_t    volume);                     
                       void  setDelay           (const  std::uint8_t    lower, 
                                                 const  std::uint8_t    upper, 
                                                 const  std:: size_t    iDelay, 
                                                 const  std:: size_t    oDelay);
               
    inline      std::size_t  channels           () const { return channelNum; }
    inline      std::size_t  sampleRate         () const { return audioSampleRate; }
    inline      std::size_t  size               () const { return elementCount.load(); }
               
    private : //Private member functions
                       bool  enqueue            (const             T    value);
                       bool  dequeue            (                  T   &value,
                                                 const          bool    mode);
                       void  clear              ();
                       void  usageRefresh       (); 
              std::vector<T> resample           (      std::vector<T>   &ptr,
                                                 const std::  size_t    targetSampleRate);
              std::vector<T> channelConversion  (      std::vector<T>  &data,
                                                 const std::  size_t    targetChannelNum);
};

#pragma region Constructors
template<typename T, typename U>
inline audioQueue<T, U>::audioQueue(const std::size_t initialCapacity)
    :   queue(initialCapacity+1), head(0), tail(0),usage(0), audioSampleRate(44100), channelNum(1), 
    elementCount(0), lowerThreshold(20), upperThreshold(80), inputDelay(45), outputDelay(15) {}
#pragma endregion

#pragma region Private member functions
template<typename T, typename U>
bool audioQueue<T,U>::enqueue(const T value)
{
    std::size_t currentTail = tail.load(std::memory_order_relaxed);
    std::size_t    nextTail = (currentTail + 1) % queue.size();

    if (nextTail == head.load(std::memory_order_acquire)) return false; // Queue is full

    queue[currentTail] = value;
    tail.store(nextTail, std::memory_order_release);
    elementCount.fetch_add(1, std::memory_order_relaxed);

    return true;
}

template<typename T, typename U >
bool audioQueue<T,U>::dequeue(T& value, const bool mode)
{
    std::size_t currentHead = head.load(std::memory_order_relaxed);

    if (currentHead == tail.load(std::memory_order_acquire)) return false; // Queue is empty

    if (!mode) value = queue[currentHead];
    else value += queue[currentHead];
    head.store((currentHead + 1) % queue.size(), std::memory_order_release);
    elementCount.fetch_sub(1, std::memory_order_relaxed);

    return true;
}

template<typename T, typename U>
inline void audioQueue<T,U>::clear()
{
    head        .store(0);
    tail        .store(0);
    elementCount.store(0);
}

template<typename T, typename U>
inline void audioQueue<T,U>::usageRefresh()
{
    usage.store(static_cast<std::uint8_t>(static_cast<double>(elementCount.load()) / queue.size() * 100.0));
}

template<typename T, typename U>
std::vector<T> audioQueue<T, U>::channelConversion(std::vector<T>& data, const std::size_t targetChannelNum)
{
    const auto newSize = data.size() / channelNum * targetChannelNum;
    data.reserve(newSize);

    //channel conversion
    for (auto i = 0; i < newSize; i += channelNum)
    {
        for (auto j = 0; j < (targetChannelNum - channelNum); j++)
        {

        }
    }

    channelNum = targetChannelNum;
}

#pragma endregion

#pragma region Public APIs
template<typename T, typename U>
void audioQueue<T,U>::push(T*&& ptr, std::size_t frames, const std::size_t outputChannelNum, const std::size_t outputSampleRate)
{   
    

    const bool needChannelConversion = (outputChannelNum != channelNum);
    const bool needResample          = (outputSampleRate != audioSampleRate);
    const auto currentSize           = frames * channelNum;
    std::list<T> temp;
    std::move(ptr, ptr + currentSize, std::back_inserter(temp));

    if (needChannelConversion)
    {
        channelConversion(temp, outputChannelNum);
    }
    if (needResample)
    {
        //const auto sampleRateRatio = static_cast<double>(outputSampleRate) / static_cast<double>(audioSampleRate);
        resample(temp,outputSampleRate);
    }
    const auto finalSize = temp.size();
    const auto estimatedUsage = usage.load() + (finalSize * 100 / queue.size());

    if (estimatedUsage >= upperThreshold) std::this_thread::sleep_for(std::chrono::milliseconds(inputDelay));

    for (auto i = 0; i < finalSize; i++)
    {
        if (!(this->enqueue(out[i])))
        {
            std::print("Warning : push operation aborted, not enough space. {} elements are pushed.\n",i+1);
            break;
        }
    }
    usageRefresh();
}

template<typename T, typename U>
void audioQueue<T,U>::pop(T*& ptr, std::size_t frames,const bool mode)
{   
    const auto size = frames * channelNum;
    const auto estimatedUsage = usage.load() >= (size * 100 / queue.size()) ? usage.load() - (size * 100 / queue.size()) : 0;
    
    if (estimatedUsage <= lowerThreshold) std::this_thread::sleep_for(std::chrono::milliseconds(outputDelay));
    for (auto i = 0; i < size; i++)
    {
       if (!this->dequeue(ptr[i],mode)) 
       {
           std::print("Warning : there is only {} elements were poped, {} demanded.\n", i, size);
           break;
       }
    }
    usageRefresh();
}

template<typename T, typename U>
inline void audioQueue<T,U>::setCapacity(std::size_t newCapacity)
{   
    if (newCapacity == queue.size()) return;
    else
    {
        if (this->size()) this->clear();
        queue.resize(newCapacity);
    }
}

template<typename T, typename U>
inline void audioQueue<T,U>::setDelay(const std::uint8_t lower, const std::uint8_t upper, const std::size_t iDelay, const std::size_t oDelay)
{
    auto isInRange = [](const std::uint8_t val) { return val >= 0 && val <= 100; };
    if (isInRange(lowerThreshold) && isInRange(upperThreshold))
    {
        lowerThreshold = lower;
        upperThreshold = upper;
        inputDelay     = iDelay;
        outputDelay    = oDelay;
    }
    else std::print("The upper and lower threshold must between 0% and 100% ! Threshold not set. ");
}
#pragma endregion

#endif// audioQueue_H

/*std::vector<T> temp0, temp1;

    
    if (resample)
    {
        SRC_STATE* srcState = src_new(SRC_SINC_BEST_QUALITY, channelNum, nullptr);

        out = new T[newSize];

        const auto resempleRatio = static_cast<double>(outputSampleRate) / static_cast<double>(audioSampleRate);
        const auto newSize = static_cast<size_t>(std::ceil(static_cast<double>(frames) * static_cast<double>(outputChannelNum) * resampleRatio));

        std::vector<T> temp0(newSize);

        if (channelConversion && resample) std::vector<T> temp1(frames * outputChannelNum);
        SRC_DATA srcData;
        srcData.data_in         = temp.data();
        srcData.data_out        = out;
        srcData.input_frames    = temp.size();
        srcData.output_frames   = newSize;
        srcData.src_ratio       = resempleRatio;

        const char* errorInfo = src_strerror(src_process(srcState, &srcData));

        src_delete(srcState);

        sampleRate = outputSampleRate;

        return true;

    }*/