#ifndef audioQueue_H
#define audioQueue_H

#include <filesystem>
#include <iostream>

#include <vector>
#include <queue>
#include <string>

#include <format>
#include <memory>
#include <type_traits>

#include <mutex>
#include <thread>
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
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    uint32_t sampleRate;
    uint32_t channelNum;
public:
    audioQueue(size_t size) : capacity(size), queue(capacity), head(0), tail(0), sampleRate(0), channelNum(0) {}

    bool enqueue(const T& value) 
    {
        size_t currentTail = tail.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) % capacity;

        if (nextTail == head.load(std::memory_order_acquire)) return false; // Queue is full

        queue[currentTail] = value;
        tail.store(nextTail, std::memory_order_release);

        return true;
    }

    bool dequeue(T& value)
    {
        size_t currentHead = head.load(std::memory_order_relaxed);

        if (currentHead == tail.load(std::memory_order_acquire)) return false; // Queue is empty

        value = queue[currentHead];
        head.store((currentHead + 1) % capacity, std::memory_order_release);

        return true;
    }
};

#endif// audioQueue_H