#ifndef audioQueue_H
#define audioQueue_H

#define DEBUG_MODE
#include <algorithm>
#include <atomic>
#include <print>
#include <thread>
#include <type_traits>
#include <vector>

template <typename T,
          typename = std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, short>>>
class audioQueue 
{
    private:
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


                       bool  enqueue         (const       T  value);
                       bool  dequeue         (            T &value);
                       void  clear           ();
                       void  usageRefresh    ();            
    public:
                             audioQueue      () = default;
                             audioQueue      (const  std::size_t  initialCapacity);

                       bool  push            (const           T*  ptr, 
                                              const  std::size_t  frames);
                       void  pop             (                T* &ptr, 
                                              const  std::size_t  frames);                

    inline             void  setSampleRate   (const  std::size_t  sRate){ audioSampleRate = sRate; }
    inline             void  setChannelNum   (const  std::size_t  cNum ){ channelNum = cNum; }

                       void  setCapacity     (const  std::size_t  newCapacity);
//                     void  setVolume       (const std::uint8_t  volume);                     
                       void  setDelay        (const std::uint8_t  lower, 
                                              const std::uint8_t  upper, 
                                              const  std::size_t  iDelay, 
                                              const  std::size_t  oDelay);
               
    inline       std::size_t  channels        () const { return channelNum; }
    inline       std::size_t  sampleRate      () const { return audioSampleRate; }
    inline       std::size_t  size            () const { return elementCount.load(); }
};

#pragma region Constructors
template<typename T, typename U>
inline audioQueue<T, U>::audioQueue(const std::size_t initialCapacity)
    : queue(initialCapacity), head(0), tail(0),usage(0), audioSampleRate(0),
      channelNum(0),  elementCount(0), lowerThreshold(20), upperThreshold(80), 
      inputDelay(45), outputDelay(15) {}
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
bool audioQueue<T,U>::dequeue(T& value)
{
    std::size_t currentHead = head.load(std::memory_order_relaxed);

    if (currentHead == tail.load(std::memory_order_acquire)) return false; // Queue is empty

    value = queue[currentHead];
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
#pragma endregion

#pragma region Public APIs
template<typename T, typename U>
bool audioQueue<T,U>::push(const T* ptr, std::size_t frames)
{
    const auto size = frames * channelNum;
    const auto estimatedUsage = usage.load() + (size * 100 / queue.size());

    #ifdef DEBUG_MODE
    std::print("                                                                                    NDI  : current usage : {}%, estimated usage after push operation: {}%\n", usage.load(), estimatedUsage);
    #endif

    if (estimatedUsage >= upperThreshold)
    {
        #ifdef DEBUG_MODE
        std::print("                                                                                    NDI  : Input Delay Called\n");
        #endif

        std::this_thread::sleep_for(std::chrono::milliseconds(inputDelay));

        #ifdef DEBUG_MODE
        std::print("                                                                                    NDI  : Input Delay ended\n");
        #endif
    }

    #ifdef DEBUG_MODE
    std::print("                                                                                    NDI  : Push operation started\n");
    std::print("                                                                                    NDI  : usage after delay: {}%, estimated usage after push operation: {}%\n", usage.load(), usage.load() + (size * 100 / queue.size()));
    #endif

    for (auto i = 0; i < size; i++)
    {
        if (!(this->enqueue(ptr[i])))
        {
            usageRefresh();
            std::print("                                                                                    NDI  : push operation aborted : not enough space\n");
            return false;
        }
        
    }
    usageRefresh();

    #ifdef DEBUG_MODE
    std::print("                                                                                    NDI  : push sucess\n");
    #endif

    return true;
}

template<typename T, typename U>
void audioQueue<T,U>::pop(T*& ptr, std::size_t frames)
{   
    const auto size = frames * channelNum;
    const auto estimatedUsage = usage.load() >= (size * 100 / queue.size()) ? usage.load() - (size * 100 / queue.size()) : 0;
    #ifdef DEBUG_MODE
    std::print("PortAudio : current usage : {}%, estimated usage after pop operation: {}%\n", usage.load(), estimatedUsage);
    #endif
    if (estimatedUsage <= lowerThreshold)
    {
    #ifdef DEBUG_MODE
        std::print("PortAudio : Output Delay Called\n");
    #endif

        std::this_thread::sleep_for(std::chrono::milliseconds(outputDelay));
    }

    if (size > elementCount.load()) std::print("Warning : there is only {} elements in the queue, {} demanded.\n", elementCount.load(), size);

    for (auto i = 0; i < size; i++)
    {
        if(!this->dequeue(ptr[i])) break;
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
        std::print("Current data :\n    Channels : {},\n    Sample rate : {}\n", channelNum, audioSampleRate);
        return;
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