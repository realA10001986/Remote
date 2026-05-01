/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * CRSF kludge: Stuff that is called from the main prop firmware
 *
 * -------------------------------------------------------------------
 * License: MIT NON-AI
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * In addition, the following restrictions apply:
 *
 * 1. The Software and any modifications made to it may not be used
 * for the purpose of training or improving machine learning algorithms,
 * including but not limited to artificial intelligence, natural
 * language processing, or data mining. This condition applies to any
 * derivatives, modifications, or updates based on the Software code.
 * Any usage of the Software in an AI-training dataset is considered a
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for
 * training or improving machine learning algorithms, including but
 * not limited to artificial intelligence, natural language processing,
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these
 * restrictions will be subject to legal action and may be held liable
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "../../remote_global.h"

#ifdef HAVE_CRSF

#include <Arduino.h>
#include <math.h>

#include "elrs_crsf_shared.h"
#include "elrs_crsf.h"
#include "crsf_kludge.h"
#include "crsf_settings.h"

// External
extern bool     loadConfigFile(const char *fn, uint8_t *buf, int len, int& validBytes, int forcefs = 0);
extern bool     saveConfigFile(const char *fn, uint8_t *buf, int len, int forcefs = 0);
extern uint32_t calcHash(uint8_t *buf, int len);

extern void     wifiOnFakePowerOn(bool showWait);

// CRSF settings
// Do not change or insert new values, this
// struct is saved as such. Append new stuff.
static struct [[gnu::packed]] {
    ELRSAxisCalibrationData elrsAxis[ELRS_GIMBAL_AXIS_COUNT] = {
        { 0, 1024, 2047 },
        { 0, 1024, 2047 },
        { 0, 1024, 2047 },
        { 0, 1024, 2047 }
    };
} crsfSettings;

static int      crsfSetValidBytes = 0;
static uint32_t crsfSettingsHash  = 0;
static bool     haveCRSFSettings  = false;

static const char *crsfCfgName  = "/crsfcfg";

static const uint16_t packetRates[5] = {
    ELRS_PACKET_RATE_50HZ,
    ELRS_PACKET_RATE_100HZ,
    ELRS_PACKET_RATE_150HZ,
    ELRS_PACKET_RATE_250HZ,
    ELRS_PACKET_RATE_500HZ
};

static const uint8_t speedUnits[2] = {
    ELRS_SPEED_UNITS_KMH,
    ELRS_SPEED_UNITS_MPH
};

static const uint8_t telemetryRatios[7] = {
    ELRS_TLM_RATIO_STD,
    ELRS_TLM_RATIO_1_2,
    ELRS_TLM_RATIO_1_4,
    ELRS_TLM_RATIO_1_8,
    ELRS_TLM_RATIO_1_16,
    ELRS_TLM_RATIO_1_32,
    ELRS_TLM_RATIO_OFF
};

static const uint8_t maxPowers[6] = {
    ELRS_MAX_POWER_10MW,
    ELRS_MAX_POWER_25MW,
    ELRS_MAX_POWER_100MW,
    ELRS_MAX_POWER_250MW,
    ELRS_MAX_POWER_500MW,
    ELRS_MAX_POWER_1000MW
};

static const uint8_t dynamicPowers[2] = {
    ELRS_DYNAMIC_POWER_OFF,
    ELRS_DYNAMIC_POWER_DYNAMIC
};

uint16_t crsf_getPacketRate(int idx)
{
    if(idx < 0 || idx > 4) idx = 3;
    return packetRates[idx];
}

uint8_t crsf_getSpeedUnits(int idx)
{
    if(idx < 0 || idx > 1) idx = 0;
    return speedUnits[idx];
}

uint8_t crsf_getTelemetryRatio(int idx)
{
    if(idx < 0 || idx > 6) idx = 0;
    return telemetryRatios[idx];
}

uint8_t crsf_getMaxPower(int idx)
{
    if(idx < 0 || idx > 5) idx = 3;
    return maxPowers[idx];
}

uint8_t crsf_getDynamicPower(int idx)
{
    if(idx < 0 || idx > 1) idx = 0;
    return dynamicPowers[idx];
}

void crsf_load_settings()
{
    if(loadConfigFile(crsfCfgName, (uint8_t *)&crsfSettings, sizeof(crsfSettings), crsfSetValidBytes, 0)) {
        crsfSettingsHash = calcHash((uint8_t *)&crsfSettings, sizeof(crsfSettings));
        haveCRSFSettings = true;
    }
}

bool crsf_save_settings(bool useCache)
{
    uint32_t oldHash = crsfSettingsHash;

    crsfSettingsHash = calcHash((uint8_t *)&crsfSettings, sizeof(crsfSettings));

    if(useCache) {
        if(oldHash == crsfSettingsHash) {
            return true;
        }
    }

    return saveConfigFile(crsfCfgName, (uint8_t *)&crsfSettings, sizeof(crsfSettings), 0);
}

void loadELRSCalibration(ELRSAxisCalibrationData *cal, int count)
{
    if(!cal) {
        return;
    }

    count = min(count, ELRS_GIMBAL_AXIS_COUNT);
    for(int i = 0; i < count; i++) {
        cal[i] = crsfSettings.elrsAxis[i];
    }
}

void saveELRSCalibration(const ELRSAxisCalibrationData *cal, int count)
{
    if(!cal) {
        return;
    }

    count = min(count, ELRS_GIMBAL_AXIS_COUNT);
    for(int i = 0; i < count; i++) {
        crsfSettings.elrsAxis[i] = cal[i];
    }
    crsf_save_settings(true);
}

bool crsf_begin(
            uint16_t packetRateHz,
            uint8_t speedDisplayUnits,
            uint8_t telemetryRatio,
            uint8_t maxPower,
            uint8_t dynamicPower,
            ButtonPack *buttonPack,
            bool haveButtonPack,
            remDisplay *remdisplay,
            remLED *pwrled,
            remLED *bLvLMeter,
            remLED *remledStop,
            bool usePowerLed,
            bool useLevelMeter,
            bool powerLedOnFakePower,
            bool levelMeterOnFakePower,
            void (*fpOnWifiHandler)(bool))
{
    return elrsMode.begin(
            packetRateHz,
            speedDisplayUnits,
            telemetryRatio,
            maxPower,
            dynamicPower,
            buttonPack,
            haveButtonPack,
            remdisplay,
            pwrled,
            bLvLMeter,
            remledStop,
            usePowerLed,
            useLevelMeter,
            powerLedOnFakePower,
            levelMeterOnFakePower,
            fpOnWifiHandler
        );
}

void crsf_loop(int battWarn)
{
    elrsMode.loop(battWarn);
}

void csrf_query_status(bool &FPBUnitIsOn)
{
    ELRSCrsfStatus elrsStatus = elrsMode.getStatus();
    FPBUnitIsOn = elrsStatus.fakePowerOn;
    //calibMode = elrsStatus.calibrating;
}

#endif
