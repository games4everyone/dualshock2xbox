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
