#ifndef audioQueue_H
#define audioQueue_H

#include <algorithm>
#include <atomic>
#include <print>
#include <type_traits>
#include <vector>

template <typename T,
          typename = std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, short>>>
class audioQueue 
{
private:
                size_t  audioSampleRate;
                size_t  channelNum;
              //uint8_t volume;
                size_t  capacity;

    std::atomic<size_t> elementCount;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    std::vector<T>      queue;

                 bool   enqueue         (const T& value);
                 bool   dequeue         (      T& value);
                 bool   isDefault       (const T  value);

public:
                        audioQueue      () = default;
                        audioQueue      (const size_t initialCapacity);

                 bool   push            (const T*  ptr, size_t frames);
                 void   pop             (      T* &ptr, size_t frames);

      //audioQueue<T>   operator+       (const audioQueue<T> other);

    inline       void   setSampleRate   (const size_t sRate){ audioSampleRate = sRate; }
    inline       void   setChannelNum   (const size_t cNum ){ channelNum = cNum; }
                 bool   setCapacity     (const size_t newCapacity);
               //void   setVolume       (const uint8_t volume);

    inline     size_t   channels        () const { return channelNum; }
    inline     size_t   sampleRate      () const { return audioSampleRate; }
    inline     size_t   size            () const { return elementCount.load(); }
    inline       bool   empty           () const { return std::all_of(queue.begin(), queue.end(), &audioQueue::isDefault);}
    
};

template<typename T, typename U>
inline      audioQueue<T, U>::audioQueue(const size_t initialCapacity)
    : capacity(initialCapacity), queue(capacity), head(0), tail(0), audioSampleRate(0), channelNum(0), elementCount(0) {}

template<typename T, typename U>
inline bool audioQueue<T,U>::enqueue(const T& value)
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
inline bool audioQueue<T,U>::dequeue(T& value)
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
inline bool audioQueue<T,U>::push(const T* ptr, size_t frames)
{
    size_t size = frames * channelNum;
    for (auto i = 0; i < size; i++)
    {
        if (!(this->enqueue(ptr[i])))
        {
            std::print("Not enough space in queue.\n");
            exit(EXIT_FAILURE);
        }
    }
    return true;
}

template<typename T, typename U>
inline void audioQueue<T,U>::pop(T*& ptr, size_t frames)
{
    size_t size = frames * channelNum;
    if (size > elementCount.load())
        std::print("Warning : there is only {} elements in the queue, {} demanded.\n", elementCount.load(), size);

    for (size_t i = 0; i < size; i++)
    {
        this->dequeue(ptr[i]);
    }
}

template<typename T, typename U>
inline bool audioQueue<T,U>::setCapacity(size_t newCapacity)
{
    if (this->empty())
    {
        queue.resize(newCapacity);
        return true;
    }
    else
    {
        std::print("It is no allowed to resize a queue in use !\n");
        return false;
    }
}
#endif// audioQueue_H