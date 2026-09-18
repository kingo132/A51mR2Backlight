//
//  kern_applebacklight.cpp
//  A51mR2Backlight
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/graphics/IODisplay.h>

#include "kern_applebacklight.hpp"
#include "kern_applebacklight_data.hpp"

namespace {

const char *pathBacklight[] {
    "/System/Library/Extensions/AppleBacklight.kext/Contents/MacOS/AppleBacklight"
};

const char *pathMccsControl[] {
    "/System/Library/Extensions/AppleMCCSControl.kext/Contents/MacOS/AppleMCCSControl"
};

KernelPatcher::KextInfo kextBacklight {
    "com.apple.driver.AppleBacklight",
    pathBacklight,
    arrsize(pathBacklight),
    {true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

KernelPatcher::KextInfo kextMccsControl {
    "com.apple.driver.AppleMCCSControl",
    pathMccsControl,
    arrsize(pathMccsControl),
    {true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

} // namespace

AppleBacklightPatcher *AppleBacklightPatcher::callback {nullptr};

void AppleBacklightPatcher::registerKexts() {
    callback = this;

    lilu.onKextLoad(&kextBacklight);
    lilu.onKextLoadForce(&kextMccsControl);
}

bool AppleBacklightPatcher::processKext(KernelPatcher &patcher, size_t index,
                                        mach_vm_address_t address, size_t size) {
    if (kextBacklight.loadIndex == index) {
        patchAppleBacklight(patcher, index, address, size);
        return true;
    }

    if (kextMccsControl.loadIndex == index) {
        patchMccsControl(patcher, index, address, size);
        return true;
    }

    return false;
}

void AppleBacklightPatcher::patchAppleBacklight(KernelPatcher &patcher, size_t index,
                                                mach_vm_address_t address, size_t size) {
    SYSLOG("apple", "processing AppleBacklight");

    KernelPatcher::RouteRequest request(
        "__ZN15AppleIntelPanel10setDisplayEP9IODisplay",
        wrapApplePanelSetDisplay,
        orgApplePanelSetDisplay);

    if (!patcher.routeMultiple(index, &request, 1, address, size)) {
        SYSLOG("apple", "failed to route AppleIntelPanel::setDisplay");
        patcher.clearError();
        return;
    }

    const uint8_t find[] = {"F%uT%04x"};
    const uint8_t replace[] = {"F%uTxxxx"};
    KernelPatcher::LookupPatch panelIdPatch {
        &kextBacklight,
        find,
        replace,
        sizeof(find),
        1
    };

    patcher.applyLookupPatch(&panelIdPatch);
    if (patcher.getError() != KernelPatcher::Error::NoError) {
        SYSLOG("apple", "failed to patch AppleBacklight panel-id format (%d)",
               patcher.getError());
        patcher.clearError();
        return;
    }

    SYSLOG("apple", "AppleBacklight hooks ready");
}

void AppleBacklightPatcher::patchMccsControl(KernelPatcher &patcher, size_t index,
                                             mach_vm_address_t address, size_t size) {
    KernelPatcher::RouteRequest requests[] {
        {"__ZN25AppleMCCSControlGibraltar5probeEP9IOServicePi", wrapFunctionReturnZero},
        {"__ZN21AppleMCCSControlCello5probeEP9IOServicePi", wrapFunctionReturnZero},
    };

    if (!patcher.routeMultiple(index, requests, address, size)) {
        SYSLOG("apple", "failed to disable AppleMCCSControl probes");
        patcher.clearError();
        return;
    }

    SYSLOG("apple", "AppleMCCSControl probes disabled");
}

bool AppleBacklightPatcher::wrapApplePanelSetDisplay(IOService *that, IODisplay *display) {
    if (!callback->panelProfilesInstalled) {
        auto panels = OSDynamicCast(OSDictionary, that->getProperty("ApplePanels"));
        if (panels == nullptr) {
            SYSLOG("apple", "ApplePanels dictionary is unavailable; will retry");
        } else {
            auto rawCopy = panels->copyCollection();
            auto copiedPanels = OSDynamicCast(OSDictionary, rawCopy);

            if (copiedPanels != nullptr) {
                size_t installedProfiles {0};

                for (const auto &profile : A51BacklightData::ApplePanelProfiles) {
                    auto data = OSData::withBytes(profile.data, sizeof(profile.data));
                    if (data != nullptr) {
                        copiedPanels->setObject(profile.name, data);
                        ++installedProfiles;

                        // Match WhateverGreen's AppleBacklight path: the profile
                        // OSData objects are intentionally kept alive here.
                    } else {
                        SYSLOG("apple", "failed to allocate panel profile %s", profile.name);
                    }
                }

                if (installedProfiles == arrsize(A51BacklightData::ApplePanelProfiles) &&
                    that->setProperty("ApplePanels", copiedPanels)) {
                    callback->panelProfilesInstalled = true;
                    SYSLOG("apple", "installed %lu AppleBacklight panel profiles",
                           installedProfiles);
                } else {
                    SYSLOG("apple", "panel profile installation incomplete (%lu/%lu); will retry",
                           installedProfiles,
                           arrsize(A51BacklightData::ApplePanelProfiles));
                }
            } else {
                SYSLOG("apple", "failed to copy ApplePanels dictionary; will retry");
            }

            if (rawCopy != nullptr)
                rawCopy->release();
        }
    }

    const bool result = FunctionCast(
        wrapApplePanelSetDisplay,
        callback->orgApplePanelSetDisplay)(that, display);

    if (!callback->panelResultLogged) {
        callback->panelResultLogged = true;

        auto parameters = display
            ? OSDynamicCast(OSDictionary, display->getProperty("IODisplayParameters"))
            : nullptr;
        const bool hasLinearBrightness =
            parameters != nullptr && parameters->getObject("linear-brightness") != nullptr;

        SYSLOG("apple", "panel display set returned %d; linear-brightness=%d",
               result, hasLinearBrightness);
    }

    return result;
}

size_t AppleBacklightPatcher::wrapFunctionReturnZero() {
    return 0;
}
