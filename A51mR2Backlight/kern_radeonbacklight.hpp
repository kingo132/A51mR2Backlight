//
//  kern_radeonbacklight.hpp
//  A51mR2Backlight
//

#ifndef kern_radeonbacklight_hpp
#define kern_radeonbacklight_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>
#include <IOKit/graphics/IOFramebuffer.h>

class RadeonBacklightPatcher {
public:
    void registerKexts();
    bool processKext(KernelPatcher &patcher, size_t index,
                     mach_vm_address_t address, size_t size);

private:
    struct BytePattern {
        const uint8_t *bytes;
        size_t size;
        const char *name;
    };

    struct SignatureSet {
        BytePattern panelInit;
        BytePattern setBacklight;
    };

    using t_DceDriverSetBacklight = void (*)(void *panelController,
                                              uint32_t backlightPwmU16_16);

    static RadeonBacklightPatcher *callback;

    uint32_t currentBrightness {0};
    uint32_t maxBrightness {0xff7b};
    uint32_t lastPwmValue {0};
    uint32_t panelInitCount {0};
    uint32_t brightnessWriteCount {0};
    uint32_t brightnessReadCount {0};
    uint32_t brightnessCapabilityFallbackCount {0};
    uint32_t brightnessRestoreCount {0};
    void *panelController {nullptr};

    mach_vm_address_t orgDcePanelCntlHwInit {0};
    mach_vm_address_t orgFramebufferSetAttribute {0};
    mach_vm_address_t orgFramebufferGetAttribute {0};
    t_DceDriverSetBacklight orgDceDriverSetBacklight {nullptr};

    bool hooksReady {false};
    bool currentBrightnessValid {false};
    bool useLegacyPwmMapping {false};
    bool restoreAfterPanelInit {true};

    bool installHooks(KernelPatcher &patcher, size_t index,
                      mach_vm_address_t address, size_t size);
    bool routeFramebufferMethods(KernelPatcher &patcher, size_t index,
                                 mach_vm_address_t address, size_t size);
    bool resolveAndRoutePwmFunctions(KernelPatcher &patcher, size_t index,
                                     mach_vm_address_t address, size_t size);

    void refreshBrightnessStateFromRegistry();
    void publishDiagnostics();
    void applyCurrentBrightness(const char *reason);

    static SignatureSet signaturesForCurrentKernel();
    static mach_vm_address_t findUniquePattern(mach_vm_address_t address, size_t size,
                                               const BytePattern &pattern);
    static mach_vm_address_t solveSymbolOrPattern(KernelPatcher &patcher, size_t index,
                                                  mach_vm_address_t address, size_t size,
                                                  const char *symbol,
                                                  const BytePattern &pattern);
    static uint32_t convertBrightnessToPwm(uint32_t value, uint32_t maxValue,
                                           bool legacyMapping);

    static uint32_t wrapDcePanelCntlHwInit(void *panelController);
    static IOReturn wrapFramebufferSetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                 IOSelect attribute, uintptr_t value);
    static IOReturn wrapFramebufferGetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                 IOSelect attribute, uintptr_t *value);
};

#endif /* kern_radeonbacklight_hpp */
