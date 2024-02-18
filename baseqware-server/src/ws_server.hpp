#pragma once

/// The websocket server, periodically sends data to the client.
/// https://github.com/zaphoyd/websocketpp/blob/master/examples/telemetry_server/telemetry_server.cpp

#include <cstdint>
#include <memory>
#include <set>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

// TODO: move this
/// Websocket send rate in Hz.
static constexpr int kSendRate =
// FIXME:
#ifdef DEBUG
  10;
#else
  60;
#endif

class ws_server {
public:
  using connection_hdl = websocketpp::connection_hdl;
  using server = websocketpp::server<websocketpp::config::asio>;

  ws_server();
  void run(uint16_t port);

private:
  // TODO: Turn this into a map, we need need data about each client.
  using con_list = std::set<connection_hdl, std::owner_less<connection_hdl>>;

  server m_endpoint;
  con_list m_connections;
  server::timer_ptr m_timer;
  /// Whether or not the server should send any render data.
  bool m_paused = false;

  void set_timer();
  void on_timer(websocketpp::lib::error_code const &);
  void on_open(connection_hdl hdl);
  void on_close(connection_hdl hdl);
  void on_message(connection_hdl hdl, server::message_ptr msg);
  void send_draw_data(connection_hdl hdl);
  void close();
};
