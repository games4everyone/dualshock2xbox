#include "test_mode.hpp"

#include "names.hpp"
#include "test_page.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
#define sock_close closesocket
#define INVALID_SOCK INVALID_SOCKET
static int sock_err() { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
#define sock_close ::close
#define INVALID_SOCK (-1)
[[maybe_unused]] static int sock_err() { return errno; }
#endif

namespace ds2x {

// default keyboard layout. stick keys set the stick to full deflection in that
// direction (diagonals combine) ; trigger keys pull the analog trigger fully.
static const std::map<std::string, std::string> kKeyMap = {
    {"ArrowUp", "dpad_up"}, {"ArrowDown", "dpad_down"},
    {"ArrowLeft", "dpad_left"}, {"ArrowRight", "dpad_right"},
    {"w", "lstick_up"}, {"s", "lstick_down"}, {"a", "lstick_left"}, {"d", "lstick_right"},
    {"i", "rstick_up"}, {"k", "rstick_down"}, {"j", "rstick_left"}, {"l", "rstick_right"},
    {"1", "square"}, {"2", "cross"}, {"3", "circle"}, {"4", "triangle"},
    {"q", "l1"}, {"e", "r1"}, {"z", "l2"}, {"x", "r2"},
    {",", "create"}, {".", "options"}, {"p", "ps"}, {"t", "touchpad"},
    {"0", "l3"}, {"9", "r3"}, {" ", "cross"},
};

const std::map<std::string, std::string>& default_key_map() { return kKeyMap; }

PadState build_pad_state(const KeyState& keys) {
    PadState s{Device::DualSense};
    int lx = 128, ly = 128, rx = 128, ry = 128;
    for (const auto& [key, effect] : kKeyMap) {
        if (!keys.count(key) || !keys.at(key)) continue;
        if (effect == "dpad_up") s.dpad = (s.dpad == 8) ? 0 : s.dpad;
        else if (effect == "dpad_down") s.dpad = (s.dpad == 8) ? 4 : s.dpad;
        else if (effect == "dpad_left") s.dpad = (s.dpad == 8) ? 6 : s.dpad;
        else if (effect == "dpad_right") s.dpad = (s.dpad == 8) ? 2 : s.dpad;
        else if (effect == "lstick_up") ly = 0;
        else if (effect == "lstick_down") ly = 255;
        else if (effect == "lstick_left") lx = 0;
        else if (effect == "lstick_right") lx = 255;
        else if (effect == "rstick_up") ry = 0;
        else if (effect == "rstick_down") ry = 255;
        else if (effect == "rstick_left") rx = 0;
        else if (effect == "rstick_right") rx = 255;
        else if (effect == "l2") { s.buttons |= L2; s.l2 = 255; }
        else if (effect == "r2") { s.buttons |= R2; s.r2 = 255; }
        else {
            int i = ps_button_index(effect);
            if (i >= 0) s.buttons |= static_cast<std::uint16_t>(1u << i);
        }
    }
    s.lx = static_cast<std::uint8_t>(lx);
    s.ly = static_cast<std::uint8_t>(ly);
    s.rx = static_cast<std::uint8_t>(rx);
    s.ry = static_cast<std::uint8_t>(ry);
    return s;
}

// --- tiny http server --------------------------------------------------------

TestServer::~TestServer() { stop(); }

bool TestServer::start(int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
    listen_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_fd_ < 0) return false;
    int one = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 0.0.0.0 so previews can reach it
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listen_fd_, 8) != 0) {
        sock_close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    return true;
}

void TestServer::stop() {
    if (listen_fd_ >= 0) { sock_close(listen_fd_); listen_fd_ = -1; }
#ifdef _WIN32
    WSACleanup();
#endif
}

static std::string url_decode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
            out += static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

static std::string query_param(const std::string& path, const std::string& name) {
    auto q = path.find('?');
    if (q == std::string::npos) return "";
    std::string qs = path.substr(q + 1);
    std::size_t pos = 0;
    while (pos <= qs.size()) {
        auto amp = qs.find('&', pos);
        auto kv = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        auto eq = kv.find('=');
        if (eq != std::string::npos && kv.substr(0, eq) == name)
            return url_decode(kv.substr(eq + 1));
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

std::string TestServer::handle(const std::string& method, const std::string& path) {
    if (path == "/" || path == "/index.html")
        return std::string("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ") +
               std::to_string(std::strlen(kTestPage)) + "\r\n\r\n" + kTestPage;
    if (path.rfind("/key", 0) == 0 && method == "POST") {
        std::string k = query_param(path, "k");
        std::string down = query_param(path, "down");
        if (!k.empty()) keys_[k] = (down == "1");
        return "HTTP/1.1 204 No Content\r\n\r\n";
    }
    if (path.rfind("/last", 0) == 0) {
        char body[256];
        std::snprintf(body, sizeof(body),
                      "{\"buttons\":%u,\"lt\":%.3f,\"rt\":%.3f,\"lx\":%.3f,\"ly\":%.3f,"
                      "\"rx\":%.3f,\"ry\":%.3f}",
                      last_.buttons, last_.lt, last_.rt, last_.lx, last_.ly, last_.rx, last_.ry);
        return std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
               std::to_string(std::strlen(body)) + "\r\n\r\n" + body;
    }
    if (path.rfind("/map", 0) == 0) {
        // the keyboard->ps table, plus the ps->xbox result, as json for the GUI table
        std::string body = "{";
        bool first = true;
        for (const auto& [k, fx] : kKeyMap) {
            if (!first) body += ",";
            first = false;
            Remap r = default_remap();
            int pi = ps_button_index(fx);
            std::uint16_t xb = 0;
            if (pi >= 0) xb = r[pi];
            else if (fx.rfind("dpad", 0) == 0) xb = xbox_button_bit(fx);
            char buf[160];
            std::snprintf(buf, sizeof(buf), "\"%s\":{\"ps\":\"%s\",\"xb\":\"%s\"}",
                          k.c_str(), fx.c_str(),
                          xb ? ("xbox#" + std::to_string(xb)).c_str() : "(stick)");
            body += buf;
        }
        body += "}";
        return std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }
    return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
}

void TestServer::poll() {
    if (listen_fd_ < 0) return;
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    sock_t fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client), &len);
    if (fd < 0 || fd == INVALID_SOCK) return;  // nothing waiting, fine
    char buf[2048] = {};
    int n = static_cast<int>(recv(fd, buf, sizeof(buf) - 1, 0));
    if (n > 0) {
        std::string req(buf, buf + n);
        auto sp1 = req.find(' ');
        auto sp2 = req.find(' ', sp1 == std::string::npos ? 0 : sp1 + 1);
        std::string method = req.substr(0, sp1);
        std::string path = sp1 == std::string::npos ? "/" : req.substr(sp1 + 1, sp2 - sp1 - 1);
        std::string resp = handle(method, path);
        send(fd, resp.data(), static_cast<int>(resp.size()), 0);
    }
    sock_close(fd);
}

} // namespace ds2x
