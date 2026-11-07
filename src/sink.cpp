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
        if (!client_) {
            std::cerr << "could not allocate a ViGEm client (out of memory?)\n";
            return false;
        }
        if (win::vigem_connect(client_) != win::VIGEM_ERROR_NONE) {
            std::cerr << "could not connect to ViGEmBus ; is the driver installed?\n";
            close();
            return false;
        }
        target_ = win::vigem_target_x360_alloc();
        if (!target_) {
            std::cerr << "could not allocate the virtual pad\n";
            close();
            return false;
        }
        const int err = win::vigem_target_add(client_, target_);
        if (err != win::VIGEM_ERROR_NONE) {
            std::cerr << "ViGEmBus refused to create the virtual pad (error 0x" << std::hex << err
                      << std::dec << ")\n";
            win::vigem_target_free(target_);
            target_ = nullptr;
            close();
            return false;
        }
        added_ = true;
        // THIS is what makes game rumble reach the real pad: without registering
        // for notifications ViGEmBus never tells us a game asked for vibration.
        // the callback runs on a ViGEm thread, so rumble_handler must be quick and
        // thread-safe (main() just stores the values and sends them from its loop).
        const int nerr = win::vigem_target_x360_register_notification(target_, &VigemSink::on_rumble, this);
        if (nerr != win::VIGEM_ERROR_NONE)
            std::cerr << "warning: could not register for rumble (error 0x" << std::hex << nerr
                      << std::dec << ") ; games won't be able to vibrate the pad\n";
        std::cout << "virtual Xbox 360 pad created\n";
        return true;
    }

    void write(const XboxState& s) override {
        win::XUSB_REPORT r{};
        r.wButtons = s.buttons;  // our XboxButton bits match XUSB's wButtons layout
        r.bLeftTrigger = to_xusb_trigger(s.lt);
        r.bRightTrigger = to_xusb_trigger(s.rt);
        r.sThumbLX = to_xusb_axis(s.lx);
        r.sThumbLY = to_xusb_axis(s.ly);
        r.sThumbRX = to_xusb_axis(s.rx);
        r.sThumbRY = to_xusb_axis(s.ry);
        win::vigem_target_x360_update(client_, target_, r);
    }

    void close() override {
        if (target_) {
            if (added_) win::vigem_target_remove(client_, target_);
            win::vigem_target_free(target_);
            target_ = nullptr;
            added_ = false;
        }
        if (client_) { win::vigem_disconnect(client_); win::vigem_free(client_); client_ = nullptr; }
    }

private:
    static void DS2X_CALLBACK on_rumble(win::PVIGEM_CLIENT, win::PVIGEM_TARGET, win::UCHAR large,
                                        win::UCHAR small, win::UCHAR /*led_number*/, win::PVOID user) {
        auto* self = static_cast<VigemSink*>(user);
        if (self && self->rumble_handler) self->rumble_handler(large, small);
    }

    win::PVIGEM_CLIENT client_ = nullptr;
    win::PVIGEM_TARGET target_ = nullptr;
    bool added_ = false;
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
