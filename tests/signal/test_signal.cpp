#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/signal_handler.h"

int main()
{
    std::cout << "testing signal" << std::endl;

    spdlog::installCrashHandlerOnce();
    // spdlog::set_async_mode(256);
    // spdlog::init_thread_pool(8192, 1);
    auto file_logger = spdlog::basic_logger_mt<spdlog::async_factory>("file_logger", "./test_log.log");

    std::cout << "started loop..." << std::endl;
    int i = 0;
    while (true)
    {
        file_logger->info("Async message #{}, #{}, #{}, #{}, #{}", i, i + 1, i + 2, i + 3, i + 4);
        std::cout << "last i: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        i++;

        if (i == 50)
        {
            std::cout << "raising signal" << std::endl;
            // std::raise(SIGABRT);
            std::raise(SIGINT);
        }
    }
}