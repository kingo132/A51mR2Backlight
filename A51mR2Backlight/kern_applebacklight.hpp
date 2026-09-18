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
    bool panelProfilesInstalled {false};
    bool panelResultLogged {false};

    void patchAppleBacklight(KernelPatcher &patcher, size_t index,
                             mach_vm_address_t address, size_t size);
    void patchMccsControl(KernelPatcher &patcher, size_t index,
                          mach_vm_address_t address, size_t size);

    static bool wrapApplePanelSetDisplay(IOService *that, IODisplay *display);
    static size_t wrapFunctionReturnZero();
};

#endif /* kern_applebacklight_hpp */
