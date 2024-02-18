#include "server.hpp"

#include <spdlog/spdlog.h>

#include "ws_server.hpp"

using namespace std::chrono_literals;

namespace {
// TODO: move this
constexpr uint16_t kPort = 8765;
} // namespace

namespace server {

void thread_main() { // NOLINT
  spdlog::info("Server thread started");
  ws_server wss;
  wss.run(kPort);
  spdlog::info("Server thread stopped");
}

void to_json(json &j, const BoxData &bd) {
  j = json{
    {"x", bd.x},
    {"y", bd.y},
    {"w", bd.w},
    {"h", bd.h},
    {"health", bd.health},
    {"is_enemy", bd.is_enemy}
  };
}

void from_json(const json &j, BoxData &bd) {
  j.at("x").get_to(bd.x);
  j.at("y").get_to(bd.y);
  j.at("w").get_to(bd.w);
  j.at("h").get_to(bd.h);
  j.at("health").get_to(bd.health);
  j.at("is_enemy").get_to(bd.is_enemy);
}

bool DrawData::send_json_if_updated(
  std::function<void(const std::string &)> send // NOLINT
) const {
  // If any of the draw data has been updated, send it to the client.

  bool updated = false;
  auto box_data = boxes.read(updated);
  // if (updated) { // TODO: uncomment
  send(json(box_data).dump());
  // }
  return updated;
}

} // namespace server
