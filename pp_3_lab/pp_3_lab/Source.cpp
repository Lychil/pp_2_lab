#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex); // ������������� �������
        std::cout << "�������� ��� ������� � ����������� �������: " << numThreads << std::endl;
    }

#if defined(_WIN32) || defined(_WIN64)
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    std::cout << "����� " << std::this_thread::get_id() << " ����������� � ����� ��������." << std::endl;
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                    if (this->stop) {
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex); // ��� ��������� ��������� ����� ���������
                            std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
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
                        std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
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
        // ��� Linux ������ ������ ����� pthread_create
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
    // ��� Windows ���������� join ��� std::thread
    for (auto& thread : workers) {
        if (thread.joinable()) {
            thread.join();
        }
    }
#else
    // ��� Linux ���������� pthread_join
    for (auto& thread : threads) {
        pthread_join(thread, nullptr);
    }
#endif

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "��� ������ ���������, ��� ������� ���������." << std::endl;
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
