#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>  // для Windows
#else
#include <pthread.h>   // для Linux
#endif

extern std::mutex consoleMutex;

class ThreadPool {
public:
    ThreadPool(size_t numThreads);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void enqueue(std::function<void()> task);

    void waitForCompletion();

private:
    void processTasks();

#if defined(_WIN32) || defined(_WIN64)
    static unsigned __stdcall run(void* param);  // Windows
#else
    static void* run(void* param);  // Linux
#endif

    // Для Windows используем std::thread
#if defined(_WIN32) || defined(_WIN64)
    std::vector<std::thread> workers;
#else
    // Для Linux используем pthread_t
    std::vector<pthread_t> threads;
#endif

    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{ false };
    std::atomic<int> activeTasks{ 0 };
    std::condition_variable completionCondition;
};
