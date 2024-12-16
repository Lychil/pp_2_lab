#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex); // синхронизаци€ потоков
        std::cout << "—оздаЄтс€ пул потоков с количеством потоков: " << numThreads << std::endl;
    }

    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i] { // создание рабочих потоков
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex); // блокировка потока
                    std::cout << "ѕоток " << std::this_thread::get_id() << " переводитс€ в режим ожидани€." << std::endl;
                    this->condition.wait(lock, [this] { // ожидание condition 
                        //std::cout << std::this_thread::get_id() << "\n";
                        return this->stop || !this->tasks.empty(); // 2 услови€ выхода из ожидани€
                        });

                    if (this->stop) { // завершение работы потока
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex); // без блокироки сообщени€ могут смешатьс€
                            std::cout << "ѕоток " << std::this_thread::get_id() << " завершает работу." << std::endl;
                        }
                        return;
                    }

                    if (!this->tasks.empty()) {
                        task = std::move(this->tasks.front()); // ссылка на 1 элемент
                        this->tasks.pop();
                        ++activeTasks; // +1 активна€ задача
                    }
                }

                if (task) {
                    {
                        std::lock_guard<std::mutex> consoleLock(consoleMutex);
                        std::cout << "ѕоток " << std::this_thread::get_id() << " выполн€ет задачу." << std::endl;
                    }
                    task(); // вызываетс€ л€мбда-функци€

                    --activeTasks; // -1 активна€ задача

                    // ”вед о том, что задачи завершены
                    if (activeTasks == 0) {
                        completionCondition.notify_one();
                    }
                }
            }
            });

    }
}


ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex); // блок мьютекса queue - мьютекс очереди задач, unique_lock блокает данный мьютекс
        stop = true; // флаг блокировки
    }
    condition.notify_all(); // увед всех потоков о завершении, потоки могут чекнуть stop


    // важен вызов дл€ каждого, чтобы дождатьс€ завершени€ всех потоков
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(consoleMutex); // снова блокаем консольный мьютекс, не нужен юник, его возможности излишни
        std::cout << "¬се потоки завершены, пул потоков уничтожен." << std::endl;
    }
}


void ThreadPool::waitForCompletion() { // новый метод дл€ ожидани€ завершени€ задач
    std::unique_lock<std::mutex> lock(queueMutex); // блокирока до определенного момента
    completionCondition.wait(lock, [this] {
        return activeTasks == 0 && tasks.empty();
     });
}


