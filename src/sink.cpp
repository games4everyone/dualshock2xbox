#include "sink.hpp"

#include <cstdio>
#include <iostream>

#ifdef DS2X_WINDOWS
#include "win_dynload.hpp"
using namespace ds2x::win;  // vigem_* / win::XUSB_REPORT / win::VIGEM_ERROR_NONE from win_dynload
#endif

namespace ds2x {

#ifdef DS2X_WINDOWS

// ViGEmBus is the driver that makes Windows believe a virtual Xbox 360 pad
 // exists. games never know the difference ; the pad is just there.
class VigemSink : public Sink {
public:
    ~VigemSink() override { close(); }

    bool open() override {
        client_ = win::vigem_alloc();
        if (win::vigem_connect(client_) != win::VIGEM_ERROR_NONE) {
            std::cerr << "could not connect to ViGEmBus ; is the driver installed?\n";
            return false;
        }
        target_ = win::vigem_target_x360_alloc();
        win::vigem_target_add(client_, target_);
        std::cout << "virtual Xbox 360 pad created\n";
        return true;
    }

    void write(const XboxState& s) override {
        win::XUSB_REPORT r{};
        r.wButtons = s.buttons;  // our XboxButton bits match XUSB's wButtons layout
        r.bLeftTrigger = static_cast<BYTE>(s.lt * 255.0f);
        r.bRightTrigger = static_cast<BYTE>(s.rt * 255.0f);
        r.sThumbLX = static_cast<SHORT>(s.lx * 32767.0f);
        r.sThumbLY = static_cast<SHORT>(s.ly * 32767.0f);
        r.sThumbRX = static_cast<SHORT>(s.rx * 32767.0f);
        r.sThumbRY = static_cast<SHORT>(s.ry * 32767.0f);
        win::vigem_target_x360_update(client_, target_, r);
    }

    void close() override {
        if (target_) { win::vigem_target_remove(client_, target_); win::vigem_target_free(target_); target_ = nullptr; }
        if (client_) { win::vigem_disconnect(client_); win::vigem_free(client_); client_ = nullptr; }
    }

private:
    win::PVIGEM_CLIENT client_ = nullptr;
    win::PVIGEM_TARGET target_ = nullptr;
};

std::unique_ptr<Sink> make_vigem_sink() { return std::make_unique<VigemSink>(); }

#endif // DS2X_WINDOWS

// --- logging sink -----------------------------------------------------------
class LoggingSink : public Sink {
public:
    bool open() override { return true; }

    void rumble(std::uint8_t large, std::uint8_t small) override {
        std::printf("rumble large=%u small=%u\n", large, small);
    }

    void write(const XboxState& s) override {
         // only print when something actually changed ; 250Hz of silence is noise
        if (s.buttons == last_.buttons && s.lt == last_.lt && s.rt == last_.rt &&
            s.lx == last_.lx && s.ly == last_.ly && s.rx == last_.rx && s.ry == last_.ry)
            return;
        last_ = s;
        std::printf("buttons=0x%04x dpad=%c%c%c%c lt=%.2f rt=%.2f l=(%.2f,%.2f) r=(%.2f,%.2f)\n",
                    s.buttons,
                    s.buttons & XDpadUp ? 'U' : '-', s.buttons & XDpadRight ? 'R' : '-',
                    s.buttons & XDpadDown ? 'D' : '-', s.buttons & XDpadLeft ? 'L' : '-',
                    s.lt, s.rt, s.lx, s.ly, s.rx, s.ry);
    }

private:
    XboxState last_{};
};

std::unique_ptr<Sink> make_logging_sink() { return std::make_unique<LoggingSink>(); }

} // namespace ds2x
