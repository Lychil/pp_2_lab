#include "Header.h"


int main() {
    setlocale(LC_ALL, "ru");

    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 1;
    }

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "��������� ������������� ������ ���� �������.\n"
            << "���������� �������: " << numThreads << "\n"
            << "������ ������ ������� ������������� ������ � ���� �����.\n"
            << "������..." << std::endl;
    }

    ThreadPool pool(numThreads);

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i] {
            std::stringstream ss;
            ss << "������ " << i << " �������������� ������� " << std::this_thread::get_id();
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << ss.str() << std::endl;
            }

            // �������� ��������, ������ �������� ������ ������ ������
            double result = 0.0;
            for (int j = 0; j < 88888888; ++j) {
                result += std::sin(j * 0.001);
            }
            });
    }

    // �������� ���������� ���� �����
    pool.waitForCompletion();

    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "��������� ���������. ��� ������ ���������." << std::endl;
    }

    return 0;
}