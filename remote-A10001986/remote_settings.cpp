/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * Settings handling
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

#include "remote_global.h"

#define ARDUINOJSON_USE_LONG_LONG 0
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STRING 0
#define ARDUINOJSON_ENABLE_NAN 0
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#ifdef USE_SPIFFS
#define MYNVS SPIFFS
#include <SPIFFS.h>
#else
#define MYNVS LittleFS
#include <LittleFS.h>
#endif
#include <Update.h>

#include "remote_main.h"
#include "remote_settings.h"
#include "remote_audio.h"
#include "remote_wifi.h"

// If defined, old settings files will be used
// and converted if no new settings file is found.
// Keep this defined for a few versions/months.
#define SETTINGS_TRANSITION
// Stage 2: Assume new settings are present, but
// still delete obsolete files.
//#define SETTINGS_TRANSITION_2

#ifdef SETTINGS_TRANSITION
#undef SETTINGS_TRANSITION_2
#endif

// Size of main config JSON
// Needs to be adapted when config grows
#define JSON_SIZE 2500
#define JSON_SIZE_MQTT 3000
#if ARDUINOJSON_VERSION_MAJOR >= 7
#define DECLARE_S_JSON(x,n) JsonDocument n;
#define DECLARE_D_JSON(x,n) JsonDocument n;
#else
#define DECLARE_S_JSON(x,n) StaticJsonDocument<x> n;
#define DECLARE_D_JSON(x,n) DynamicJsonDocument n(x);
#endif

#define NUM_AUDIOFILES 22
#define SND_REQ_VERSION "RM10"
#define AC_FMTV 2
#define AC_TS   815107
#define AC_OHSZ (14 + ((NUM_AUDIOFILES+1)*(32+4)))

static const char *CONFN  = "/REMA.bin";
static const char *CONFND = "/REMA.old";
static const char *CONID  = "REMA";
const char        rspv[] = SND_REQ_VERSION;
static uint32_t   soa = AC_TS;
static bool       ic = false;
static uint8_t*   f(uint8_t *d, uint32_t m, int y) { return d; }
static char       *uploadFileNames[MAX_SIM_UPLOADS] = { NULL };
static char       *uploadRealFileNames[MAX_SIM_UPLOADS] = { NULL };

// Secondary settings
// Do not change or insert new values, this
// struct is saved as such. Append new stuff.
static struct [[gnu::packed]] {
    int32_t up             = 5;
    int32_t dn             = -5;
    int32_t zero           = 0;
    uint8_t brightness     = DEF_BRI;
    uint8_t curSoftVol     = DEFAULT_VOLUME;
    uint8_t movieMode      = DEF_MOV_MD;
    uint8_t displayGPSMode = DEF_DISP_GPS;
    uint8_t showUpdAvail   = 1;
} secSettings;

// Tertiary settings (SD only)
// Do not change or insert new values, this
// struct is saved as such. Append new stuff.
static struct [[gnu::packed]] {
    uint16_t visMode      = 0;
    uint8_t  musFolderNum = 0;
    uint8_t  mpShuffle    = 0;
} terSettings;

static int      secSetValidBytes = 0;
static uint32_t secSettingsHash  = 0;
static bool     haveSecSettings  = false;
static int      terSetValidBytes = 0;
static uint32_t terSettingsHash  = 0;
static bool     haveTerSettings  = false;

static uint32_t mainConfigHash = 0;
static uint32_t ipHash = 0;
#ifdef REMOTE_HAVEMQTT
static uint32_t mqttConfigHash = 0;
#endif

static const char *cfgName    = "/remconfig.json";   // Main config (flash)
#ifdef REMOTE_HAVEMQTT
static const char *haCfgName  = "/remhacfg.json";    // HA/MQTT config (flash/SD)
#endif
static const char *ipCfgName  = "/remipcfg";         // IP config (flash)
static const char *idName     = "/remidid";          // Remote ID (flash)
static const char *secCfgName = "/rem2cfg";          // Secondary settings (flash/SD)
static const char *terCfgName = "/rem3cfg";          // Tertiary settings (SD)

#ifdef SETTINGS_TRANSITION
static const char *ipCfgNameO = "/remipcfg.json";    // IP config (flash)
static const char *idNameO    = "/remid.json";       // Remote ID (flash)
static const char *volCfgName = "/remvolcfg.json";   // Volume config (flash/SD)
static const char *briCfgName = "/rembricfg.json";   // Brightness config (flash/SD)
static const char *caCfgName  = "/remcacfg.json";    // Calibration config (flash/SD)
static const char *visCfgName = "/remviscfg.json";   // Visual config (SD only)
static const char *musCfgName = "/remmcfg.json";     // Music config (SD only)
#endif
#ifdef SETTINGS_TRANSITION_2
static const char *obsFiles[] = {
    "/remipcfg.json", "/remid.json",     "/remvolcfg.json", "/rembricfg.json",
    "/remcacfg.json", "/remviscfg.json", "/remmcfg.json",   "/_installing.mp3",
    NULL
};
#endif

static const char fwfn[]      = "/remfw.bin";
static const char fwfnold[]   = "/remfw.old";

static const char *fsNoAvail = "Filesystem not available";
static const char *failFileWrite = "Failed to open file for writing";
#ifdef REMOTE_DBG
static const char *badConfig = "Settings bad/missing/incomplete; writing new file";
#endif

// If LittleFS/SPIFFS is mounted
bool haveFS = false;

// If a SD card is found
bool haveSD = false;

// Save secondary settings on SD?
static bool configOnSD = false;

// Paranoia: No writes Flash-FS
bool FlashROMode = false;

// If SD contains default audio files
static bool allowCPA = false;

// If current audio data is installed
bool haveAudioFiles = false;

// Music Folder Number
uint8_t musFolderNum = 0;

static uint8_t*  (*r)(uint8_t *, uint32_t, int);
static bool read_settings(File configFile, int cfgReadCount);
#ifdef REMOTE_HAVEMQTT
static void read_mqtt_settings();
#endif

static bool CopyTextParm(const char *json, char *setting, int setSize);
static bool CopyCheckValidNumParm(const char *json, char *text, uint8_t psize, int lowerLim, int upperLim, int setDefault);
static bool CopyCheckValidNumParmF(const char *json, char *text, uint8_t psize, float lowerLim, float upperLim, float setDefault);
static bool checkValidNumParm(char *text, int lowerLim, int upperLim, int setDefault);
static bool checkValidNumParmF(char *text, float lowerLim, float upperLim, float setDefault);
static bool handleMQTTButton(const char *json, char *text, uint8_t psize);

static void loadUpdAvail();

static bool     loadId();
static uint32_t createId();
static void     saveId();

static bool copy_audio_files(bool& delIDfile);
static void cfc(File& sfile, bool doCopy, int& haveErr, int& haveWriteErr);

static bool audio_files_present(int& alienVER);

static bool formatFlashFS(bool userSignal);
static void reInstallFlashFS();

static DeserializationError readJSONCfgFile(JsonDocument& json, File& configFile, uint32_t *newHash = NULL);
static bool writeJSONCfgFile(const JsonDocument& json, const char *fn, bool useSD, uint32_t oldHash = 0, uint32_t *newHash = NULL);

static bool writeFileToSD(const char *fn, uint8_t *buf, int len);
static bool writeFileToFS(const char *fn, uint8_t *buf, int len);

static bool loadConfigFile(const char *fn, uint8_t *buf, int len, int& validBytes, int forcefs = 0);
static bool saveConfigFile(const char *fn, uint8_t *buf, int len, int forcefs = 0);
static uint32_t calcHash(uint8_t *buf, int len);
static bool saveSecSettings(bool useCache);
static bool saveTerSettings(bool useCache);
#ifdef SETTINGS_TRANSITION
static void removeOldFiles(const char *oldfn);
#endif

static void firmware_update();

/*
 * settings_setup()
 * 
 * Mount MYNVS/LittleFS and SD (if available).
 * Read configuration from JSON config file
 * If config file not found, create one with default settings
 *
 */
void settings_setup()
{
    #ifdef REMOTE_DBG
    const char *funcName = "settings_setup";
    #endif
    bool writedefault = false;
    bool freshFS = false;
    bool SDres = false;
    int alienVER = -1;
    int cfgReadCount = 0;
  
    #ifdef REMOTE_DBG
    Serial.printf("%s: Mounting flash FS... ", funcName);
    #endif

    if(MYNVS.begin()) {
  
        haveFS = true;
  
    } else {
  
        #ifdef REMOTE_DBG
        Serial.print("failed, formatting... ");
        #endif

        haveFS = formatFlashFS(true);
        freshFS = true;
  
    }

    if(haveFS) {

        #ifdef REMOTE_DBG
        Serial.printf("ok.\nFlashFS: %d total, %d used\n", MYNVS.totalBytes(), MYNVS.usedBytes());
        #endif

        // Remove sound files that no longer should exist
        MYNVS.remove("/throttleup.wav");
    
        if(MYNVS.exists(cfgName)) {
            File configFile = MYNVS.open(cfgName, "r");
            if(configFile) {
                writedefault = read_settings(configFile, cfgReadCount);
                cfgReadCount++;
                configFile.close();
            } else {
                writedefault = true;
            }
        } else {
            writedefault = true;
        }
    
        // Write new config file after mounting SD and determining FlashROMode
  
    } else {
  
        Serial.println("failed.\n*** Mounting flash FS failed. Using SD (if available)");

    }

    // Set up SD card
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  
    haveSD = false;
  
    uint32_t sdfreq = (settings.sdFreq[0] == '0') ? 16000000 : 4000000;
    #ifdef REMOTE_DBG
    Serial.printf("%s: SD/SPI frequency %dMHz\n", funcName, sdfreq / 1000000);
    #endif
  
    #ifdef REMOTE_DBG
    Serial.printf("%s: Mounting SD... ", funcName);
    #endif
  
    if(!(SDres = SD.begin(SD_CS_PIN, SPI, sdfreq))) {
        #ifdef REMOTE_DBG
        Serial.printf("Retrying at 25Mhz... ");
        #endif
        SDres = SD.begin(SD_CS_PIN, SPI, 25000000);
    }

    if(SDres) {

        #ifdef REMOTE_DBG
        Serial.println("ok");
        #endif

        uint8_t cardType = SD.cardType();
       
        #ifdef REMOTE_DBG
        const char *sdTypes[5] = { "No card", "MMC", "SD", "SDHC", "unknown (SD not usable)" };
        Serial.printf("SD card type: %s\n", sdTypes[cardType > 4 ? 4 : cardType]);
        #endif

        haveSD = ((cardType != CARD_NONE) && (cardType != CARD_UNKNOWN));

    }

    if(haveSD) {

        firmware_update();
        
        if(SD.exists("/REM_FLASH_RO") || !haveFS) {
            bool writedefault2 = true;
            FlashROMode = true;
            Serial.println("Flash-RO mode: All settings/states stored on SD. Reloading settings.");
            if(SD.exists(cfgName)) {
                File configFile = SD.open(cfgName, "r");
                if(configFile) {
                    writedefault2 = read_settings(configFile, cfgReadCount);
                    configFile.close();
                }
            }
            if(writedefault2) {
                #ifdef REMOTE_DBG
                Serial.printf("%s: %s\n", funcName, badConfig);
                #endif
                mainConfigHash = 0;
                write_settings();
            }
        }

    } else {      
        Serial.println("No SD card found");
    }

    // Check if (current) audio data is installed
    haveAudioFiles = audio_files_present(alienVER);

    // Re-format flash FS if either alien VER found, or
    // neither VER nor our config file exist.
    // (Note: LittleFS crashes when flash FS is full.)
    if(!haveAudioFiles && haveFS && !FlashROMode) {
        if((alienVER > 0) || 
           (alienVER < 0 && !freshFS && !cfgReadCount)) {
            #ifdef REMOTE_DBG
            Serial.printf("Reformatting. Alien VER: %d, used space %d", alienVER, MYNVS.usedBytes());
            #endif
            writedefault = true;
            formatFlashFS(true);
        }
    }

    // Now write new config to flash FS if old one somehow bad
    // Only write this file if FlashROMode is off
    if(haveFS && writedefault && !FlashROMode) {
        #ifdef REMOTE_DBG
        Serial.printf("%s: %s\n", funcName, badConfig);
        #endif
        mainConfigHash = 0;
        write_settings();
    }

    #ifdef SETTINGS_TRANSITION_2
    if(haveSD) {
        for(int i = 0; ; i++) {
            if(!obsFiles[i]) break;
            SD.remove(obsFiles[i]);
        }
    }
    #endif

    // Load/create "Remote ID"
    if(!loadId()) {
        myRemID = createId();
        #ifdef REMOTE_DBG
        Serial.printf("Created Remote ID: 0x%lx\n", myRemID);
        #endif
        saveId();
    }

    // Determine if secondary settings are to be stored on SD
    configOnSD = (haveSD && ((settings.CfgOnSD[0] != '0') || FlashROMode));

    // Load secondary config file
    if(loadConfigFile(secCfgName, (uint8_t *)&secSettings, sizeof(secSettings), secSetValidBytes)) {
        secSettingsHash = calcHash((uint8_t *)&secSettings, sizeof(secSettings));
        haveSecSettings = true;
    }

    // Load tertiary config file (SD only)
    if(haveSD) {
        if(loadConfigFile(terCfgName, (uint8_t *)&terSettings, sizeof(terSettings), terSetValidBytes, 1)) {
            terSettingsHash = calcHash((uint8_t *)&terSettings, sizeof(terSettings));
            haveTerSettings = true;
        }
    }

    // Load HA/MQTT settings
    #ifdef REMOTE_HAVEMQTT
    read_mqtt_settings();
    #endif

    loadUpdAvail();
    updateConfigPortalUpdValues();

    // Check if SD contains the default sound files
    if((r = m) && haveSD && (haveFS || FlashROMode)) {
        allowCPA = check_if_default_audio_present();
    }

    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        uploadFileNames[i] = uploadRealFileNames[i] = NULL;
    }
}

void unmount_fs()
{
    if(haveFS) {
        MYNVS.end();
        #ifdef REMOTE_DBG
        Serial.println("Unmounted Flash FS");
        #endif
        haveFS = false;
    }
    if(haveSD) {
        SD.end();
        #ifdef REMOTE_DBG
        Serial.println("Unmounted SD card");
        #endif
        haveSD = false;
    }
}

static bool read_settings(File configFile, int cfgReadCount)
{
    const char *funcName = "read_settings";
    bool wd = false;
    size_t jsonSize = 0;
    DECLARE_D_JSON(JSON_SIZE,json);
    
    DeserializationError error = readJSONCfgFile(json, configFile, &mainConfigHash);

    #if ARDUINOJSON_VERSION_MAJOR < 7
    jsonSize = json.memoryUsage();
    if(jsonSize > JSON_SIZE) {
        Serial.printf("ERROR: Config file too large (%d vs %d), memory corrupted, awaiting doom.\n", jsonSize, JSON_SIZE);
    }

    #ifdef REMOTE_DBG
    if(jsonSize > JSON_SIZE - 256) {
          Serial.printf("%s: WARNING: JSON_SIZE needs to be adapted **************\n", funcName);
    }
    Serial.printf("%s: Size of document: %d (JSON_SIZE %d)\n", funcName, jsonSize, JSON_SIZE);
    #endif
    #endif

    if(!error) {

        // WiFi Configuration

        if(!cfgReadCount) {
            memset(settings.ssid, 0, sizeof(settings.ssid));
            memset(settings.pass, 0, sizeof(settings.pass));
        }

        if(json["ssid"]) {
            memset(settings.ssid, 0, sizeof(settings.ssid));
            memset(settings.pass, 0, sizeof(settings.pass));
            strncpy(settings.ssid, json["ssid"], sizeof(settings.ssid) - 1);
            if(json["pass"]) {
                strncpy(settings.pass, json["pass"], sizeof(settings.pass) - 1);
            }
        } else {
            if(!cfgReadCount) {
                // Set a marker for "no ssid tag in config file", ie read from NVS.
                settings.ssid[1] = 'X';
            } else if(settings.ssid[0] || settings.ssid[1] != 'X') {
                // FlashRO: If flash-config didn't set the marker, write new file 
                // with ssid/pass from flash-config
                wd = true;
            }
        }

        wd |= CopyTextParm(json["hostName"], settings.hostName, sizeof(settings.hostName));
        wd |= CopyCheckValidNumParm(json["wifiConRetries"], settings.wifiConRetries, sizeof(settings.wifiConRetries), 1, 10, DEF_WIFI_RETRY);
        wd |= CopyCheckValidNumParm(json["wifiConTimeout"], settings.wifiConTimeout, sizeof(settings.wifiConTimeout), 7, 25, DEF_WIFI_TIMEOUT);
        wd |= CopyCheckValidNumParm(json["rcOFP"], settings.reconOnFP, sizeof(settings.reconOnFP), 0, 1, DEF_RECON_ON_FP);
    
        wd |= CopyTextParm(json["systemID"], settings.systemID, sizeof(settings.systemID));
        wd |= CopyTextParm(json["appw"], settings.appw, sizeof(settings.appw));
        wd |= CopyCheckValidNumParm(json["apch"], settings.apChnl, sizeof(settings.apChnl), 0, 11, DEF_AP_CHANNEL);
        wd |= CopyCheckValidNumParm(json["wAOD"], settings.wifiAPOffDelay, sizeof(settings.wifiAPOffDelay), 0, 99, DEF_WIFI_APOFFDELAY);
        wd |= CopyCheckValidNumParm(json["rAOFP"], settings.reactAPOnFP, sizeof(settings.reactAPOnFP), 0, 1, DEF_REACT_AP_ON_FP);

        // Settings

        // autoThrottle, doCoast, powerMaster overruled by terSettings (if available)

        wd |= CopyCheckValidNumParm(json["at"], settings.autoThrottle, sizeof(settings.autoThrottle), 0, 1, DEF_AT);
        wd |= CopyCheckValidNumParm(json["coast"], settings.coast, sizeof(settings.coast), 0, 1, DEF_COAST);
        wd |= CopyCheckValidNumParm(json["playClick"], settings.playClick, sizeof(settings.playClick), 0, 1, DEF_PLAY_CLK);
        wd |= CopyCheckValidNumParm(json["playALsnd"], settings.playALsnd, sizeof(settings.playALsnd), 0, 1, DEF_PLAY_ALM_SND);
        
        wd |= CopyTextParm(json["tcdIP"], settings.tcdIP, sizeof(settings.tcdIP));
        wd |= CopyCheckValidNumParm(json["pwM"], settings.pwrMst, sizeof(settings.pwrMst), 0, 1, DEF_PWR_MST);
        
        wd |= CopyCheckValidNumParm(json["CfgOnSD"], settings.CfgOnSD, sizeof(settings.CfgOnSD), 0, 1, DEF_CFG_ON_SD);
        //wd |= CopyCheckValidNumParm(json["sdFreq"], settings.sdFreq, sizeof(settings.sdFreq), 0, 1, DEF_SD_FREQ);

        wd |= CopyCheckValidNumParm(json["oorst"], settings.oorst, sizeof(settings.oorst), 0, 1, DEF_OORST);
        wd |= CopyCheckValidNumParm(json["oott"], settings.ooTT, sizeof(settings.ooTT), 0, 1, DEF_OO_TT);

        #ifdef ALLOW_DIS_UB
        wd |= CopyCheckValidNumParm(json["disBP"], settings.disBPack, sizeof(settings.disBPack), 0, 1, DEF_DIS_BPACK);
        #endif
        
        wd |= CopyCheckValidNumParm(json["b0Mt"], settings.bPb0Maint, sizeof(settings.bPb0Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b1Mt"], settings.bPb1Maint, sizeof(settings.bPb1Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b2Mt"], settings.bPb2Maint, sizeof(settings.bPb2Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b3Mt"], settings.bPb3Maint, sizeof(settings.bPb3Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b4Mt"], settings.bPb4Maint, sizeof(settings.bPb4Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b5Mt"], settings.bPb5Maint, sizeof(settings.bPb5Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b6Mt"], settings.bPb6Maint, sizeof(settings.bPb6Maint), 0, 1, DEF_BPMAINT);
        wd |= CopyCheckValidNumParm(json["b7Mt"], settings.bPb7Maint, sizeof(settings.bPb7Maint), 0, 1, DEF_BPMAINT);

        wd |= CopyCheckValidNumParm(json["b0MtO"], settings.bPb0MtO, sizeof(settings.bPb0MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b1MtO"], settings.bPb1MtO, sizeof(settings.bPb1MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b2MtO"], settings.bPb2MtO, sizeof(settings.bPb2MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b3MtO"], settings.bPb3MtO, sizeof(settings.bPb3MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b4MtO"], settings.bPb4MtO, sizeof(settings.bPb4MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b5MtO"], settings.bPb5MtO, sizeof(settings.bPb5MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b6MtO"], settings.bPb6MtO, sizeof(settings.bPb6MtO), 0, 1, DEF_BPMTOO);
        wd |= CopyCheckValidNumParm(json["b7MtO"], settings.bPb7MtO, sizeof(settings.bPb7MtO), 0, 1, DEF_BPMTOO);

        wd |= CopyCheckValidNumParm(json["uPLED"], settings.usePwrLED, sizeof(settings.usePwrLED), 0, 1, DEF_USE_PLED);
        wd |= CopyCheckValidNumParm(json["pLEDFP"], settings.pwrLEDonFP, sizeof(settings.pwrLEDonFP), 0, 1, DEF_PLEDFP);
        wd |= CopyCheckValidNumParm(json["uLvLM"], settings.useLvlMtr, sizeof(settings.useLvlMtr), 0, 1, DEF_USE_LVLMTR);
        wd |= CopyCheckValidNumParm(json["uLvLMFP"], settings.LvLMtronFP, sizeof(settings.LvLMtronFP), 0, 1, DEF_LVLFP);

        #ifdef HAVE_PM
        wd |= CopyCheckValidNumParm(json["uPM"], settings.usePwrMon, sizeof(settings.usePwrMon), 0, 1, DEF_USE_PWRMON);
        wd |= CopyCheckValidNumParm(json["bTy"], settings.batType, sizeof(settings.batType), 0, 4, DEF_BAT_TYPE);
        wd |= CopyCheckValidNumParm(json["bCa"], settings.batCap, sizeof(settings.batCap), 1000, 6000, DEF_BAT_CAP);
        #endif
  
        // HA/MQTT Settings (transitional; now in separate file)

        #ifdef REMOTE_HAVEMQTT
        CopyCheckValidNumParm(json["useMQTT"], settings.useMQTT, sizeof(settings.useMQTT), 0, 1, 0);
        CopyTextParm(json["mqttServer"], settings.mqttServer, sizeof(settings.mqttServer));
        // mqttV never saved in main settings
        CopyTextParm(json["mqttUser"], settings.mqttUser, sizeof(settings.mqttUser));
        handleMQTTButton(json["mqttb1t"], settings.mqttbt[0], sizeof(settings.mqttbt[0]) - 1);
        handleMQTTButton(json["mqttb1o"], settings.mqttbo[0], sizeof(settings.mqttbo[0]) - 1);
        handleMQTTButton(json["mqttb1f"], settings.mqttbf[0], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb2t"], settings.mqttbt[1], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb2o"], settings.mqttbo[1], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb2f"], settings.mqttbf[1], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb3t"], settings.mqttbt[2], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb3o"], settings.mqttbo[2], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb3f"], settings.mqttbf[2], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb4t"], settings.mqttbt[3], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb4o"], settings.mqttbo[3], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb4f"], settings.mqttbf[3], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb5t"], settings.mqttbt[4], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb5o"], settings.mqttbo[4], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb5f"], settings.mqttbf[4], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb6t"], settings.mqttbt[5], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb6o"], settings.mqttbo[5], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb6f"], settings.mqttbf[5], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb7t"], settings.mqttbt[6], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb7o"], settings.mqttbo[6], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb7f"], settings.mqttbf[6], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb8t"], settings.mqttbt[7], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb8o"], settings.mqttbo[7], sizeof(settings.mqttbf[0]) - 1);
        handleMQTTButton(json["mqttb8f"], settings.mqttbf[7], sizeof(settings.mqttbf[0]) - 1);
        #endif

    } else {

        wd = true;

    }

    return wd;
}

void write_settings()
{
    const char *funcName = "write_settings";
    DECLARE_D_JSON(JSON_SIZE,json);

    if(!haveFS && !FlashROMode) {
        Serial.printf("%s: %s\n", funcName, fsNoAvail);
        return;
    }

    #ifdef REMOTE_DBG
    Serial.printf("%s: Writing config file\n", funcName);
    #endif

    // Write this only if either set, or also present in file read earlier
    if(settings.ssid[0] || settings.ssid[1] != 'X') {
        json["ssid"] = (const char *)settings.ssid;
        json["pass"] = (const char *)settings.pass;
    }

    json["hostName"] = (const char *)settings.hostName;
    json["wifiConRetries"] = (const char *)settings.wifiConRetries;
    json["wifiConTimeout"] = (const char *)settings.wifiConTimeout;
    json["rcOFP"] = (const char *)settings.reconOnFP;
    
    json["systemID"] = (const char *)settings.systemID;
    json["appw"] = (const char *)settings.appw;
    json["apch"] = (const char *)settings.apChnl;
    json["wAOD"] = (const char *)settings.wifiAPOffDelay;
    json["rAOFP"] = (const char *)settings.reactAPOnFP;

    json["at"] = (const char *)settings.autoThrottle;
    json["coast"] = (const char *)settings.coast;
    json["playClick"] = (const char *)settings.playClick;
    json["playALsnd"] = (const char *)settings.playALsnd;
    
    json["tcdIP"] = (const char *)settings.tcdIP;
    json["pwM"] = (const char *)settings.pwrMst;
    
    json["CfgOnSD"] = (const char *)settings.CfgOnSD;
    //json["sdFreq"] = (const char *)settings.sdFreq;

    json["oorst"] = (const char *)settings.oorst;
    json["oott"] = (const char *)settings.ooTT;

    #ifdef ALLOW_DIS_UB
    json["disBP"] = (const char *)settings.disBPack;
    #endif
    
    json["b0Mt"] = (const char *)settings.bPb0Maint;
    json["b1Mt"] = (const char *)settings.bPb1Maint;
    json["b2Mt"] = (const char *)settings.bPb2Maint;
    json["b3Mt"] = (const char *)settings.bPb3Maint;
    json["b4Mt"] = (const char *)settings.bPb4Maint;
    json["b5Mt"] = (const char *)settings.bPb5Maint;
    json["b6Mt"] = (const char *)settings.bPb6Maint;
    json["b7Mt"] = (const char *)settings.bPb7Maint;

    json["b0MtO"] = (const char *)settings.bPb0MtO;
    json["b1MtO"] = (const char *)settings.bPb1MtO;
    json["b2MtO"] = (const char *)settings.bPb2MtO;
    json["b3MtO"] = (const char *)settings.bPb3MtO;
    json["b4MtO"] = (const char *)settings.bPb4MtO;
    json["b5MtO"] = (const char *)settings.bPb5MtO;
    json["b6MtO"] = (const char *)settings.bPb6MtO;
    json["b7MtO"] = (const char *)settings.bPb7MtO;

    json["uPLED"] = (const char *)settings.usePwrLED;
    json["pLEDFP"] = (const char *)settings.pwrLEDonFP;
    json["uLvLM"] = (const char *)settings.useLvlMtr;
    json["uLvLMFP"] = (const char *)settings.LvLMtronFP;

    #ifdef HAVE_PM
    json["uPM"] = (const char *)settings.usePwrMon;
    json["bTy"] = (const char *)settings.batType;
    json["bCa"] = (const char *)settings.batCap;
    #endif
  
    writeJSONCfgFile(json, cfgName, FlashROMode, mainConfigHash, &mainConfigHash);
}

bool checkConfigExists()
{
    return FlashROMode ? SD.exists(cfgName) : (haveFS && MYNVS.exists(cfgName));
}

/*
 *  Helpers for parm copying & checking
 */

static bool CopyTextParm(const char *json, char *setting, int setSize)
{
    if(!json) return true;
    
    memset(setting, 0, setSize);
    strncpy(setting, json, setSize - 1);
    return false;
}

static bool CopyCheckValidNumParm(const char *json, char *text, uint8_t psize, int lowerLim, int upperLim, int setDefault)
{
    if(!json) return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return checkValidNumParm(text, lowerLim, upperLim, setDefault);
}

static bool CopyCheckValidNumParmF(const char *json, char *text, uint8_t psize, float lowerLim, float upperLim, float setDefault)
{
    if(!json) return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return checkValidNumParmF(text, lowerLim, upperLim, setDefault);
}

static bool checkValidNumParm(char *text, int lowerLim, int upperLim, int setDefault)
{
    int i, len = strlen(text);
    bool ret = false;

    if(!len) {
        i = setDefault;
        ret = true;
    } else {
        for(int j = 0; j < len; j++) {
            if(text[j] < '0' || text[j] > '9') {
                i = setDefault;
                ret = true;
                break;
            }
        }
        if(!ret) {
            i = atoi(text);   
            if(i < lowerLim) {
                i = lowerLim;
                ret = true;
            } else if(i > upperLim) {
                i = upperLim;
                ret = true;
            }
        }
    }

    // Re-do to get rid of formatting errors (eg "000")
    sprintf(text, "%d", i);

    return ret;
}

static bool checkValidNumParmF(char *text, float lowerLim, float upperLim, float setDefault)
{
    int i, len = strlen(text);
    bool ret = false;
    float f;

    if(!len) {
        f = setDefault;
        ret = true;
    } else {
        for(i = 0; i < len; i++) {
            if(text[i] != '.' && text[i] != '-' && (text[i] < '0' || text[i] > '9')) {
                f = setDefault;
                ret = true;
                break;
            }
        }
        if(!ret) {
            f = strtof(text, NULL);
            if(f < lowerLim) {
                f = lowerLim;
                ret = true;
            } else if(f > upperLim) {
                f = upperLim;
                ret = true;
            }
        }
    }
    // Re-do to get rid of formatting errors (eg "0.")
    sprintf(text, "%.1f", f);

    return ret;
}

bool evalBool(char *s)
{
    if(*s == '0') return false;
    return true;
}

static bool handleMQTTButton(const char *json, char *text, uint8_t psize)
{
    if(!json) return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return false;
}

static bool openCfgFileRead(const char *fn, File& f, bool SDonly = false)
{
    bool haveConfigFile = false;
    
    if(configOnSD || SDonly) {
        if(SD.exists(fn)) {
            haveConfigFile = (f = SD.open(fn, "r"));
        }
    } 
    if(!haveConfigFile && !SDonly && haveFS) {
        if(MYNVS.exists(fn)) {
            haveConfigFile = (f = MYNVS.open(fn, "r"));
        }
    }

    return haveConfigFile;
}

/*
 * Load/save HA/MQTT config
 */

#ifdef REMOTE_HAVEMQTT
static void read_mqtt_settings()
{
    const char *funcName = "read_mqtt_settings";
    File configFile;
    bool wd = true;

    if(!haveFS && !configOnSD) {
        #ifdef REMOTE_DBG
        Serial.printf("%s: %s\n", funcName, fsNoAvail);
        #endif
        return;
    }

    #ifdef REMOTE_DBG
    Serial.printf("%s: Loading from %s\n", funcName, configOnSD ? "SD" : "flash FS");
    #endif

    if(openCfgFileRead(haCfgName, configFile)) {
        DECLARE_D_JSON(JSON_SIZE_MQTT,json);
        if(!readJSONCfgFile(json, configFile, &mqttConfigHash)) {
            wd = false;
            wd |= CopyCheckValidNumParm(json["useMQTT"], settings.useMQTT, sizeof(settings.useMQTT), 0, 1, 0);
            wd |= CopyTextParm(json["mqttServer"], settings.mqttServer, sizeof(settings.mqttServer));
            wd |= CopyCheckValidNumParm(json["mqttV"], settings.mqttVers, sizeof(settings.mqttVers), 0, 1, 0);
            wd |= CopyTextParm(json["mqttUser"], settings.mqttUser, sizeof(settings.mqttUser));
            wd |= handleMQTTButton(json["mqttb1t"], settings.mqttbt[0], sizeof(settings.mqttbt[0]) - 1);
            wd |= handleMQTTButton(json["mqttb1o"], settings.mqttbo[0], sizeof(settings.mqttbo[0]) - 1);
            wd |= handleMQTTButton(json["mqttb1f"], settings.mqttbf[0], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb2t"], settings.mqttbt[1], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb2o"], settings.mqttbo[1], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb2f"], settings.mqttbf[1], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb3t"], settings.mqttbt[2], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb3o"], settings.mqttbo[2], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb3f"], settings.mqttbf[2], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb4t"], settings.mqttbt[3], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb4o"], settings.mqttbo[3], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb4f"], settings.mqttbf[3], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb5t"], settings.mqttbt[4], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb5o"], settings.mqttbo[4], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb5f"], settings.mqttbf[4], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb6t"], settings.mqttbt[5], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb6o"], settings.mqttbo[5], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb6f"], settings.mqttbf[5], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb7t"], settings.mqttbt[6], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb7o"], settings.mqttbo[6], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb7f"], settings.mqttbf[6], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb8t"], settings.mqttbt[7], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb8o"], settings.mqttbo[7], sizeof(settings.mqttbf[0]) - 1);
            wd |= handleMQTTButton(json["mqttb8f"], settings.mqttbf[7], sizeof(settings.mqttbf[0]) - 1);
        }   
    }

    if(wd) {
        mqttConfigHash = 0;
        write_mqtt_settings();
    }
}

void write_mqtt_settings()
{
    const char *funcName = "write_mqtt_settings";
    DECLARE_D_JSON(JSON_SIZE_MQTT,json);

    if(!haveFS && !configOnSD) {
        Serial.printf("%s: %s\n", funcName, fsNoAvail);
        return;
    }

    #ifdef REMOTE_DBG
    Serial.printf("%s: Writing config file\n", funcName);
    #endif
    
    json["useMQTT"] = (const char *)settings.useMQTT;
    json["mqttServer"] = (const char *)settings.mqttServer;
    json["mqttV"] = (const char *)settings.mqttVers;
    json["mqttUser"] = (const char *)settings.mqttUser;
    json["mqttb1t"] = (const char *)settings.mqttbt[0];
    json["mqttb1o"] = (const char *)settings.mqttbo[0];
    json["mqttb1f"] = (const char *)settings.mqttbf[0];
    json["mqttb2t"] = (const char *)settings.mqttbt[1];
    json["mqttb2o"] = (const char *)settings.mqttbo[1];
    json["mqttb2f"] = (const char *)settings.mqttbf[1];
    json["mqttb3t"] = (const char *)settings.mqttbt[2];
    json["mqttb3o"] = (const char *)settings.mqttbo[2];
    json["mqttb3f"] = (const char *)settings.mqttbf[2];
    json["mqttb4t"] = (const char *)settings.mqttbt[3];
    json["mqttb4o"] = (const char *)settings.mqttbo[3];
    json["mqttb4f"] = (const char *)settings.mqttbf[3];
    json["mqttb5t"] = (const char *)settings.mqttbt[4];
    json["mqttb5o"] = (const char *)settings.mqttbo[4];
    json["mqttb5f"] = (const char *)settings.mqttbf[4];
    json["mqttb6t"] = (const char *)settings.mqttbt[5];
    json["mqttb6o"] = (const char *)settings.mqttbo[5];
    json["mqttb6f"] = (const char *)settings.mqttbf[5];
    json["mqttb7t"] = (const char *)settings.mqttbt[6];
    json["mqttb7o"] = (const char *)settings.mqttbo[6];
    json["mqttb7f"] = (const char *)settings.mqttbf[6];
    json["mqttb8t"] = (const char *)settings.mqttbt[7];
    json["mqttb8o"] = (const char *)settings.mqttbo[7];
    json["mqttb8f"] = (const char *)settings.mqttbf[7];

    writeJSONCfgFile(json, haCfgName, configOnSD, mqttConfigHash, &mqttConfigHash);
}
#endif

/*
 * Load/save calibration config
 */

void loadCalib()
{
    if(haveSecSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadCalib: extracting from secSettings\n");
        #endif
        if(useRotEnc) {
            rotEnc.setMaxStepsUp(secSettings.up);
            rotEnc.setMaxStepsDown(secSettings.dn);
            if(!rotEnc.dynZeroPos()) {
                rotEnc.setZeroPos(secSettings.zero);
            }
        }
    } else {
        #ifdef SETTINGS_TRANSITION      
        File configFile;
        if(!haveFS && !configOnSD) return;
        if(openCfgFileRead(caCfgName, configFile)) {
            DECLARE_S_JSON(512,json);
            if(!readJSONCfgFile(json, configFile)) {
                if(json["Up"] && json["Down"]) {
                    int32_t up = atoi(json["Up"]);
                    int32_t dn = atoi(json["Down"]);
                    int32_t zero = json["Zero"] ? atoi(json["Zero"]) : 0;
                    if(up && dn) {
                        if(useRotEnc) {
                            rotEnc.setMaxStepsUp(up);
                            rotEnc.setMaxStepsDown(dn);
                            if(!rotEnc.dynZeroPos()) {
                                if(zero) {
                                    rotEnc.setZeroPos(zero);
                                } else {
                                    rotEnc.setZeroPos((max(up,dn)-min(up,dn))/2);
                                }
                            }
                        }
                        saveCalib();
                    }
                } 
            } 
            configFile.close();
        }
        removeOldFiles(caCfgName);
        #endif
    }
}

void saveCalib()
{
    if(useRotEnc) {
        secSettings.up   = rotEnc.getMaxStepsUp();
        secSettings.dn   = rotEnc.getMaxStepsDown();
        secSettings.zero = (!rotEnc.dynZeroPos()) ? rotEnc.getZeroPos() : 0;
        saveSecSettings(true);
    }
}

/*
 *  Load/save display brightness
 */

void loadBrightness()
{
    if(haveSecSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadCurVolume: extracting from secSettings");
        #endif
        remdisplay.setBrightness(secSettings.brightness, true);
    } else {
        #ifdef SETTINGS_TRANSITION
        char temp[6];
        File configFile;
        if(!haveFS && !configOnSD) return;
        if(openCfgFileRead(briCfgName, configFile)) {
            DECLARE_S_JSON(512,json);
            if(!readJSONCfgFile(json, configFile)) {
                if(!CopyCheckValidNumParm(json["brightness"], temp, sizeof(temp), 0, 15, DEF_BRI)) {
                    remdisplay.setBrightness((uint8_t)atoi(temp), true);
                }
            } 
            configFile.close();
            saveBrightness();
        }
        removeOldFiles(briCfgName);
        #endif
    }
}

void storeBrightness()
{
    secSettings.brightness = remdisplay.getBrightness();
}

void saveBrightness()
{
    storeBrightness();
    saveSecSettings(true);
}

/*
 *  Load/save volume
 */

void loadCurVolume()
{
    if(haveSecSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadCurVolume: extracting from secSettings");
        #endif
        if(secSettings.curSoftVol <= 19) {
            curSoftVol = secSettings.curSoftVol;
        }
    } else {
        #ifdef SETTINGS_TRANSITION
        char temp[6];
        File configFile;
        if(!haveFS && !configOnSD) return;
        if(openCfgFileRead(volCfgName, configFile)) {
            DECLARE_S_JSON(512,json);
            if(!readJSONCfgFile(json, configFile)) {
                if(!CopyCheckValidNumParm(json["volume"], temp, sizeof(temp), 0, 19, DEFAULT_VOLUME)) {
                    uint8_t ncv = atoi(temp);
                    if(ncv <= 19) curSoftVol = ncv;
                }
            } 
            configFile.close();
            saveCurVolume();
        }
        removeOldFiles(volCfgName);
        #endif
    }
}

void storeCurVolume()
{
    // Used to keep secSettings up-to-date in case
    // of delayed save
    secSettings.curSoftVol = curSoftVol;
}

void saveCurVolume()
{
    storeCurVolume();
    saveSecSettings(true);
}

/*
 * Load/save movie mode
 */

void loadMovieMode()
{
    if(haveSecSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadMovieMode: extracting from secSettings");
        #endif
        movieMode = !!secSettings.movieMode;
    }
}

void saveMovieMode()
{
    secSettings.movieMode = movieMode ? 1 : 0;
    saveSecSettings(true);
}

/*
 * Load/save "display TCD speed while off"
 */

void loadDisplayGPSMode()
{
    if(haveSecSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadDisplayGPSMode: extracting from secSettings");
        #endif
        displayGPSMode = !!secSettings.displayGPSMode;
    }
}

void saveDisplayGPSMode()
{
    secSettings.displayGPSMode = displayGPSMode ? 1 : 0;
    saveSecSettings(true);
}

/*
 *  Load/save "show update notification at boot"
 */

static void loadUpdAvail()
{
    if(haveSecSettings) {
        showUpdAvail = !!secSettings.showUpdAvail;
    }
}

void saveUpdAvail()
{
    secSettings.showUpdAvail = showUpdAvail ? 1 : 0;
    saveSecSettings(true);
}

// Special for CP where several settings are possibly
// changed at the same time. We don't want to write the
// file more than once.
void saveAllSecCP()
{
    secSettings.movieMode = movieMode ? 1 : 0;
    secSettings.displayGPSMode = displayGPSMode ? 1 : 0;
    secSettings.showUpdAvail = showUpdAvail ? 1 : 0;
    saveSecSettings(true);
}


/*
 *  Load/save the visual config (SD only)
 */

bool loadVis()
{
    if(!haveSD)
        return false;

    if(haveTerSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadVis: extracting from terSettings");
        #endif
        visMode = terSettings.visMode;
        return true;
    } else {
        #ifdef SETTINGS_TRANSITION
        char temp[16];
        File configFile;
        if(openCfgFileRead(visCfgName, configFile, true)) {
            DECLARE_S_JSON(512,json);
            if(!readJSONCfgFile(json, configFile)) {
                if(!CopyCheckValidNumParm(json["visMode"], temp, sizeof(temp), 0, 0xff, 0)) {
                    visMode = (uint16_t)atoi(temp);
                }
            } 
            configFile.close();
            disectOldVisMode();
            updateVisMode();
            saveVis();
            saveMovieMode();
            saveDisplayGPSMode();
        }
        removeOldFiles(visCfgName);
        #endif
    }
    return false;
}

void storeVis()
{
    // Used to keep secSettings up-to-date in case
    // of delayed save
    terSettings.visMode = visMode;
}

void saveVis()
{
    storeVis();
    saveTerSettings(true);
}

/*
 *  Load/save Music Folder Number (SD only)
 */
void loadMusFoldNum()
{
    if(!haveSD)
        return;

    if(haveTerSettings) {
        #ifdef REMOTE_DBG
        Serial.println("loadMusFoldNum: extracting from terSettings");
        #endif
        if(terSettings.musFolderNum <= 9) {
            musFolderNum = terSettings.musFolderNum;
        }
    } else {
        #ifdef SETTINGS_TRANSITION
        bool writedefault = true;
        char temp[4];
        if(SD.exists(musCfgName)) {
            File configFile = SD.open(musCfgName, "r");
            if(configFile) {
                DECLARE_S_JSON(512,json);
                if(!readJSONCfgFile(json, configFile)) {
                    if(!CopyCheckValidNumParm(json["folder"], temp, sizeof(temp), 0, 9, 0)) {
                        musFolderNum = atoi(temp);
                        writedefault = false;
                    }
                } 
                configFile.close();
            }
        }
        if(writedefault) musFolderNum = 0;
        saveMusFoldNum();
        removeOldFiles(musCfgName);
        #endif
    }
}

void saveMusFoldNum()
{
    terSettings.musFolderNum = musFolderNum;
    saveTerSettings(true);
}

void loadShuffle()
{
    if(haveSD && haveTerSettings) {
        mpShuffle = !!terSettings.mpShuffle;
    }
}

void saveShuffle()
{
    terSettings.mpShuffle = mpShuffle;
    saveTerSettings(true);
}

void saveAllTerCP()
{
    terSettings.musFolderNum = musFolderNum;
    terSettings.visMode = visMode;
    saveTerSettings(true);
}

/*
 * Load/save/delete settings for static IP configuration
 */

#ifdef SETTINGS_TRANSITION
static bool CopyIPParm(const char *json, char *text, uint8_t psize)
{
    if(!json) return true;

    if(strlen(json) == 0) 
        return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return false;
}
#endif

bool loadIpSettings()
{
    memset((void *)&ipsettings, 0, sizeof(ipsettings));

    if(!haveFS && !FlashROMode)
        return false;

    int vb = 0;
    if(loadConfigFile(ipCfgName, (uint8_t *)&ipsettings, sizeof(ipsettings), vb, -1)) {
        #ifdef REMOTE_DBG
        Serial.println("loadIpSettings: Loaded bin settings");
        #endif
        if(strlen(ipsettings.ip)) {
            if(checkIPConfig()) {
                ipHash = calcHash((uint8_t *)&ipsettings, sizeof(ipsettings));
                return true;
            } else {
                #ifdef REMOTE_DBG
                Serial.println("loadIpSettings: IP settings invalid; deleting file");
                #endif
                memset((void *)&ipsettings, 0, sizeof(ipsettings));
                deleteIpSettings();
            }
        }
    } else {
        #ifdef SETTINGS_TRANSITION
        bool invalid = false;
        bool haveConfig = false;
        if( (!FlashROMode && MYNVS.exists(ipCfgNameO)) ||
            (FlashROMode && SD.exists(ipCfgNameO)) ) {
            File configFile = FlashROMode ? SD.open(ipCfgNameO, "r") : MYNVS.open(ipCfgNameO, "r");
            if(configFile) {
                DECLARE_S_JSON(512,json);
                DeserializationError error = readJSONCfgFile(json, configFile);
                if(!error) {
                    invalid |= CopyIPParm(json["IpAddress"], ipsettings.ip, sizeof(ipsettings.ip));
                    invalid |= CopyIPParm(json["Gateway"], ipsettings.gateway, sizeof(ipsettings.gateway));
                    invalid |= CopyIPParm(json["Netmask"], ipsettings.netmask, sizeof(ipsettings.netmask));
                    invalid |= CopyIPParm(json["DNS"], ipsettings.dns, sizeof(ipsettings.dns));
                    haveConfig = !invalid;
                } else {
                    invalid = true;
                }
                configFile.close();
            }
            removeOldFiles(ipCfgNameO);
        }
        if(invalid) {
            memset((void *)&ipsettings, 0, sizeof(ipsettings));
        } else {
            writeIpSettings();
        }
        return haveConfig;
        #endif
    }

    ipHash = 0;
    return false;
}

void writeIpSettings()
{
    if(!haveFS && !FlashROMode)
        return;

    if(!strlen(ipsettings.ip))
        return;

    uint32_t nh = calcHash((uint8_t *)&ipsettings, sizeof(ipsettings));
    
    if(ipHash) {
        if(nh == ipHash) {
            #ifdef REMOTE_DBG
            Serial.printf("writeIpSettings: Not writing, hash identical (%x)\n", ipHash);
            #endif
            return;
        }
    }

    ipHash = nh;
    
    saveConfigFile(ipCfgName, (uint8_t *)&ipsettings, sizeof(ipsettings), -1);
}

void deleteIpSettings()
{
    #ifdef REMOTE_DBG
    Serial.println("deleteIpSettings: Deleting ip config");
    #endif

    ipHash = 0;

    if(FlashROMode) {
        SD.remove(ipCfgName);
    } else if(haveFS) {
        MYNVS.remove(ipCfgName);
    }
}

/*
   Load/save/delete remote ID
*/

static bool loadId()
{
    uint32_t buf = 0;
    int vb = 0;
    if(loadConfigFile(idName, (uint8_t *)&buf, sizeof(buf), vb, -1)) {
        #ifdef REMOTE_DBG
        Serial.println("loadId: Loaded bin settings");
        #endif
        myRemID = buf;
        return true;
    } else {
        #ifdef SETTINGS_TRANSITION
        bool invalid = false;
        bool haveConfig = false;
        if(!haveFS && !FlashROMode) return false;
        if( (!FlashROMode && MYNVS.exists(idNameO)) ||
             (FlashROMode && SD.exists(idNameO)) ) {
            File configFile = FlashROMode ? SD.open(idNameO, "r") : MYNVS.open(idNameO, "r");
            if(configFile) {
                DECLARE_S_JSON(512, json);
                DeserializationError error = readJSONCfgFile(json, configFile);
                if(!error) {
                    myRemID = (uint32_t)json["ID"];
                    invalid = (myRemID == 0);
                    if(!invalid) saveId();
                    haveConfig = !invalid;
                }
                configFile.close();
            }
            removeOldFiles(idNameO);
        }
        return haveConfig;
        #endif
    }

    return false;
}

static uint32_t createId()
{
    return esp_random() ^ esp_random() ^ esp_random();
}

static void saveId()
{
    if(!haveFS && !FlashROMode)
        return;

    saveConfigFile(idName, (uint8_t *)&myRemID, sizeof(myRemID), -1);
}

/*
 * Sound pack installer
 *
 */
bool check_allow_CPA()
{
    return allowCPA;
}

static uint32_t getuint32(uint8_t *buf)
{
    uint32_t t = 0;
    for(int i = 3; i >= 0; i--) {
      t <<= 8;
      t += buf[i];
    }
    return t;
}

bool check_if_default_audio_present()
{
    uint8_t dbuf[16];
    File file;
    size_t ts;

    ic = false;
    
    if(!haveSD)
        return false;

    if(SD.exists(CONFN)) {
        if(file = SD.open(CONFN, FILE_READ)) {
            ts = file.size();
            file.read(dbuf, 14);
            file.close();
            if((!memcmp(dbuf, CONID, 4))             && 
               ((*(dbuf+4) & 0x7f) == AC_FMTV)       &&
               (!memcmp(dbuf+5, rspv, 4))            &&
               (*(dbuf+9) == (NUM_AUDIOFILES+1))     &&
               (getuint32(dbuf+10) == soa)           &&
               (ts > soa + AC_OHSZ)) {
                ic = true;
                if(!(*(dbuf+4) & 0x80)) r  = f;
            }
        }
    }

    return ic;
}

/*
 * Install default audio files from SD to flash FS #############
 */

bool prepareCopyAudioFiles()
{
    int i, haveErr = 0, haveWriteErr = 0;
    
    if(!ic)
        return true;

    File sfile;
    if(sfile = SD.open(CONFN, FILE_READ)) {
        sfile.seek(14);
        for(i = 0; i < NUM_AUDIOFILES+1; i++) {
           cfc(sfile, false, haveErr, haveWriteErr);
           if(haveErr) break;
        }
        sfile.close();
    } else {
        return false;
    }

    return (haveErr == 0);
}

void doCopyAudioFiles()
{
    bool delIDfile = false;

    if((!copy_audio_files(delIDfile)) && !FlashROMode) {
        // If copy fails because of a write error, re-format flash FS
        reInstallFlashFS();
        copy_audio_files(delIDfile);// Retry copy
    }

    if(haveSD) {
        SD.remove("/_installing.mp3");
    }

    if(delIDfile) {
        delete_ID_file();
    } else {
        showCopyError();
        mydelay(5000, true);
    }

    mydelay(500, true);
    
    allOff();

    flushDelayedSave();

    unmount_fs();
    delay(500);
    
    esp_restart();
}

// Returns false if copy failed because of a write error (which 
//    might be cured by a reformat of the FlashFS)
// Returns true if ok or source error (file missing, read error)
// Sets delIDfile to true if copy fully succeeded
static bool copy_audio_files(bool& delIDfile)
{
    int i, haveErr = 0, haveWriteErr = 0;

    if(!allowCPA) {
        delIDfile = false;
        return true;
    }

    if(ic) {
        File sfile;
        if(sfile = SD.open(CONFN, FILE_READ)) {
            sfile.seek(14);
            for(i = 0; i < NUM_AUDIOFILES+1; i++) {
               cfc(sfile, true, haveErr, haveWriteErr);
               if(haveErr) break;
            }
            sfile.close();
        } else {
            haveErr++;
        }
    } else {
        haveErr++;
    }

    delIDfile = (haveErr == 0);

    return (haveWriteErr == 0);
}

static void cfc(File& sfile, bool doCopy, int& haveErr, int& haveWriteErr)
{
    const char *funcName = "cfc";
    uint8_t buf1[1+32+4];
    uint8_t buf2[1024];
    uint32_t s;
    bool skip = false, tSD = false;
    File dfile;

    buf1[0] = '/';
    sfile.read(buf1 + 1, 32+4);   
    s = getuint32((*r)(buf1 + 1, soa, 32) + 32);
    if(buf1[1] == '_') {
        tSD = true;
        skip = doCopy;
    } else {
        skip = !doCopy;
    }
    if(!skip) {
        if((dfile = (tSD || FlashROMode) ? SD.open((const char *)buf1, FILE_WRITE) : MYNVS.open((const char *)buf1, FILE_WRITE))) {
            uint32_t t = 1024;
            #ifdef REMOTE_DBG
            Serial.printf("%s: Opened destination file: %s, length %d\n", funcName, (const char *)buf1, s);
            #endif
            while(s > 0) {
                t = (s < t) ? s : t;
                if(sfile.read(buf2, t) != t) {
                    haveErr++;
                    break;
                }
                if(dfile.write((*r)(buf2, soa, t), t) != t) {
                    haveErr++;
                    haveWriteErr++;
                    break;
                }
                s -= t;
            }
        } else {
            haveErr++;
            haveWriteErr++;
            Serial.printf("%s: Error opening destination file: %s\n", funcName, buf1);
        }
    } else {
        #ifdef REMOTE_DBG
        Serial.printf("%s: Skipped file: %s, length %d\n", funcName, (const char *)buf1, s);
        #endif
        sfile.seek(sfile.position() + s);
    }
}

static bool audio_files_present(int& alienVER)
{
    File file;
    uint8_t buf[4];
    const char *fn = "/VER";

    // alienVER is -1 if no VER found,
    //              0 if our VER-type found,
    //              1 if alien VER-type found
    alienVER = -1;

    if(FlashROMode) {
        if(!(file = SD.open(fn, FILE_READ)))
            return false;
    } else {
        // No SD, no FS - don't even bother....
        if(!haveFS)
            return true;
        if(!MYNVS.exists(fn))
            return false;
        if(!(file = MYNVS.open(fn, FILE_READ)))
            return false;
    }

    file.read(buf, 4);
    file.close();

    if(!FlashROMode) {
        alienVER = memcmp(buf, rspv, 2) ? 1 : 0;
    }

    return (!memcmp(buf, rspv, 4));
}

void delete_ID_file()
{
    if(haveSD && ic) {
        SD.remove(CONFND);
        SD.rename(CONFN, CONFND);
    }
}

/*
   Various helpers
*/

static bool formatFlashFS(bool userSignal)
{
    bool ret = false;

    if(userSignal) {
        // Show the user some action
        showWaitSequence();
    } else {
        #ifdef REMOTE_DBG
        Serial.println("Formatting flash FS");
        #endif
    }

    MYNVS.format();
    if(MYNVS.begin()) ret = true;

    if(userSignal) {
        endWaitSequence();
    }

    return ret;
}

/*
 * Re-format flash FS and write back all settings.
 * Used during audio file installation when flash FS needs
 * to be re-formatted.
 * Is never called in FlashROmode.
 * Needs a reboot afterwards!
 */
static void reInstallFlashFS()
{
    // Format partition
    formatFlashFS(false);

    // Rewrite all settings residing in NVS
    #ifdef REMOTE_DBG
    Serial.println("Re-writing main, ip settings and RemoteID");
    #endif

    saveId();
    
    mainConfigHash = 0;
    write_settings();

    ipHash = 0;
    writeIpSettings();

    if(!configOnSD) {
        #ifdef REMOTE_DBG
        Serial.println("Re-writing MQTT and secondary settings");
        #endif
        #ifdef REMOTE_HAVEMQTT
        mqttConfigHash = 0;
        write_mqtt_settings();
        #endif
        saveSecSettings(false);
    }
}

/* 
 * Copy secondary settings from/to SD if user
 * changed "save to SD"-option in CP
 */
void moveSettings()
{       
    if(!haveSD || !haveFS) 
        return;

    if(configOnSD && FlashROMode) {
        #ifdef REMOTE_DBG
        Serial.println("moveSettings: Writing to flash prohibted (FlashROMode), aborting.");
        #endif
    }

    // Flush pending saves
    flushDelayedSave();

    configOnSD = !configOnSD;
    
    #ifdef REMOTE_HAVEMQTT
    mqttConfigHash = 0;
    write_mqtt_settings();
    #endif
    saveSecSettings(false);

    configOnSD = !configOnSD;

    if(configOnSD) {
        #ifdef REMOTE_HAVEMQTT
        SD.remove(haCfgName);
        #endif
        SD.remove(secCfgName);
    } else {
        #ifdef REMOTE_HAVEMQTT
        MYNVS.remove(haCfgName);
        #endif
        MYNVS.remove(secCfgName);
    }
}    

/*
 * Helpers for JSON config files
 */
static DeserializationError readJSONCfgFile(JsonDocument& json, File& configFile, uint32_t *readHash)
{
    const char *buf = NULL;
    size_t bufSize = configFile.size();
    DeserializationError ret;

    if(!(buf = (const char *)malloc(bufSize + 1))) {
        Serial.printf("rJSON: Buffer allocation failed (%d)\n", bufSize);
        return DeserializationError::NoMemory;
    }

    memset((void *)buf, 0, bufSize + 1);

    configFile.read((uint8_t *)buf, bufSize);

    #ifdef REMOTE_DBG
    Serial.println(buf);
    #endif

    if(readHash) {
        *readHash = calcHash((uint8_t *)buf, bufSize);
    }
    
    ret = deserializeJson(json, buf);

    free((void *)buf);

    return ret;
}

static bool writeJSONCfgFile(const JsonDocument& json, const char *fn, bool useSD, uint32_t oldHash, uint32_t *newHash)
{
    char *buf;
    size_t bufSize = measureJson(json);
    bool success = false;

    if(!(buf = (char *)malloc(bufSize + 1))) {
        Serial.printf("wJSON: Buffer allocation failed (%d)\n", bufSize);
        return false;
    }

    memset(buf, 0, bufSize + 1);
    serializeJson(json, buf, bufSize);

    #ifdef REMOTE_DBG
    Serial.printf("Writing %s to %s\n", fn, useSD ? "SD" : "FS");
    Serial.println((const char *)buf);
    #endif

    if(oldHash || newHash) {
        uint32_t newH = calcHash((uint8_t *)buf, bufSize);
        
        if(newHash) *newHash = newH;
    
        if(oldHash) {
            if(oldHash == newH) {
                #ifdef REMOTE_DBG
                Serial.printf("Not writing %s, hash identical (%x)\n", fn, oldHash);
                #endif
                free(buf);
                return true;
            }
        }
    }

    if(useSD) {
        success = writeFileToSD(fn, (uint8_t *)buf, (int)bufSize);
    } else {
        success = writeFileToFS(fn, (uint8_t *)buf, (int)bufSize);
    }

    free(buf);

    if(!success) {
        Serial.printf("wJSON: %s\n", failFileWrite);
    }

    return success;
}

/*
 * Generic file readers/writers
 */

static bool readFile(File& myFile, uint8_t *buf, int len)
{
    if(myFile) {
        size_t bytesr = myFile.read(buf, len);
        myFile.close();
        return (bytesr == len);
    } else
        return false;
}

static bool readFileU(File& myFile, uint8_t*& buf, int& len)
{
    if(myFile) {
        len = myFile.size();
        buf = (uint8_t *)malloc(len+1);
        if(buf) {
            buf[len] = 0;
            return readFile(myFile, buf, len);
        } else {
            myFile.close();
        }
    }
    return false;
}

// Read file of unknown size from SD
static bool readFileFromSDU(const char *fn, uint8_t*& buf, int& len)
{   
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_READ);
    return readFileU(myFile, buf, len);
}

// Read file of unknown size from NVS
static bool readFileFromFSU(const char *fn, uint8_t*& buf, int& len)
{   
    if(!haveFS || !MYNVS.exists(fn))
        return false;

    File myFile = MYNVS.open(fn, FILE_READ);
    return readFileU(myFile, buf, len);
}

// Read file of known size from SD
static bool readFileFromSD(const char *fn, uint8_t *buf, int len)
{   
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_READ);
    return readFile(myFile, buf, len);
}

// Read file of known size from NVS
static bool readFileFromFS(const char *fn, uint8_t *buf, int len)
{
    if(!haveFS || !MYNVS.exists(fn))
        return false;

    File myFile = MYNVS.open(fn, FILE_READ);
    return readFile(myFile, buf, len);
}

static bool writeFile(File& myFile, uint8_t *buf, int len)
{
    if(myFile) {
        size_t bytesw = myFile.write(buf, len);
        myFile.close();
        return (bytesw == len);
    } else
        return false;
}

// Write file to SD
static bool writeFileToSD(const char *fn, uint8_t *buf, int len)
{
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_WRITE);
    return writeFile(myFile, buf, len);
}

// Write file to NVS
static bool writeFileToFS(const char *fn, uint8_t *buf, int len)
{
    if(!haveFS)
        return false;

    File myFile = MYNVS.open(fn, FILE_WRITE);
    return writeFile(myFile, buf, len);
}

static uint8_t cfChkSum(const uint8_t *buf, int len)
{
    uint16_t s = 0;
    while(len--) {
        s += *buf++;
    }
    s = (s >> 8) + (s & 0xff);
    s += (s >> 8);
    return (uint8_t)(~s);
}

static bool loadConfigFile(const char *fn, uint8_t *buf, int len, int& validBytes, int forcefs)
{
    bool haveConfigFile = false;
    int fl;
    uint8_t *bbuf = NULL;

    // forcefs: > 0: SD only; = 0 either (configOnSD); < 0: Flash if !FlashROMode, SD if FlashROMode

    if(haveSD && ((!forcefs && configOnSD) || forcefs > 0 || (forcefs < 0 && FlashROMode))) {
        haveConfigFile = readFileFromSDU(fn, bbuf, fl);
    }
    if(!haveConfigFile && haveFS && (!forcefs || (forcefs < 0 && !FlashROMode))) {
        haveConfigFile = readFileFromFSU(fn, bbuf, fl);
    }
    if(haveConfigFile) {
        uint8_t chksum = cfChkSum(bbuf, fl - 1);
        if((haveConfigFile = (bbuf[fl - 1] == chksum))) {
            validBytes = bbuf[0] | (bbuf[1] << 8);
            memcpy(buf, bbuf + 2, min(len, validBytes));
            haveConfigFile = true; // (len <= validBytes);
            #ifdef REMOTE_DBG
            Serial.printf("loadConfigFile: loaded %s: need %d, got %d bytes: ", fn, len, validBytes);
            for(int k = 0; k < len; k++) Serial.printf("%02x ", buf[k]);
            Serial.printf("chksum %02x\n", chksum);
            #endif
        } else {
            #ifdef REMOTE_DBG
            Serial.printf("loadConfigFile: Bad checksum %02x %02x\n", chksum, bbuf[fl - 1]);
            #endif
        }
    }

    if(bbuf) free(bbuf);

    return haveConfigFile;
}

static bool saveConfigFile(const char *fn, uint8_t *buf, int len, int forcefs)
{
    uint8_t *bbuf;
    bool ret = false;

    if(!(bbuf = (uint8_t *)malloc(len + 3)))
        return false;

    bbuf[0] = len & 0xff;
    bbuf[1] = len >> 8;
    memcpy(bbuf + 2, buf, len);
    bbuf[len + 2] = cfChkSum(bbuf, len + 2);
    
    #ifdef REMOTE_DBG
    Serial.printf("saveConfigFile: %s: ", fn);
    for(int k = 0; k < len + 3; k++) Serial.printf("0x%02x ", bbuf[k]);
    Serial.println("");
    #endif

    if((!forcefs && configOnSD) || forcefs > 0 || (forcefs < 0 && FlashROMode)) {
        ret = writeFileToSD(fn, bbuf, len + 3);
    } else if(haveFS) {
        ret = writeFileToFS(fn, bbuf, len + 3);
    }

    free(bbuf);

    return ret;
}

static uint32_t calcHash(uint8_t *buf, int len)
{
    uint32_t hash = 2166136261UL;
    for(int i = 0; i < len; i++) {
        hash = (hash ^ buf[i]) * 16777619;
    }
    return hash;
}

static bool saveSecSettings(bool useCache)
{
    uint32_t oldHash = secSettingsHash;

    secSettingsHash = calcHash((uint8_t *)&secSettings, sizeof(secSettings));
    
    if(useCache) {
        if(oldHash == secSettingsHash) {
            #ifdef REMOTE_DBG
            Serial.printf("saveSecSettings: Data up to date, not writing (%x)\n", secSettingsHash);
            #endif
            return true;
        }
    }
    
    return saveConfigFile(secCfgName, (uint8_t *)&secSettings, sizeof(secSettings), 0);
}

static bool saveTerSettings(bool useCache)
{
    if(!haveSD)
        return false;

    uint32_t oldHash = terSettingsHash;
    
    terSettingsHash = calcHash((uint8_t *)&terSettings, sizeof(terSettings));
    
    if(useCache) {
        if(oldHash == terSettingsHash) {
            #ifdef REMOTE_DBG
            Serial.printf("saveTerSettings: Data up to date, not writing (%x)\n", terSettingsHash);
            #endif
            return true;
        }
    }
    
    return saveConfigFile(terCfgName, (uint8_t *)&terSettings, sizeof(terSettings), 1);
}

#ifdef SETTINGS_TRANSITION
static void removeOldFiles(const char *oldfn)
{
    if(haveSD) SD.remove(oldfn);
    if(haveFS) MYNVS.remove(oldfn);
    #ifdef REMOTE_DBG
    Serial.printf("removeOldFiles: Removing %s\n", oldfn);
    #endif
}
#endif


/*
 * File upload
 */

static char *allocateUploadFileName(const char *fn, int idx)
{
    if(uploadFileNames[idx]) {
        free(uploadFileNames[idx]);
    }
    if(uploadRealFileNames[idx]) {
        free(uploadRealFileNames[idx]);
    }
    uploadFileNames[idx] = uploadRealFileNames[idx] = NULL;

    if(!strlen(fn))
        return NULL;
  
    if(!(uploadFileNames[idx] = (char *)malloc(strlen(fn)+4)))
        return NULL;

    if(!(uploadRealFileNames[idx] = (char *)malloc(strlen(fn)+4))) {
        free(uploadFileNames[idx]);
        uploadFileNames[idx] = NULL;
        return NULL;
    }

    return uploadRealFileNames[idx];
}

bool openUploadFile(String& fn, File& file, int idx, bool haveAC, int& opType, int& errNo)
{
    char *uploadFileName = NULL;
    bool ret = false;
    
    if(haveSD) {

        errNo = 0;
        opType = 0;  // 0=normal, 1=AC, -1=deletion

        if(!(uploadFileName = allocateUploadFileName(fn.c_str(), idx))) {
            errNo = UPL_MEMERR;
            return false;
        }
        strcpy(uploadFileNames[idx], fn.c_str());
        
        uploadFileName[0] = '/';
        uploadFileName[1] = '-';
        uploadFileName[2] = 0;

        if(fn.length() > 4 && fn.endsWith(".mp3")) {

            strcat(uploadFileName, fn.c_str());

            if((strlen(uploadFileName) > 9) &&
               (strstr(uploadFileName, "/-delete-") == uploadFileName)) {

                uploadFileName[8] = '/';
                SD.remove(uploadFileName+8);
                opType = -1;
                
            }

        } else if(fn.endsWith(".bin")) {

            if(!haveAC) {
                strcat(uploadFileName, CONFN+1);  // Skip '/', already there
                opType = 1;
            } else {
                errNo = UPL_DPLBIN;
                opType = -1;
            }

        } else {

            errNo = UPL_UNKNOWN;
            opType = -1;
            // ret must be false!

        }

        if(opType >= 0) {
            if((file = SD.open(uploadFileName, FILE_WRITE))) {
                ret = true;
            } else {
                errNo = UPL_OPENERR;
            }
        }

    } else {
      
        errNo = UPL_NOSDERR;
        
    }

    return ret;
}

size_t writeACFile(File& file, uint8_t *buf, size_t len)
{
    return file.write(buf, len);
}

void closeACFile(File& file)
{
    file.close();
}

void removeACFile(int idx)
{
    if(haveSD) {
        if(uploadRealFileNames[idx]) {
            SD.remove(uploadRealFileNames[idx]);
        }
    }
}

int getUploadFileNameLen(int idx)
{
    if(idx >= MAX_SIM_UPLOADS) return 0; 
    if(!uploadFileNames[idx]) return 0;
    return strlen(uploadFileNames[idx]);
}

char *getUploadFileName(int idx)
{
    if(idx >= MAX_SIM_UPLOADS) return NULL; 
    return uploadFileNames[idx];
}

void freeUploadFileNames()
{
    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        if(uploadFileNames[i]) {
            free(uploadFileNames[i]);
            uploadFileNames[i] = NULL;
        }
        if(uploadRealFileNames[i]) {
            free(uploadRealFileNames[i]);
            uploadRealFileNames[i] = NULL;
        }
    }
}

void renameUploadFile(int idx)
{
    char *uploadFileName = uploadRealFileNames[idx];
    
    if(haveSD && uploadFileName) {

        char *t = (char *)malloc(strlen(uploadFileName)+4);
        t[0] = uploadFileName[0];
        t[1] = 0;
        strcat(t, uploadFileName+2);
        
        SD.remove(t);
        
        SD.rename(uploadFileName, t);

        // Real name is now changed
        strcpy(uploadFileName, t);
        
        free(t);
    }
}

// Emergency firmware update from SD card
static void fw_error_blink(int n)
{
    bool leds = false;

    for(int i = 0; i < n; i++) {
        leds = !leds;
        digitalWrite(STOPOUT_PIN, leds ? HIGH : LOW);
        delay(500);
    }
    digitalWrite(STOPOUT_PIN, LOW);
}

static void firmware_update()
{
    const char *upderr = "Firmware update error %d\n";
    uint8_t  buf[1024];
    unsigned int lastMillis = millis();
    bool     leds = false;
    size_t   s;

    if(!SD.exists(fwfn))
        return;
    
    File myFile = SD.open(fwfn, FILE_READ);
    
    if(!myFile)
        return;

    pinMode(STOPOUT_PIN, OUTPUT);
    
    if(!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Serial.printf(upderr, Update.getError());
        fw_error_blink(5);
        return;
    }

    while((s = myFile.read(buf, 1024))) {
        if(Update.write(buf, s) != s) {
            break;
        }
        if(millis() - lastMillis > 1000) {
            leds = !leds;
            digitalWrite(STOPOUT_PIN, leds ? HIGH : LOW);
            lastMillis = millis();
        }
    }
    
    if(Update.hasError() || !Update.end(true)) {
        Serial.printf(upderr, Update.getError());
        fw_error_blink(5);
    } 
    myFile.close();
    // Rename/remove in any case, we don't
    // want an update loop hammer our flash
    SD.remove(fwfnold);
    SD.rename(fwfn, fwfnold);
    unmount_fs();
    delay(1000);
    fw_error_blink(0);
    esp_restart();
}    
