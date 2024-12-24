#include "Header.h"

int main() {

    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 1;
    }

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "The program demonstrates the operation of a thread pool.\n"
            << "Number of threads:" << numThreads << "\n"
            << "Each task outputs a thread ID and its own number.\n"
            << "Launch..." << std::endl;
    }

    ThreadPool pool(numThreads);

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i] {
            std::stringstream ss;
            ss << "Task" << i << " processed by a thread " << std::this_thread::get_id();
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << ss.str() << std::endl;
            }

            // Имитация нагрузки, расчёт значений синуса
            double result = 0.0;
            for (int j = 0; j < 88888888; ++j) {
                result += std::sin(j * 0.001);
            }
            });
    }

    // Ожидание завершения всех задач
    pool.waitForCompletion();

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "The program is completed. All tasks are completed." << std::endl;
    }

    return 0;
}
