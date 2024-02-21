#include "ws_server.hpp"

#include <chrono>
#include <string_view>

#include <spdlog/spdlog.h>

#include "server.hpp"

using namespace std::chrono_literals;
using ::server::g_screen_height;
using ::server::g_screen_width;
using ::server::g_stop_flag;

ws_server::ws_server() {
  m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
  m_endpoint.set_access_channels(websocketpp::log::alevel::access_core);
  m_endpoint.set_access_channels(websocketpp::log::alevel::app);

  m_endpoint.init_asio();

  using websocketpp::lib::bind;
  using websocketpp::lib::placeholders::_1;
  using websocketpp::lib::placeholders::_2;
  m_endpoint.set_open_handler(bind(&ws_server::on_open, this, _1));
  m_endpoint.set_close_handler(bind(&ws_server::on_close, this, _1));
  m_endpoint.set_message_handler(bind(&ws_server::on_message, this, _1, _2));
}

void ws_server::run(uint16_t port) {
  spdlog::info("Websocket server starting on port {}", port);

  m_endpoint.listen(port);
  m_endpoint.start_accept();
  this->set_timer();

  try {
    m_endpoint.run();
  } catch (const std::exception &e) {
    spdlog::error("Websocket server exception: {}", e.what());
  }
}

void ws_server::set_timer() {
  m_timer = m_endpoint.set_timer(
    (std::chrono::milliseconds(1s) / kSendRate).count(),
    websocketpp::lib::bind(
      &ws_server::on_timer, this, websocketpp::lib::placeholders::_1
    )
  );
}

void ws_server::on_timer(websocketpp::lib::error_code const &ec) {
  if (ec) {
    spdlog::error("Websocket server timer error: {}", ec.message());
    return;
  }

  if (g_stop_flag.load(std::memory_order_relaxed)) {
    this->close();
    return;
  }

  for (auto hdl : m_connections) { // NOLINT
    try {
      this->send_draw_data(hdl);
    } catch (const std::exception &e) { // NOLINT
      // spdlog::error("Websocket server send exception: {}", e.what());
      // FIXME: Uh I don't really care
    }
  }

  this->set_timer();
}

void ws_server::send_draw_data(connection_hdl hdl) { // NOLINT
  // m_endpoint.send(hdl, "ping", websocketpp::frame::opcode::text);
  ::server::g_draw_data.send_json_if_updated([this,
                                              hdl](const std::string &json) {
    m_endpoint.send(hdl, json, websocketpp::frame::opcode::text);
  });
}

void ws_server::on_message( // NOLINT
  connection_hdl hdl,       // NOLINT
  server::message_ptr msg   // NOLINT
) {
  // TODO: placeholder
  std::string_view payload{msg->get_payload()};
  if (!payload.empty() && payload[0] == '>') {
    // Handle command.
    if (payload == ">stop") {
      g_stop_flag.store(true, std::memory_order_relaxed);
      g_stop_flag.notify_all();
      return;
    }
    return;
  }

  // On connection, the client sends the width and the height of the screen.
  json j = json::parse(payload);
  g_screen_width.store(j.at("width"), std::memory_order_relaxed);
  g_screen_height.store(j.at("height"), std::memory_order_relaxed);
  spdlog::debug("Websocket server received message: {}", msg->get_payload());
}

void ws_server::close() {
  m_endpoint.stop_listening();
  for (auto hdl : m_connections) { // NOLINT
    m_endpoint.close(hdl, websocketpp::close::status::going_away, "");
  }
  m_connections.clear();
  m_endpoint.stop();
  g_screen_height.store(0, std::memory_order_relaxed);
  g_screen_width.store(0, std::memory_order_relaxed);
}

void ws_server::on_open(connection_hdl hdl) { // NOLINT
  m_connections.insert(hdl);
}

void ws_server::on_close(connection_hdl hdl) { // NOLINT
  m_connections.erase(hdl);
}
