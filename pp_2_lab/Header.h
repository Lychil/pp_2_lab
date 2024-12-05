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

std::mutex consoleMutex;

class ThreadPool {
public:
    ThreadPool(size_t numThreads) {
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "�������� ��� ������� � ����������� �������: " << numThreads << std::endl;
        }

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                            });

                        if (this->stop && this->tasks.empty()) {
                            {
                                std::lock_guard<std::mutex> consoleLock(consoleMutex);
                                std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
                            }
                            return;
                        }

                        if (!this->tasks.empty()) {
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                            ++activeTasks; // +1 �������� ������
                        }
                    }

                    if (task) {
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex);
                            std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
                        }
                        task();

                        --activeTasks; // -1 �������� ������

                        // ���� � ���, ��� ������ ���������
                        if (activeTasks == 0) {
                            completionCondition.notify_one();
                        }
                    }
                }
                });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "��� ������ ���������, ��� ������� ���������." << std::endl;
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    void waitForCompletion() { // ����� ����� ��� �������� ���������� �����
        std::unique_lock<std::mutex> lock(queueMutex);
        completionCondition.wait(lock, [this] {
            return activeTasks == 0 && tasks.empty();
            });
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{ false };
    std::atomic<int> activeTasks{ 0 }; // ������� �������� �����
    std::condition_variable completionCondition; // ����������� ��� �������� ����������
};