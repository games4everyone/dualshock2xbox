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

Build / Run / Options / Project layout / Safety / Notes: see the full file in
the repo ; stick shaping is ported from DS4Windows (GPL-3.0), so this
project is GPL-3.0 too. See NOTICE.txt.
