#ifndef audioQueue_H
#define audioQueue_H

#include <filesystem>
#include <format>
#include <iostream>
#include <print>

#include <vector>
#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "portaudio.h"
#include "sndfile.hh"
#include "samplerate.h"

template <typename T,
          typename = std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, short>>>
class audioQueue 
{
private:
    size_t capacity;
    std::vector<T> queue;
    std::atomic<size_t> elementCount;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;

    uint32_t sampleRate;
    uint32_t channelNum;

    bool isDefault(T value)
    {
        if constexpr (std::is_same_v<T, float>) 
            return value == 0.0f;
        else if constexpr (std::is_same_v<T, short>) 
            return value == 0;
        else 
            return false;
    }
    bool enqueue(const T& value) 
    {
        size_t currentTail = tail.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) % capacity;

        if (nextTail == head.load(std::memory_order_acquire)) return false; // Queue is full

        queue[currentTail] = value;
        tail.store(nextTail, std::memory_order_release);
        elementCount.fetch_add(1, std::memory_order_relaxed);

        return true;
    }
    bool dequeue(T& value)
    {
        size_t currentHead = head.load(std::memory_order_relaxed);

        if (currentHead == tail.load(std::memory_order_acquire)) return false; // Queue is empty

        value = queue[currentHead];
        queue[currentHead] = T{};
        head.store((currentHead + 1) % capacity, std::memory_order_release);
        elementCount.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

public:
    audioQueue() = default;
    audioQueue(size_t size) : capacity(size), queue(capacity), head(0), tail(0), sampleRate(0), channelNum(0), elementCount(0){}

    inline void setSampleRate(uint32_t sRate) { sampleRate = sRate; }
    inline void setChannelNum(uint32_t cNum) { channelNum = cNum; }
    inline size_t size() { return elementCount.load(); }

    bool push(const T* ptr, size_t size)
    {
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
    void pop(float*& ptr, size_t size) 
    {
        auto start = std::chrono::high_resolution_clock::now();
        if(size > elementCount.load())
            std::print("Warning : there is only {} elements in the queue, {} demanded.\n", elementCount.load(), size);

        for (size_t i = 0; i < size; i++) 
        {
            this->dequeue(ptr[i]);
        }
        std::print("pop time : {} microseconds.\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count());
    }
    bool resize(size_t newCapacity)
    {
        bool emptyCheck = std::all_of(queue.begin(), queue.end(), &audioQueue::isDefault);
        if (emptyCheck)
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
};

#endif// audioQueue_H