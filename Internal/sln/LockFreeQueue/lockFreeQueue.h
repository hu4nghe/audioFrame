#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

template <typename T>
class LockFreeQueue 
{
public:

    LockFreeQueue() : head(nullptr), tail(nullptr) {}

    void push(const T& value) 
    {
        Node* newNode = new Node(value);
        Node* currentTail = tail.load();
        while (true) 
        {
            Node* currentTailNext = currentTail->next.load();
            if (currentTail == tail.load()) 
            {
                if (currentTailNext == nullptr) 
                {
                    if (currentTail->next.compare_exchange_weak(currentTailNext, newNode)) 
                    {
                        tail.compare_exchange_weak(currentTail, newNode);
                        return;
                    }
                }
                else 
                {
                    tail.compare_exchange_weak(currentTail, currentTailNext);
                }
            }
        }
    }

    bool pop(T& value) 
    {
        Node* currentHead = head.load();
        while (true) 
        {
            Node* currentHeadNext = currentHead->next.load();
            if (currentHead == head.load()) 
            {
                if (currentHeadNext == nullptr) 
                {
                    return false;
                }
                if (head.compare_exchange_weak(currentHead, currentHeadNext)) 
                {
                    value = currentHeadNext->data;
                    delete currentHead;
                    return true;
                }
            }
        }
    }

private:
    struct Node 
    {
        T data;
        std::atomic<Node*> next;

        Node(const T& val) : data(val), next(nullptr) {}
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};