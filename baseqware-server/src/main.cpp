#include <cstdio>
#include <thread>

#include <spdlog/spdlog.h>

#include "cheat.hpp"
#include "server.hpp"

using namespace std::chrono_literals;

void init_log() {
  spdlog::set_pattern("[%H:%M:%S] [%^%L%$] [tid %t] %v");
#ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
#endif
}

void run() {
  spdlog::info("Main thread");
  std::thread server_thread{server::thread_main};
  std::thread cheat_thread{cheat::thread_main};
  auto _ = std::getchar();
  server::g_stop_flag.store(true, std::memory_order_acquire);
  server::g_stop_flag.notify_all();
  server_thread.join();
  cheat_thread.join();
}

int main(int /*argc*/, char ** /*argv*/) {
  init_log();
  run();
  return 0;
}
