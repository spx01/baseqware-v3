#include "cheat.hpp"

#include <thread>

#include <spdlog/spdlog.h>

#include "../../offsets/client.dll.hpp"
#include "../../offsets/offsets.hpp"
#include "driver_interface.hpp"
#include "sdk/entity.hpp"
#include "server.hpp"
#include "util.hpp"

using namespace std::chrono_literals;

using driver_interface::ModuleInfo;
using driver_interface::read;

namespace {
void (*g_tick)();
union {
  ModuleInfo arr[driver_interface::PASED_MODULE_COUNT_];
  struct {
    ModuleInfo client;
    ModuleInfo engine;
  };
} g_modules{};
const char *k_module_names[driver_interface::PASED_MODULE_COUNT_] = {
  "client.dll",
  "engine2.dll",
};

struct {
  /// Local player pawn
  sdk::BaseEntity local_player{0};
  /// Local player controller
  uintptr_t local_player_ctrl = 0;
  bool is_alive = false;
  sdk::EntityList entity_list{0};
} g;

void load_modules();
void get_globals();
void main_tick();
void stop() {
  server::g_stop_flag.store(true, std::memory_order_acquire);
  server::g_stop_flag.notify_all();
}

/// (E)lse (S)witch state to next
#define ES(expr, next) \
  {                    \
    if (!(expr)) {     \
      g_tick = (next); \
      return;          \
    }                  \
  }

/// (R)ead (W)rap
#define RW(expr) ES(expr, get_globals)

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
    uint64_t build_number = 0;
    if (!g_modules.engine.read(engine2_dll::dwBuildNumber, build_number)) {
      spdlog::error("Failed to read build number");
      g_tick = stop;
      return;
    }
    spdlog::info("Build number: {:x}", build_number);
    if (build_number != game_info::buildNumber) {
      spdlog::error("Wrong build number! Did the game update?");
      g_tick = stop;
      return;
    }
    g_tick = get_globals;
  }
  std::this_thread::sleep_for(1s);
}

void get_globals() {
  util::ScopeGuard sg([] { std::this_thread::sleep_for(1s); });

  g_modules.client.read(client_dll::dwLocalPlayerPawn, g.local_player);
  g_modules.client.read(
    client_dll::dwLocalPlayerController, g.local_player_ctrl
  );
  g_modules.client.read(client_dll::dwEntityList, g.entity_list);
  if (!g.local_player_ctrl || !g.local_player || !g.entity_list) {
    return;
  }
  spdlog::info("entity list: 0x{:x}", g.entity_list.addr);
  read(g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive);
  if (g.is_alive) {
    g_tick = main_tick;
    sg.defuse();
  }
}

void main_tick() {
  static util::SyncTimer timer{1s};
  util::ScopeGuard sg([] { timer.wait(); });

  RW(read(g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive)
  );
  ES(g.is_alive, get_globals);
}
} // namespace

namespace cheat {

void thread_main() {
  if (!driver_interface::initialize()) {
    return;
  }
  bool game_running = false;

  g_tick = load_modules; // state machine
  spdlog::info("Waiting for modules...");
  while (!server::g_stop_flag.load(std::memory_order_acquire)) {
    if (driver_interface::is_game_running()) {
      game_running = true;
      g_tick();
    } else if (game_running) {
      spdlog::info("Game stopped, exiting");
      stop();
      break;
    } else {
      std::this_thread::sleep_for(1s);
    }
  }
  driver_interface::finalize();
}

} // namespace cheat
