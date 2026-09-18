//
//  kern_radeonbacklight.cpp
//  A51mR2Backlight
//
//  Navi10 internal-panel brightness routing for the Alienware Area-51m R2.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/graphics/IODisplay.h>

#include "kern_diagnostics.hpp"
#include "kern_radeonbacklight.hpp"
#include "kern_radeon_signatures.hpp"

namespace {

const char *pathRadeonX6000Framebuffer[] {
    "/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/MacOS/AMDRadeonX6000Framebuffer"
};

KernelPatcher::KextInfo kextRadeonX6000Framebuffer {
    "com.apple.kext.AMDRadeonX6000Framebuffer",
    pathRadeonX6000Framebuffer,
    arrsize(pathRadeonX6000Framebuffer),
    {},
    {},
    KernelPatcher::KextInfo::Unloaded
};

constexpr UInt32 BacklightAttribute = static_cast<UInt32>('bklt');
constexpr uint32_t FullBrightnessPwm = 0x1FF00;
constexpr uint32_t MaxEightBitBacklightBelowFull = 0xFE;

} // namespace

RadeonBacklightPatcher *RadeonBacklightPatcher::callback {nullptr};

void RadeonBacklightPatcher::registerKexts() {
    callback = this;

    useLegacyPwmMapping = checkKernelArgument("-a51bkllegacycurve");
    restoreAfterPanelInit = !checkKernelArgument("-a51bklnorestore");

    A51Diagnostics::setString(
        "PwmMapping",
        useLegacyPwmMapping ? "legacy-100-step" : "linear-8-bit");
    A51Diagnostics::setBool("RestoreAfterPanelInit", restoreAfterPanelInit);

    lilu.onKextLoadForce(&kextRadeonX6000Framebuffer);
}

bool RadeonBacklightPatcher::processKext(KernelPatcher &patcher, size_t index,
                                         mach_vm_address_t address, size_t size) {
    if (kextRadeonX6000Framebuffer.loadIndex != index)
        return false;

    SYSLOG("radeon", "processing AMDRadeonX6000Framebuffer");
    installHooks(patcher, index, address, size);
    return true;
}

bool RadeonBacklightPatcher::installHooks(KernelPatcher &patcher, size_t index,
                                          mach_vm_address_t address, size_t size) {
    if (!routeFramebufferMethods(patcher, index, address, size))
        return false;

    if (!resolveAndRoutePwmFunctions(patcher, index, address, size))
        return false;

    hooksReady = true;
    publishDiagnostics();
    SYSLOG("radeon", "RX 5000 PWM brightness hooks are ready (%s mapping)",
           useLegacyPwmMapping ? "legacy" : "linear-8-bit");
    return true;
}

bool RadeonBacklightPatcher::routeFramebufferMethods(KernelPatcher &patcher, size_t index,
                                                      mach_vm_address_t address, size_t size) {
    KernelPatcher::RouteRequest requests[] {
        {
            "__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25setAttributeForConnectionEijm",
            wrapFramebufferSetAttribute,
            orgFramebufferSetAttribute
        },
        {
            "__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25getAttributeForConnectionEijPm",
            wrapFramebufferGetAttribute,
            orgFramebufferGetAttribute
        },
    };

    if (!patcher.routeMultiple(index, requests, address, size, true, true)) {
        SYSLOG("radeon", "failed to route framebuffer brightness methods");
        patcher.clearError();
        return false;
    }

    SYSLOG("radeon", "framebuffer brightness methods routed");
    return true;
}

bool RadeonBacklightPatcher::resolveAndRoutePwmFunctions(KernelPatcher &patcher, size_t index,
                                                         mach_vm_address_t address, size_t size) {
    const auto signatures = signaturesForCurrentKernel();

    const auto panelInit = solveSymbolOrPattern(
        patcher, index, address, size,
        "_dce_panel_cntl_hw_init", signatures.panelInit);
    if (panelInit == 0)
        return false;

    const auto driverSetBacklight = solveSymbolOrPattern(
        patcher, index, address, size,
        "_dce_driver_set_backlight", signatures.setBacklight);
    if (driverSetBacklight == 0)
        return false;

    orgDcePanelCntlHwInit = patcher.routeFunction(
        panelInit,
        reinterpret_cast<mach_vm_address_t>(wrapDcePanelCntlHwInit),
        true);

    if (patcher.getError() != KernelPatcher::Error::NoError ||
        orgDcePanelCntlHwInit == 0) {
        SYSLOG("radeon", "failed to route dce_panel_cntl_hw_init (%d)",
               patcher.getError());
        patcher.clearError();
        return false;
    }

    orgDceDriverSetBacklight =
        reinterpret_cast<t_DceDriverSetBacklight>(driverSetBacklight);

    SYSLOG("radeon", "private PWM functions resolved and panel-init routed");
    return true;
}

RadeonBacklightPatcher::SignatureSet RadeonBacklightPatcher::signaturesForCurrentKernel() {
    if (getKernelVersion() >= KernelVersion::Sonoma) {
        return {
            {A51RadeonSignatures::PanelInitModern, sizeof(A51RadeonSignatures::PanelInitModern), "dce_panel_cntl_hw_init"},
            {A51RadeonSignatures::SetBacklightModern, sizeof(A51RadeonSignatures::SetBacklightModern), "dce_driver_set_backlight"}
        };
    }

    return {
        {A51RadeonSignatures::PanelInitLegacy, sizeof(A51RadeonSignatures::PanelInitLegacy), "dce_panel_cntl_hw_init"},
        {A51RadeonSignatures::SetBacklightLegacy, sizeof(A51RadeonSignatures::SetBacklightLegacy), "dce_driver_set_backlight"}
    };
}

mach_vm_address_t RadeonBacklightPatcher::findUniquePattern(mach_vm_address_t address,
                                                            size_t size,
                                                            const BytePattern &pattern) {
    if (address == 0 || pattern.bytes == nullptr || pattern.size == 0 || size < pattern.size)
        return 0;

    const auto base = reinterpret_cast<const uint8_t *>(address);
    mach_vm_address_t found {0};
    size_t matches {0};

    for (size_t offset = 0; offset <= size - pattern.size; ++offset) {
        bool equal = true;

        for (size_t i = 0; i < pattern.size; ++i) {
            if (base[offset + i] != pattern.bytes[i]) {
                equal = false;
                break;
            }
        }

        if (!equal)
            continue;

        found = address + offset;
        if (++matches > 1)
            break;
    }

    if (matches == 1) {
        DBGLOG("radeon", "found unique %s signature at %p",
               pattern.name, reinterpret_cast<void *>(found));
        return found;
    }

    SYSLOG("radeon", "%s signature match count is %lu; refusing unsafe hook",
           pattern.name, matches);
    return 0;
}

mach_vm_address_t RadeonBacklightPatcher::solveSymbolOrPattern(
    KernelPatcher &patcher, size_t index,
    mach_vm_address_t address, size_t size,
    const char *symbol, const BytePattern &pattern) {

    auto solved = patcher.solveSymbol<mach_vm_address_t>(
        index, symbol, address, size);

    if (patcher.getError() == KernelPatcher::Error::NoError && solved != 0) {
        DBGLOG("radeon", "resolved %s by symbol at %p",
               pattern.name, reinterpret_cast<void *>(solved));
        return solved;
    }

    patcher.clearError();
    DBGLOG("radeon", "%s symbol unavailable; trying validated signature",
           pattern.name);
    return findUniquePattern(address, size, pattern);
}

void RadeonBacklightPatcher::refreshBrightnessStateFromRegistry() {
    OSDictionary *matching = IOService::serviceMatching("AppleBacklightDisplay");
    if (matching == nullptr) {
        DBGLOG("radeon", "AppleBacklightDisplay matching dictionary is unavailable");
        return;
    }

    OSIterator *iterator = IOService::getMatchingServices(matching);
    if (iterator == nullptr) {
        matching->release();
        DBGLOG("radeon", "AppleBacklightDisplay service is unavailable");
        return;
    }

    auto display = OSDynamicCast(IORegistryEntry, iterator->getNextObject());
    if (display != nullptr) {
        auto parameters = OSDynamicCast(
            OSDictionary, display->getProperty("IODisplayParameters"));
        auto linearBrightness = parameters
            ? OSDynamicCast(OSDictionary, parameters->getObject("linear-brightness"))
            : nullptr;
        auto maximum = linearBrightness
            ? OSDynamicCast(OSNumber, linearBrightness->getObject("max"))
            : nullptr;
        auto current = linearBrightness
            ? OSDynamicCast(OSNumber, linearBrightness->getObject("value"))
            : nullptr;

        if (maximum != nullptr && maximum->unsigned32BitValue() != 0)
            maxBrightness = maximum->unsigned32BitValue();

        if (!currentBrightnessValid && current != nullptr) {
            currentBrightness = current->unsigned32BitValue();
            currentBrightnessValid = true;
            DBGLOG("radeon", "initial brightness from AppleBacklight is 0x%x / 0x%x",
                   currentBrightness, maxBrightness);
        }
    }

    iterator->release();
    matching->release();
    publishDiagnostics();
}

void RadeonBacklightPatcher::publishDiagnostics() {
    A51Diagnostics::setBool("RadeonHooksReady", hooksReady);
    A51Diagnostics::setBool("PanelControllerCaptured", panelController != nullptr);
    A51Diagnostics::setBool("CurrentBrightnessValid", currentBrightnessValid);
    A51Diagnostics::setBool("RestoreAfterPanelInit", restoreAfterPanelInit);
    A51Diagnostics::setString(
        "PwmMapping",
        useLegacyPwmMapping ? "legacy-100-step" : "linear-8-bit");
    A51Diagnostics::setUInt32("PanelInitCount", panelInitCount);
    A51Diagnostics::setUInt32("BrightnessWriteCount", brightnessWriteCount);
    A51Diagnostics::setUInt32("BrightnessReadCount", brightnessReadCount);
    A51Diagnostics::setUInt32("BrightnessCapabilityFallbackCount",
                              brightnessCapabilityFallbackCount);
    A51Diagnostics::setUInt32("BrightnessRestoreCount", brightnessRestoreCount);
    A51Diagnostics::setUInt32("CurrentBrightness", currentBrightness);
    A51Diagnostics::setUInt32("MaxBrightness", maxBrightness);
    A51Diagnostics::setUInt32("LastPWM", lastPwmValue);
}

void RadeonBacklightPatcher::applyCurrentBrightness(const char *reason) {
    if (!hooksReady ||
        !currentBrightnessValid ||
        panelController == nullptr ||
        orgDceDriverSetBacklight == nullptr ||
        maxBrightness == 0) {
        return;
    }

    lastPwmValue = convertBrightnessToPwm(
        currentBrightness,
        maxBrightness,
        useLegacyPwmMapping);

    orgDceDriverSetBacklight(panelController, lastPwmValue);

    DBGLOG("radeon", "%s brightness=0x%x max=0x%x pwm=0x%x",
           reason,
           currentBrightness,
           maxBrightness,
           lastPwmValue);

    publishDiagnostics();
}

uint32_t RadeonBacklightPatcher::convertBrightnessToPwm(uint32_t value,
                                                        uint32_t maxValue,
                                                        bool legacyMapping) {
    if (maxValue == 0)
        return 0;

    if (value >= maxValue)
        return FullBrightnessPwm;

    if (legacyMapping) {
        const uint64_t percent = static_cast<uint64_t>(value) * 100ULL / maxValue;
        return static_cast<uint32_t>((percent * 0xFFULL / 100ULL) << 8U);
    }

    // AppleBacklight already converts the user-facing brightness curve into
    // linear-brightness. Preserve that linear domain and only quantise once to
    // AMD's 8-bit backlight value. Reserve 0xFF for the explicit full-brightness
    // encoding above, where bit 16 must be set (0x1FF00).
    uint64_t level =
        (static_cast<uint64_t>(value) * 0xFFULL + maxValue / 2ULL) / maxValue;
    if (level > MaxEightBitBacklightBelowFull)
        level = MaxEightBitBacklightBelowFull;

    return static_cast<uint32_t>(level << 8U);
}

uint32_t RadeonBacklightPatcher::wrapDcePanelCntlHwInit(void *panelController) {
    callback->panelController = panelController;
    ++callback->panelInitCount;
    callback->refreshBrightnessStateFromRegistry();

    const uint32_t result = FunctionCast(
        wrapDcePanelCntlHwInit,
        callback->orgDcePanelCntlHwInit)(panelController);

    if (callback->panelInitCount == 1) {
        SYSLOG("radeon", "panel controller captured; max brightness 0x%x current 0x%x valid=%d",
               callback->maxBrightness,
               callback->currentBrightness,
               callback->currentBrightnessValid);
    } else {
        DBGLOG("radeon", "panel init #%u; max 0x%x current 0x%x valid=%d",
               callback->panelInitCount,
               callback->maxBrightness,
               callback->currentBrightness,
               callback->currentBrightnessValid);
    }

    if (callback->restoreAfterPanelInit &&
        callback->panelInitCount > 1 &&
        callback->currentBrightnessValid) {
        ++callback->brightnessRestoreCount;
        callback->applyCurrentBrightness("restored after panel init");
    }

    callback->publishDiagnostics();
    return result;
}

IOReturn RadeonBacklightPatcher::wrapFramebufferSetAttribute(
    IOService *framebuffer, IOIndex connectIndex,
    IOSelect attribute, uintptr_t value) {

    IOReturn result = FunctionCast(
        wrapFramebufferSetAttribute,
        callback->orgFramebufferSetAttribute)(
            framebuffer, connectIndex, attribute, value);

    if (attribute != BacklightAttribute)
        return result;

    ++callback->brightnessWriteCount;
    callback->currentBrightness = static_cast<uint32_t>(value);
    callback->currentBrightnessValid = true;

    DBGLOG("radeon", "bklt write #%u index=%u value=0x%llx ready=%d panel=%p",
           callback->brightnessWriteCount,
           connectIndex,
           static_cast<uint64_t>(value),
           callback->hooksReady,
           callback->panelController);

    if (!callback->hooksReady ||
        callback->panelController == nullptr ||
        callback->orgDceDriverSetBacklight == nullptr ||
        callback->maxBrightness == 0) {
        callback->publishDiagnostics();
        DBGLOG("radeon", "brightness write recorded but PWM path is not ready");
        return result;
    }

    callback->applyCurrentBrightness("PWM write");
    return kIOReturnSuccess;
}

IOReturn RadeonBacklightPatcher::wrapFramebufferGetAttribute(
    IOService *framebuffer, IOIndex connectIndex,
    IOSelect attribute, uintptr_t *value) {

    IOReturn result = FunctionCast(
        wrapFramebufferGetAttribute,
        callback->orgFramebufferGetAttribute)(
            framebuffer, connectIndex, attribute, value);

    if (callback->hooksReady &&
        attribute == BacklightAttribute &&
        value != nullptr) {
        ++callback->brightnessReadCount;

        // This hook is not only a cached-value accessor. AppleIntelPanelA uses
        // getAttributeForConnection('bklt') as a capability probe before it
        // calls AppleIntelPanel::setDisplay. The original WEG Navi10 patch
        // deliberately returns success here even before a meaningful cached
        // brightness exists. Requiring CurrentBrightnessValid makes the probe
        // fail, so AppleIntelPanelA exits early and never builds
        // linear-brightness. Keep state validity separate from capability.
        if (callback->currentBrightnessValid) {
            *value = callback->currentBrightness;
        } else if (result != kIOReturnSuccess) {
            *value = 0;
            ++callback->brightnessCapabilityFallbackCount;
        }

        DBGLOG("radeon",
               "bklt read #%u original=0x%x valid=%d value=0x%llx fallback=%u",
               callback->brightnessReadCount,
               result,
               callback->currentBrightnessValid,
               static_cast<uint64_t>(*value),
               callback->brightnessCapabilityFallbackCount);

        result = kIOReturnSuccess;
        callback->publishDiagnostics();
    }

    return result;
}
