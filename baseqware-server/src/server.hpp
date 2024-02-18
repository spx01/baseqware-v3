#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp> // IWYU pragma: keep

#include "tbuffer.hpp"

using json = nlohmann::json;

namespace server {

inline std::atomic<bool> g_stop_flag{false};

struct BoxData {
  int x, y, w, h;
  int health;
  bool is_enemy;
  BoxData() { memset(this, 0, sizeof(*this)); }
};

void to_json(json &j, const BoxData &bd);
void from_json(const json &j, BoxData &bd);

/// Draw data sent to the client.
inline struct DrawData {
  // Each TBuffer member should be written to by a single thread at a time.
  TBuffer<std::vector<BoxData>> boxes;

  /// Sends the draw data to the client if it has been updated. Returns true if
  /// the data was sent.
  bool send_json_if_updated(std::function<void(const std::string &)> send
  ) const;
} g_draw_data;

void thread_main();

} // namespace server
