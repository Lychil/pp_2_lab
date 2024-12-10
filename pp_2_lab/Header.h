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
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ThreadPool(size_t numThreads) {
        {
            std::lock_guard<std::mutex> lock(consoleMutex); // синхронизация потоков
            std::cout << "Создаётся пул потоков с количеством потоков: " << numThreads << std::endl;
        }

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] { // создание рабочих потоков
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex); // блокировка потока
                        this->condition.wait(lock, [this] { // ожидание condition 
                            return this->stop || !this->tasks.empty(); // 2 условия выхода из ожидания
                            });

                        if (this->stop && this->tasks.empty()) { // завершение работы потока
                            {
                                std::lock_guard<std::mutex> consoleLock(consoleMutex); // без блокироки сообщения могут смешаться
                                std::cout << "Поток " << std::this_thread::get_id() << " завершает работу." << std::endl;
                            }
                            return;
                        }

                        if (!this->tasks.empty()) {
                            task = std::move(this->tasks.front()); // ссылка на 1 элемент
                            this->tasks.pop();
                            ++activeTasks; // +1 активная задача
                        }
                    }

                    if (task) {
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex);
                            std::cout << "Поток " << std::this_thread::get_id() << " выполняет задачу." << std::endl;
                        }
                        task(); // вызывается лямбда-функция

                        --activeTasks; // -1 активная задача

                        // Увед о том, что задачи завершены
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
            std::unique_lock<std::mutex> lock(queueMutex); // блок мьютекса queue - мьютекс очереди задач, unique_lock блокает данный мьютекс
            stop = true; // флаг блокировки
        }
        condition.notify_all(); // увед всех потоков о завершении, потоки могут чекнуть stop


        // важен вызов для каждого, чтобы избежать ошибок
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join(); // блокает текущий поток, пока worker не завершится
            }
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex); // снова блокаем консольный мьютекс, не нужен юник, его возможности излишни
            std::cout << "Все потоки завершены, пул потоков уничтожен." << std::endl;
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queueMutex); // блокаем, чтобы не допустить состояние гонки
            tasks.emplace(std::forward<F>(f)); // добавление задачи
        }
        condition.notify_one(); // увед о появлении задачи
    }

    void waitForCompletion() { // новый метод для ожидания завершения задач
        std::unique_lock<std::mutex> lock(queueMutex); // блокирока до определенного момента
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
    std::atomic<int> activeTasks{ 0 }; // Счётчик активных задач
    std::condition_variable completionCondition; // Уведомление для ожидания завершения
};