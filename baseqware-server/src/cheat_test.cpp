#include "cheat_test.hpp"

#include <thread>

#include <nlohmann/json.hpp>

#include "server.hpp"

using namespace std::chrono_literals;
using namespace nlohmann::json_literals;

namespace cheat {

void test::thread_main() {
  std::vector<server::BoxData> boxes(1);
  boxes[0] =
    "{\"x\": 200, \"y\": 200, \"w\": 500, \"h\": 500, \"health\": 5, \"is_enemy\": true}"_json
      .template get<server::BoxData>();
  while (!server::g_stop_flag.load(std::memory_order_relaxed)) {
    server::g_draw_data.boxes.write(std::vector(boxes));
    std::this_thread::sleep_for(4ms);
  }
}
} // namespace cheat
