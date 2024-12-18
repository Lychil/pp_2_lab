#include "Header.h"

int main() {
    setlocale(LC_ALL, "ru");

    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 1;
    }

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Программа демонстрирует работу пула потоков.\n"
            << "Количество потоков: " << numThreads << "\n"
            << "Каждая задача выводит идентификатор потока и свой номер.\n"
            << "Запуск..." << std::endl;
    }

    ThreadPool pool(numThreads);

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i] {
            std::stringstream ss;
            ss << "Задача " << i << " обрабатывается потоком " << std::this_thread::get_id();
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
        std::cout << "Программа завершена. Все задачи выполнены." << std::endl;
    }

    return 0;
}
