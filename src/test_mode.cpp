#include "test_mode.hpp"

#include "names.hpp"
#include "test_page.hpp"

#include <algorithm>
#include <cctype>
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
#include <fcntl.h>
#include <netinet/in.h>
#include <cerrno>
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
    // loopback only: the GUI is for this machine, and binding 0.0.0.0 would let
    // anyone on the LAN press buttons on the virtual pad
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listen_fd_, 8) != 0) {
        sock_close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    // poll() promises not to block, so the listening socket must be non-blocking
    // (a blocking accept() would freeze the translation loop until a browser connects)
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(static_cast<SOCKET>(listen_fd_), FIONBIO, &nb);
#else
    fcntl(listen_fd_, F_SETFL, fcntl(listen_fd_, F_GETFL, 0) | O_NONBLOCK);
#endif
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

static std::string lower(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// "localhost", "127.0.0.1" or "[::1]", optionally followed by :port
static bool is_local_host(const std::string& hostport) {
    std::string h = lower(hostport);
    if (h.rfind("[::1]", 0) == 0) { h = h.substr(5); }
    else {
        auto colon = h.find(':');
        std::string name = colon == std::string::npos ? h : h.substr(0, colon);
        if (name != "localhost" && name != "127.0.0.1") return false;
        h = colon == std::string::npos ? "" : h.substr(colon);
    }
    if (h.empty()) return true;
    if (h[0] != ':' || h.size() < 2 || h.size() > 6) return false;
    for (std::size_t i = 1; i < h.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(h[i]))) return false;
    return true;
}

bool is_local_request(const std::string& host, const std::string& origin) {
    if (!is_local_host(host)) return false;  // also rejects a missing Host header
    if (origin.empty()) return true;         // same-origin GETs and non-browser clients send none
    const std::string o = lower(origin);
    return o.rfind("http://", 0) == 0 && is_local_host(o.substr(7));
}

// value of a request header (case-insensitive name), trimmed ; "" if absent
static std::string header_value(const std::string& req, const std::string& name) {
    const std::string want = lower(name) + ":";
    std::size_t pos = req.find("\r\n");
    while (pos != std::string::npos) {
        std::size_t start = pos + 2;
        std::size_t end = req.find("\r\n", start);
        std::string line = req.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (line.empty()) break;  // end of headers
        if (lower(line.substr(0, want.size())) == want) {
            std::string v = line.substr(want.size());
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            return v;
        }
        pos = end;
    }
    return "";
}

std::string TestServer::handle(const std::string& method, const std::string& path,
                               const std::string& host, const std::string& origin) {
    if (!is_local_request(host, origin))
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    return handle(method, path);
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
            std::string xb;
            if (fx == "l2") xb = "LT (trigger)";
            else if (fx == "r2") xb = "RT (trigger)";
            else if (pi >= 0) xb = xbox_button_name(r[pi]);
            else if (fx.rfind("dpad", 0) == 0) xb = fx;
            else xb = "(stick)";
            if (xb.empty()) xb = "(unmapped)";
            // key names come from our own table, but escape anyway: these land in JSON
            auto esc = [](const std::string& s) {
                std::string o;
                for (char ch : s) { if (ch == '"' || ch == '\\') o += '\\'; o += ch; }
                return o;
            };
            body += "\"" + esc(k) + "\":{\"ps\":\"" + esc(fx) + "\",\"xb\":\"" + esc(xb) + "\"}";
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
    // browsers open speculative connections that never send a byte ; without a
    // timeout one of those would block the whole translation loop in recv()
#ifdef _WIN32
    DWORD timeout_ms = 100;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    timeval tv{0, 100 * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    char buf[4096] = {};
    int n = static_cast<int>(recv(fd, buf, sizeof(buf) - 1, 0));
    if (n > 0) {
        std::string req(buf, buf + n);
        auto sp1 = req.find(' ');
        auto sp2 = req.find(' ', sp1 == std::string::npos ? 0 : sp1 + 1);
        std::string method = sp1 == std::string::npos ? "" : req.substr(0, sp1);
        std::string path = (sp1 == std::string::npos || sp2 == std::string::npos)
                               ? "/" : req.substr(sp1 + 1, sp2 - sp1 - 1);
        std::string resp = handle(method, path, header_value(req, "Host"), header_value(req, "Origin"));
        send(fd, resp.data(), static_cast<int>(resp.size()), 0);
    }
    sock_close(fd);
}

} // namespace ds2x
