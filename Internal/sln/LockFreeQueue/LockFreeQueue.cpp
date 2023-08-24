#include "lockFreeQueue.h"


int main()
{
    LockFreeQueue<int> queue;

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&queue, i]()
            {
                for (int j = 0; j < 10; ++j)
                {
                    queue.push(i * 10 + j);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    int value;
    while (queue.pop(value))
    {
        std::cout << "Popped: " << value << std::endl;
    }

    return 0;
}