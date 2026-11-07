#pragma once

#include "hid_report.hpp"
#include "mapping.hpp"

#include <map>
#include <string>

namespace ds2x {

// keyboard state: key name -> pressed
using KeyState = std::map<std::string, bool>;

// turn the pressed keys into a fake DualShock sample using the default
// keyboard layout (see kDefaultKeyMap in the .cpp)
PadState build_pad_state(const KeyState& keys);

// the default keyboard -> playstation mapping, exposed for the GUI table
const std::map<std::string, std::string>& default_key_map();

// tiny http server for the test-mode GUI. poll() accepts and answers requests
// without blocking, so the main loop can keep translating at full speed.
// The test server only ever talks to a page served by itself on this machine.
// Without this check any website open in your browser could POST /key to
// localhost and press buttons on your virtual Xbox pad (a plain cross-site POST
// needs no permission), and a DNS-rebinding page could do the same under its
// own hostname. So: Host must be localhost / 127.0.0.1 / [::1] (any port), and
// an Origin header, when the browser sends one, must be one of those too.
bool is_local_request(const std::string& host, const std::string& origin);

class TestServer {
public:
    ~TestServer();
    bool start(int port);
    void stop();
    void poll();  // non-blocking accept + answer

    // pure request handler, unit-testable without sockets:
    // GET / -> the page, POST /key?k=W&down=1 -> set key, GET /last -> json state
    std::string handle(const std::string& method, const std::string& path);

    // same, but first checks the Host / Origin headers (see is_local_request);
    // this is what the socket loop uses
    std::string handle(const std::string& method, const std::string& path,
                       const std::string& host, const std::string& origin);

    const KeyState& keys() const { return keys_; }
    void set_last_xbox(const XboxState& s) { last_ = s; }

private:
    KeyState keys_;
    XboxState last_{};
    int listen_fd_ = -1;
};

} // namespace ds2x
