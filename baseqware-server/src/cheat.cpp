#include "cheat.hpp"

#include <thread>

#include <spdlog/spdlog.h>

#include "../../offsets/client.dll.hpp"
#include "../../offsets/offsets.hpp"
#include "driver_interface.hpp"
#include "sdk/entity.hpp"
#include "sdk/view_matrix.hpp"
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
  uintptr_t local_player_ctrl{0};
  bool is_alive{false};
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

  g_modules.client.read(client_dll::dwLocalPlayerPawn, g.local_player.addr);
  g_modules.client.read(
    client_dll::dwLocalPlayerController, g.local_player_ctrl
  );
  g_modules.client.read(client_dll::dwEntityList, g.entity_list.addr);
  if (!g.local_player_ctrl || !g.local_player || !g.entity_list) {
    return;
  }
  read(g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive);
  if (g.is_alive) {
    g_tick = main_tick;
    sg.defuse();
  }
}

std::optional<server::BoxData> to_box(
  const sdk::Vec3 &origin,
  const sdk::ViewMatrix &view_matrix,
  const std::pair<int, int> &screen_dim,
  bool crouching
) {
  constexpr float k_crouching_height = 54.F;
  constexpr float k_standing_height = 72.F;
  constexpr float k_player_width = 32.F;

  server::BoxData boxdata;
  sdk::Vec2 screen1;
  sdk::Vec2 screen2;
  bool on_screen = view_matrix.w2s(origin, screen_dim, screen1);
  if (!on_screen) {
    return boxdata;
  }
  auto player_height = crouching ? k_crouching_height : k_standing_height;
  auto _ = view_matrix.w2s(
    origin + sdk::Vec3{0.F, 0.F, player_height}, screen_dim, screen2 // NOLINT
  );

  auto screen_height = screen2.y - screen1.y;
  auto screen_width = screen_height * k_player_width / player_height;

  boxdata.x = static_cast<int>(screen2.x - screen_width / 2);
  boxdata.y = static_cast<int>(screen1.y);
  boxdata.w = static_cast<int>(screen_width);
  boxdata.h = static_cast<int>(screen_height);
  return boxdata;
}

void main_tick() {
  static util::SyncTimer timer{4ms};
  util::ScopeGuard sg([] { timer.wait(); });

  // TODO:
  RW(read(g.local_player_ctrl + CCSPlayerController::m_bPawnIsAlive, g.is_alive)
  );

  sdk::ViewMatrix view_matrix;
  RW(g_modules.client.read(client_dll::dwViewMatrix, view_matrix));

  bool rc = true;
  auto ent_list_entry = g.entity_list.get_entry(rc);
  if (!rc) {
    return;
  }

  auto boxes = []() -> auto {
    std::vector<server::BoxData> vec;
    vec.reserve(sdk::EntityListEntry::k_max_idx);
    return vec;
  }();
  boxes.clear();

  int local_team = g.local_player.m_iTeamNum(rc);

  for (int i = 0; i < sdk::EntityListEntry::k_max_idx; ++i) {
    rc = true;
    auto pc = ent_list_entry.at(i, rc);
    if (!pc) {
      continue;
    }
    if (!pc.m_bPawnIsAlive(rc)) {
      continue;
    }
    // spdlog::info("Player controller {}: 0x{:x}", i, pc.addr);
    auto name_addr = pc.m_sSanitizedPlayerName(rc);
    auto name = pc.get_name(rc);
    if (!rc) {
      continue;
    }
    // spdlog::info("Player: {}", name);

    // TODO: add a utility function in server.hpp
    std::pair<int, int> screen_dim = {
      server::g_screen_width, server::g_screen_height
    };
    if (screen_dim.first == 0 || screen_dim.second == 0) {
      continue;
    }

    auto entity = pc.get_pawn(g_modules.client, rc);
    if (!rc || entity.addr == g.local_player.addr) {
      continue;
    }
    auto game_scene_node = sdk::GameSceneNode(entity.m_pGameSceneNode(rc));
    if (!rc || !game_scene_node) {
      continue;
    }

    auto origin = game_scene_node.m_vecAbsOrigin(rc);
    // spdlog::info("Origin: ({}, {}, {})", origin.x, origin.y, origin.z);

    sdk::Vec2 screen;
    bool on_screen = view_matrix.w2s(origin, screen_dim, screen);
    if (!on_screen) {
      continue;
    }

    // spdlog::info("Screen: ({}, {})", screen.x, screen.y);
    auto box = to_box(origin, view_matrix, screen_dim, false);
    if (box) {
      if (entity.m_iTeamNum(rc) != local_team) {
        box->is_enemy = true;
      }
      boxes.push_back(*box);
    }
  }

  if (!boxes.empty()) {
    server::g_draw_data.boxes.write(std::vector(boxes));
  }
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
