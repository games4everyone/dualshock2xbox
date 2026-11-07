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

} // namespace ds2x::win

#endif // DS2X_WINDOWS
