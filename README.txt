dualshock2xbox

Play PC games with a DualShock 4 or DualSense (PS5) controller by translating it
into a virtual Xbox 360 gamepad. Windows-only at runtime ; that's where the
ViGEmBus driver lives ; but the report parsing and button mapping are plain
portable C++ with zero dependencies, so they build and test anywhere.

How it works


DualShock 4 / DualSense --HID--> parse raw report --> map to Xbox layout --> ViGEm virtual X360 pad --> game


1. Read the controller's raw HID reports (hidapi). Both USB and Bluetooth,
   both generations ; the report layouts differ, the parser knows them all.
2. Map the PlayStation layout onto the Xbox layout:
   Cross->A, Circle->B, Square->X, Triangle->Y, L1->LB, R1->RB, Share/Create->Back,
   Options->Start, L3->LS, R3->RS, PS->Guide, dpad->dpad, L2/R2->LT/RT (analog).
3. Emit an Xbox 360 controller through ViGEmBus, which games see as a real pad.

Prebuilt Windows exe

No compiler needed: Releases has a ready-to-run dualshock2xbox-setup.exe that installs to
Program Files with Start Menu/Desktop shortcuts and can install the ViGEmBus
driver via winget. Without the installer: run setup.ps1 once, keep
hidapi.dll next to the exe, then double-click run.bat. The exe loads
ViGEmClient.dll from the ViGEmBus driver at runtime, so it has no other
dependencies.

Quick start (the dumb-simple way)

1. Build the exe (once): see "Build" below. You end up with dualshock2xbox.exe.
2. Run setup.ps1 once as Administrator (right-click, "Run with PowerShell").
   It installs the ViGEmBus driver for you.
3. Plug in or Bluetooth-pair your DualShock 4 / DualSense, then double-click
   run.bat. That's it ; your pad now appears to games as an Xbox 360
   controller. Close the window to stop.

Test mode (no controller needed): dualshock2xbox.exe --test-mode opens a
web GUI at http://localhost:8080 with a visual pad ; your keyboard becomes a
DualShock (WASD/arrows/IJKL = sticks and dpad, 1-4 = face buttons, q/e =
shoulders, z/x = triggers, space = cross, ...), and you watch it arrive as an
Xbox pad on the other side. Great for checking a profile before using a real
pad. --port N changes the port.

Forgot something? Run dualshock2xbox.exe --check ; it tells you in plain
language whether the driver and the controller are both detected.

Want to tweak things? Edit settings.ini next to the exe (poll rate, stick
deadzone, gyro aiming on/off and strength, light bar, player LED, adaptive
triggers, button remaps) ; no command line needed. CLI flags still override
the file if you ever want them.

Profiles: keep several setups in profiles/<name>.ini and pick one with
--profile <name> (e.g. dualshock2xbox.exe --profile example-fps, or
run.bat --profile example-fps). --list-profiles shows what's available.
See profiles/example-fps.ini for a commented starting point.

Requirements (Windows)

- Windows 10/11
- ViGEmBus (https://github.com/ViGEm/ViGEmBus/releases) driver installed
  (this is what makes the fake Xbox pad show up in Device Manager)
- hidapi and ViGEmClient development files (e.g. via vcpkg:
  vcpkg install hidapi:x64-windows vigemclient:x64-windows)

Build

powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release


Run

powershell
plug in / pair the controller, then:
.\build\Release\dualshock2xbox.exe

no hardware handy? watch the translation happen without a pad:
.\build\Release\dualshock2xbox.exe --dry-run


Options: --hz N (poll rate, default 250 ; the native X360 rate),
--deadzone F (stick deadzone 0..1, default 0.08), --list (show detected pads).

Project layout


run.bat                  double-click launcher for Windows (runs --check first, stays open on exit,
                         passes extra args through, e.g. run.bat --profile example-fps)
setup.ps1                one-time setup ; installs the ViGEmBus driver via winget
profiles/                extra settings files, selected with --profile <name>
settings.ini             default settings file
src/
  hid_report.{hpp,cpp}   raw DS4/DualSense HID report parsing (portable, tested)
  mapping.{hpp,cpp}      PlayStation state -> Xbox state + remap table (portable, tested)
  names.{hpp,cpp}        button name tables shared by remap and tooling
  profiles.{hpp,cpp}     profile discovery/loading (profiles/*.ini)
  log.{hpp,cpp}          tiny [info]/[warn] logging
  source.{hpp,cpp}       hidapi reader (Windows) + scripted source for dry-run
  sink.{hpp,cpp}         ViGEm sink (Windows) + logging sink for dry-run
  main.cpp               CLI + main loop
tests/
  test_runner.cpp        runs all module tests
  test_hid_report.cpp    report parsing (all 4 layouts, gyro, garbage, transport)
  test_mapping.cpp       mapping, deadzone, gyro, remap
  test_output_report.cpp crc32, rumble, lightbar, player LEDs, triggers
  test_config.cpp        settings.ini + profiles


Safety (read this if you're nervous about your controllers)

The tool only ever sends standard HID output reports (rumble, LED, trigger
effects) ; the same packets games send. It has no firmware/flash code path, so
it cannot brick a pad. Still, there is a read-only mode:

    dualshock2xbox.exe --safe        # or safe_mode=1 in settings.ini

In safe mode the controller is only ever read ; nothing is ever written to it.
For a first test, use a USB cable (not Bluetooth) ; that's the most boring,
reliable path.

Notes

- Rumble works both ways: when the game rumbles the virtual Xbox pad, ViGEm
  sends us the motor values and we forward them to the real DualShock/DualSense
  as output reports (DS4: USB 0x05 / BT 0x11, DualSense: USB 0x02 / BT 0x31,
  Bluetooth frames get the CRC-32 trailer over 0xA2 + report). XInput's large
  motor maps to the strong motor, small to weak.
- The touchpad click has no Xbox equivalent, so it stays on the PlayStation side
  of the border and is ignored.
- Bluetooth DualSense controllers start in a "simple" report mode that is
  literally the old DualShock 4 format. The parser handles it; Sony reused the
  paperwork.
- Gyro aiming is supported: set gyro=1 in settings.ini and the gyro steers
  the right stick (yaw turns, pitch looks up/down), like a mouse.
- Light bar: set lightbar=r,g,b in settings.ini and the pad glows that color
  when the translator starts.
- DualSense player LEDs: set player_led=1..5 in settings.ini.
- DualSense adaptive triggers work: trigger_mode=1..3 in settings.ini
  (resistance / weapon click / vibration), with trigger_start,
  trigger_strength, trigger_freq.
- Buttons are remappable in settings.ini with remap_<ps>=<xbox> lines, e.g.
  remap_cross=b for the Japanese layout or remap_touchpad=rs to give the
  touchpad click a job.
- Bluetooth is fully handled: the translator detects the transport from the
  HID path, performs the calibration-feature handshake that switches BT pads
  from the stripped-down report to the full one (DS4 0x11, DualSense 0x31),
  and automatically reconnects if the pad drops.

Credits: stick shaping (deadzone / anti-deadzone / max-zone) is ported
from DS4Windows (GPL-3.0), so this project is GPL-3.0 too. See NOTICE.txt.
