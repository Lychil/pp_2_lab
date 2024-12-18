#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex); // синхронизация потоков
        std::cout << "Создаётся пул потоков с количеством потоков: " << numThreads << std::endl;
    }

#if defined(_WIN32) || defined(_WIN64)
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    std::cout << "Поток " << std::this_thread::get_id() << " переводится в режим ожидания." << std::endl;
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                    if (this->stop) {
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex); // без блокироки сообщения могут смешаться
                            std::cout << "Поток " << std::this_thread::get_id() << " завершает работу." << std::endl;
                        }
                        return;
                    }

                    if (!this->tasks.empty()) {
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        ++activeTasks;
                    }
                }

                if (task) {
                    {
                        std::lock_guard<std::mutex> consoleLock(consoleMutex);
                        std::cout << "Поток " << std::this_thread::get_id() << " выполняет задачу." << std::endl;
                    }
                    task();
                    --activeTasks;
                    if (activeTasks == 0) {
                        completionCondition.notify_one();
                    }
                }
            }
            });
    }
#else
    for (size_t i = 0; i < numThreads; ++i) {
        // Для Linux создаём потоки через pthread_create
        pthread_t thread;
        pthread_create(&thread, nullptr, &ThreadPool::run, this);
        threads.push_back(thread);
    }
#endif
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();

#if defined(_WIN32) || defined(_WIN64)
    // Для Windows используем join для std::thread
    for (auto& thread : workers) {
        if (thread.joinable()) {
            thread.join();
        }
    }
#else
    // Для Linux используем pthread_join
    for (auto& thread : threads) {
        pthread_join(thread, nullptr);
    }
#endif

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Все потоки завершены, пул потоков уничтожен." << std::endl;
    }
}

void ThreadPool::waitForCompletion() {
    std::unique_lock<std::mutex> lock(queueMutex);
    completionCondition.wait(lock, [this] {
        return activeTasks == 0 && tasks.empty();
        });
}

#if defined(_WIN32) || defined(_WIN64)
unsigned __stdcall ThreadPool::run(void* param) {
    auto* pool = reinterpret_cast<ThreadPool*>(param);
    pool->processTasks();
    return 0;
}
#else
void* ThreadPool::run(void* param) {
    auto* pool = reinterpret_cast<ThreadPool*>(param);
    pool->processTasks();
    return nullptr;
}
#endif

void ThreadPool::processTasks() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(this->queueMutex);
            this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

            if (this->stop && this->tasks.empty()) {
                return;
            }

            if (!this->tasks.empty()) {
                task = std::move(this->tasks.front());
                this->tasks.pop();
                ++activeTasks;
            }
        }

        if (task) {
            task();
            --activeTasks;
            if (activeTasks == 0) {
                completionCondition.notify_one();
            }
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        tasks.emplace(std::move(task));
    }
    condition.notify_one();
}
