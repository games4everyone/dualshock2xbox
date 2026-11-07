#pragma once

// Windows runtime loading of hidapi.dll and ViGEmClient.dll.
// we resolve every function with GetProcAddress instead of linking import
// libs, so the shipped .exe has zero build-time dependencies ; the DLLs are
// looked up at runtime (ViGEmClient.dll ships with the ViGEmBus driver in
// System32, hidapi.dll goes next to the exe).

#ifdef DS2X_WINDOWS

#include <atomic>
#include <cstdint>
#include <string>

// the callbacks ViGEmClient calls are __stdcall on 32-bit Windows, plain on x64
#if defined(_M_IX86) || defined(__i386__)
#define DS2X_CALLBACK __stdcall
#else
#define DS2X_CALLBACK
#endif

namespace ds2x::win {

// --- hidapi types (layout matches hidapi 0.14) ---
struct hid_device_;
typedef hid_device_ hid_device;
struct hid_device_info {
    char* path;
    wchar_t* serial_number;
    wchar_t* manufacturer_string;
    wchar_t* product_string;
    unsigned short vendor_id;
    unsigned short product_id;
    wchar_t* release_number;
    int usage_page;
    int usage;
    int interface_number;
    int interface_class;
    int interface_subclass;
    int interface_protocol;
    struct hid_device_info* next;
};

typedef int (*HidInitFn)(void);
typedef int (*HidExitFn)(void);
typedef hid_device_info* (*HidEnumerateFn)(unsigned short, unsigned short);
typedef void (*HidFreeEnumerationFn)(hid_device_info*);
typedef hid_device* (*HidOpenPathFn)(const char*);
typedef void (*HidCloseFn)(hid_device*);
typedef int (*HidSetNonblockingFn)(hid_device*, int);
typedef int (*HidReadFn)(hid_device*, unsigned char*, size_t);
typedef int (*HidWriteFn)(hid_device*, const unsigned char*, size_t);
typedef int (*HidGetFeatureReportFn)(hid_device*, unsigned char*, size_t);

extern HidInitFn hid_init;
extern HidExitFn hid_exit;
extern HidEnumerateFn hid_enumerate;
extern HidFreeEnumerationFn hid_free_enumeration;
extern HidOpenPathFn hid_open_path;
extern HidCloseFn hid_close;
extern HidSetNonblockingFn hid_set_nonblocking;
extern HidReadFn hid_read;
extern HidWriteFn hid_write;
extern HidGetFeatureReportFn hid_get_feature_report;

// --- ViGEmClient types ---
typedef struct vigem_client* PVIGEM_CLIENT;
typedef struct vigem_target* PVIGEM_TARGET;
typedef int VIGEM_ERROR;
constexpr int VIGEM_ERROR_NONE = 0;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned char BYTE;
typedef void VOID;
typedef void* PVOID;
typedef struct {
    USHORT wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} XUSB_REPORT;
typedef VOID (DS2X_CALLBACK *VIGEM_X360_NOTIFICATION)(PVIGEM_CLIENT, PVIGEM_TARGET, UCHAR, UCHAR, UCHAR, PVOID);

typedef PVIGEM_CLIENT (*VigemAllocFn)(void);
typedef void (*VigemFreeFn)(PVIGEM_CLIENT);
typedef VIGEM_ERROR (*VigemConnectFn)(PVIGEM_CLIENT);
typedef void (*VigemDisconnectFn)(PVIGEM_CLIENT);
typedef PVIGEM_TARGET (*VigemTargetX360AllocFn)(void);
typedef void (*VigemTargetFreeFn)(PVIGEM_TARGET);
typedef VIGEM_ERROR (*VigemTargetAddFn)(PVIGEM_CLIENT, PVIGEM_TARGET);
typedef VIGEM_ERROR (*VigemTargetRemoveFn)(PVIGEM_CLIENT, PVIGEM_TARGET);
typedef VIGEM_ERROR (*VigemTargetX360UpdateFn)(PVIGEM_CLIENT, PVIGEM_TARGET, XUSB_REPORT);
typedef VIGEM_ERROR (*VigemRegisterNotificationFn)(PVIGEM_TARGET, VIGEM_X360_NOTIFICATION, PVOID);

extern VigemAllocFn vigem_alloc;
extern VigemFreeFn vigem_free;
extern VigemConnectFn vigem_connect;
extern VigemDisconnectFn vigem_disconnect;
extern VigemTargetX360AllocFn vigem_target_x360_alloc;
extern VigemTargetFreeFn vigem_target_free;
extern VigemTargetAddFn vigem_target_add;
extern VigemTargetRemoveFn vigem_target_remove;
extern VigemTargetX360UpdateFn vigem_target_x360_update;
extern VigemRegisterNotificationFn vigem_target_x360_register_notification;

// load both DLLs and resolve every symbol ; false + last_error() on failure
bool load();
std::string last_error();

// Windows sleeps in ~15 ms steps by default, which would wreck a 250 Hz loop.
// these raise the timer resolution to 1 ms (winmm is loaded at runtime, so the
// exe still links against nothing extra) and put it back.
void begin_high_res_timer();
void end_high_res_timer();

// Ctrl+C / closing the console window sets *flag, so the main loop can stop
// rumble and remove the virtual pad instead of just vanishing
void install_quit_handler(std::atomic<bool>* flag);

} // namespace ds2x::win

#endif // DS2X_WINDOWS
