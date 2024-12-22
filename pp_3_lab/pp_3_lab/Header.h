#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <cmath>
#include <atomic>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>  // Для Windows
#else
#include <pthread.h>   // Для Linux
#include <semaphore.h>
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
    void workerLoop();

#if defined(_WIN32) || defined(_WIN64)
    std::vector<HANDLE> workers;
    HANDLE taskSemaphore;                 // Семафор для задач
    HANDLE completionSemaphore;           // Семафор для завершения задач
#else
    std::vector<pthread_t> threads;
    sem_t taskSemaphore;                  // Семафор для задач
    sem_t completionSemaphore;            // Семафор для завершения задач
#endif

    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::atomic<bool> stop{ false };
    std::atomic<int> activeTasks{ 0 };
};
