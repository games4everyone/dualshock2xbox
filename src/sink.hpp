#pragma once

#include "mapping.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace ds2x {

// a sink consumes translated Xbox states
struct Sink {
    virtual ~Sink() = default;
    virtual bool open() = 0;
    virtual void write(const XboxState& state) = 0;
    // game rumble coming back from the virtual pad (large/strong, small/weak)
    virtual void rumble(std::uint8_t large, std::uint8_t small) { (void)large; (void)small; }
    virtual void close() {}

    // on Windows the ViGEm sink calls this when the game rumbles;
    // main() points it at the real pad's output reports
    std::function<void(std::uint8_t large, std::uint8_t small)> rumble_handler;
};

#ifdef DS2X_WINDOWS
// virtual Xbox 360 pad via ViGEmBus (Windows build)
std::unique_ptr<Sink> make_vigem_sink();
#endif

// prints state changes to stdout; used by --dry-run on any OS
std::unique_ptr<Sink> make_logging_sink();

} // namespace ds2x
