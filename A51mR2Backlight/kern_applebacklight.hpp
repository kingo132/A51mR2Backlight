//
//  kern_applebacklight.hpp
//  A51mR2Backlight
//

#ifndef kern_applebacklight_hpp
#define kern_applebacklight_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>

class IODisplay;

class AppleBacklightPatcher {
public:
    void registerKexts();
    bool processKext(KernelPatcher &patcher, size_t index,
                     mach_vm_address_t address, size_t size);

private:
    static AppleBacklightPatcher *callback;

    mach_vm_address_t orgApplePanelSetDisplay {0};

    bool appleBacklightHooksReady {false};
    bool mccsSuppressionReady {false};
    bool panelProfilesReady {false};

    uint32_t panelSetDisplayCount {0};
    uint32_t panelProfileRepairCount {0};

    void patchAppleBacklight(KernelPatcher &patcher, size_t index,
                             mach_vm_address_t address, size_t size);
    void patchMccsControl(KernelPatcher &patcher, size_t index,
                          mach_vm_address_t address, size_t size);

    bool ensurePanelProfiles(IOService *panelService);
    void publishDiagnostics();

    static bool wrapApplePanelSetDisplay(IOService *that, IODisplay *display);
    static size_t wrapFunctionReturnZero();
};

#endif /* kern_applebacklight_hpp */
