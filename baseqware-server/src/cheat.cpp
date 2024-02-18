#include "cheat.hpp"

#include <thread>

#include "driver_interface.hpp"
#include "server.hpp"

using namespace std::chrono_literals;

namespace cheat {

void thread_main() {
  if (!driver_interface::initialize()) {
    return;
  }
  while (!server::g_stop_flag.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(1s);
  }
  driver_interface::finalize();
}

} // namespace cheat
