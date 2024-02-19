#include "cheat.hpp"

#include <thread>

#include <spdlog/spdlog.h>

#include "../../offsets/client.dll.hpp"
#include "../../offsets/offsets.hpp"
#include "driver_interface.hpp"
#include "server.hpp"

using namespace std::chrono_literals;
using driver_interface::ModuleInfo;

namespace {
void (*g_tick)();
union {
  ModuleInfo arr[driver_interface::PASED_MODULE_COUNT_];
  struct {
    ModuleInfo client;
  };
} g_modules{};
const char *k_module_names[] = {
  "client.dll",
};

struct {
  /// Local player pawn
  uintptr_t local_player = 0;
  /// Local player controller
  uintptr_t local_player_ctrl = 0;
  bool is_alive;
} g;

void load_modules();
void get_globals();
void main_tick();

void load_modules() {
  bool found_missing = false;
  for (int i = 0; i < driver_interface::PASED_MODULE_COUNT_; ++i) {
    if (g_modules.arr[i].base_address != 0) {
      continue;
    }
    auto [success, info] = driver_interface::get_module_info(
      static_cast<driver_interface::ModuleRequest>(i)
    );

    if (success) {
      spdlog::info(
        "Found module: {} at 0x{:x}", k_module_names[i], info.base_address
      );
      g_modules.arr[i] = info;
    } else {
      found_missing = true;
    }
  }
  if (!found_missing) {
    g_tick = get_globals;
  }
  std::this_thread::sleep_for(1s);
}

void get_globals() {
  [] {
    g_modules.client.read(client_dll::dwLocalPlayerPawn, g.local_player);
    g_modules.client.read(
      client_dll::dwLocalPlayerController, g.local_player_ctrl
    );
    if (!bool(g.local_player_ctrl) || !bool(g.local_player)) {
      return;
    }
    driver_interface::read(
      g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive
    );
    spdlog::info("Alive: {}", g.is_alive);
    if (g.is_alive) {
      g_tick = main_tick;
    }
  }();
  std::this_thread::sleep_for(1s);
}

void main_tick() {
  spdlog::info("test");

  driver_interface::read(
    g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive
  );
  if (!g.is_alive) {
    g_tick = get_globals;
  }
  std::this_thread::sleep_for(1s);
}
} // namespace

namespace cheat {

void thread_main() {
  if (!driver_interface::initialize()) {
    return;
  }
  bool game_running = false;

  g_tick = load_modules; // weird state machine
  spdlog::info("Waiting for modules...");
  // TODO: add timer
  while (!server::g_stop_flag.load(std::memory_order_acquire)) {
    if (driver_interface::is_game_running()) {
      game_running = true;
      g_tick();
    } else if (game_running) {
      spdlog::info("Game stopped, exiting");
      server::g_stop_flag.store(true, std::memory_order_acquire);
      server::g_stop_flag.notify_all();
      break;
    }
  }
  driver_interface::finalize();
}

} // namespace cheat
