dualshock2xbox

Play PC games with a DualShock 4 or DualSense (PS5) controller by translating it
into a virtual Xbox 360 gamepad. Windows-only at runtime; that's where the
ViGEmBus driver lives; but the report parsing and button mapping are plain
portable C++ with zero dependencies, so they build and test anywhere.

How it works

DualShock 4 / DualSense --HID--> parse raw report --> map to Xbox layout --> ViGEm virtual X360 pad --> game

1. Read the controller's raw HID reports (hidapi). Both USB and Bluetooth,
   both generations; the report layouts differ, the parser knows them all.
2. Map the PlayStation layout onto the Xbox layout:
   Cross->A, Circle->B, Square->X, Triangle->Y, L1->LB, R1->RB, Share/Create->Back,
   Options->Start, L3->LS, R3->RS, PS->Guide, dpad->dpad, L2/R2->LT/RT (analog).
3. Emit an Xbox 360 controller through ViGEmBus, which games see as a real pad.

Prebuilt Windows exe

No compiler needed: grab dualshock2xbox-setup.exe from the Releases page
and run it as Administrator; it installs to Program Files with Start Menu
shortcuts and can install the ViGEmBus driver for you. The exe loads
ViGEmClient.dll from the ViGEmBus driver at runtime and needs hidapi.dll
next to it (the installer ships it), so it has no other dependencies.

Quick start (the dumb-simple way)

1. Download dualshock2xbox-setup.exe from Releases and run it as
   Administrator (it installs the ViGEmBus driver via winget if missing).
2. Plug in or Bluetooth-pair your DualShock 4 / DualSense, then double-click
   run.bat. That's it; your pad now appears to games as an Xbox 360
   controller. Close the window to stop.

Test mode (no controller needed): dualshock2xbox.exe --test-mode opens a
web GUI at http://localhost:8080 with a visual pad; your keyboard becomes a
DualShock, and you watch it arrive as an Xbox pad on the other side.
--port N changes the port.

Forgot something? Run dualshock2xbox.exe --check; it tells you in plain
language whether the driver and the controller are both detected.

Want to tweak things? Edit settings.ini next to the exe (poll rate, stick
deadzone, gyro aiming, light bar, player LED, adaptive triggers, button
remaps); CLI flags still override the file.

Profiles: keep several setups in profiles/<name>.ini and pick one with
--profile <name>. --list-profiles shows what's available.

Requirements (Windows)

- Windows 10/11
- ViGEmBus (https://github.com/ViGEm/ViGEmBus/releases) driver installed

Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release        (runs the dependency-free tests)

The translator needs hidapi.dll next to the exe and the ViGEmBus driver at
runtime ; both are loaded on demand, so building needs nothing but a C++20
compiler. On Linux/macOS you can build and run everything except the real
controller path: ./dualshock2xbox --dry-run plays back a scripted pad.

Options

    --check            diagnose the setup and exit (driver, controller)
    --profile NAME     load profiles/NAME.ini instead of settings.ini
    --list-profiles    show available profiles and exit
    --hz N             poll rate, 30-1000 (default 250)
    --deadzone F       stick deadzone, 0-0.95
    --safe             read-only: never write anything to the pad
    --test-mode        keyboard -> DualShock -> Xbox playground with a web GUI
    --port N           test-mode GUI port (default 8080)
    --dry-run          scripted pad, no hardware, no driver (any OS)

Typos are not forgiven silently: an unknown option, a bad number, an unknown
profile or an unknown line in settings.ini is reported, and out-of-range
settings are corrected with a warning instead of being trusted.

Test mode security: the GUI listens on 127.0.0.1 only and ignores requests whose
Host or Origin isn't local, so other machines on your network and other web
pages in your browser can't press buttons on your virtual pad.

Project layout

    src/hid_report.*     raw report parsing (DS4/DualSense, USB/Bluetooth)
    src/mapping.*        PlayStation -> Xbox state, remaps, gyro, XUSB conversion
    src/ds4w_shaping.*   stick deadzone / anti-deadzone / max zone
    src/gyro.*           gyro bias auto-calibration
    src/output_report.*  rumble, light bar, player LEDs, adaptive triggers (+ BT CRC)
    src/config.*, profiles.*   settings.ini and profiles
    src/test_mode.*, test_page.hpp   the keyboard playground and its web page
    src/source.*, sink.*, win_dynload.*   the Windows runtime (hidapi, ViGEm)
    tests/               dependency-free unit tests

Safety

--safe (or safe_mode=1) makes the tool strictly read-only: it translates
input but never sends rumble, light bar or trigger reports to the controller.

Notes

- The Bluetooth output reports carry a CRC-32 (seed 0xA2); without it the pad
  silently ignores them. src/output_report.cpp builds it for you.
- The report offsets follow the layouts documented by the Linux hid-playstation
  driver. If something on your particular pad/firmware misbehaves, the
  --test-mode GUI and the tests in tests/ are the quickest way to narrow it down.
- Stick shaping is ported from DS4Windows (GPL-3.0), so this project is
  GPL-3.0 too. See NOTICE.txt.
