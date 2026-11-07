#include "win_dynload.hpp"

#ifdef DS2X_WINDOWS

#include <windows.h>

namespace ds2x::win {

HidInitFn hid_init = nullptr;
HidExitFn hid_exit = nullptr;
HidEnumerateFn hid_enumerate = nullptr;
HidFreeEnumerationFn hid_free_enumeration = nullptr;
HidOpenPathFn hid_open_path = nullptr;
HidCloseFn hid_close = nullptr;
HidSetNonblockingFn hid_set_nonblocking = nullptr;
HidReadFn hid_read = nullptr;
HidWriteFn hid_write = nullptr;
HidGetFeatureReportFn hid_get_feature_report = nullptr;

VigemAllocFn vigem_alloc = nullptr;
VigemFreeFn vigem_free = nullptr;
VigemConnectFn vigem_connect = nullptr;
VigemDisconnectFn vigem_disconnect = nullptr;
VigemTargetX360AllocFn vigem_target_x360_alloc = nullptr;
VigemTargetFreeFn vigem_target_free = nullptr;
VigemTargetAddFn vigem_target_add = nullptr;
VigemTargetRemoveFn vigem_target_remove = nullptr;
VigemTargetX360UpdateFn vigem_target_x360_update = nullptr;
VigemRegisterNotificationFn vigem_target_x360_register_notification = nullptr;

static std::string g_error;
std::string last_error() { return g_error; }

template <typename T>
static bool resolve(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(GetProcAddress(dll, name));
    if (!out) {
        g_error = std::string("missing symbol ") + name;
        return false;
    }
    return true;
}

bool load() {
    g_error.clear();
    HMODULE hid = LoadLibraryA("hidapi.dll");
    if (!hid) {
        g_error = "hidapi.dll not found ; put it next to dualshock2xbox.exe";
        return false;
    }
    bool ok = true;
    ok &= resolve(hid, "hid_init", hid_init);
    ok &= resolve(hid, "hid_exit", hid_exit);
    ok &= resolve(hid, "hid_enumerate", hid_enumerate);
    ok &= resolve(hid, "hid_free_enumeration", hid_free_enumeration);
    ok &= resolve(hid, "hid_open_path", hid_open_path);
    ok &= resolve(hid, "hid_close", hid_close);
    ok &= resolve(hid, "hid_set_nonblocking", hid_set_nonblocking);
    ok &= resolve(hid, "hid_read", hid_read);
    ok &= resolve(hid, "hid_write", hid_write);
    ok &= resolve(hid, "hid_get_feature_report", hid_get_feature_report);
    if (!ok) return false;

    // ViGEmClient.dll ships with the ViGEmBus driver and lives in System32
    HMODULE vigem = LoadLibraryA("ViGEmClient.dll");
    if (!vigem) {
        g_error = "ViGEmClient.dll not found ; install the ViGEmBus driver first (setup.ps1)";
        return false;
    }
    ok = true;
    ok &= resolve(vigem, "vigem_alloc", vigem_alloc);
    ok &= resolve(vigem, "vigem_free", vigem_free);
    ok &= resolve(vigem, "vigem_connect", vigem_connect);
    ok &= resolve(vigem, "vigem_disconnect", vigem_disconnect);
    ok &= resolve(vigem, "vigem_target_x360_alloc", vigem_target_x360_alloc);
    ok &= resolve(vigem, "vigem_target_free", vigem_target_free);
    ok &= resolve(vigem, "vigem_target_add", vigem_target_add);
    ok &= resolve(vigem, "vigem_target_remove", vigem_target_remove);
    ok &= resolve(vigem, "vigem_target_x360_update", vigem_target_x360_update);
    ok &= resolve(vigem, "vigem_target_x360_register_notification", vigem_target_x360_register_notification);
    if (!ok) return false;
    return true;
}

// --- timer resolution --------------------------------------------------------
typedef unsigned int (WINAPI *TimePeriodFn)(unsigned int);  // winmm is __stdcall
static TimePeriodFn g_time_begin = nullptr;
static TimePeriodFn g_time_end = nullptr;
static bool g_timer_raised = false;

void begin_high_res_timer() {
    if (g_timer_raised) return;
    HMODULE winmm = LoadLibraryA("winmm.dll");
    if (!winmm) return;  // not fatal ; sleeps are just coarser
    g_time_begin = reinterpret_cast<TimePeriodFn>(GetProcAddress(winmm, "timeBeginPeriod"));
    g_time_end = reinterpret_cast<TimePeriodFn>(GetProcAddress(winmm, "timeEndPeriod"));
    if (g_time_begin && g_time_end) { g_time_begin(1); g_timer_raised = true; }
}

void end_high_res_timer() {
    if (g_timer_raised && g_time_end) g_time_end(1);
    g_timer_raised = false;
}

// --- clean shutdown ----------------------------------------------------------
static std::atomic<bool>* g_quit_flag = nullptr;

static BOOL WINAPI on_console_event(DWORD type) {
    if (g_quit_flag) g_quit_flag->store(true);
    // for close / logoff / shutdown Windows kills the process as soon as we
    // return, so hold on briefly to let the main loop tidy up
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT)
        Sleep(2000);
    return TRUE;
}

void install_quit_handler(std::atomic<bool>* flag) {
    g_quit_flag = flag;
    SetConsoleCtrlHandler(on_console_event, TRUE);
}

} // namespace ds2x::win

#endif // DS2X_WINDOWS
