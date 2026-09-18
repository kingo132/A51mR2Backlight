//
//  kern_backlight.hpp
//  A51mR2Backlight
//
//  Standalone extraction of the Navi10 PWM backlight support originally
//  contributed to WhateverGreen in PR #90 by kingo132.
//

#ifndef kern_backlight_hpp
#define kern_backlight_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>
#include <IOKit/graphics/IOFramebuffer.h>

class IODisplay;

class A51BKL {
public:
    void init();
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    struct ApplePanelData {
        const char *deviceName;
        uint8_t deviceData[36];
    };

    using t_DceDriverSetBacklight = void (*)(void *panelCntl, uint32_t backlightPwmU16_16);

    static A51BKL *callback;
    static ApplePanelData appleBacklightData[];

    uint32_t curPwmBacklightLvl {0};
    uint32_t maxPwmBacklightLvl {0xff7b};
    void *panelCntlPtr {nullptr};

    mach_vm_address_t orgDcePanelCntlHwInit {0};
    mach_vm_address_t orgFramebufferSetAttribute {0};
    mach_vm_address_t orgFramebufferGetAttribute {0};
    mach_vm_address_t orgApplePanelSetDisplay {0};
    t_DceDriverSetBacklight orgDceDriverSetBacklight {nullptr};

    bool amdHooksReady {false};
    bool applePanelDisplaySet {false};

    void processRadeonFramebuffer(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
    void processAppleBacklight(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
    void processMCCSControl(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    void updatePwmMaxBrightnessFromInternalDisplay();

    static mach_vm_address_t findUniquePattern(mach_vm_address_t address, size_t size,
                                                const uint8_t *pattern, size_t patternSize,
                                                const char *name);
    static mach_vm_address_t solveSymbolOrPattern(KernelPatcher &patcher, size_t index,
                                                   mach_vm_address_t address, size_t size,
                                                   const char *symbol,
                                                   const uint8_t *pattern, size_t patternSize,
                                                   const char *name);

    static uint32_t wrapDcePanelCntlHwInit(void *panelCntl);
    static IOReturn wrapFramebufferSetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                 IOSelect attribute, uintptr_t value);
    static IOReturn wrapFramebufferGetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                 IOSelect attribute, uintptr_t *value);
    static bool wrapApplePanelSetDisplay(IOService *that, IODisplay *display);
    static size_t wrapFunctionReturnZero();
};

#endif /* kern_backlight_hpp */
