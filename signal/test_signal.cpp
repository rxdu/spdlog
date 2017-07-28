#include <iostream>
#include <thread>
#include <chrono>

#include "spdlog/spdlog.h"

int main()
{
    std::cout << "testing signal" << std::endl;

    spdlog::set_async_mode(256);
    auto file_logger = spdlog::rotating_logger_mt("file_logger", "./test_log.log", 1048576 * 5, 1);

    std::cout << "started loop..." << std::endl;
    int i = 0;
    while(true)
    {
        file_logger->info("Async message #{}, #{}, #{}, #{}, #{}", i, i+1, i+2, i+3, i+4);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i++;

        // if(i == 50)
        // {
        //     std::cout << "raising signal" << std::endl;
        //     //std::raise(SIGABRT);
        //     std::raise(SIGINT);
        // }
    }

}