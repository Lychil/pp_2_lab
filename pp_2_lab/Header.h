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

extern std::mutex consoleMutex;
class ThreadPool {
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ThreadPool(size_t numThreads);

    ~ThreadPool();

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queueMutex); // блокаем, чтобы не допустить состояние гонки
            tasks.emplace(std::forward<F>(f)); // добавление задачи
        }
        condition.notify_one(); // увед о появлении задачи
    }

    void waitForCompletion();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{ false };
    std::atomic<int> activeTasks{ 0 }; // Счётчик активных задач
    std::condition_variable completionCondition; // Уведомление для ожидания завершения
};