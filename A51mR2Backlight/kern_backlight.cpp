//
//  kern_backlight.cpp
//  A51mR2Backlight
//
//  This file intentionally contains only the pieces required for the
//  Alienware Area-51m R2 / Navi10 internal-panel brightness path.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/graphics/IODisplay.h>

#include "kern_backlight.hpp"

static const char *pathRadeonX6000Framebuffer[] {
    "/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/MacOS/AMDRadeonX6000Framebuffer"
};
static const char *pathBacklight[] {
    "/System/Library/Extensions/AppleBacklight.kext/Contents/MacOS/AppleBacklight"
};
static const char *pathMCCSControl[] {
    "/System/Library/Extensions/AppleMCCSControl.kext/Contents/MacOS/AppleMCCSControl"
};

static KernelPatcher::KextInfo kextRadeonX6000Framebuffer {
    "com.apple.kext.AMDRadeonX6000Framebuffer", pathRadeonX6000Framebuffer,
    arrsize(pathRadeonX6000Framebuffer), {}, {}, KernelPatcher::KextInfo::Unloaded
};
static KernelPatcher::KextInfo kextBacklight {
    "com.apple.driver.AppleBacklight", pathBacklight, arrsize(pathBacklight),
    {true}, {}, KernelPatcher::KextInfo::Unloaded
};
static KernelPatcher::KextInfo kextMCCSControl {
    "com.apple.driver.AppleMCCSControl", pathMCCSControl, arrsize(pathMCCSControl),
    {true}, {}, KernelPatcher::KextInfo::Unloaded
};

A51BKL *A51BKL::callback;

A51BKL::ApplePanelData A51BKL::appleBacklightData[] {
    {
        "F14Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x00, 0x34, 0x00, 0x52, 0x00, 0x73, 0x00, 0x94, 0x00, 0xBE, 0x00, 0xFA,
            0x01, 0x36, 0x01, 0x72, 0x01, 0xC5, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x1A, 0x05, 0x0A,
            0x06, 0x0E, 0x07, 0x10
        }
    },
    {
        "F15Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x00, 0x36, 0x00, 0x54, 0x00, 0x7D, 0x00, 0xB2, 0x00, 0xF5, 0x01, 0x49,
            0x01, 0xB1, 0x02, 0x2B, 0x02, 0xB8, 0x03, 0x59, 0x04, 0x13, 0x04, 0xEC, 0x05, 0xF3, 0x07, 0x34,
            0x08, 0xAF, 0x0A, 0xD9
        }
    },
    {
        "F16Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x00, 0x18, 0x00, 0x27, 0x00, 0x3A, 0x00, 0x52, 0x00, 0x71, 0x00, 0x96,
            0x00, 0xC4, 0x00, 0xFC, 0x01, 0x40, 0x01, 0x93, 0x01, 0xF6, 0x02, 0x6E, 0x02, 0xFE, 0x03, 0xAA,
            0x04, 0x78, 0x05, 0x6C
        }
    },
    {
        "F17Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x34, 0x00, 0x4F, 0x00, 0x71, 0x00, 0x9B, 0x00, 0xCF,
            0x01, 0x0E, 0x01, 0x5D, 0x01, 0xBB, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x29, 0x05, 0x1E,
            0x06, 0x44, 0x07, 0xA1
        }
    },
    {
        "F18Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x00, 0x53, 0x00, 0x8C, 0x00, 0xD5, 0x01, 0x31, 0x01, 0xA2, 0x02, 0x2E,
            0x02, 0xD8, 0x03, 0xAE, 0x04, 0xAC, 0x05, 0xE5, 0x07, 0x59, 0x09, 0x1C, 0x0B, 0x3B, 0x0D, 0xD0,
            0x10, 0xEA, 0x14, 0x99
        }
    },
    {
        "F19Txxxx",
        {
            0x00, 0x11, 0x00, 0x00, 0x02, 0x8F, 0x03, 0x53, 0x04, 0x5A, 0x05, 0xA1, 0x07, 0xAE, 0x0A, 0x3D,
            0x0E, 0x14, 0x13, 0x74, 0x1A, 0x5E, 0x24, 0x18, 0x31, 0xA9, 0x44, 0x59, 0x5E, 0x76, 0x83, 0x11,
            0xB6, 0xC7, 0xFF, 0x7B
        }
    },
    {
        "F24Txxxx",
        {
            0x00, 0x11, 0x00, 0x01, 0x00, 0x34, 0x00, 0x52, 0x00, 0x73, 0x00, 0x94, 0x00, 0xBE, 0x00, 0xFA,
            0x01, 0x36, 0x01, 0x72, 0x01, 0xC5, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x1A, 0x05, 0x0A,
            0x06, 0x0E, 0x07, 0x10
        }
    }
};

void A51BKL::init() {
    callback = this;

    lilu.onKextLoadForce(nullptr, 0,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<A51BKL *>(user)->processKext(patcher, index, address, size);
        }, this);

    // These are the only three Apple components this plugin needs.
    lilu.onKextLoadForce(&kextRadeonX6000Framebuffer);
    lilu.onKextLoad(&kextBacklight);
    lilu.onKextLoadForce(&kextMCCSControl);
}

void A51BKL::processKext(KernelPatcher &patcher, size_t index,
                                   mach_vm_address_t address, size_t size) {
    if (kextRadeonX6000Framebuffer.loadIndex == index) {
        processRadeonFramebuffer(patcher, index, address, size);
        return;
    }

    if (kextBacklight.loadIndex == index) {
        processAppleBacklight(patcher, index, address, size);
        return;
    }

    if (kextMCCSControl.loadIndex == index) {
        processMCCSControl(patcher, index, address, size);
        return;
    }
}

mach_vm_address_t A51BKL::findUniquePattern(mach_vm_address_t address, size_t size,
                                                      const uint8_t *pattern, size_t patternSize,
                                                      const char *name) {
    if (address == 0 || pattern == nullptr || patternSize == 0 || size < patternSize)
        return 0;

    auto base = reinterpret_cast<const uint8_t *>(address);
    mach_vm_address_t found = 0;
    size_t matches = 0;

    for (size_t i = 0; i <= size - patternSize; i++) {
        bool equal = true;
        for (size_t j = 0; j < patternSize; j++) {
            if (base[i + j] != pattern[j]) {
                equal = false;
                break;
            }
        }

        if (equal) {
            matches++;
            found = address + i;
            if (matches > 1)
                break;
        }
    }

    if (matches == 1) {
        DBGLOG("a51bkl", "found unique %s pattern at %p", name, reinterpret_cast<void *>(found));
        return found;
    }

    SYSLOG("a51bkl", "%s signature match count is %lu; refusing unsafe hook", name, matches);
    return 0;
}

mach_vm_address_t A51BKL::solveSymbolOrPattern(KernelPatcher &patcher, size_t index,
                                                         mach_vm_address_t address, size_t size,
                                                         const char *symbol,
                                                         const uint8_t *pattern, size_t patternSize,
                                                         const char *name) {
    auto solved = patcher.solveSymbol<mach_vm_address_t>(index, symbol, address, size);
    if (patcher.getError() == KernelPatcher::Error::NoError && solved != 0) {
        DBGLOG("a51bkl", "resolved %s by symbol at %p", name, reinterpret_cast<void *>(solved));
        return solved;
    }

    patcher.clearError();
    DBGLOG("a51bkl", "%s symbol unavailable; trying validated signature", name);
    return findUniquePattern(address, size, pattern, patternSize, name);
}

void A51BKL::processRadeonFramebuffer(KernelPatcher &patcher, size_t index,
                                                mach_vm_address_t address, size_t size) {
    // C++ framebuffer methods remain routable on the versions this patch targets.
    KernelPatcher::RouteRequest framebufferRequests[] = {
        {"__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25setAttributeForConnectionEijm",
            wrapFramebufferSetAttribute, orgFramebufferSetAttribute},
        {"__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25getAttributeForConnectionEijPm",
            wrapFramebufferGetAttribute, orgFramebufferGetAttribute},
    };

    if (!patcher.routeMultiple(index, framebufferRequests, address, size, true, true)) {
        SYSLOG("a51bkl", "failed to route AMDRadeonX6000 framebuffer brightness methods");
        patcher.clearError();
        return;
    }

    // These 20-byte prologues come from the user's working WEG fork.  Apple
    // stopped exposing the two C symbols reliably on newer macOS versions.
    static const uint8_t panelInitLegacy[] {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55,
        0x41, 0x54, 0x53, 0x50, 0x49, 0x89, 0xFD, 0x4C, 0x8D, 0x45
    };
    static const uint8_t panelInitModern[] {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54,
        0x53, 0x48, 0x83, 0xEC, 0x10, 0x48, 0x89, 0xFB, 0x4C, 0x8D
    };
    static const uint8_t setBacklightLegacy[] {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55,
        0x41, 0x54, 0x53, 0x50, 0x41, 0x89, 0xF7, 0x49, 0x89, 0xFE
    };
    static const uint8_t setBacklightModern[] {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55,
        0x41, 0x54, 0x53, 0x50, 0x41, 0x89, 0xF6, 0x48, 0x89, 0xFB
    };

    const bool modern = getKernelVersion() >= KernelVersion::Sonoma;
    const uint8_t *panelPattern = modern ? panelInitModern : panelInitLegacy;
    const uint8_t *backlightPattern = modern ? setBacklightModern : setBacklightLegacy;

    auto panelInit = solveSymbolOrPattern(patcher, index, address, size,
        "_dce_panel_cntl_hw_init", panelPattern, sizeof(panelInitLegacy), "dce_panel_cntl_hw_init");
    if (panelInit == 0)
        return;

    auto driverSetBacklight = solveSymbolOrPattern(patcher, index, address, size,
        "_dce_driver_set_backlight", backlightPattern, sizeof(setBacklightLegacy), "dce_driver_set_backlight");
    if (driverSetBacklight == 0)
        return;

    orgDcePanelCntlHwInit = patcher.routeFunction(panelInit,
        reinterpret_cast<mach_vm_address_t>(wrapDcePanelCntlHwInit), true);
    if (patcher.getError() != KernelPatcher::Error::NoError || orgDcePanelCntlHwInit == 0) {
        SYSLOG("a51bkl", "failed to route dce_panel_cntl_hw_init (%d)", patcher.getError());
        patcher.clearError();
        return;
    }

    orgDceDriverSetBacklight = reinterpret_cast<t_DceDriverSetBacklight>(driverSetBacklight);
    amdHooksReady = true;
    SYSLOG("a51bkl", "RX 5000 PWM brightness hooks are ready");
}

void A51BKL::processAppleBacklight(KernelPatcher &patcher, size_t index,
                                             mach_vm_address_t address, size_t size) {
    KernelPatcher::RouteRequest request(
        "__ZN15AppleIntelPanel10setDisplayEP9IODisplay",
        wrapApplePanelSetDisplay, orgApplePanelSetDisplay);

    if (!patcher.routeMultiple(index, &request, 1, address, size)) {
        SYSLOG("a51bkl", "failed to route AppleIntelPanel::setDisplay");
        patcher.clearError();
        return;
    }

    const uint8_t find[] = {"F%uT%04x"};
    const uint8_t replace[] = {"F%uTxxxx"};
    KernelPatcher::LookupPatch patch = {&kextBacklight, find, replace, sizeof(find), 1};
    patcher.applyLookupPatch(&patch);
    if (patcher.getError() != KernelPatcher::Error::NoError) {
        SYSLOG("a51bkl", "failed to patch AppleBacklight panel-id format (%d)", patcher.getError());
        patcher.clearError();
    } else {
        DBGLOG("a51bkl", "AppleBacklight panel profiles enabled");
    }
}

void A51BKL::processMCCSControl(KernelPatcher &patcher, size_t index,
                                         mach_vm_address_t address, size_t size) {
    KernelPatcher::RouteRequest requests[] = {
        {"__ZN25AppleMCCSControlGibraltar5probeEP9IOServicePi", wrapFunctionReturnZero},
        {"__ZN21AppleMCCSControlCello5probeEP9IOServicePi", wrapFunctionReturnZero},
    };

    if (!patcher.routeMultiple(index, requests, address, size)) {
        SYSLOG("a51bkl", "failed to disable AppleMCCSControl probes");
        patcher.clearError();
    }
}

void A51BKL::updatePwmMaxBrightnessFromInternalDisplay() {
    OSDictionary *matching = IOService::serviceMatching("AppleBacklightDisplay");
    if (matching == nullptr)
        return;

    OSIterator *iter = IOService::getMatchingServices(matching);
    if (iter == nullptr) {
        matching->release();
        return;
    }

    auto display = OSDynamicCast(IORegistryEntry, iter->getNextObject());
    if (display != nullptr) {
        auto params = OSDynamicCast(OSDictionary, display->getProperty("IODisplayParameters"));
        auto linear = params ? OSDynamicCast(OSDictionary, params->getObject("linear-brightness")) : nullptr;
        auto maxBrightness = linear ? OSDynamicCast(OSNumber, linear->getObject("max")) : nullptr;
        if (maxBrightness != nullptr && maxBrightness->unsigned32BitValue() != 0) {
            maxPwmBacklightLvl = maxBrightness->unsigned32BitValue();
            DBGLOG("a51bkl", "AppleBacklight max brightness is 0x%x", maxPwmBacklightLvl);
        }
    }

    iter->release();
    matching->release();
}

uint32_t A51BKL::wrapDcePanelCntlHwInit(void *panelCntl) {
    callback->panelCntlPtr = panelCntl;
    callback->updatePwmMaxBrightnessFromInternalDisplay();
    return FunctionCast(wrapDcePanelCntlHwInit, callback->orgDcePanelCntlHwInit)(panelCntl);
}

IOReturn A51BKL::wrapFramebufferSetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                       IOSelect attribute, uintptr_t value) {
    IOReturn ret = FunctionCast(wrapFramebufferSetAttribute, callback->orgFramebufferSetAttribute)(
        framebuffer, connectIndex, attribute, value);

    if (attribute != static_cast<UInt32>('bklt'))
        return ret;

    if (!callback->amdHooksReady || callback->panelCntlPtr == nullptr ||
        callback->orgDceDriverSetBacklight == nullptr || callback->maxPwmBacklightLvl == 0) {
        DBGLOG("a51bkl", "brightness write ignored because PWM path is not ready");
        return ret;
    }

    callback->curPwmBacklightLvl = static_cast<uint32_t>(value);

    uint32_t pwmValue;
    if (callback->curPwmBacklightLvl >= callback->maxPwmBacklightLvl) {
        // AMD's DMCU conversion expects bit 16 to be set for full brightness.
        pwmValue = 0x1FF00;
    } else {
        const uint64_t percent = static_cast<uint64_t>(callback->curPwmBacklightLvl) * 100ULL /
                                 callback->maxPwmBacklightLvl;
        pwmValue = static_cast<uint32_t>((percent * 0xFFULL / 100ULL) << 8U);
    }

    callback->orgDceDriverSetBacklight(callback->panelCntlPtr, pwmValue);
    return kIOReturnSuccess;
}

IOReturn A51BKL::wrapFramebufferGetAttribute(IOService *framebuffer, IOIndex connectIndex,
                                                       IOSelect attribute, uintptr_t *value) {
    IOReturn ret = FunctionCast(wrapFramebufferGetAttribute, callback->orgFramebufferGetAttribute)(
        framebuffer, connectIndex, attribute, value);

    if (callback->amdHooksReady && attribute == static_cast<UInt32>('bklt') && value != nullptr) {
        *value = callback->curPwmBacklightLvl;
        ret = kIOReturnSuccess;
    }

    return ret;
}

bool A51BKL::wrapApplePanelSetDisplay(IOService *that, IODisplay *display) {
    if (!callback->applePanelDisplaySet) {
        callback->applePanelDisplaySet = true;

        auto panels = OSDynamicCast(OSDictionary, that->getProperty("ApplePanels"));
        if (panels != nullptr) {
            auto rawPanels = panels->copyCollection();
            auto copiedPanels = OSDynamicCast(OSDictionary, rawPanels);

            if (copiedPanels != nullptr) {
                for (auto &entry : appleBacklightData) {
                    auto data = OSData::withBytes(entry.deviceData, sizeof(entry.deviceData));
                    if (data != nullptr)
                        copiedPanels->setObject(entry.deviceName, data);
                }
                that->setProperty("ApplePanels", copiedPanels);
            }

            if (rawPanels != nullptr)
                rawPanels->release();
        }
    }

    return FunctionCast(wrapApplePanelSetDisplay, callback->orgApplePanelSetDisplay)(that, display);
}

size_t A51BKL::wrapFunctionReturnZero() {
    return 0;
}
