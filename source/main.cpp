#include <string.h>

#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemBoolean.h>

#include <whb/log.h>
#include <whb/log_udp.h>
#include <coreinit/title.h>
#include <sysapp/title.h>
#include <gx2/surface.h>

#include "buttons.h"

WUPS_PLUGIN_NAME("Better Daily Log");
WUPS_PLUGIN_DESCRIPTION("An aroma plugin that improves the Daily Log user experience!");
WUPS_PLUGIN_VERSION("v1.1");
WUPS_PLUGIN_AUTHOR("Fangal, TheMysteriousStar");
WUPS_PLUGIN_LICENSE("GPLv3");

#define MIRROR_SCREENS_CONFIG_ID "mirrorScreens"
#define INPUT_REDIRECTION_CONFIG_ID "inputRedirection"

#define MIRROR_SCREENS_DEFAULT true
#define INPUT_REDIRECTION_DEFAULT true

WUPS_USE_STORAGE("Better_Daily_Log");

bool mirrorScreens = MIRROR_SCREENS_DEFAULT;
bool inputRedirection = INPUT_REDIRECTION_DEFAULT;
bool isConfigOpen = false;

uint64_t sysTID = 0;
uint64_t currentTID = 0;


/*
 * Called when the Mirror Screens option is changed.
 */
void mirrorScreensChanged(ConfigItemBoolean *item, bool newVal) {
    mirrorScreens = newVal;

    WUPSStorageError err =
        WUPSStorageAPI::Store(MIRROR_SCREENS_CONFIG_ID, mirrorScreens);

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to save mirrorScreens: %d",
            err
        );
    }
}


/*
 * Called when the Input Redirection option is changed.
 */
void inputRedirectionChanged(ConfigItemBoolean *item, bool newVal) {
    inputRedirection = newVal;

    WUPSStorageError err =
        WUPSStorageAPI::Store(INPUT_REDIRECTION_CONFIG_ID, inputRedirection);

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to save inputRedirection: %d",
            err
        );
    }
}


/*
 * Called when the Aroma configuration menu is opened.
 */
WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(
    WUPSConfigCategoryHandle rootHandle
) {
    try {
        WUPSConfigCategory root(rootHandle);

        auto options = WUPSConfigCategory::Create("Options");

        options.add(
            WUPSConfigItemBoolean::Create(
                MIRROR_SCREENS_CONFIG_ID,
                "Mirror GamePad screen to the TV",
                MIRROR_SCREENS_DEFAULT,
                mirrorScreens,
                &mirrorScreensChanged
            )
        );

        options.add(
            WUPSConfigItemBoolean::Create(
                INPUT_REDIRECTION_CONFIG_ID,
                "Redirect inputs",
                INPUT_REDIRECTION_DEFAULT,
                inputRedirection,
                &inputRedirectionChanged
            )
        );

        root.add(std::move(options));

    } catch (...) {
        WHBLogPrintf(
            "Better Daily Log: Failed to create configuration menu"
        );

        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    isConfigOpen = true;

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}


/*
 * Called when the Aroma configuration menu is closed.
 */
void ConfigMenuClosedCallback() {
    WUPSStorageError err = WUPSStorageAPI::SaveStorage();

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to save storage: %d",
            err
        );
    }

    isConfigOpen = false;
}


/*
 * Plugin initialization.
 */
INITIALIZE_PLUGIN() {
    WHBLogUdpInit();
    WHBLogPrintf("Hola from Better Daily Log!");

    sysTID =
        _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_DAILY_LOG);


    /*
     * Initialize the modern WUPS configuration API.
     */
    WUPSConfigAPIOptionsV1 configOptions = {
        .name = "Better Daily Log"
    };

    if (WUPSConfigAPI_Init(
            configOptions,
            ConfigMenuOpenedCallback,
            ConfigMenuClosedCallback
        ) != WUPSCONFIG_API_RESULT_SUCCESS) {

        WHBLogPrintf(
            "Better Daily Log: Failed to initialize config API"
        );
    }


    /*
     * Load saved settings, or create them with their defaults.
     */
    WUPSStorageError err;

    err = WUPSStorageAPI::GetOrStoreDefault(
        MIRROR_SCREENS_CONFIG_ID,
        mirrorScreens,
        MIRROR_SCREENS_DEFAULT
    );

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to load mirrorScreens: %d",
            err
        );
    }


    err = WUPSStorageAPI::GetOrStoreDefault(
        INPUT_REDIRECTION_CONFIG_ID,
        inputRedirection,
        INPUT_REDIRECTION_DEFAULT
    );

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to load inputRedirection: %d",
            err
        );
    }


    /*
     * Save the storage after loading/defaulting it.
     */
    err = WUPSStorageAPI::SaveStorage();

    if (err != WUPS_STORAGE_ERROR_SUCCESS) {
        WHBLogPrintf(
            "Better Daily Log: Failed to save storage: %d",
            err
        );
    }
}


ON_APPLICATION_START() {
    currentTID = OSGetTitleID();
}


/*
 * Mirror the GamePad/DRC screen onto the TV
 * when the Daily Log application is running.
 */
DECL_FUNCTION(
    void,
    GX2CopyColorBufferToScanBuffer,
    GX2ColorBuffer *colorBuffer,
    GX2ScanTarget scan_target
) {
    if (currentTID == sysTID) {

        if (mirrorScreens) {

            if (scan_target == GX2_SCAN_TARGET_DRC) {
                real_GX2CopyColorBufferToScanBuffer(
                    colorBuffer,
                    GX2_SCAN_TARGET_DRC | GX2_SCAN_TARGET_TV
                );
            } else {
                real_GX2CopyColorBufferToScanBuffer(
                    colorBuffer,
                    scan_target
                );
            }

        } else {
            real_GX2CopyColorBufferToScanBuffer(
                colorBuffer,
                scan_target
            );
        }

    } else {
        real_GX2CopyColorBufferToScanBuffer(
            colorBuffer,
            scan_target
        );
    }
}


/*
 * Redirect GamePad input from a Pro Controller/
 * Classic Controller when no GamePad is available.
 */
DECL_FUNCTION(
    int32_t,
    VPADRead,
    VPADChan chan,
    VPADStatus *vStatus,
    uint32_t size,
    VPADReadError *err
) {
    int32_t res =
        real_VPADRead(chan, vStatus, size, err);

    if (
        currentTID == sysTID &&
        !isConfigOpen &&
        inputRedirection
    ) {

        /*
         * If a GamePad isn't connected, pretend that
         * the VPAD read succeeded so we can inject
         * controller input.
         */
        if (err && *err != VPAD_READ_SUCCESS) {
            *err = VPAD_READ_SUCCESS;
            res = 1;

            memset(
                vStatus,
                0,
                sizeof(VPADStatus)
            );
        }

        calcVPAD(vStatus);
    }

    return res;
}


/*
 * Read Classic Controller / Wii U Pro Controller input.
 */
DECL_FUNCTION(
    int32_t,
    KPADReadEx,
    KPADChan chan,
    KPADStatus *kStatus,
    uint32_t size,
    KPADError *err
) {
    int32_t res =
        real_KPADReadEx(
            chan,
            kStatus,
            size,
            err
        );

    if (currentTID == sysTID) {
        calcKPAD(kStatus);
    }

    return res;
}


/*
 * Function replacements.
 */
WUPS_MUST_REPLACE(
    GX2CopyColorBufferToScanBuffer,
    WUPS_LOADER_LIBRARY_GX2,
    GX2CopyColorBufferToScanBuffer
);

WUPS_MUST_REPLACE(
    KPADReadEx,
    WUPS_LOADER_LIBRARY_PADSCORE,
    KPADReadEx
);

WUPS_MUST_REPLACE(
    VPADRead,
    WUPS_LOADER_LIBRARY_VPAD,
    VPADRead
);
