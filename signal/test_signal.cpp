#include <iostream>
#include <thread>
#include <chrono>

#include "spdlog/spdlog.h"

int main()
{
    std::cout << "testing signal" << std::endl;

    // size_t q_size = 4096; //queue size must be power of 2
    // spdlog::set_async_mode(q_size);
    // auto async_file = spdlog::daily_logger_st("async_file_logger", "./async_log.log");

    spdlog::set_async_mode(8192);
    auto file_logger = spdlog::rotating_logger_mt("file_logger", "./test_log", 1048576 * 5, 1);

    int i = 0;
    while(true)
    {
        file_logger->info("Async message #{}, #{}, #{}, #{}, #{}", i, i+1, i+2, i+3, i+4);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i++;
    }

}