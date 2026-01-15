#include <stdint.h>
#include <string.h>
#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <cell/pad.h>

SYS_MODULE_INFO(dualsense_bridge, 0, 1, 0);
SYS_MODULE_START(_start);
SYS_MODULE_STOP(_stop);

#define HOOK_ADDR_PADINFO  0x80000000002D3F50ULL
#define HOOK_ADDR_PADDATA  0x80000000002D4020ULL

typedef int (*cellPadGetInfo_t)(CellPadInfo2*);
typedef int (*cellPadGetData_t)(uint32_t, CellPadData*);

static cellPadGetInfo_t real_cellPadGetInfo;
static cellPadGetData_t real_cellPadGetData;

typedef void (*vsh_xmb_call_t)(int);
#define VSH_XMB_EXPORT 0x800000000087BB0001ULL

static inline void icache_flush(void* addr, size_t size) {
    __asm__ volatile("sync");
    __asm__ volatile("icbi 0,%0" :: "r"(addr));
    __asm__ volatile("sync");
    __asm__ volatile("isync");
}

static void hook_function(uint64_t addr, void* func, void** original) {
    uint32_t* patch = (uint32_t*)addr;
    uint32_t branch = 0x48000000 | (((uint32_t)func - (uint32_t)addr) & 0x03FFFFFC);
    *original = (void*)(addr + 4);
    patch[0] = branch;
    icache_flush(patch, 8);
}

int cellPadGetInfo_hook(CellPadInfo2* info) {
    int ret = real_cellPadGetInfo(info);
    for (int i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) {
        if (info->port_status[i] == CELL_PAD_STATUS_CONNECTED) {
            info->device_type[i] = CELL_PAD_DEV_TYPE_STANDARD;
        }
    }
    return ret;
}

int cellPadGetData_hook(uint32_t port, CellPadData* data) {
    int ret = real_cellPadGetData(port, data);
    if (!ret && data->len > 0) {
        uint8_t l2 = data->button[CELL_PAD_BTN_OFFSET_ANALOG_L2];
        uint8_t r2 = data->button[CELL_PAD_BTN_OFFSET_ANALOG_R2];
        data->button[CELL_PAD_BTN_OFFSET_PRESS_L2] = l2;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_R2] = r2;

        uint16_t btn = data->button[CELL_PAD_BTN_OFFSET_DIGITAL];
        if ((btn & CELL_PAD_CTRL_L3) &&
            (btn & CELL_PAD_CTRL_R3) &&
            (btn & CELL_PAD_CTRL_START)) {
            vsh_xmb_call_t xmb = (vsh_xmb_call_t)VSH_XMB_EXPORT;
            xmb(1);
        }
    }
    return ret;
}

void plugin_thread(uint64_t arg) {
    hook_function(HOOK_ADDR_PADINFO, cellPadGetInfo_hook, (void**)&real_cellPadGetInfo);
    hook_function(HOOK_ADDR_PADDATA, cellPadGetData_hook, (void**)&real_cellPadGetData);
    sys_ppu_thread_exit(0);
}

int _start(void) {
    sys_ppu_thread_t tid;
    sys_ppu_thread_create(&tid, plugin_thread, 0, 1000, 0x1000, 0, "dualsense_bridge");
    return SYS_PRX_RESIDENT;
}

int _stop(void) {
    return SYS_PRX_STOP_OK;
}
