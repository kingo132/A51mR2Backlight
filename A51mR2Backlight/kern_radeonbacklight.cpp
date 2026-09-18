//
//  kern_radeonbacklight.cpp
//  A51mR2Backlight
//
//  Navi10 internal-panel brightness routing for the Alienware Area-51m R2.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/graphics/IODisplay.h>

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

} // namespace

RadeonBacklightPatcher *RadeonBacklightPatcher::callback {nullptr};

void RadeonBacklightPatcher::registerKexts() {
    callback = this;
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
    SYSLOG("radeon", "RX 5000 PWM brightness hooks are ready");
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

void RadeonBacklightPatcher::updateMaxBrightnessFromRegistry() {
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

        if (maximum != nullptr && maximum->unsigned32BitValue() != 0) {
            maxBrightness = maximum->unsigned32BitValue();
            DBGLOG("radeon", "AppleBacklight max brightness is 0x%x", maxBrightness);
        }
    }

    iterator->release();
    matching->release();
}

uint32_t RadeonBacklightPatcher::convertBrightnessToPwm(uint32_t value,
                                                        uint32_t maxValue) {
    if (maxValue == 0)
        return 0;

    if (value >= maxValue)
        return 0x1FF00;

    const uint64_t percent = static_cast<uint64_t>(value) * 100ULL / maxValue;
    return static_cast<uint32_t>((percent * 0xFFULL / 100ULL) << 8U);
}

uint32_t RadeonBacklightPatcher::wrapDcePanelCntlHwInit(void *panelController) {
    const bool firstCapture = callback->panelController == nullptr;
    callback->panelController = panelController;
    callback->updateMaxBrightnessFromRegistry();

    if (firstCapture) {
        SYSLOG("radeon", "panel controller captured; max brightness 0x%x",
               callback->maxBrightness);
    }

    return FunctionCast(wrapDcePanelCntlHwInit,
                        callback->orgDcePanelCntlHwInit)(panelController);
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

    DBGLOG("radeon", "bklt write index=%u value=0x%llx ready=%d panel=%p",
           connectIndex,
           static_cast<uint64_t>(value),
           callback->hooksReady,
           callback->panelController);

    if (!callback->hooksReady ||
        callback->panelController == nullptr ||
        callback->orgDceDriverSetBacklight == nullptr ||
        callback->maxBrightness == 0) {
        DBGLOG("radeon", "brightness write ignored because PWM path is not ready");
        return result;
    }

    callback->currentBrightness = static_cast<uint32_t>(value);
    const uint32_t pwmValue = convertBrightnessToPwm(
        callback->currentBrightness,
        callback->maxBrightness);

    DBGLOG("radeon", "PWM write brightness=0x%x max=0x%x pwm=0x%x",
           callback->currentBrightness,
           callback->maxBrightness,
           pwmValue);

    callback->orgDceDriverSetBacklight(callback->panelController, pwmValue);
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
        *value = callback->currentBrightness;
        result = kIOReturnSuccess;
    }

    return result;
}
