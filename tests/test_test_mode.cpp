// test-mode tests: keyboard -> dualshock mapping and the http handler

#include "test_util.hpp"

#include "test_mode.hpp"

#include <cstring>

using namespace ds2x;

void test_build_pad_state() {
    KeyState keys;
    auto neutral = build_pad_state(keys);
    CHECK(neutral.lx == 128 && neutral.ly == 128 && neutral.buttons == 0 && neutral.dpad == 8);

    keys["w"] = true;
    keys["d"] = true;
    auto s = build_pad_state(keys);
    CHECK(s.lx == 255 && s.ly == 0);  // up+right diagonal

    keys["2"] = true;  // cross
    keys["ArrowLeft"] = true;
    keys["z"] = true;  // l2 analog
    s = build_pad_state(keys);
    CHECK(s.buttons & Cross);
    CHECK(s.dpad == 6);               // left
    CHECK(s.buttons & L2 && s.l2 == 255);
}

void test_server_handle() {
    TestServer srv;
    auto page = srv.handle("GET", "/");
    CHECK(page.find("200 OK") != std::string::npos);
    CHECK(page.find("test mode") != std::string::npos);

    auto r = srv.handle("POST", "/key?k=w&down=1");
    CHECK(r.find("204") != std::string::npos);
    CHECK(srv.keys().at("w") == true);
    srv.handle("POST", "/key?k=w&down=0");
    CHECK(srv.keys().at("w") == false);

    srv.set_last_xbox(map_state(build_pad_state(KeyState{{"2", true}})));
    auto last = srv.handle("GET", "/last");
    CHECK(last.find("\"buttons\":4096") != std::string::npos);  // 0x1000 = A from cross

    auto map = srv.handle("GET", "/map");
    CHECK(map.find("\"2\"") != std::string::npos && map.find("cross") != std::string::npos);

    CHECK(srv.handle("GET", "/nope").find("404") != std::string::npos);
}

// the GUI must only answer this machine, and only pages served by itself
void test_server_rejects_strangers() {
    // hosts
    CHECK(is_local_request("localhost:8080", ""));
    CHECK(is_local_request("127.0.0.1:8080", ""));
    CHECK(is_local_request("[::1]:8080", ""));
    CHECK(is_local_request("LocalHost", ""));
    CHECK(!is_local_request("", ""));                       // no Host header at all
    CHECK(!is_local_request("evil.example", ""));
    CHECK(!is_local_request("localhost.evil.example", ""));  // dns-rebinding lookalike
    CHECK(!is_local_request("127.0.0.1.evil.example:80", ""));
    CHECK(!is_local_request("192.168.1.20:8080", ""));
    CHECK(!is_local_request("localhost:80x", ""));
    // origins: a cross-site POST from any web page carries its own Origin
    CHECK(is_local_request("localhost:8080", "http://localhost:8080"));
    CHECK(is_local_request("127.0.0.1:8080", "http://127.0.0.1:8080"));
    CHECK(!is_local_request("localhost:8080", "https://evil.example"));
    CHECK(!is_local_request("localhost:8080", "http://evil.example"));
    CHECK(!is_local_request("localhost:8080", "null"));       // sandboxed iframes send this

    TestServer srv;
    auto bad = srv.handle("POST", "/key?k=w&down=1", "localhost:8080", "https://evil.example");
    CHECK(bad.find("403") != std::string::npos);
    CHECK(srv.keys().find("w") == srv.keys().end() || srv.keys().at("w") == false);  // nothing was pressed
    auto rebind = srv.handle("GET", "/last", "attacker.example:8080", "");
    CHECK(rebind.find("403") != std::string::npos);
    auto good = srv.handle("POST", "/key?k=w&down=1", "localhost:8080", "http://localhost:8080");
    CHECK(good.find("204") != std::string::npos && srv.keys().at("w") == true);
}

void test_map_names_are_readable() {
    TestServer srv;
    auto map = srv.handle("GET", "/map");
    CHECK(map.find("xbox#") == std::string::npos);                 // no raw bit numbers
    CHECK(map.find("\"ps\":\"cross\",\"xb\":\"a\"") != std::string::npos);  // cross -> a, spelled out
    CHECK(map.find("LT (trigger)") != std::string::npos);          // l2 is a trigger, not a stick
}
