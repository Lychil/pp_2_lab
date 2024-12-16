#include "Header.h"

std::mutex consoleMutex;

ThreadPool::ThreadPool(size_t numThreads) {
    {
        std::lock_guard<std::mutex> lock(consoleMutex); // ������������� �������
        std::cout << "�������� ��� ������� � ����������� �������: " << numThreads << std::endl;
    }

    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i] { // �������� ������� �������
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex); // ���������� ������
                    std::cout << "����� " << std::this_thread::get_id() << " ����������� � ����� ��������." << std::endl;
                    this->condition.wait(lock, [this] { // �������� condition 
                        //std::cout << std::this_thread::get_id() << "\n";
                        return this->stop || !this->tasks.empty(); // 2 ������� ������ �� ��������
                        });

                    if (this->stop) { // ���������� ������ ������
                        {
                            std::lock_guard<std::mutex> consoleLock(consoleMutex); // ��� ��������� ��������� ����� ���������
                            std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
                        }
                        return;
                    }

                    if (!this->tasks.empty()) {
                        task = std::move(this->tasks.front()); // ������ �� 1 �������
                        this->tasks.pop();
                        ++activeTasks; // +1 �������� ������
                    }
                }

                if (task) {
                    {
                        std::lock_guard<std::mutex> consoleLock(consoleMutex);
                        std::cout << "����� " << std::this_thread::get_id() << " ��������� ������." << std::endl;
                    }
                    task(); // ���������� ������-�������

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


ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex); // ���� �������� queue - ������� ������� �����, unique_lock ������� ������ �������
        stop = true; // ���� ����������
    }
    condition.notify_all(); // ���� ���� ������� � ����������, ������ ����� ������� stop


    // ����� ����� ��� �������, ����� ��������� ���������� ���� �������
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(consoleMutex); // ����� ������� ���������� �������, �� ����� ����, ��� ����������� �������
        std::cout << "��� ������ ���������, ��� ������� ���������." << std::endl;
    }
}


void ThreadPool::waitForCompletion() { // ����� ����� ��� �������� ���������� �����
    std::unique_lock<std::mutex> lock(queueMutex); // ��������� �� ������������� �������
    completionCondition.wait(lock, [this] {
        return activeTasks == 0 && tasks.empty();
     });
}


