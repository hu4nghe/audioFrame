#ifndef audioQueue_H
#define audioQueue_H

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
                 size_t audioSampleRate;
                 size_t capacity;
                 size_t channelNum;
                 size_t delayTime;
                uint8_t lowerThreshold;
                uint8_t upperThreshold;
//              uint8_t volume;                 
                    
    std::atomic<size_t> elementCount;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
   std::atomic<uint8_t> usage;
         std::vector<T> queue;

                 bool   enqueue         (const T& value);
                 bool   dequeue         (      T& value);
                 bool   isDefault       (const T  value);
                 void   clear           ();
                 void   usageRefresh    ();
                
public:
                        audioQueue      () = default;
                        audioQueue      (const size_t initialCapacity);

                 bool   push            (const T*  ptr, size_t frames);
                 void   pop             (      T* &ptr, size_t frames);                

//      audioQueue<T>   operator+       (const audioQueue<T>&& other);

    inline       void   setSampleRate   (const size_t  sRate){ audioSampleRate = sRate; }
    inline       void   setChannelNum   (const size_t  cNum ){ channelNum = cNum; }
                 void   setCapacity     (const size_t  newCapacity);
                 void   setDelay        (const uint8_t lower, const uint8_t upper, const size_t time);
//               void   setVolume       (const uint8_t volume);                 

    inline     size_t   channels        () const { return channelNum; }
    inline     size_t   sampleRate      () const { return audioSampleRate; }
    inline     size_t   size            () const { return elementCount.load(); }
    inline       bool   empty           () const { return elementCount == 0;}

};

// Constructor ////////////////////////////////////////////////////////////////////////////////////////// 

template<typename T, typename U>
inline audioQueue<T, U>::audioQueue(const size_t initialCapacity)
    : capacity(initialCapacity), queue(capacity), head(0), tail(0),
      audioSampleRate(0), channelNum(0), elementCount(0), usage(0),
      lowerThreshold(20), upperThreshold(85), delayTime(10){}

// Private member functions ///////////////////////////////////////////////////////////////////////////// 

template<typename T, typename U>
bool audioQueue<T,U>::enqueue(const T& value)
{
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % capacity;

    if (nextTail == head.load(std::memory_order_acquire)) return false; // Queue is full

    queue[currentTail] = value;
    tail.store(nextTail, std::memory_order_release);
    elementCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}

template<typename T, typename U >
bool audioQueue<T,U>::dequeue(T& value)
{
    size_t currentHead = head.load(std::memory_order_relaxed);

    if (currentHead == tail.load(std::memory_order_acquire)) return false; // Queue is empty

    value = queue[currentHead];
    queue[currentHead] = T{};
    head.store((currentHead + 1) % capacity, std::memory_order_release);
    elementCount.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

template<typename T, typename U>
inline bool audioQueue<T,U>::isDefault(T value)
{
    if constexpr (std::is_same_v<T, float>)
        return value == 0.0f;
    else if constexpr (std::is_same_v<T, short>)
        return value == 0;
    else
        return false;
}

template<typename T, typename U>
inline void audioQueue<T,U>::clear()
{
    head.store(0);
    tail.store(0);
    elementCount.store(0);
    for (auto& i : queue)
        if (!isDefault(i))
            i = 0.0f;
}

template<typename T, typename U>
inline void audioQueue<T,U>::usageRefresh()
{
    usage.store(static_cast<uint8_t>(static_cast<double>(elementCount.load()) / capacity * 100.0));
    std::print("Queue usage : {}%.\n", usage.load());
}

// Public APIs //////////////////////////////////////////////////////////////////////////////////////////

template<typename T, typename U>
bool audioQueue<T,U>::push(const T* ptr, size_t frames)
{
    
    if (usage.load() >= upperThreshold)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayTime));
        std::print("Input Delay Called\n");
    }
    size_t size = frames * channelNum;
    for (auto i = 0; i < size; i++)
    {
        if (!(this->enqueue(ptr[i])))
        {
            std::print("Not enough space in queue, data will be lose !\n");
        }
    }
    std::print("Data fed !\n");
    usageRefresh();
    
    return true;
}

template<typename T, typename U>
void audioQueue<T,U>::pop(T*& ptr, size_t frames)
{
    size_t size = frames * channelNum;

    if (usage.load() <= lowerThreshold)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayTime));
        std::print("Output Delay Called\n");
    }

    if (size > elementCount.load())
        std::print("Warning : there is only {} elements in the queue, {} demanded.\n", elementCount.load(), size);

    for (size_t i = 0; i < size; i++)
    {
        this->dequeue(ptr[i]);
    }
    usageRefresh();
}

template<typename T, typename U>
inline void audioQueue<T,U>::setCapacity(size_t newCapacity)
{   
    if (newCapacity == capacity)
        return;
    else
    {
        if (!this->empty())
            this->clear();

        queue.resize(newCapacity);
        capacity = newCapacity;
        std::print("current capacity : {}\n", capacity);
        return;
    }
}

template<typename T, typename U>
inline void audioQueue<T,U>::setDelay(const uint8_t lower, const uint8_t upper, const size_t time)
{
    auto isInRange = [](const uint8_t val) { return val >= 0 && val <= 100; };
    
    if (isInRange(lowerThreshold) && isInRange(upperThreshold))
    {
        lowerThreshold = lower;
        upperThreshold = upper;
        delayTime = time;
    }
    else
        std::print("The upper and lower threshold must between 0% and 100% !");
}

#endif// audioQueue_H