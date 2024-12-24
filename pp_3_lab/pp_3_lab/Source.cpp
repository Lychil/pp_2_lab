#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "A thread pool is created with the number of threads:" << numThreads << std::endl;
    }

#if defined(_WIN32) || defined(_WIN64)
    taskSemaphore = CreateSemaphore(nullptr, 0, INT_MAX, nullptr);
    if (!taskSemaphore) {
        DWORD error = GetLastError();
        throw std::runtime_error("Failed to create taskSemaphore. Error: " + std::to_string(error));
    }

    completionSemaphore = CreateSemaphore(nullptr, 0, INT_MAX, nullptr);
    if (!completionSemaphore) {
        DWORD error = GetLastError();
        throw std::runtime_error("Failed to create completionSemaphore. Error: " + std::to_string(error));
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
            DWORD error = GetLastError();
            throw std::runtime_error("Failed to create a thread. Error: " + std::to_string(error));
        }
    }
#elif defined(__linux__)
    if (sem_init(&taskSemaphore, 0, 0) != 0) {
        int error = errno;
        throw std::runtime_error("Failed to initialize taskSemaphore. Error: " + std::string(strerror(error)));
    }

    if (sem_init(&completionSemaphore, 0, 0) != 0) {
        int error = errno;
        throw std::runtime_error("Failed to initialize completionSemaphore. Error: " + std::string(strerror(error)));
        }

    for (size_t i = 0; i < numThreads; ++i) {
        pthread_t thread;
        if (pthread_create(&thread, nullptr, [](void* param) -> void* {
            static_cast<ThreadPool*>(param)->workerLoop();
            return nullptr;
        }, this) != 0) {
            int error = errno;
            throw std::runtime_error("Failed to create a thread. Error: " + std::string(strerror(error)));
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
    // Уведомляем все потоки о завершении
    for (size_t i = 0; i < workers.size(); ++i) {
        ReleaseSemaphore(taskSemaphore, 1, nullptr);
    }

    // Для Windows ждём завершения всех потоков
    for (auto& threadHandle : workers) {
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
    }

    CloseHandle(taskSemaphore);
    CloseHandle(completionSemaphore);
#elif defined(__linux__)
    for (size_t i = 0; i < threads.size(); ++i) {
        sem_post(&taskSemaphore); // Уведомляем поток через семафор
    }

    for (auto& thread : threads) {
        pthread_join(thread, nullptr); // Ожидаем завершения потока
    }

    // Уничтожаем семафоры
    sem_destroy(&taskSemaphore);
    sem_destroy(&completionSemaphore);
#endif

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "All threads are completed, the thread pool is destroyed." << std::endl;
    }
}


void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        tasks.emplace(std::move(task));
    }

#if defined(_WIN32) || defined(_WIN64)
    ReleaseSemaphore(taskSemaphore, 1, nullptr);
#elif defined(__linux__)
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
#elif defined(__linux__)
        sem_wait(&completionSemaphore);
#endif
    }
}

void ThreadPool::workerLoop() {
    while (true) {
#if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(taskSemaphore, INFINITE);
#elif defined(__linux__)
        sem_wait(&taskSemaphore);
#endif

        if (stop) {
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Stream" << std::this_thread::get_id() << " completes the work." << std::endl;
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
                std::cout << "Stream " << std::this_thread::get_id() << " performs the task." << std::endl;
            }
            task();
            --activeTasks;

#if defined(_WIN32) || defined(_WIN64)
            ReleaseSemaphore(completionSemaphore, 1, nullptr);
#elif defined(__linux__)
            sem_post(&completionSemaphore);
#endif
        }
    }
}