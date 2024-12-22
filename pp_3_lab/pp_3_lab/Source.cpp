#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "�������� ��� ������� � ����������� �������: " << numThreads << std::endl;
    }

#if defined(_WIN32) || defined(_WIN64)
    taskSemaphore = CreateSemaphore(nullptr, 0, INT_MAX, nullptr);
    completionSemaphore = CreateSemaphore(nullptr, 0, INT_MAX, nullptr);

    if (!taskSemaphore || !completionSemaphore) {
        throw std::runtime_error("�� ������� ������� ��������.");
    }

    for (size_t i = 0; i < numThreads; ++i) {
        HANDLE threadHandle = (HANDLE)_beginthreadex(
            nullptr, 0, [](void* param) -> unsigned
            {
                static_cast<ThreadPool*>(param)->workerLoop();
                return 0; },
            this, 0, nullptr);

        if (threadHandle) {
            workers.push_back(threadHandle);
        }
        else {
            throw std::runtime_error("�� ������� ������� �����.");
        }
    }
#else
    sem_init(&taskSemaphore, 0, 0);
    sem_init(&completionSemaphore, 0, 0);

    for (size_t i = 0; i < numThreads; ++i) {
        pthread_t thread;
        if (pthread_create(&thread, nullptr, [](void* param) -> void* {
            static_cast<ThreadPool*>(param)->workerLoop();
            return nullptr;
            }, this) != 0) {
            throw std::runtime_error("�� ������� ������� �����.");
        }
        threads.push_back(thread);
    }
#endif
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }

#if defined(_WIN32) || defined(_WIN64)
    // ���������� ��� ������ � ����������
    for (size_t i = 0; i < workers.size(); ++i) {
        ReleaseSemaphore(taskSemaphore, 1, nullptr);
    }

    // ��� Windows ��� ���������� ���� �������
    for (auto& threadHandle : workers) {
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
    }

    CloseHandle(taskSemaphore);
    CloseHandle(completionSemaphore);
#else
    for (size_t i = 0; i < threads.size(); ++i) {
        sem_post(&taskSemaphore); // ���������� ����� ����� �������
    }

    for (auto& thread : threads) {
        pthread_join(thread, nullptr); // ������� ���������� ������
    }

    // ���������� ��������
    sem_destroy(&taskSemaphore);
    sem_destroy(&completionSemaphore);
#endif

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "��� ������ ���������, ��� ������� ���������." << std::endl;
    }
}


void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        tasks.emplace(std::move(task));
    }

#if defined(_WIN32) || defined(_WIN64)
    ReleaseSemaphore(taskSemaphore, 1, nullptr);
#else
    sem_post(&taskSemaphore);
#endif
}

void ThreadPool::waitForCompletion() {
    while (true) {
        if (activeTasks == 0 && tasks.empty()) {
            break;
        }

#if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(completionSemaphore, INFINITE);
#else
        sem_wait(&completionSemaphore);
#endif
    }
}

void ThreadPool::workerLoop() {
    while (true) {
#if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(taskSemaphore, INFINITE);
#else
        sem_wait(&taskSemaphore);
#endif

        if (stop) {
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
            }
            return;
        }

        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!tasks.empty()) {
                task = std::move(tasks.front());
                tasks.pop();
                ++activeTasks;
            }
        }

        if (task) {
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
            }
            task();
            --activeTasks;

#if defined(_WIN32) || defined(_WIN64)
            ReleaseSemaphore(completionSemaphore, 1, nullptr);
#else
            sem_post(&completionSemaphore);
#endif
        }
    }
}