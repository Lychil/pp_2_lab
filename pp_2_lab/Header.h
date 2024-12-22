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
            std::lock_guard<std::mutex> lock(queueMutex); // �������, ����� �� ��������� ��������� �����
            tasks.emplace(std::forward<F>(f)); // ���������� ������
        }
        condition.notify_one(); // ���� � ��������� ������
    }

    void waitForCompletion();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{ false };
    std::atomic<int> activeTasks{ 0 }; // ������� �������� �����
    std::condition_variable completionCondition; // ����������� ��� �������� ����������
};