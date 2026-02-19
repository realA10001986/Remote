/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * Main
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

#include <Arduino.h>
#include <WiFi.h>
#include "display.h"
#include "input.h"
#ifdef REMOTE_HAVETEMP
#include "sensors.h"
#endif

#include "remote_main.h"
#include "remote_settings.h"
#include "remote_audio.h"
#include "remote_wifi.h"

// i2c slave addresses

#define DISPLAY_ADDR   0x70 // LED segment display

                            // rotary encoders / ADC
#define ADDA4991_ADDR  0x36 // [default]
#define DUPPAV2_ADDR   0x01 // [user configured: A0 closed]
#define DFRGR360_ADDR  0x54 // [default; SW1=0, SW2=0]
#define ADS1X15_ADDR   0x48 // Addr->GND

                            // rotary encoders for volume
#define ADDA4991V_ADDR 0x37 // [non-default: A0 closed]
#define DUPPAV2V_ADDR  0x03 // [user configured: A0+A1 closed]
#define DFRGR360V_ADDR 0x55 // [SW1=0, SW2=1]

#define PCA9554_ADDR   0x20 // all Ax -> GND
#define PCA9554A_ADDR  0x38 // all Ax -> GND
#define PCA8574_ADDR   0x21 // non-default
#define PCA8574A_ADDR  0x39 // non-default

#define LC709204F_ADDR 0x0b

unsigned long powerupMillis = 0;

// The segment display object
remDisplay remdisplay(DISPLAY_ADDR);

// The LED objects
remLED remledStop;
remLED pwrled;
remLED bLvLMeter;

#ifdef HAVE_PM
remPowMon pwrMon(LC709204F_ADDR);
#endif

// The rotary encoder/ADC object
static const uint8_t rotEncAddr[4*2] = { 
    ADDA4991_ADDR, REM_RE_TYPE_ADA4991,
    DUPPAV2_ADDR,  REM_RE_TYPE_DUPPAV2,
    DFRGR360_ADDR, REM_RE_TYPE_DFRGR360,
    ADS1X15_ADDR,  REM_RE_TYPE_ADS1X15
};
static const uint8_t rotEncVAddr[3*2] = { 
    ADDA4991V_ADDR, REM_RE_TYPE_ADA4991,
    DUPPAV2V_ADDR,  REM_RE_TYPE_DUPPAV2,
    DFRGR360V_ADDR, REM_RE_TYPE_DFRGR360
};
REMRotEnc rotEnc(4, rotEncAddr);
static REMRotEnc rotEncV(3, rotEncVAddr); 
static REMRotEnc *rotEncVol;

// The Button / Switch objects
static RemButton powerswitch;
static RemButton brake;
static RemButton calib;

static RemButton buttonA;
static RemButton buttonB;

// The ButtonPack object
static const uint8_t butPackAddr[4*2] = { 
    PCA9554_ADDR,  REM_BP_TYPE_PCA9554,
    PCA9554A_ADDR, REM_BP_TYPE_PCA9554,
    PCA8574_ADDR,  REM_BP_TYPE_PCA8574,
    PCA8574A_ADDR, REM_BP_TYPE_PCA8574
};
ButtonPack butPack(4, butPackAddr);
                          
bool useRotEnc = false;
static bool useBPack = true;
static bool useRotEncVol = false;

bool showUpdAvail = true;

bool        remoteAllowed = false;
uint32_t    myRemID = 0x12345678;

static bool pwrLEDonFP = true;
static bool LvLMtronFP = true;
static bool usePwrLED = false;
static bool useLvlMtr = false;

// Battery monitor
static bool usePwrMon = false;
bool        havePwrMon = false;
static int  battWarn = 0;
static int  oldBattWarn = 0;
static unsigned long battWarnBlinkNow = 0;
static bool battWarnDispBlink = false;

#ifdef HAVE_PM
static int  dispSOC = 0;
static bool battWarnSnd = false;
#endif

static bool powerState = false;
static bool brakeState = false;
bool        powerMaster;
static bool triggerCompleteUpdate = false;
static bool cancelBrakeWarning = false;
static unsigned long brakeWarningNow = 0;
static bool brakeWarning = false;

int32_t        throttlePos = 0;
static int32_t oldThrottlePos = 0;
static int     currSpeedF = 0;
static int     currSpeed = 0;
static unsigned long lastSpeedUpd = 0;
static bool    lockThrottle = false;
bool           doCoast = false;
bool           keepCounting = false;
bool           autoThrottle = false;

bool        calibMode = false;
static bool calibUp = false;
static bool calibIP = true;

static unsigned long brichgtimer = 0;
static unsigned long volchgtimer = 0;

static bool          offDisplayTimer = false;
static unsigned long offDisplayNow = 0;

#define  VISB_MOV_MD 0    // No longer part of visMode (transitional)
#define  VISB_DGPS   1    // No longer part of visMode (transitional)
#define  VISB_AT     2
#define  VISB_COAST  3
#define  VISB_PWRM   7
uint16_t visMode   = (DEF_AT<<VISB_AT)|(DEF_COAST<<VISB_COAST)|(DEF_PWR_MST<<VISB_PWRM);
bool     movieMode = DEF_MOV_MD;

static bool          etmr = false;
static unsigned long enow = 0;

static bool ooTT = true;
static int  triggerTTonThrottle = 0;
static int  triggerIntTTonThrottle = 0;
static unsigned long triggerTTonThrottleNow = 0;

bool ooresBri = true;

// Durations of tt phases as defined by TCD
#define TT_P1_TOTAL     8000
#define TT_P1_DELAY_P1  1400  
#define TT_P1_POINT88   1400
#define TT_P1_EXT       TT_P1_TOTAL - TT_P1_DELAY_P1
#define P0_DUR          TT_P1_POINT88   // acceleration phase (stand-alone)
#define P1_DUR          6600            // time tunnel phase (synced; overruled by TCD network commands)
#define P2_ALARM_DELAY  6400            // Delay for "beep" after reentry (sync'd)
#define P2_ALARM_DLY_SA 6400            // Delay for "beep" after reentry (stand-alone)

static unsigned long accelDelay = 1000/10;
static int32_t       accelStep = 1;

// Linear mode
static const unsigned long accelDelays[5] = { 50, 42, 35, 28, 20 };  // 1-100%
static const int32_t       accelSteps[5]  = {  1,  1,  1,  1,  1 };  // 1-100%
static const int32_t       decelSteps[5]  = {  1,  1,  1,  2,  2 };  // 1-100%

/* movie mode
 *       0- 7:  90ms/mph
 *      20-24: 197ms/mph
 *      32-39: 200ms/mph
 *      55-59: 220ms/mph
 *      77-81: 300ms/mph
*/
static const unsigned long strictAccelFacts[5] = { 25, 21, 17, 14, 10 };
static const unsigned long strictAccelDelays[89] =
{
      0,  90,  90,  90,  90,  90,  90,  95,  95, 100,  // m0 - 9  10mph  0- 9: 0.83s  (m=measured, i=interpolated)
    105, 110, 115, 120, 125, 130, 135, 140, 145, 150,  // i10-19  20mph 10-19: 1.27s
    155, 160, 165, 170, 175, 180, 185, 190, 195, 200,  // i20-29  30mph 20-29: 1.77s
    200, 200, 202, 203, 204, 205, 206, 207, 208, 209,  // m30-39  40mph 30-39: 2s
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219,  // i40-49  50mph 40-49: 2.1s
    220, 221, 222, 223, 224, 225, 226, 227, 228, 229,  // m50-59  60mph 50-59: 2.24s
    230, 233, 236, 240, 243, 246, 250, 253, 256, 260,  // i60-69  70mph 60-69: 2.47s
    263, 266, 270, 273, 276, 280, 283, 286, 290, 293,  // m70-79  80mph 70-79: 2.78s
    296, 300, 300, 303, 303, 306, 310, 310,   0        // i80-88  90mph 80-88: 2.4s   total 17.6 secs
};
static const int32_t  strictdecelSteps[5]  = {  2,  2,  2,  2,  2 };  // 1-100%

#define P1_START_SPD_M 835
#define P1_START_SPD_L 820

static const int16_t coastCurve[89][2] =
{
    {6, 2}, {6, 2}, {6, 2}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 4}, // 0 
    {6, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 3}, {6, 3}, // 10
    {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, // 20
    {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 3}, // 30
    {6, 3}, {6, 3}, {6, 3}, {6, 3}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, // 40
    {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, // 50 
    {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, // 60 
    {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {6, 2}, {5, 1}, {5, 1}, {5, 1}, // 70
    {5, 1}, {5, 1}, {5, 1}, {5, 1}, {5, 1}, {5, 1}, {5, 1}, {5, 1}, {5, 1}          // 80
};

static unsigned long lastCommandSent = 0;

static bool doForceDispUpd = false;

static bool isFPBKeyPressed = false;
static bool isFPBKeyChange = false;
static bool isBrakeKeyPressed = false;
static bool isBrakeKeyChange = false;
static bool isCalibKeyPressed = false;
static bool isCalibKeyLongPressed = false;
static bool isCalibKeyChange = false;
static bool isbuttonAKeyPressed = false;
static bool isbuttonAKeyLongPressed = false;
static bool isbuttonAKeyChange = false;
static bool isbuttonBKeyPressed = false;
static bool isbuttonBKeyLongPressed = false;
static bool isbuttonBKeyChange = false;

static bool isbutPackKeyPressed[PACK_SIZE] = { false };
static bool isbutPackKeyLongPressed[PACK_SIZE] = { false };
static bool isbutPackKeyChange[PACK_SIZE] = { false };
static bool buttonPackMomentary[PACK_SIZE] = { false };
static bool buttonPackMtOnOnly[PACK_SIZE] = { false };

static int  MQTTbuttonOnLen[PACK_SIZE]  = { 0 };
static int  MQTTbuttonOffLen[PACK_SIZE] = { 0 };

bool networkTimeTravel = false;
bool networkReentry    = false;
bool networkAbort      = false;
bool networkAlarm      = false;
uint16_t networkLead   = P0_DUR;
uint16_t networkP1     = P1_DUR;

bool doPrepareTT = false;
bool doWakeup = false;

static bool tcdIsBusy = false;
bool        remBusy   = false;

// Time travel status flags etc.
bool                 TTrunning = false;
static bool          IntP0running = false;
static bool          extTT = false;
static unsigned long TTstart = 0;
static unsigned long P0duration = P0_DUR;
static unsigned long P1duration = P1_DUR;
static unsigned long P1_maxtimeout = 10000;
static bool          TTP0 = false;
static bool          TTP0end = false;
static bool          TTP1 = false;
static bool          TTP2 = false;
static bool          TTFlag = false;
static unsigned long triggerP1 = 0;
static bool          triggerP1NoLead = false;

// Volume-factor for "travelstart" sounds
#define TT_SOUND_FACT 0.60f

static bool          volchanged = false;
static unsigned long volchgnow = 0;
static bool          brichanged = false;
static unsigned long brichgnow = 0;
static bool          vischanged = false;
static unsigned long vischgnow = 0;

bool                 FPBUnitIsOn = true;
static unsigned long justBootedNow = 0;
static bool          bootFlag = false;
static bool          sendBootStatus = false;
bool                 blockScan = false;

static int           brakecnt = 0;

static bool          havePOFFsnd = false;
static bool          haveBOFFsnd = false;
static bool          haveThUp = false;
static const char *  powerOnSnd  = "/poweron.mp3";   // Default provided
static const char *  powerOffSnd = "/poweroff.mp3";  // No default sound
static const char *  brakeOnSnd  = "/brakeon.mp3";   // Default provided
static const char *  brakeOffSnd = "/brakeoff.mp3";  // No default sound
static const char *  throttleUpSnd = "/throttleup.mp3";   // Default provided (wav)
static char          brakeRem[]  = "/rbrake1.mp3";

// BTTF network
#define BTTFN_VERSION              1
#define BTTFN_SUP_MC            0x80
#define BTTFN_SUP_ND            0x40
#define BTTF_PACKET_SIZE          48
#define BTTF_DEFAULT_LOCAL_PORT 1338
#define BTTFN_POLL_INT          1300
#define BTTFN_POLL_INT_FAST      500
#define BTTFN_RESPONSE_TO        700
#define BTTFN_DATA_TO          18600
#define BTTFN_TYPE_ANY     0    // Any, unknown or no device
#define BTTFN_TYPE_FLUX    1    // Flux Capacitor
#define BTTFN_TYPE_SID     2    // SID
#define BTTFN_TYPE_PCG     3    // Dash Gauges
#define BTTFN_TYPE_VSR     4    // VSR
#define BTTFN_TYPE_AUX     5    // Aux (user custom device)
#define BTTFN_TYPE_REMOTE  6    // Futaba remote control
#define BTTFN_NOT_PREPARE  1
#define BTTFN_NOT_TT       2
#define BTTFN_NOT_REENTRY  3
#define BTTFN_NOT_ABORT_TT 4
#define BTTFN_NOT_ALARM    5
#define BTTFN_NOT_REFILL   6
#define BTTFN_NOT_FLUX_CMD 7
#define BTTFN_NOT_SID_CMD  8
#define BTTFN_NOT_PCG_CMD  9
#define BTTFN_NOT_WAKEUP   10
#define BTTFN_NOT_AUX_CMD  11
#define BTTFN_NOT_VSR_CMD  12
#define BTTFN_NOT_REM_CMD  13
#define BTTFN_NOT_REM_SPD  14
#define BTTFN_NOT_SPD      15
#define BTTFN_NOT_INFO     16
#define BTTFN_NOT_DATA     128  // bit only, not value
#define BTTFN_REMCMD_PING       1   // Implicit "Register"/keep-alive
#define BTTFN_REMCMD_BYE        2   // Forced unregister
#define BTTFN_REMCMD_COMBINED   3   // All switches & speed combined
#define BTTFN_REM_MAX_COMMAND   BTTFN_REMCMD_COMBINED
#define BTTFN_REMCMD_KEEPALIVE 101
#define BTTFN_SSRC_NONE         0
#define BTTFN_SSRC_GPS          1
#define BTTFN_SSRC_ROTENC       2
#define BTTFN_SSRC_REM          3
#define BTTFN_SSRC_P0           4
#define BTTFN_SSRC_P1           5
#define BTTFN_SSRC_P2           6
#define BTTFN_TCDI1_NOREM   0x0001
#define BTTFN_TCDI1_NOREMKP 0x0002
#define BTTFN_TCDI1_EXT     0x0004
#define BTTFN_TCDI1_OFF     0x0008
#define BTTFN_TCDI1_NM      0x0010
#define BTTFN_TCDI2_BUSY    0x0001
static const uint8_t BTTFUDPHD[4] = { 'B', 'T', 'T', 'F' };
static bool          useBTTFN = false;
static WiFiUDP       bttfUDP;
static UDP*          remUDP;
static WiFiUDP       bttfMcUDP;
static UDP*          remMcUDP;
static byte          BTTFUDPBuf[BTTF_PACKET_SIZE];
static byte          BTTFUDPTBuf[BTTF_PACKET_SIZE];
static unsigned long BTTFNUpdateNow = 0;
static unsigned long bttfnRemPollInt = BTTFN_POLL_INT;
static unsigned long BTTFNTSRQAge = 0;
static bool          BTTFNPacketDue = false;
static bool          BTTFNWiFiUp = false;
static uint8_t       BTTFNfailCount = 0;
static uint32_t      BTTFUDPID = 0;
static unsigned long lastBTTFNpacket = 0;
static unsigned long bttfnLastNotData = 0;
static bool          BTTFNBootTO = false;
static bool          haveTCDIP = false;
static IPAddress     bttfnTcdIP;
static uint8_t       bttfnReqStatus = 0x52; // Request capabilities, status, speed
static bool          TCDSupportsNOTData = false;
static bool          bttfnDataNotEnabled = false;
static uint32_t      tcdHostNameHash = 0;
static byte          BTTFMCBuf[BTTF_PACKET_SIZE];
static IPAddress     bttfnMcIP(224, 0, 0, 224);
static uint32_t      bttfnSeqCnt[BTTFN_REM_MAX_COMMAND+1] = { 1 };
static uint32_t      bttfnTCDDataSeqCnt = 0;
static uint32_t      bttfnSessionID = 0;
static unsigned long bttfnCurrLatency = 0, bttfnPacketSentNow = 0;
static int16_t       tcdCurrSpeed = -1;
//static bool          tcdSpdIsRotEnc = false;
//static bool          tcdSpdIsRemote = false;
static int16_t       currSpeedOldGPS = -2;
bool                 displayGPSMode = DEF_DISP_GPS;
uint16_t             tcdIsInP0 = 0, tcdIsInP0Old = 1000;
static uint16_t      tcdSpeedP0 = 0, tcdSpeedP0Old = 1000;
static uint16_t      tcdIsInP0stalled = 0;
static unsigned long tcdInP0now = 0;
static uint32_t      bttfnTCDSeqCnt = 0;
static uint16_t      tcdSpdFake100 = 0;
static unsigned long tcdSpdChgNow = 0;
static unsigned long tcdClickNow = 0;
static uint16_t      remSpdAtP0Start = 0;

static int      iCmdIdx = 0;
static int      oCmdIdx = 0;
static uint32_t commandQueue[16] = { 0 };

#ifdef ESP32
/*  "warning: taking address of packed member of 'struct <anonymous>' may 
 *  result in an unaligned pointer value"
 *  "GCC will issue this warning when accessing an unaligned member of 
 *  a packed struct due to the incurred penalty of unaligned memory 
 *  access. However, all ESP chips (on both Xtensa and RISC-V 
 *  architectures) allow for unaligned memory access and incur no extra 
 *  penalty."
 *  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/migration-guides/release-5.x/5.0/gcc.html
 */
#define GET32(a,b)    *((uint32_t *)((a) + (b)))
#define SET32(a,b,c)  *((uint32_t *)((a) + (b))) = c
#else
#define GET32(a,b)          \
    (((a)[b])            |  \
    (((a)[(b)+1]) << 8)  |  \
    (((a)[(b)+2]) << 16) |  \
    (((a)[(b)+3]) << 24))   
#define SET32(a,b,c)                        \
    (a)[b]       = ((uint32_t)(c)) & 0xff;  \
    ((a)[(b)+1]) = ((uint32_t)(c)) >> 8;    \
    ((a)[(b)+2]) = ((uint32_t)(c)) >> 16;   \
    ((a)[(b)+3]) = ((uint32_t)(c)) >> 24; 
#endif 

static void displayVolume();
static void increaseBrightness();
static void decreaseBrightness();
static void displayBrightness();

static void disectVisMode();
static void triggerSaveVis();

static void condPLEDaBLvl(bool sLED, bool sLvl);

static void timeTravel(bool networkTriggered, uint16_t P0Dur = P0_DUR, uint16_t P1Dur = P1_DUR);
static void showDot();

static void execute_remote_command();

static void display_ip();
static bool display_soc_voltage(int type, bool displayAndReturn = false);

static void play_startup();
static void playBrakeWarning();

static void powKeyPressed();
static void powKeyLongPressStop();
static void brakeKeyPressed();
static void brakeKeyLongPressStop();
static void calibKeyPressed();
static void calibKeyPressStop();
static void calibKeyLongPressed();
static void buttonAKeyPressed();
static void buttonAKeyPressStop();
static void buttonAKeyLongPressed();
static void buttonBKeyPressed();
static void buttonBKeyPressStop();
static void buttonBKeyLongPressed();

static void butPackKeyPressed(int);
static void butPackKeyPressStop(int);
static void butPackKeyLongPressed(int);
static void butPackKeyLongPressStop(int i);
static void buttonPackActionPress(int i, bool stopOnly);
static void buttonPackActionLongPress(int i);
#ifdef REMOTE_HAVEMQTT
static void mqtt_send_button_on(int i);
static void mqtt_send_button_off(int i);
#endif

#ifdef HAVE_VOL_ROTENC
static void re_vol_reset();
#endif

static void myloop(bool withBTTFN);

static bool bttfn_connected();
static bool bttfn_trigger_tt(bool probe);
//static void bttfn_remote_keepalive();
static void bttfn_remote_send_combined(bool powerstate, bool brakestate, uint8_t speed);
static void bttfn_setup();
static void bttfn_loop_quick();

void main_boot()
{
}

void main_boot2()
{
    int i = 9;
    #ifdef HAVE_PM
    int batType = 0, batCap = 0;
    #endif
    const int8_t resetanim[10][4] = {
        {  1,  0, 0, 0 },
        { 32,  1, 0, 0 },
        { 32, 32, 1, 0 },
        { 32, 32, 2, 0 },
        { 32, 32, 3, 0 },
        { 32, 32, 4, 0 },
        { 32,  4, 0, 0 },
        {  4,  0, 0, 0 },
        {  5,  0, 0, 0 },
        {  6,  0, 0, 0 }
    };
    
    // Init LEDs

    remledStop.begin(STOPOUT_PIN);    // "Stop" LED + Switch output

    // Init Power LED and Level Meter
    usePwrLED = evalBool(settings.usePwrLED);
    useLvlMtr = evalBool(settings.useLvlMtr);

    // Power LED
    pwrled.begin(PWRLED_PIN, usePwrLED);
    // Battery level meter (treated as LED)
    bLvLMeter.begin(LVLMETER_PIN, useLvlMtr);

    // Power LED on Fake Power (or Real Power)
    if(usePwrLED || useLvlMtr) {
        if(!(pwrLEDonFP = evalBool(settings.pwrLEDonFP))) {
            pwrled.setState(true);
        }
        if(!(LvLMtronFP = evalBool(settings.LvLMtronFP))) {
            bLvLMeter.setState(true);
        }
    } else {
        pwrLEDonFP = LvLMtronFP = false;
    }

    // Init LED segment display
    if(!remdisplay.begin()) {
        Serial.println("Display not found");
    } else {
        loadBrightness();
    }

    #ifdef HAVE_PM
    // Init power monitor (if to be used)
    usePwrMon = evalBool(settings.usePwrMon);
    batType = atoi(settings.batType);
    batCap = atoi(settings.batCap);
    if(batType < BAT_PROF_01 || batType > BAT_PROF_MAX) {
        Serial.printf("Bad battery type %d\n", batType);
        usePwrMon = false;
    } else if(batCap < 1000 || batCap > 6000) {
        Serial.printf("Bad battery capacity %d, correcting to limit\n", batCap);
        if(batCap < 1000) batCap = 1000;
        if(batCap > 6000) batCap = 6000;
    }
    
    if(!(usePwrMon = pwrMon.begin(usePwrMon, batType, (uint16_t)batCap))) {
        Serial.println("Battery monitor hardware not found or disabled");
    }
    havePwrMon = pwrMon.havePM();
    #endif

    // Allow user to delete static IP data by holding Calibration button
    // while booting and pressing Button A ("O.O") twice within 10 seconds

    // Pre-maturely init button IOs (initialized again later)
    pinMode(CALIBB_IO_PIN, INPUT);
    pinMode(BUTA_IO_PIN, INPUT);
    delay(50);

    if(!digitalRead(CALIBB_IO_PIN)) {
        delay(50);
        if(!digitalRead(CALIBB_IO_PIN)) {

            unsigned long mnow = millis(), seqnow = 0;
            bool ssState, newSSState;
            int ssCount = 0;

            remdisplay.on();

            ssState = digitalRead(BUTA_IO_PIN);

            while(1) {

                if(millis() - seqnow > 50) {
                    seqnow = millis();
                    remdisplay.setText((char *)resetanim[i--]);
                    remdisplay.show();
                    if(!i) i = 9;
                }

                if((ssCount == 4) || (millis() - mnow > 10*1000)) break;

                if(digitalRead(BUTA_IO_PIN) != ssState) {
                    delay(50);
                    if((newSSState = digitalRead(BUTA_IO_PIN)) != ssState) {
                        ssCount++;
                        ssState = newSSState;
                    }
                } else {
                    delay(50);
                }
                
            }

            if(ssCount == 4) {

                remdisplay.setText("RST");
                remdisplay.show();

                Serial.println("Deleting ip config; temporarily clearing AP mode WiFi password");
    
                deleteIpSettings();
    
                // Set AP mode password to empty (not written, only until reboot!)
                settings.appw[0] = 0;

                while(!digitalRead(CALIBB_IO_PIN)) { }
            }

            remdisplay.clearBuf();
            remdisplay.show();
        }
    }

    #ifdef HAVE_PM
    if(!display_soc_voltage(0, true)) {
    #endif
        showWaitSequence();
    #ifdef HAVE_PM
    }
    #endif
}

void main_setup()
{
    unsigned long now;
    bool initScanBP = false;
    
    Serial.println("DTM Remote Control version " REMOTE_VERSION " " REMOTE_VERSION_EXTRA);

    loadMovieMode();
    loadDisplayGPSMode();
    
    // Eval from main settings
    autoThrottle = evalBool(settings.autoThrottle);
    doCoast = evalBool(settings.coast);
    powerMaster = evalBool(settings.pwrMst);
    // Overrule by terSettings if available
    // (In transition, might also overrule movieMode and GPSmode)
    if(loadVis()) {
        disectVisMode();
    } else {
        updateVisMode();
    }
    updateConfigPortalVisValues();
    updateConfigPortalVis2Values();

    ooTT = evalBool(settings.ooTT);
    ooresBri = !evalBool(settings.oorst);

    for(int i = 0; i < BTTFN_REM_MAX_COMMAND+1; i++) {
        bttfnSeqCnt[i] = 1;
    }

    buttonPackMomentary[0] = !evalBool(settings.bPb0Maint);
    buttonPackMomentary[1] = !evalBool(settings.bPb1Maint);
    buttonPackMomentary[2] = !evalBool(settings.bPb2Maint);
    buttonPackMomentary[3] = !evalBool(settings.bPb3Maint);
    buttonPackMomentary[4] = !evalBool(settings.bPb4Maint);
    buttonPackMomentary[5] = !evalBool(settings.bPb5Maint);
    buttonPackMomentary[6] = !evalBool(settings.bPb6Maint);
    buttonPackMomentary[7] = !evalBool(settings.bPb7Maint);

    buttonPackMtOnOnly[0] = evalBool(settings.bPb0MtO);
    buttonPackMtOnOnly[1] = evalBool(settings.bPb1MtO);
    buttonPackMtOnOnly[2] = evalBool(settings.bPb2MtO);
    buttonPackMtOnOnly[3] = evalBool(settings.bPb3MtO);
    buttonPackMtOnOnly[4] = evalBool(settings.bPb4MtO);
    buttonPackMtOnOnly[5] = evalBool(settings.bPb5MtO);
    buttonPackMtOnOnly[6] = evalBool(settings.bPb6MtO);
    buttonPackMtOnOnly[7] = evalBool(settings.bPb7MtO);

    #ifdef REMOTE_HAVEMQTT
    for(int i = 0; i < PACK_SIZE; i++) {
        MQTTbuttonOnLen[i] = MQTTbuttonOffLen[i] = 0;
    }
    if(useMQTT) {
        for(int i = 0; i < PACK_SIZE; i++) {
            if(*settings.mqttbt[i]) {
                MQTTbuttonOnLen[i] = strlen(settings.mqttbo[i]);
                MQTTbuttonOffLen[i] = strlen(settings.mqttbf[i]);
            }
        }
    }
    #endif

    // Invoke audio file installer if SD content qualifies
    #ifdef REMOTE_DBG
    Serial.println("Probing for audio data on SD");
    #endif
    if(check_allow_CPA()) {
        showWaitSequence();
        if(prepareCopyAudioFiles()) {
            play_file("/_installing.mp3", PA_ALLOWSD, 1.0f);
            waitAudioDone(false);
        }
        doCopyAudioFiles();
        // We never return here. The ESP is rebooted.
    }

    // Init music player (don't check for SD here)
    switchMusicFolder(musFolderNum, true);

    playClicks = evalBool(settings.playClick);

    havePOFFsnd = check_file_SD(powerOffSnd);
    haveBOFFsnd = check_file_SD(brakeOffSnd);
    haveThUp = check_file_SD(throttleUpSnd);

    // Initialize throttle
    if(rotEnc.begin()) {
        useRotEnc = true;
        loadCalib();
    } else {
        Serial.println("ADC throttle not found");
    }

    // Check for RotEnc for volume on secondary i2c addresses
    #ifdef HAVE_VOL_ROTENC
    if(rotEncV.begin(false)) {
        useRotEncVol = true;
        rotEncVol = &rotEncV;
        re_vol_reset();
    }
    #endif

    // Initialize switches and buttons
    powerswitch.begin(FPOWER_IO_PIN, true, true);  // active low, pullup
    powerswitch.setTiming(50, 50);
    powerswitch.attachLongPressStart(powKeyPressed);
    powerswitch.attachLongPressStop(powKeyLongPressStop);
    powerswitch.scan();

    brake.begin(STOPS_IO_PIN, false, false);       // active high, pulldown on board    
    brake.setTiming(50, 50);
    brake.attachLongPressStart(brakeKeyPressed);
    brake.attachLongPressStop(brakeKeyLongPressStop);
    brake.scan();

    calib.begin(CALIBB_IO_PIN, true, true);        // active low, pullup
    calib.setTiming(50, 2000);
    calib.attachPressDown(calibKeyPressed);
    calib.attachPressEnd(calibKeyPressStop);
    calib.attachLongPressStart(calibKeyLongPressed);
    calib.attachLongPressStop(calibKeyPressed);

    // Button A ("O.O")
    buttonA.begin(BUTA_IO_PIN, true, true);            // active low, pullup
    buttonA.setTiming(50, 2000);
    buttonA.attachPressDown(buttonAKeyPressed);
    buttonA.attachPressEnd(buttonAKeyPressStop);
    buttonA.attachLongPressStart(buttonAKeyLongPressed);
    buttonA.attachLongPressStop(buttonAKeyPressed);

    // Button B ("RESET")
    buttonB.begin(BUTB_IO_PIN, true, true);            // active low, pullup
    buttonB.setTiming(50, 2000);
    buttonB.attachPressDown(buttonBKeyPressed);
    buttonB.attachPressEnd(buttonBKeyPressStop);
    buttonB.attachLongPressStart(buttonBKeyLongPressed);
    buttonB.attachLongPressStop(buttonBKeyPressed);

    #ifdef ALLOW_DIS_UB
    if(!evalBool(settings.disBPack)) {
    #endif
        if((useBPack = butPack.begin())) {
            butPack.setScanInterval(50);
            butPack.attachPressDown(butPackKeyPressed);
            butPack.attachPressEnd(butPackKeyPressStop);
            butPack.attachLongPressStart(butPackKeyLongPressed);
            butPack.attachLongPressStop(butPackKeyLongPressStop);
            for(int i = 0; i < butPack.getPackSize(); i++) {
                isbutPackKeyPressed[i] = false;
                isbutPackKeyLongPressed[i] = false;
                isbutPackKeyChange[i] = false;
                if(buttonPackMomentary[i]) {
                    butPack.setTiming(i, 50, 2000);
                } else {
                    butPack.setTiming(i, 50, 50);
                    initScanBP = true;
                }
            }
            if(initScanBP) {
                butPack.scan();
            }
        } else {
            #ifdef REMOTE_DBG
            Serial.println("ButtonPack not detected");
            #endif
        }
    #ifdef ALLOW_DIS_UB    
    } else {
        useBPack = false;
        #ifdef REMOTE_DBG
        Serial.println("ButtonPack disabled");
        #endif
    }
    #endif
    
    now = millis();

    if(!haveAudioFiles) {
        #ifdef REMOTE_DBG
        Serial.println("Current audio data not installed");
        #endif
        remdisplay.on();
        remdisplay.setText("ISP");
        remdisplay.show();
        delay(1000);
        remdisplay.clearBuf();
        remdisplay.show();
    } else if(showUpdAvail && updateAvailable()) {
        remdisplay.on();
        remdisplay.setText("UPD");
        remdisplay.show();
        delay(500);
        remdisplay.clearBuf();
        remdisplay.show();
    }

    // Initialize BTTF network
    bttfn_setup();
    bttfn_loop();
  
    FPBUnitIsOn = false;
    justBootedNow = millis();
    bootFlag = true;

    // Short delay to allow .scan(s) in loop to detect powerswitch 
    // and brake changes in first iteration, and to bridge debounce
    // time for buttons 1-8 if maintained
    now = millis() - now;
    if(now < 60) {
        mydelay(60 - now, true);
    }

    // For maintained switches, do second scan and clear Change flag 
    // so initial position does not trigger an event
    if(initScanBP) {
        butPack.scan();
        for(int i = 0; i < butPack.getPackSize(); i++) {
            if(!buttonPackMomentary[i]) {
                isbutPackKeyChange[i] = false;
            }
        }
    }
}

void main_loop()
{
    unsigned long now = millis();

    if(triggerCompleteUpdate) {
        triggerCompleteUpdate = false;
        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
    }

    #ifdef HAVE_PM
    // Scan battery monitor
    if(!TTrunning && !tcdIsInP0 && !calibMode) {
        battWarn = pwrMon.loop();
    }
    #endif
   
    /*
     * "Battery Low" warning
     * If PowerLED is used, it blinks.
     * Else: If Level Meter is used, it is reset to zero; when Fake Power
     *       is off, "BAT" is displayed every 30 seconds.
     * Else: If Fake power is off, "BAT" is displayed every 30 seconds,
     *       Else: Display blinks for 5 seconds every 30 seconds.
     */
    if(battWarn) {
        if(oldBattWarn != battWarn) {
            if(useLvlMtr) {
                bLvLMeter.setState(false);
            }
            battWarnBlinkNow = 0;
            battWarnDispBlink = false;
            battWarnSnd = true;
        }
        oldBattWarn = battWarn;
        if(battWarnSnd && !TTrunning && !calibMode && !tcdIsInP0 && !throttlePos && !keepCounting) {
            append_file("/pwrlow.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
            battWarnSnd = false;
        }
        if(usePwrLED) {
            if(now - battWarnBlinkNow > 1000) {
                pwrled.setState(!pwrled.getState());
                battWarnBlinkNow = now;
            }
        } else if(!useLvlMtr || !FPBUnitIsOn) {
            if(!FPBUnitIsOn) {
                if(!calibMode && (now - battWarnBlinkNow > 30000)) {
                    remdisplay.setText("BAT");
                    remdisplay.show();
                    remdisplay.on();
                    offDisplayTimer = true;
                    offDisplayNow = now;
                    battWarnBlinkNow = now;
                }
            } else {
                if(!battWarnDispBlink) {
                    if((!battWarnBlinkNow) || (now - battWarnBlinkNow > 30000)) {
                        battWarnBlinkNow = now;
                        remdisplay.blink(true);
                        battWarnDispBlink = !battWarnDispBlink;
                    }
                } else {
                    if(now - battWarnBlinkNow > 5000) {
                        battWarnBlinkNow = now;
                        remdisplay.blink(false);
                        battWarnDispBlink = !battWarnDispBlink;
                    }
                }
            }
        }
    } else if(oldBattWarn) {
        condPLEDaBLvl(FPBUnitIsOn, FPBUnitIsOn);
        remdisplay.blink(false);
        oldBattWarn = false;
    }

    // Scan power switch
    powerswitch.scan();
    if(isFPBKeyChange) {
        isFPBKeyChange = false;
        powerState = isFPBKeyPressed;
        #ifdef REMOTE_DBG
        if(bootFlag) {
            Serial.printf("Power change detected at boot\n");
        }
        #endif
        if(isFPBKeyPressed) {
            if(!FPBUnitIsOn) {
                bool wifiblocks = false;
                
                // Fake power on:
                FPBUnitIsOn = true;

                // power LED and level meter
                condPLEDaBLvl(true, true);
  
                calibMode = false;    // Cancel calibration

                offDisplayTimer = false;

                etmr = false;

                // Re-connect if we're in AP mode but
                // there is a configured WiFi network
                if(!justBootedNow && wifiNeedReConnect(wifiblocks)) {
                    if(wifiblocks) {
                        showWaitSequence();
                        remdisplay.on();
                    }
                    // Enable WiFi
                    wifiOn(0);
                    if(wifiblocks) {
                        endWaitSequence();
                    }
                }
                justBootedNow = 0;

                currSpeedF = 0;
                currSpeed = 0;
                keepCounting = false;
                triggerTTonThrottle = 0;

                calibIP = true;

                // Re-init brake
                remledStop.setState(true);

                // Display ON
                remdisplay.setBrightness(255);

                // Scan brake switch
                brake.scan();

                play_startup();

                // Scan brake switch again
                brake.scan();
                if(isBrakeKeyChange) {
                    isBrakeKeyChange = false;
                    brakeState = isBrakeKeyPressed;
                    // Do NOT play sound
                }

                //if(powerMaster) {
                //    bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                //}

                remdisplay.blink(false);
                remdisplay.on();
                remdisplay.setSpeed(currSpeedF);
                remdisplay.show();
                doForceDispUpd = false;

                tcdSpeedP0Old = 2000;
                tcdIsInP0 = tcdIsInP0Old = 0;
                remSpdAtP0Start = 0;

                lockThrottle = false;

                networkTimeTravel = false;
                networkAbort = false;

                bttfn_remote_send_combined(powerState, brakeState, currSpeed);

                play_file(powerOnSnd, PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
            }
        } else {
            if(FPBUnitIsOn) {
                // Fake power off:
                FPBUnitIsOn = false;

                // Reset here for not using an 
                // old value if P0 starts while
                // we are off
                currSpeedF = 0;
                currSpeed = 0;
                triggerTTonThrottle = 0;

                // power LED and level meter
                condPLEDaBLvl(false, false);

                // Stop musicplayer & audio in general
                mp_stop();
                stopAudio();

                remledStop.setState(false);
                cancelBrakeWarning = true;

                TTrunning = TTP0 = TTP1 = TTP2 = false;
                IntP0running = false;
                
                offDisplayTimer = false;
                if(displayGPSMode) {
                    currSpeedOldGPS = -2;   // Trigger GPS speed display update
                }

                remdisplay.off();

                flushDelayedSave();

                bttfn_remote_send_combined(powerState, brakeState, currSpeed);

                if(havePOFFsnd) {
                    play_file(powerOffSnd, PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
                }
            }
        }
    } else {
        if(bootFlag) {
            #ifdef REMOTE_DBG
            Serial.printf("no power change at boot detected\n");
            #endif
        
            // Send initial status to TCD
            sendBootStatus = true;
        }
    }

    // Eval flags set in handle_tcd_notification
    if(doPrepareTT) {
        if(FPBUnitIsOn && !TTrunning) {
            prepareTT();
        }
        doPrepareTT = false;
    }
    if(doWakeup) {
        if(FPBUnitIsOn && !TTrunning) {
            wakeup();
        }
        doWakeup = false;
    }

    // Scan brake switch
    if(FPBUnitIsOn) {
        brake.scan();
        if(isBrakeKeyChange) {
            isBrakeKeyChange = false;
            brakeState = isBrakeKeyPressed;
            cancelBrakeWarning = !brakeState;
            brakeWarning = false;
            bttfn_remote_send_combined(powerState, brakeState, currSpeed);
            sendBootStatus = false;

            if(!TTrunning) {
                if(brakeState) {
                    play_file(brakeOnSnd, PA_ALLOWSD|PA_DYNVOL, 1.0f);
                } else if(haveBOFFsnd) {
                    play_file(brakeOffSnd, PA_ALLOWSD|PA_DYNVOL, 1.0f);
                }
            }
        }
    }

    // Button A "O.O":
    //    Fake-power on:
    //        If buttonPack is enabled/present:
    //            Short press: BTTFN-wide TT or MusicPlayer Previous Song (depending on option)
    //            Long press:  MusicPlayer Play/Stop
    //        If buttonPack is disabled/not present: 
    //            Short press: Play "key3"
    //            Long press:  MusicPlayer Play/Stop
    //    Fake-power off:
    //        Short press: Increase volume
    //        Long press: Increase brightness or take over Fake-Power control (depending on option)
    // Button B "RESET":
    //    Fake-power on: 
    //        If buttonPack is enabled/present:
    //            Short press: MusicPlayer Next Song
    //            Long press:  MusicPlayer: Toggle shuffle
    //        If buttonPack is disabled/not present: 
    //            Short press: Play "key6"
    //            Long press:  MusicPlayer Next Song
    //    Fake-power off:
    //        Short press: Decrease volume
    //        Long press: Decrease brightness or relinquish Fake-Power control (depending on option)
    buttonA.scan();
    if(isbuttonAKeyChange) {
        isbuttonAKeyChange = false;
        if(FPBUnitIsOn) {
            if(!TTrunning && !tcdIsInP0) {
                if(useBPack) {
                    if(isbuttonAKeyPressed) {
                        if(ooTT) {
                            brakeWarning = false;
                            if(!triggerTTonThrottle) {
                                // Here we only trigger a stand-alone TT
                                // if we are not connected. If the TCD is
                                // busy, we refuse. (Different to below
                                // because when hitting 88 there is no time
                                // for the TCD to change its busy status
                                // and therefore not chance for an unwanted
                                // dual-tt.)
                                triggerIntTTonThrottle = 0;
                                if(!bttfn_trigger_tt(true)) {
                                    triggerTTonThrottle = 1;
                                    triggerIntTTonThrottle = 1;
                                } else if(tcdIsBusy) {
                                    play_bad();
                                } else {
                                    triggerTTonThrottle = 1;
                                }
                                if(triggerTTonThrottle) {
                                    play_file("/rdy.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                                }
                            } else if(triggerTTonThrottle == 1) {
                                triggerTTonThrottle = 0;
                                play_bad();
                            }
                        } else {
                            if(haveMusic) {
                                mp_prev(mpActive);
                            } else {
                                play_bad();
                            }
                        }
                    } else if(isbuttonAKeyLongPressed) {
                        if(haveMusic) {
                            if(mpActive) {
                                mp_stop();
                            } else {
                                mp_play();
                            }
                        } else {
                            play_bad();
                        }
                    }
                #ifdef ALLOW_DIS_UB
                } else {
                    if(isbuttonAKeyPressed) {
                        play_key(3);
                    } else if(isbuttonAKeyLongPressed) {
                        if(haveMusic) {
                            if(mpActive) {
                                mp_stop();
                            } else {
                                mp_play();
                            }
                        } else {
                            play_bad();
                        }
                    }
                #endif  // ALLOW_DIS_UB
                }
            }
        } else if(!calibMode) {           // When off, but not in calibMode
            if(isbuttonAKeyPressed) {
                if(!useRotEncVol) {
                    // Do not change, just display on first press (within 10 seconds)
                    if(volchgtimer && millis() - volchgtimer < 10*1000) {
                        increaseVolume();
                    }
                    displayVolume();
                    offDisplayTimer = true;
                    offDisplayNow = volchgtimer = millis();
                    play_file("/volchg.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                }
            } else if(isbuttonAKeyLongPressed) {
                if(ooresBri) {
                    // Do not change, just display on first press (within 10 seconds)
                    if(brichgtimer && millis() - brichgtimer < 10*1000) {
                        increaseBrightness();
                    }
                    displayBrightness();
                    offDisplayTimer = true;
                    offDisplayNow = brichgtimer = millis();
                } else {
                    powerMaster = true;
                    updateVisMode();
                    triggerSaveVis();
                    bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                    play_file("/pmon.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                }
            }
        }
    }
    buttonB.scan();
    if(isbuttonBKeyChange) {
        isbuttonBKeyChange = false;
        if(FPBUnitIsOn) {
            if(!TTrunning && !tcdIsInP0) {
                if(useBPack) {
                    if(isbuttonBKeyPressed) {
                        if(haveMusic) {
                            mp_next(mpActive);
                        } else {
                            play_bad();
                        }
                    } else if(isbuttonBKeyLongPressed) {
                        mp_makeShuffle(!mpShuffle);
                        if(haveMusic) {
                            play_file(mpShuffle ? "/shufon.mp3" : "/shufoff.mp3", PA_ALLOWSD, 1.0f);
                        } else {
                            play_bad();
                        }
                    }
                #ifdef ALLOW_DIS_UB
                } else {
                    if(isbuttonBKeyPressed) {
                        play_key(6);
                    } else if(isbuttonBKeyLongPressed) {
                        if(haveMusic) {
                            mp_next(mpActive);
                        } else {
                            play_bad();
                        }
                    }
                #endif  // ALLOW_DIS_UB
                }
            }
        } else if(!calibMode) {           // When off, but not in calibMode
            if(isbuttonBKeyPressed) {
                if(!useRotEncVol) {
                    // Do not change, just display on first press (within 10 seconds)
                    if(volchgtimer && millis() - volchgtimer < 10*1000) {
                        decreaseVolume();
                    }
                    displayVolume();
                    offDisplayTimer = true;
                    offDisplayNow = volchgtimer = millis();
                    play_file("/volchg.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                }
            } else if(isbuttonBKeyLongPressed) {
                if(ooresBri) {
                    // Do not change, just display on first press (within 10 seconds)
                    if(brichgtimer && millis() - brichgtimer < 10*1000) { 
                        decreaseBrightness();
                    }
                    displayBrightness();
                    offDisplayTimer = true;
                    offDisplayNow = brichgtimer = millis();
                } else {
                    powerMaster = false;
                    updateVisMode();
                    triggerSaveVis();
                    bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                    play_file("/pmoff.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                }
            }
        }
    }

    // Calibration button:
    //    Fake-power is off: Throttle calibration
    //        - Short press registers current position as "center" (zero) position.
    //        - Long press enters calib mode for full throttle forward and backward:
    //             * When "UP" is displayed: Hold throttle full up, 
    //             * short-press calibration button, 
    //             * When "DN" is displayed: Hold throttle full down, 
    //             * short-press calibration button.
    //          If calib button is long-pressed during calib mode, calibration is cancelled
    //        No calibration as long as there is a battery warning.
    //    Fake-power on:
    //        Short press: Reset speed to 0
    // .      Long press:  First time: Display IP address, subsequently SOC and TTE/TTF alternately
    calib.scan();
    if(isCalibKeyChange) {
        isCalibKeyChange = false;
        if(!FPBUnitIsOn) {
            if(battWarn) {
                remdisplay.setText("BAT");
                remdisplay.show();
                remdisplay.on();
                offDisplayTimer = true;
                offDisplayNow = millis();
                isCalibKeyPressed = isCalibKeyLongPressed = false;
            } else if(isCalibKeyPressed) {
                if(calibMode) {
                    if(calibUp) {
                        if(useRotEnc && rotEnc.setMaxStepsUp(0)) {
                            remdisplay.setText("DN");
                            remdisplay.show();
                            calibUp = false;
                        } else {
                            remdisplay.setText("ERR");
                            remdisplay.show();
                            offDisplayTimer = true;
                            offDisplayNow = millis();
                            calibMode = false;
                            condPLEDaBLvl(false, false);
                        }
                    } else {
                        if(useRotEnc && rotEnc.setMaxStepsDown(0)) {
                            calibMode = false;
                            saveCalib();
                            remdisplay.clearBuf();
                            remdisplay.show();
                            remdisplay.off();
                            currSpeedOldGPS = -2;   // force GPS speed display update
                        } else {
                            remdisplay.setText("ERR");
                            remdisplay.show();
                            offDisplayTimer = true;
                            offDisplayNow = millis();
                            calibMode = false;
                        }
                        condPLEDaBLvl(false, false);
                    }
                } else {
                    condPLEDaBLvl(true, true);
                    remdisplay.setText("CAL");
                    remdisplay.show();
                    remdisplay.on();
                    delay((pwrLEDonFP || LvLMtronFP) ? 2000 : 200);    // Stabilize voltage after turning on display, LED, level meter
                    offDisplayTimer = true;
                    offDisplayNow = millis();
                    if(useRotEnc) {
                        rotEnc.zeroPos(true);
                        if(!rotEnc.dynZeroPos()) {
                            saveCalib();
                        } 
                    }
                    condPLEDaBLvl(false, false);
                }
            } else if(isCalibKeyLongPressed) {
                if(calibMode) {
                    calibMode = false;
                    remdisplay.clearBuf();
                    remdisplay.show();
                    remdisplay.off();
                    condPLEDaBLvl(false, false);
                    currSpeedOldGPS = -2;   // force GPS speed display update
                } else {
                    calibMode = true;
                    offDisplayTimer = false;
                    condPLEDaBLvl(true, true);
                    if(pwrLEDonFP || LvLMtronFP) {
                        showWaitSequence();
                        remdisplay.on();
                        delay(2000);
                    }
                    remdisplay.setText("UP");
                    remdisplay.show();
                    remdisplay.on();
                    calibUp = true;
                }
            }
        } else {
            if(!TTrunning && !tcdIsInP0) {
                if(isCalibKeyPressed) {
                    if(!throttlePos) {
                        currSpeedF = 0;
                        currSpeed = 0;
                        keepCounting = false;
                        triggerTTonThrottle = 0;
                        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                        doForceDispUpd = true;
                    }
                } else if(isCalibKeyLongPressed) {
                    flushDelayedSave();
                    #ifdef HAVE_PM
                    if(calibIP) {
                        display_ip();
                        if(havePwrMon) calibIP = false;
                    } else {
                        if(display_soc_voltage(dispSOC)) {
                            dispSOC++;
                            if(dispSOC > 1) dispSOC = 0;
                        } else {
                            display_ip();
                        }
                    }
                    #else
                    display_ip();
                    #endif
                    doForceDispUpd = true;
                    triggerTTonThrottle = 0;
                }
            }
        }
    }

    // Optional button pack: Up to 8 momentary buttons or maintained switches
    if(useBPack) {
        butPack.scan();
        for(int i = 0; i < butPack.getPackSize(); i++) {
            if(isbutPackKeyChange[i]) {
                isbutPackKeyChange[i] = false;
                if(FPBUnitIsOn) {
                    if(!TTrunning && !tcdIsInP0) {
                        if(!buttonPackMomentary[i]) {
                            // Maintained: 
                            // If "audio on ON only" checked, play if switched ON, and stop
                            // on OFF if same sound triggered by ON is still playing.
                            // If "audio on ON only" unchecked: Every flip triggers keyX 
                            // playback. A second flip while the same keyX sound is still 
                            // being played, causes a stop.
                            buttonPackActionPress(i, (buttonPackMtOnOnly[i] && !isbutPackKeyPressed[i]));
                        } else {
                            // Momentary: Press triggers keyX, long press keyXl.
                            // A second press/longpress while the same keyX(l) 
                            // sound is still being played, causes a stop.
                            if(isbutPackKeyPressed[i]) {
                                buttonPackActionPress(i, false);
                            } else if(isbutPackKeyLongPressed[i]) {
                                buttonPackActionLongPress(i);
                            }
                        }
                    }
                } else {
                    flushDelayedSave();
                }
            } 
        }
    }

    // tcdIsInP0 is set while TTrunning is still false
    // IntP0running is set while TTrunning is already true

    // Scan throttle position
    if(triggerTTonThrottle) {
        throttlePos = rotEnc.updateThrottlePos();
        if(FPBUnitIsOn && !TTrunning && !tcdIsInP0) {
            if(triggerTTonThrottle == 1 && throttlePos > 0) {
                if(brakeState) {
                    if(!brakeWarning) {
                        playBrakeWarning();
                        brakeWarning = true;
                    }
                } else {
                    triggerTTonThrottle++;
                    if(triggerIntTTonThrottle) {
                        timeTravel(false, 65535);
                        triggerTTonThrottle = 0;
                    } else {
                        bttfn_trigger_tt(false);
                        triggerTTonThrottleNow = millis();
                    }
                    brakeWarning = false;
                }
            } else if(triggerTTonThrottle > 1) {
                // If bttfn_trigger_tt() didn't lead to P0 or
                // a tt for whatever reason, reset the flag.
                // Might happen due to delay in mutual network 
                // communication on brakeState, tt phases, ...
                if(millis() - triggerTTonThrottleNow > 1000) {
                    triggerTTonThrottle = 0;
                    play_bad();
                }
            }
        }
    } else if(!calibMode && !tcdIsInP0) {
        throttlePos = rotEnc.updateThrottlePos();
        if(FPBUnitIsOn && (!TTrunning || IntP0running)) {
            int tas = 0, tidx = 0;

            // IntP0running is part of the TT sequence
            // so TTrunning is true. (Unlike tcdIsInP0)
            if(IntP0running) {
              
                throttlePos = 100;
                lockThrottle = false;
            
            } else {
            
                if(!throttlePos) {
                    lockThrottle = false;
                }
    
                // Auto throttle
                if(keepCounting) {
                    if(throttlePos < 0) {
                        keepCounting = false;
                    } else if(oldThrottlePos > 0) {
                        if(throttlePos < oldThrottlePos) {
                            throttlePos = oldThrottlePos;
                        }
                    }
                } else if(autoThrottle) {
                    keepCounting = (throttlePos > 0 && !oldThrottlePos);
                }

            }
            
            if(movieMode) {

                if((oldThrottlePos = throttlePos)) {
                    tas = sizeof(strictAccelFacts)/sizeof(strictAccelFacts[0]);
                    tidx = (abs(throttlePos)-1) * tas / 100;
                    accelDelay = strictAccelDelays[currSpeedF / 10] * strictAccelFacts[tidx] / 100;
                    if(throttlePos < 0 && accelDelay < 13) accelDelay = 13;
                    accelStep = throttlePos > 0 ? 1 : strictdecelSteps[tidx];
                } else {
                    accelDelay = doCoast ? ((esp_random() % 30) + 60) : 0;
                }

            } else {
              
                if((oldThrottlePos = throttlePos)) {
                    tas = sizeof(accelDelays)/sizeof(accelDelays[0]);
                    tidx = (abs(throttlePos)-1) * tas / 100;
                    accelDelay = accelDelays[tidx];
                    accelStep = throttlePos > 0 ? accelSteps[tidx] : decelSteps[tidx];
                } else {
                    accelDelay = doCoast ? ((esp_random() % 30) + 55)  : 0;
                }

            }

            if(tidx < tas - 1 || throttlePos >= 0 || currSpeedF) {
                etmr = false;
            } else if(!etmr) {
                etmr = true;
                enow = millis();
            }
            if(etmr && millis() - enow > 5000) {
                etmr = false;
                play_file("/tmd.mp3", 0, 1.0f);
            }
            
            if(!lockThrottle) {
                if(millis() - lastSpeedUpd > accelDelay) {
                    int sbf = currSpeedF;
                    int sb  = sbf / 10;
                    if(throttlePos > 0) {
                        if(!currSpeedF) {
                            if(haveThUp) {
                                play_file(throttleUpSnd, PA_THRUP|PA_INTRMUS|PA_ALLOWSD, 1.0f);
                            } else {
                                play_throttleup();
                            }
                        }
                        currSpeedF += accelStep;
                        if(currSpeedF >= 880) {
                            currSpeedF = 880;
                            keepCounting = false;
                            if(!IntP0running) {
                                // Here we trigger a stand-alone TT if the
                                // TCD is busy. See above on why this is
                                // handled differently to tt-on-throttle.
                                if(!bttfn_connected() || tcdIsBusy) {
                                    timeTravel(false, 0, P1_DUR);
                                }
                            } else {
                                TTP0end = true;
                                IntP0running = false;
                            }
                        }
                    } else if(throttlePos < 0) {
                        currSpeedF -= accelStep;
                        if(currSpeedF < 0) currSpeedF = 0;
                        keepCounting = false;
                    } else if(doCoast) {
                        if(currSpeedF > 0) {
                            currSpeedF -= max(0, (int)(esp_random() % coastCurve[currSpeedF/10][0]) - coastCurve[currSpeedF/10][1]);
                            if(currSpeedF < 0) currSpeedF = 0;
                        }
                    }
                    if(currSpeedF != sbf) {
                        currSpeed = currSpeedF / 10;
                        if(currSpeed != sb) {
                            bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                            if(throttlePos > 0) {
                                play_click();
                            }
                        }
                        lastSpeedUpd = millis();                
                        remdisplay.setSpeed(currSpeedF);
                        remdisplay.show();
                    }
                }
            }

            // Network-latency-depending display sync is nice'n'all but latency
            // measurement is dead as soon as audio comes into play. (1-2 vs 10-13).
            #ifdef REMOTE_DBG
            if(bttfnCurrLatency > 10) {
                Serial.printf("latency %d\n", bttfnCurrLatency);
            }
            #endif
        }
        if(!FPBUnitIsOn && !calibMode && !offDisplayTimer) {
            if(displayGPSMode) {
                if(tcdCurrSpeed != currSpeedOldGPS) {
                    remdisplay.on();
                    remdisplay.setSpeed(tcdCurrSpeed * 10);
                    remdisplay.show();
                    currSpeedOldGPS = tcdCurrSpeed;
                }
            } else {
                offDisplayTimer = true;
                offDisplayNow = millis() - 1001;
            }
        }
    }

    // If the TCD is in P0, we follow its speed.
    // P0 is independent of TTrunning; TTrunning will be true
    // when the TCD triggers a bttfn TT (which usually
    // happens at some point towards the end of P0)
    if(tcdIsInP0) {
        unsigned long now = millis();
        if(!tcdIsInP0Old || (tcdSpeedP0 != tcdSpeedP0Old)) {
            if(!tcdIsInP0Old) {
                triggerTTonThrottle = 0;
                tcdIsInP0Old = tcdIsInP0;
                tcdClickNow = 0;
                tcdInP0now = now;
                remSpdAtP0Start = currSpeedF / 10;
                #ifdef REMOTE_DBG
                Serial.printf("Switching to P0\n");
                #endif
            } else {
                tcdInP0now = now;
            }
            if(FPBUnitIsOn) {
                if(tcdSpeedP0 > remSpdAtP0Start) {  // yes, ">"
                    if(!tcdIsInP0stalled) {
                        if(!tcdSpeedP0) {
                            if(haveThUp) {
                                play_file(throttleUpSnd, PA_THRUP|PA_INTRMUS|PA_ALLOWSD, 1.0f);
                            } else {
                                play_throttleup();
                            }
                        } else if(!tcdClickNow || now - tcdClickNow > 25) {
                            tcdClickNow = now;
                            play_click();
                        }
                    }
                    currSpeed = tcdSpeedP0;
                    currSpeedF = tcdSpeedP0 * 10;
                    if(tcdIsInP0stalled) {
                        currSpeedF += remdisplay.getSpeedPostDot();
                    }
                    remdisplay.on();
                    remdisplay.setSpeed(currSpeedF);
                    remdisplay.show();
                }
                tcdSpeedP0Old = tcdSpeedP0;
                tcdSpdChgNow = now;
                tcdSpdFake100 = 0;
            }
        } else if(FPBUnitIsOn) {
            // Fake .1s
            if(tcdSpeedP0 >= remSpdAtP0Start) {   // yes, ">="
                if(!tcdIsInP0stalled && (now - tcdSpdChgNow > accelDelays[4])) {
                    tcdSpdChgNow = now;
                    tcdSpdFake100++;
                    if(tcdSpdFake100 > 9) tcdSpdFake100 = 1;
                    currSpeed = tcdSpeedP0;
                    currSpeedF = (tcdSpeedP0 * 10) + tcdSpdFake100;
                    remdisplay.on();
                    remdisplay.setSpeed(currSpeedF);
                    remdisplay.show();
                }
            }
        }
        if(now - tcdInP0now > 10*1000) {
            #ifdef REMOTE_DBG
            Serial.printf("Ending P0: delay %ums (limit 10000)\n", now - tcdInP0now);
            #endif
            tcdIsInP0 = 0;
            doForceDispUpd = true;
            triggerTTonThrottle = 0;
        }
    } else if(tcdIsInP0Old) {
        tcdSpeedP0Old = 2000;
        tcdIsInP0Old = 0;
        doForceDispUpd = true;
        triggerTTonThrottle = 0;
        #ifdef REMOTE_DBG
        Serial.printf("P0 is off\n");
        #endif
    }
    
    if(FPBUnitIsOn && doForceDispUpd && !TTrunning && !tcdIsInP0) {
        doForceDispUpd = false;
        remdisplay.setSpeed(currSpeedF);
        remdisplay.show();
    }

    // This serves as our KEEP_ALIVE
    if(millis() - lastCommandSent > 10*1000) {
        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
    }

    if(justBootedNow && (millis() - justBootedNow > 10*1000)) {
        justBootedNow = 0;
    }

    if(!FPBUnitIsOn && offDisplayTimer && millis() - offDisplayNow > 1000) {
        offDisplayTimer = false;
        remdisplay.off();
        currSpeedOldGPS = -2;   // force GPS speed display update
    }

    // Execute remote commands
    // No commands in calibMode, during a TT (or P0), and when acceleration is running
    // FPBUnitIsOn checked for each individually
    if(!TTrunning && !calibMode && !tcdIsInP0 && !throttlePos && !keepCounting) {
        execute_remote_command();
    }

    if(FPBUnitIsOn) {
      
        // TT triggered?
        if(!TTrunning) {
            if(networkTimeTravel) {
                networkTimeTravel = false;
                if(!networkAbort) {
                    timeTravel(true, networkLead, networkP1);
                } else {
                    networkAbort = false;
                }
            }
        }

    }

    now = millis();
    
    // TT sequence logic: TT is mutually exclusive with stuff below
    
    if(TTrunning) {

        if(TTP0) {   // Acceleration - runs for P0duration ms

            if(extTT && !networkAbort && (now - TTstart < P0duration)) {

                // P0 is handled through tcdIsInP0 mode; this block
                // might not even be executed if the TCD calls TT() with
                // a networkLead of 0.
                
            } else if(!extTT && !TTP0end && (now - TTstart < P0duration)) {

                if(!TTFlag && !triggerP1NoLead && currSpeedF > triggerP1) {
                    play_file("/travelstart.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, TT_SOUND_FACT);
                    TTFlag = true;
                }

            } else {

                if(triggerP1NoLead) {
                    play_file("/travelstart2.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, TT_SOUND_FACT);
                }

                TTP0 = IntP0running = false;

                if(networkAbort) {

                    // If we were aborted during P0, we skip P1
                    TTP2 = true;
                    P1duration = P2_ALARM_DELAY;
                    
                } else {

                    TTP1 = true;

                    remdisplay.setText("88.0");
                    remdisplay.show();
                    remdisplay.on();
                }

                tcdIsInP0 = 0;
                
                TTFlag = false;
                TTstart = now;

            }
        }

        if(TTP1) {   // Peak/"time tunnel" 

            if(extTT) {         // external TT: ends with "REENTRY" or "ABORT" (or a long timeout)

                if(!networkReentry && !networkAbort && (now - TTstart < P1_maxtimeout)) {
                    
                    // Display "." at half of P1
            
                    if(!TTFlag && (now - TTstart > P1duration / 2)) {
    
                        showDot();
    
                        // Reset speed to 0, so TCD counts down
                        currSpeedF = 0;
                        currSpeed = 0;
                        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                      
                        TTFlag = true;
                    }
                    
                } else {

                    // If Reentry or Abort came early, do it here:
                    if(!TTFlag) {
                        showDot();
    
                        // Reset speed to 0, so TCD counts down
                        currSpeedF = 0;
                        currSpeed = 0;
                        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                    }

                    TTP1 = false;
                    TTP2 = true; 
    
                    TTstart = now;
                    TTFlag = false;

                    P1duration = P2_ALARM_DELAY;

                }
              
            } else {            // stand alone: runs for P1duration ms

                if(now - TTstart < P1duration) {

                    if(!TTFlag && (now - TTstart > P1duration / 2)) {
    
                        showDot();
    
                        // Reset speed to 0, so TCD counts down if it
                        // for some reason is connected despite we are
                        // running a stand-alone TT
                        currSpeedF = 0;
                        currSpeed = 0;
                        bttfn_remote_send_combined(powerState, brakeState, currSpeed);
                      
                        TTFlag = true;
                    }
                  
                } else {

                    TTP1 = false;
                    TTP2 = true;

                    //if(playTTsounds) {
                        play_file("/timetravel.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
                    //}
    
                    TTstart = now;
                    TTFlag = false;

                    P1duration = P2_ALARM_DLY_SA;
                  
                }

            }

        }

        if(TTP2) {   // Reentry - up to us

            if(TTFlag || networkAbort) {
            
                // Lock accel until lever is in zero-pos
                lockThrottle = true;

                doForceDispUpd = true;

                keepCounting = false;
                triggerTTonThrottle = 0;
  
                TTP2 = false;
                TTrunning = false;
                networkAbort = false;

            } else if(now - TTstart > P1duration) {
              
                play_file("/reentry.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
                TTFlag = true;

            }
            
        }

    } else {    // No TT currently

        if(networkAlarm && !calibMode && !tcdIsInP0 && !throttlePos && !keepCounting) {

            networkAlarm = false;

            if(evalBool(settings.playALsnd) > 0) {
                play_file("/alarm.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
            }
        
        } 
        
    }

    now = millis();

    // If network is interrupted, return to stand-alone
    if(useBTTFN) {
        if( !bttfnDataNotEnabled &&
            ((lastBTTFNpacket && (now - lastBTTFNpacket > 30*1000)) ||
             (!BTTFNBootTO && !lastBTTFNpacket && (now - powerupMillis > 60*1000))) ) {
            lastBTTFNpacket = 0;
            BTTFNBootTO = true;
            tcdCurrSpeed = -1;
            // P0 expires automatically
        }
    }

    // Poll RotEnv for volume. Don't in calibmode, P0 or during acceleration
    #ifdef HAVE_VOL_ROTENC
    if(useRotEncVol && !calibMode && !tcdIsInP0 && !throttlePos && !keepCounting) {
        int oldVol = curSoftVol;
        curSoftVol = rotEncVol->updateVolume(curSoftVol, false);
        if(oldVol != curSoftVol) {
            volchanged = true;
            volchgnow = millis();
            storeCurVolume();
            if(!FPBUnitIsOn && !TTrunning) {
                play_file("/volchg.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);
            }
        }
    }
    #endif

    if(!TTrunning && !tcdIsInP0 && !calibMode && !throttlePos && !keepCounting) {
        // Save on-the-fly settings 8/3 seconds after last change
        if(brichanged && (now - brichgnow > 8000)) {
            brichanged = false;
            saveBrightness();
        }
        if(volchanged && (now - volchgnow > 8000)) {
            volchanged = false;
            saveCurVolume();
        }
        if(vischanged && (now - vischgnow > 3000)) {
            vischanged = false;
            saveVis();
        }
    }

    if(bootFlag) {
        bootFlag = false;
        if(sendBootStatus) {
            bttfn_remote_send_combined(powerState, brakeState, currSpeed);
            sendBootStatus = false;
        }
    }
}

void flushDelayedSave()
{
    if(brichanged) {
        brichanged = false;
        saveBrightness();
    }
    if(volchanged) {
        volchanged = false;
        saveCurVolume();
    }
    if(vischanged) {
        vischanged = false;
        saveVis();
    }
}

static void chgVolume(int d)
{
    int nv = curSoftVol;
    nv += d;
    if(nv < 0 || nv > 19)
        nv = curSoftVol;
        
    curSoftVol = nv;

    volchanged = true;
    volchgnow = millis();
    storeCurVolume();
}

void increaseVolume()
{
    chgVolume(1);
}

void decreaseVolume()
{
    chgVolume(-1);
}

static void displayVolume()
{
    char buf[8];
    sprintf(buf, "%3d", curSoftVol);
    remdisplay.setText(buf);
    remdisplay.show();
    remdisplay.on();
}

static void changeBrightness(int d)
{
    int b = (int)remdisplay.getBrightness();
    
    b += d;
    if(b < 0) b = 0;
    else if(b > 15) b = 15;
    
    remdisplay.setBrightness(b);
    brichanged = true;
    brichgnow = millis();
    storeBrightness();
}

static void increaseBrightness()
{
    changeBrightness(1);
}

static void decreaseBrightness()
{
    changeBrightness(-1);
}

static void displayBrightness()
{
    char buf[8];
    sprintf(buf, "%3d", remdisplay.getBrightness());
    remdisplay.setText(buf);
    remdisplay.show();
    remdisplay.on();
}

static void disectVisMode()
{
    autoThrottle   = !!(visMode & (1<<VISB_AT));
    doCoast        = !!(visMode & (1<<VISB_COAST));
    powerMaster    = !!(visMode & (1<<VISB_PWRM));
}

void disectOldVisMode()
{
    movieMode      = !!(visMode & (1<<VISB_MOV_MD));
    displayGPSMode = !!(visMode & (1<<VISB_DGPS));
    disectVisMode();
}

void updateVisMode()
{
    visMode = 0;
    if(autoThrottle)   visMode |= (1<<VISB_AT);
    if(doCoast)        visMode |= (1<<VISB_COAST);
    if(powerMaster)    visMode |= (1<<VISB_PWRM);
}

static void triggerSaveVis()
{
    vischanged = true;
    vischgnow = millis();
    storeVis();
    updateConfigPortalVisValues();
}

void setAutoThrottle(bool isOn)
{
    autoThrottle = isOn;
    updateVisMode();
    triggerSaveVis();
}
static void toggleAutoThrottle()
{
    setAutoThrottle(!autoThrottle);
}

void setCoast(bool isOn)
{
    doCoast = isOn;
    updateVisMode();
    triggerSaveVis();
}
static void toggleCoast()
{
    setCoast(!doCoast);
}

static void togglePwrMst()
{
    powerMaster = !powerMaster;
    updateVisMode();
    triggerSaveVis();

    bttfn_remote_send_combined(powerState, brakeState, currSpeed);
}

void setMovieMode(bool isOn)
{
    movieMode = isOn;
    saveMovieMode();
    updateConfigPortalVis2Values();
}
static void toggleMovieMode()
{
    setMovieMode(!movieMode);
}

void setDisplayGPS(bool isOn)
{
    displayGPSMode = isOn;
    saveDisplayGPSMode();
    updateConfigPortalVis2Values();

    if(!FPBUnitIsOn && displayGPSMode) {
        currSpeedOldGPS = -2;   // force GPS speed display update
    }
}
static void toggleDisplayGPS()
{
    setDisplayGPS(!displayGPSMode);
}

static void condPLEDaBLvl(bool sLED, bool sLvl)
{
    if(!battWarn) {
        if(pwrLEDonFP) {
            pwrled.setState(sLED);
        }
        if(LvLMtronFP) {
            bLvLMeter.setState(sLvl);
        }
    }
}

/*
 * Time travel
 * 
 */

static void timeTravel(bool networkTriggered, uint16_t P0Dur, uint16_t P1Dur)
{
    if(TTrunning)
        return;

    flushDelayedSave();
        
    TTrunning = true;
    TTstart = millis();

    TTP0 = true;   // phase 0
    TTP0end = IntP0running = false;
    TTFlag = false;

    extTT = networkTriggered;
    triggerP1NoLead = false;
    
    P0duration = P0Dur;
    P1duration = P1Dur;

    if(!extTT) {

        // Stand-alone TT:
        // Either count-up (P0Dur = 65536) or triggered at 88 (P0Dur = 0)
        // Count-up: P0 ends with TTP0end flag
        // Immediate: P0 is 0, we jump over P0
      
        if(P0Dur) {
            
            IntP0running = true;

            if(movieMode) {
                triggerP1 = P1_START_SPD_M;
                if(currSpeedF > triggerP1) {
                    triggerP1NoLead = true;
                }
            } else {
                triggerP1 = P1_START_SPD_L;
                if(currSpeedF > triggerP1) {
                    triggerP1NoLead = true;
                }
            }

        } else {

            // P0Dur is zero, play sound here (as P0 is practically jumped over)
            play_file("/travelstart2.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, TT_SOUND_FACT);
            
        }
        
    }

    P1_maxtimeout = P1Dur + 3000;

    etmr = false;

    #ifdef REMOTE_DBG
    Serial.printf("P0 duration is %d, P1 %d\n", P0duration, P1duration);
    #endif
}

static void showDot()
{
    // Only for time travel sequ
    remdisplay.setText("  .");
    remdisplay.show();
    remdisplay.on();
}

/*
 * Show special signals
 */

void showWaitSequence()
{
    remdisplay.setText("\78\7");
    remdisplay.show();
}

void endWaitSequence()
{
    remdisplay.clearBuf();
    remdisplay.show();
    doForceDispUpd = true;
}

void showCopyError()
{
    remdisplay.setText("ERR");
    remdisplay.show();
    doForceDispUpd = true;
}

void showNumber(int num)
{
    char buf[8];
    sprintf(buf, "%3d", num);
    remdisplay.setText(buf);
    remdisplay.show();
    doForceDispUpd = true;
}

void prepareTT()
{
    // We do nothing here.
    doPrepareTT = false;
}

// Wakeup: Sent by TCD upon entering dest date,
// return from tt, triggering delayed tt via ETT
// For audio-visually synchronized behavior
// >>> Also called when RotEnc speed is changed, so
//     could ignore if we are not Speed master
void wakeup()
{
    // We do nothing here.
    doWakeup = false;
}

static void execute_remote_command()
{
    uint32_t command = commandQueue[oCmdIdx];
    bool     injected = false;

    // No command execution during timed sequences
    // (Checked by caller)

    if(!command)
        return;

    commandQueue[oCmdIdx] = 0;
    oCmdIdx++;
    oCmdIdx &= 0x0f;

    if(command & 0x80000000) {
        injected = true;
        command &= ~0x80000000;
        // Allow user to directly use TCD code
        if(command >= 7000 && command <= 7999) {
            command -= 7000;
        } else if(command >= 7000000 && command <= 7999999) {
            command -= 7000000;
        }
        if(!command) return;
    }

    if(command < 10) {                                // 700x

        // All here only when we're on
        if(!FPBUnitIsOn)
            return;

        switch(command) {
        case 1:                                       // 7001: play "key1.mp3"
            play_key(1);
            break;
        case 2:                                       // 7002: Prev song
            if(haveMusic) {
                mp_prev(mpActive);                    
            }
            break;
        case 3:                                       // 7003: play "key3.mp3"
            play_key(3);
            break;
        case 4:                                       // 7004: play "key4.mp3"
            play_key(4);
            break;
        case 5:                                       // 7005: Play/stop
            if(haveMusic) {
                if(mpActive) {
                    mp_stop();
                } else {
                    mp_play();
                }
            }
            break;
        case 6:                                       // 7006: Play "key6.mp3"
            play_key(6);
            break;
        case 7:                                       // 7007: play "key7.mp3"
            play_key(7);
            break;
        case 8:                                       // 7008: Next song
            if(haveMusic) {
                mp_next(mpActive);
            }
            break;
        case 9:                                       // 7009: play "key9.mp3"
            play_key(9);
            break;
        }
      
    } else if(command < 100) {                        // 70xx

        // All here allowed when we're off

        switch(command) {
        case 60:                              // 7060  enable/disable Movie-like accel
            toggleMovieMode();
            break;
        case 61:                              // 7061  enable/disable "display TCD speed while fake-off"
            toggleDisplayGPS();
            triggerCompleteUpdate = true;
            break;
        case 62:                              // 7062  enable/disable autoThrottle
            toggleAutoThrottle();
            break;
        case 63:                              // 7063  enable/disable coasting
            toggleCoast();
            break;
        case 90:                              // 7090: Display IP address
            flushDelayedSave();
            remdisplay.on();
            display_ip();
            if(FPBUnitIsOn) {
                doForceDispUpd = true;
            } else {
                remdisplay.off();
                // force GPS speed display update
                currSpeedOldGPS = -2;   
            }
            break;
        case 91:                              // 7091: Display battery SOC
        case 92:                              // 7092: Display battery TTE
        case 93:                              // 7093: Display battery voltage
            #ifdef HAVE_PM
            if(havePwrMon) {
                flushDelayedSave();
                if(display_soc_voltage(command - 91)) {
                    if(FPBUnitIsOn) {
                        doForceDispUpd = true;
                    } else {
                        remdisplay.off();
                        // force GPS speed display update
                        currSpeedOldGPS = -2;   
                    }
                }
            }
            #endif
            break;
        case 96:                              // 7096: Toggle "Remote is power master"
            togglePwrMst();
            break;
        default:
            if(command >= 50 && command <= 59) {   // 7050-7059: Set music folder number
                if(haveSD) {
                    switchMusicFolder((uint8_t)command - 50);
                }
            }
            
        }
        
    } else if (command < 1000) {                      // 7xxx

        if(command >= 300 && command <= 399) {

            command -= 300;                           // 7300-7319/7399: Set fixed volume level
            if(command == 99) {
                // nada
            } else if(command <= 19) {
                curSoftVol = command;
                volchanged = true;
                volchgnow = millis();
                storeCurVolume();
                #ifdef HAVE_VOL_ROTENC
                re_vol_reset();
                #endif
            } else if(command == 50 || command == 51) { // 7350/7351: Disable/enable acceleration click sound
                playClicks = (command == 51);
            }

        } else if(command >= 400 && command <= 415) {

            command -= 400;                           // 7400-7415: Set brightness
            remdisplay.setBrightness(command);
            brichanged = true;
            brichgnow = millis();
            storeBrightness();

        } else if(command >= 501 && command <= 519) {

            if(!FPBUnitIsOn)
                return;

            if(command >= 501 && command <= 509) {    // 7501-7509/7511-7519 play keyX / keyXL
                play_key(command - 500, false);
            } else if(command >= 511 && command <= 519) {
                play_key(command - 510, true);
            }

        } else {

            // All here only when we're on
            if(!FPBUnitIsOn)
                return;

            switch(command) {
            case 222:                                 // 7222/7555 Disable/enable shuffle
            case 555:
                mp_makeShuffle((command == 555));
                break;
            case 888:                                 // 7888 go to song #0
                if(haveMusic) {
                    mp_gotonum(0, mpActive);
                }
                break;
            }
        }

    } else if(command < 10000) {                      // MQTT/internal commands

        #ifdef REMOTE_HAVEMQTT
        if(!injected) {
        
            command -= 1000;
            
            switch(command) {
            case 1:
                if(!FPBUnitIsOn) return;
                stop_key();
                break;
            case 2:
            case 3:
                setAutoThrottle((command == 2));
                break;
            case 4:
            case 5:
                setCoast((command == 4));
                break;
            case 6:
            case 7:
                setMovieMode((command == 6));
                break;
            case 8:
            case 9:
                setDisplayGPS((command == 8));
                break;
            case 12:
                if(!FPBUnitIsOn) return;
                if(haveMusic) mp_play();
                break;
            case 13:
                if(!FPBUnitIsOn) return;
                if(haveMusic && mpActive) {
                    mp_stop();
                }
                break;
            case 14:
                if(!FPBUnitIsOn) return;
                if(haveMusic) mp_next(mpActive);
                break;
            case 15:
                if(!FPBUnitIsOn) return;
                if(haveMusic) mp_prev(mpActive);
                break;
            // Internal commands
            case 900:
                if((millis() - brakeWarningNow < 2000) && !cancelBrakeWarning) {
                    playBrakeWarning();
                }
                break;
            }

        }
        #endif
        
    } else {
      
        switch(command) {
        case 53281:                               // 7053281: Toggle "update available" signal at boot
            showUpdAvail = !showUpdAvail;
            saveUpdAvail();
            updateConfigPortalUpdValues();
            break;
        case 64738:                               // 7064738: reboot
            if(!injected) {
                bttfn_remote_unregister();
                prepareReboot();
                delay(500);
                esp_restart();
            }
            break;
        case 123456:
            flushDelayedSave();
            if(!injected) {
                deleteIpSettings();               // 7123456: deletes IP settings
                if(settings.appw[0]) {
                    settings.appw[0] = 0;         //          and clears AP mode WiFi password
                    write_settings();
                }
            }
            break;
        default:                                  // 7888xxx: goto song #xxx
            if((command / 1000) == 888) {
                if(FPBUnitIsOn) {
                    uint16_t num = command - 888000;
                    num = mp_gotonum(num, mpActive);
                }
            }
            break;
        }

    }
}

static void display_ip()
{
    uint8_t a[4];
    char buf[8];

    remdisplay.blink(false);
    remdisplay.setText("IP");
    remdisplay.show();

    blockScan = true;

    mydelay(500, true);

    wifi_getIP(a[0], a[1], a[2], a[3]);

    for(int i = 0; i < 4; i++) {
        sprintf(buf, "%3d%s", a[i], (i < 3) ? "." : "");
        remdisplay.setText(buf);
        remdisplay.show();
        mydelay(1000, true);
    }
    
    remdisplay.clearBuf();
    remdisplay.show();
    mydelay(500, true);

    blockScan = false;
}

#ifdef HAVE_PM
static bool display_soc_voltage(int type, bool displayAndReturn)
{
    char buf[8];
    uint16_t tte;
    bool blink = false;

    if(havePwrMon) {
        remdisplay.on();
        switch(type) {
        case 0:
            if(!pwrMon._haveSOC) {
                if(!pwrMon.readSOC())
                    return false;
            }
            if(pwrMon._soc >= 100) {
                buf[0] = 'F';
                buf[1] = 'U';
                buf[2] = 'L';
                buf[3] = 0;
            } else {
                sprintf(buf, "%2d&", pwrMon._soc);
            }
            break;
        case 1:
            if(!pwrMon._haveTTE) {
                if(!pwrMon.readTimeToEmpty())
                    return false;
            }
            tte = pwrMon._tte;
            if(!tte || tte == 0xffff) {
                buf[0] = '-';
                buf[1] = '-';
                buf[2] = '-';
                buf[3] = 0;
            } else {
                if(tte > 999) tte = 999;
                sprintf(buf, "%3d", tte);
            }
            if(pwrMon._haveCharging) {
                blink = pwrMon._charging;
            }
            break;
        default:
            if(!pwrMon._haveVolt) {
                if(!pwrMon.readVoltage())
                    return false;
            }
            sprintf(buf, "%4.1f", pwrMon._voltage);
            break;
        }
        #ifdef REMOTE_DBG
        Serial.printf("[%s]\n", buf);
        #endif
        remdisplay.setText(buf);
        remdisplay.show();
        remdisplay.blink(blink);
        if(displayAndReturn) return true;
        blockScan = true;
        mydelay(2000, true);
        remdisplay.clearBuf();
        remdisplay.show();
        if(blink) remdisplay.blink(false);
        mydelay(500, true);
        blockScan = false;
        return true;
    }
    return false;
}
#endif

static void play_startup()
{    
    blockScan = true;
    remdisplay.setSpeed(0);
    remdisplay.show();
    remdisplay.on();

    mydelay(200, true);
    
    for(int i = 10; i <= 110; i+=10) {
        remdisplay.setSpeed(i);
        remdisplay.show();
        mydelay(80, true);
    }

    remdisplay.setSpeed(0);
    remdisplay.show();
    blockScan = false;
}

static void playBrakeWarning()
{
    if(brakecnt <= 4) {
        brakeRem[7] = brakecnt + '1';
        brakecnt++;
    } else {
        char t;
        do {
            t = (char)(esp_random() % 4) + '1';
        } while(t == brakeRem[7]);
        brakeRem[7] = t;
    }
    play_file(brakeRem, PA_NOINTR|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
}

void allOff()
{
    remdisplay.off();
    remledStop.setState(false);
}

void prepareReboot()
{
    mp_stop();
    stopAudio();

    allOff();

    pwrled.setState(false);
    bLvLMeter.setState(false);
        
    flushDelayedSave();
   
    delay(500);
    unmount_fs();
    delay(100);
}

bool switchMusicFolder(uint8_t nmf, bool isSetup)
{
    bool waitShown = false;

    if(nmf > 9) return false;

    if((musFolderNum != nmf) || isSetup) {

        remBusy = true;
        blockScan = true;
        
        if(!isSetup) {
            musFolderNum = nmf;
            // Initializing the MP can take a while;
            // need to stop all audio before calling
            // mp_init()
            if(haveMusic && mpActive) {
                mp_stop();
            }
            stopAudio();
        }
        if(haveSD) {
            if(mp_checkForFolder(musFolderNum) == -1) {
                flushDelayedSave();
                showWaitSequence();
                waitShown = true;
                play_file("/renaming.mp3", PA_INTRMUS|PA_ALLOWSD);
                waitAudioDone(true);
            }
        }
        if(!isSetup) {
            saveMusFoldNum();
            updateConfigPortalMFValues();
        }
        mp_init(isSetup);
        if(waitShown) {
            endWaitSequence();
        }

        blockScan = false;
        remBusy = false;
    }

    return waitShown;
}

void waitAudioDone(bool withBTTFN)
{
    int timeout = 400;

    while(!checkAudioDone() && timeout--) {
        mydelay(10, withBTTFN);
    }
}

static void powKeyPressed()
{
    isFPBKeyPressed = true;
    isFPBKeyChange = true;
}
static void powKeyLongPressStop()
{
    isFPBKeyPressed = false;
    isFPBKeyChange = true;
}

static void brakeKeyPressed()
{
    isBrakeKeyPressed = true;
    isBrakeKeyChange = true;
}
static void brakeKeyLongPressStop()
{
    isBrakeKeyPressed = false;
    isBrakeKeyChange = true;
}  

static void calibKeyPressed()
{
    isCalibKeyPressed = false;
    isCalibKeyLongPressed = false;
}

static void calibKeyPressStop()
{
    isCalibKeyPressed = true;
    isCalibKeyLongPressed = false;
    isCalibKeyChange = true;
}

static void calibKeyLongPressed()
{
    isCalibKeyPressed = false;
    isCalibKeyLongPressed = true;
    isCalibKeyChange = true;
}

static void buttonAKeyPressed()
{
    isbuttonAKeyPressed = false;
    isbuttonAKeyLongPressed = false;
}

static void buttonAKeyPressStop()
{
    isbuttonAKeyPressed = true;
    isbuttonAKeyLongPressed = false;
    isbuttonAKeyChange = true;
}

static void buttonAKeyLongPressed()
{
    isbuttonAKeyPressed = false;
    isbuttonAKeyLongPressed = true;
    isbuttonAKeyChange = true;
}

static void buttonBKeyPressed()
{
    isbuttonBKeyPressed = false;
    isbuttonBKeyLongPressed = false;
}

static void buttonBKeyPressStop()
{
    isbuttonBKeyPressed = true;
    isbuttonBKeyLongPressed = false;
    isbuttonBKeyChange = true;
}

static void buttonBKeyLongPressed()
{
    isbuttonBKeyPressed = false;
    isbuttonBKeyLongPressed = true;
    isbuttonBKeyChange = true;
}

static void butPackKeyPressed(int i)
{
    if(buttonPackMomentary[i]) {
        isbutPackKeyPressed[i] = false;
        isbutPackKeyLongPressed[i] = false;

        #ifdef REMOTE_HAVEMQTT
        mqtt_send_button_on(i);
        #endif
    }
}

static void butPackKeyPressStop(int i)
{
    if(buttonPackMomentary[i]) {
        isbutPackKeyPressed[i] = true;
        isbutPackKeyLongPressed[i] = false;
        isbutPackKeyChange[i] = true;
        
        #ifdef REMOTE_HAVEMQTT
        mqtt_send_button_off(i);
        #endif
    }
}

static void butPackKeyLongPressed(int i)
{
    if(buttonPackMomentary[i]) {
        isbutPackKeyPressed[i] = false;
        isbutPackKeyLongPressed[i] = true;
    } else {
        isbutPackKeyPressed[i] = true;
        isbutPackKeyLongPressed[i] = false;
        #ifdef REMOTE_HAVEMQTT
        mqtt_send_button_on(i);
        #endif
    }
    isbutPackKeyChange[i] = true;
}

static void butPackKeyLongPressStop(int i)
{
    isbutPackKeyPressed[i] = false;
    isbutPackKeyLongPressed[i] = false;
    isbutPackKeyChange[i] = true;
    #ifdef REMOTE_HAVEMQTT
    mqtt_send_button_off(i);
    #endif
}

static void buttonPackActionPress(int i, bool stopOnly)
{
    switch(i) {
    case 0:
        play_key(1, false, stopOnly);
        break;
    case 1:
        play_key(2, false, stopOnly);
        break;
    case 2:
        play_key(3, false, stopOnly);
        break;
    case 3:
        play_key(4, false, stopOnly);
        break;
    case 4:
        play_key(5, false, stopOnly);
        break;
    case 5:
        play_key(6, false, stopOnly);
        break;
    case 6:
        play_key(7, false, stopOnly);
        break;
    case 7:
        play_key(8, false, stopOnly);
        break;
    }
}

static void buttonPackActionLongPress(int i)
{
    switch(i) {
    case 0:
        play_key(1, true);
        break;
    case 1:
        play_key(2, true);
        break;
    case 2:
        play_key(3, true);
        break;
    case 3:
        play_key(4, true);
        break;
    case 4:
        play_key(5, true);
        break;
    case 5:
        play_key(6, true);
        break;
    case 6:
        play_key(7, true);
        break;
    case 7:
        play_key(8, true);
        break;
    }
}

#ifdef REMOTE_HAVEMQTT
static void mqtt_send_button_on(int i)
{
    if(!MQTTbuttonOnLen[i] || !FPBUnitIsOn)
        return;

    mqttPublish(settings.mqttbt[i], settings.mqttbo[i], MQTTbuttonOnLen[i]);
}

static void mqtt_send_button_off(int i)
{
    if(!MQTTbuttonOffLen[i] || !FPBUnitIsOn)
        return;

    mqttPublish(settings.mqttbt[i], settings.mqttbf[i], MQTTbuttonOffLen[i]);
}
#endif

#ifdef HAVE_VOL_ROTENC
static void re_vol_reset()
{
    if(useRotEncVol) {
        rotEncVol->zeroPos(curSoftVol);
    }
}
#endif

/*
 *  Do this whenever we are caught in a while() loop
 *  This allows other stuff to proceed
 */
static void myloop(bool withBTTFN)
{
    wifi_loop();
    audio_loop();
    if(withBTTFN) bttfn_loop_quick();
}

/*
 * MyDelay:
 * Calls myloop() periodically
 */
void mydelay(unsigned long mydel, bool withBTTFN)
{
    unsigned long startNow = millis();
    myloop(withBTTFN);
    while(millis() - startNow < mydel) {
        delay(10);
        myloop(withBTTFN);
    }
}

/*
 * Basic Telematics Transmission Framework (BTTFN)
 */

static bool check_packet(uint8_t *buf)
{
    // Basic validity check
    if(memcmp(buf, BTTFUDPHD, 4))
        return false;

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += buf[i] ^ 0x55;
    }

    return (buf[BTTF_PACKET_SIZE - 1] == a);
}

void addCmdQueue(uint32_t command)
{
    if(!command) return;

    commandQueue[iCmdIdx] = command;
    iCmdIdx++;
    iCmdIdx &= 0x0f;

    if(command == 1900) { 
        brakeWarningNow = millis();
    }
}

static void bttfn_eval_response(uint8_t *buf, bool checkCaps)
{
    if(checkCaps && (buf[5] & 0x40)) {
        bttfnReqStatus &= ~0x40;     // Do no longer poll capabilities
        if(buf[31] & 0x01) {
            bttfnReqStatus &= ~0x02; // Do no longer poll speed, comes over multicast
        }
        if(buf[31] & 0x10) {
            TCDSupportsNOTData = true;
        }
    }
    
    if(buf[5] & 0x10) {
        remoteAllowed = !!(buf[26] & 0x04);
        tcdIsBusy     = !!(buf[26] & 0x10);
        if(!remoteAllowed) {
            tcdIsInP0 = 0;
        }
    } else {
        remoteAllowed = false;
        tcdIsInP0 = 0;
    }

    if(buf[5] & 0x02) {
        tcdCurrSpeed = (int16_t)(buf[18] | (buf[19] << 8));
        if(tcdCurrSpeed > 88) tcdCurrSpeed = 88;
        //tcdSpdIsRotEnc = !!(buf[26] & 0x80); 
        //tcdSpdIsRemote = !!(buf[26] & 0x20);
    }
}

static void handle_tcd_notification(uint8_t *buf)
{
    uint32_t seqCnt;

    // Note: This might be called while we are in a
    // wait-delay-loop. Best to just set flags here
    // that are evaluated synchronously (=later).
    // Do not stuff that messes with display, input,
    // etc.

    if(buf[5] & BTTFN_NOT_DATA) {
        if(TCDSupportsNOTData) {
            bttfnDataNotEnabled = true;
            bttfnLastNotData = millis();
            seqCnt = GET32(buf, 27);
            if(bttfnSessionID && (bttfnSessionID != seqCnt)) {
                bttfnTCDDataSeqCnt = 1;
            }
            bttfnSessionID = seqCnt;
            seqCnt = GET32(buf, 6);
            if(seqCnt > bttfnTCDDataSeqCnt || seqCnt == 1) {
                #ifdef REMOTE_DBG_NET
                Serial.println("Valid NOT_DATA packet received");
                #endif
                bttfn_eval_response(buf, false);
            } else {
                #ifdef REMOTE_DBG_NET
                Serial.printf("Out-of-sequence NOT_DATA packet received %d %d\n", seqCnt, bttfnTCDDataSeqCnt);
                #endif
            }
            bttfnTCDDataSeqCnt = seqCnt;
        }
        return;
    }

    switch(buf[5]) {
    case BTTFN_NOT_SPD:       // TCD fw >= 10/26/2024 (MC)
        seqCnt = GET32(buf, 12);
        if(seqCnt > bttfnTCDSeqCnt || seqCnt == 1) {
            int t = buf[8] | (buf[9] << 8);
            tcdCurrSpeed = buf[6] | (buf[7] << 8);
            if(tcdCurrSpeed > 88) tcdCurrSpeed = 88;
            switch(t) {
            case BTTFN_SSRC_P0:
                tcdSpeedP0 = (uint16_t)tcdCurrSpeed;
                tcdIsInP0 = (FPBUnitIsOn && remoteAllowed && !TTP1 && !TTP2 && !remBusy) ? 1 : 0;
                tcdIsInP0stalled = buf[10] | (buf[11] << 8);  // TCD 3.9+
                break;
            default:
                tcdIsInP0 = 0;
            }
            #ifdef REMOTE_DBG_NET
            Serial.printf("TCD sent NOT_SPD: %d src %d (IsP0:%d)\n", tcdCurrSpeed, t, tcdIsInP0);
            #endif
        } else {
            #ifdef REMOTE_DBG_NET
            Serial.printf("Out-of-sequence packet received from TCD %d %d\n", seqCnt, bttfnTCDSeqCnt);
            #endif
        }
        bttfnTCDSeqCnt = seqCnt;
        break;
    case BTTFN_NOT_PREPARE:
        // Prepare for TT. Comes at some undefined point,
        // an undefined time before the actual tt, and
        // may not come at all.
        if(remoteAllowed) {
            doPrepareTT = true;
        }
        break;
    case BTTFN_NOT_TT:
        // Trigger Time Travel (if not running already)
        if(FPBUnitIsOn && remoteAllowed && !TTrunning && !remBusy) {
            networkTimeTravel = true;
            networkReentry = false;
            networkAbort = false;
            networkLead = buf[6] | (buf[7] << 8);
            networkP1   = buf[8] | (buf[9] << 8);
        }
        break;
    case BTTFN_NOT_REENTRY:
        // Start re-entry (if TT currently running or triggered)
        if(networkTimeTravel || TTrunning) {
            networkReentry = true;
        }
        break;
    case BTTFN_NOT_ABORT_TT:
        // Abort TT (if TT currently running or triggered)
        if(networkTimeTravel || TTrunning) {
            networkAbort = true;
        }
        break;
    case BTTFN_NOT_ALARM:
        networkAlarm = true;
        break;
    case BTTFN_NOT_REM_CMD:
        if(!remBusy) {
            addCmdQueue(GET32(buf, 6));
        }
        break;
    case BTTFN_NOT_WAKEUP:
        if(remoteAllowed) {
            doWakeup = true;
        }
        break;
    case BTTFN_NOT_INFO:
        {
            uint16_t tcdi1 = buf[6] | (buf[7] << 8);
            uint16_t tcdi2 = buf[8] | (buf[9] << 8);
            if(!(remoteAllowed = !(tcdi1 & BTTFN_TCDI1_NOREM))) {
                tcdIsInP0 = 0;
            }
            tcdIsBusy = !!(tcdi2 & BTTFN_TCDI2_BUSY);
        }
        break;
    case BTTFN_NOT_REM_SPD:     // TCD fw < 10/26/2024 (non-MC)
        seqCnt = GET32(buf, 12);
        if(seqCnt > bttfnTCDSeqCnt || seqCnt == 1) {
            tcdSpeedP0 = buf[6] | (buf[7] << 8);
            if(remoteAllowed && !remBusy) {
                tcdIsInP0  = buf[8] | (buf[9] << 8);
            } else {
                tcdIsInP0 = 0;
            }
            #ifdef REMOTE_DBG_NET
            Serial.printf("TCD sent REM_SPD: %d %d\n", tcdIsInP0, tcdSpeedP0);
            #endif
        } else {
            #ifdef REMOTE_DBG_NET
            Serial.printf("Out-of-sequence packet received from TCD %d %d\n", seqCnt, bttfnTCDSeqCnt);
            #endif
        }
        bttfnTCDSeqCnt = seqCnt;
        break;
    }
}

// Check for pending MC packet and parse it
static bool bttfn_checkmc()
{
    int psize = remMcUDP->parsePacket();

    if(!psize) {
        return false;
    }

    // This returns true as long as a packet was received
    // regardless whether it was for us or not. Point is
    // to clear the receive buffer.
    
    remMcUDP->read(BTTFMCBuf, BTTF_PACKET_SIZE);

    if(haveTCDIP) {
        if(bttfnTcdIP != remMcUDP->remoteIP())
            return true;
    } else {
        // Do not use tcdHostNameHash; let DISCOVER do its work
        // and wait for a result.
        return true;
    }

    if(!check_packet(BTTFMCBuf))
        return true;

    if((BTTFMCBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFMCBuf);
    
    }

    return true;
}

// Check for pending packet and parse it
static void BTTFNCheckPacket()
{
    unsigned long mymillis = millis();
    
    int psize = remUDP->parsePacket();
    if(!psize) {
        if(!bttfnDataNotEnabled && BTTFNPacketDue) {
            if((mymillis - BTTFNTSRQAge) > BTTFN_RESPONSE_TO) {
                // Packet timed out
                BTTFNPacketDue = false;
                // Immediately trigger new request for
                // the first 10 timeouts, after that
                // the new request is only triggered
                // in greater intervals via bttfn_loop().
                if(haveTCDIP && BTTFNfailCount < 10) {
                    BTTFNfailCount++;
                    BTTFNUpdateNow = 0;
                }
                bttfnCurrLatency = 0;
            }
        }
        return;
    }
    
    remUDP->read(BTTFUDPBuf, BTTF_PACKET_SIZE);

    if(!check_packet(BTTFUDPBuf))
        return;

    if((BTTFUDPBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFUDPBuf);
        
    } else {

        // (Possibly) a response packet
    
        if(GET32(BTTFUDPBuf, 6) != BTTFUDPID)
            return;
    
        // Response marker missing or wrong version, bail
        if((BTTFUDPBuf[4] & 0x8f) != (BTTFN_VERSION | 0x80))
            return;

        bttfnCurrLatency = (mymillis - bttfnPacketSentNow) / 2;

        BTTFNfailCount = 0;
    
        // If it's our expected packet, no other is due for now
        BTTFNPacketDue = false;

        if(BTTFUDPBuf[5] & 0x80) {
            if(!haveTCDIP) {
                bttfnTcdIP = remUDP->remoteIP();
                haveTCDIP = true;
                #ifdef REMOTE_DBG_NET
                Serial.printf("Discovered TCD IP %d.%d.%d.%d\n", bttfnTcdIP[0], bttfnTcdIP[1], bttfnTcdIP[2], bttfnTcdIP[3]);
                #endif
            } else {
                #ifdef REMOTE_DBG_NET
                Serial.println("Internal error - received unexpected DISCOVER response");
                #endif
            }
        }

        lastBTTFNpacket = mymillis;

        bttfn_eval_response(BTTFUDPBuf, true);
    }
}

static void BTTFNPreparePacketTemplate()
{
    memset(BTTFUDPTBuf, 0, BTTF_PACKET_SIZE);

    // ID
    memcpy(BTTFUDPTBuf, BTTFUDPHD, 4);

    // Tell the TCD about our hostname
    // 13 bytes total. If hostname is longer, last in buf is '.'
    memcpy(BTTFUDPTBuf + 10, settings.hostName, 13);
    if(strlen(settings.hostName) > 13) BTTFUDPTBuf[10+12] = '.';

    BTTFUDPTBuf[10+13] = BTTFN_TYPE_REMOTE;

    // Version, MC-marker, ND-marker
    BTTFUDPTBuf[4] = BTTFN_VERSION | BTTFN_SUP_MC | BTTFN_SUP_ND;
    
    // Remote-ID
    SET32(BTTFUDPTBuf, 35, myRemID);                 
}

static void BTTFNPreparePacket()
{
    memcpy(BTTFUDPBuf, BTTFUDPTBuf, BTTF_PACKET_SIZE);          
}

static void BTTFNDispatch()
{
    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    BTTFUDPBuf[BTTF_PACKET_SIZE - 1] = a;

    if(haveTCDIP) {
        remUDP->beginPacket(bttfnTcdIP, BTTF_DEFAULT_LOCAL_PORT);
    } else {
        remUDP->beginPacket(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 1);
    }
    remUDP->write(BTTFUDPBuf, BTTF_PACKET_SIZE);
    remUDP->endPacket();
}

// Send a new data request
static bool BTTFNSendRequest()
{
    BTTFNPacketDue = false;

    BTTFNUpdateNow = millis();

    if(WiFi.status() != WL_CONNECTED) {
        BTTFNWiFiUp = false;
        return false;
    }

    BTTFNWiFiUp = true;

    // Send new packet
    BTTFNPreparePacket();
    
    // Serial
    BTTFUDPID = (uint32_t)millis();
    SET32(BTTFUDPBuf, 6, BTTFUDPID);

    // Request flags
    BTTFUDPBuf[5] = bttfnReqStatus;        

    if(!haveTCDIP) {
        BTTFUDPBuf[5] |= 0x80;
        SET32(BTTFUDPBuf, 31, tcdHostNameHash);
    }

    BTTFNDispatch();

    BTTFNTSRQAge = bttfnPacketSentNow = millis();
    
    BTTFNPacketDue = true;
    
    return true;
}

static bool bttfn_connected()
{
    if(!useBTTFN)
        return false;

    if(!haveTCDIP)
        return false;

    if(WiFi.status() != WL_CONNECTED)
        return false;

    if(!lastBTTFNpacket)
        return false;

    return true;
}

static bool bttfn_trigger_tt(bool probe)
{
    // BBTFN-wide TT can be triggered even
    // if remoteAllowed is false; Remote will,
    // however, not take part in the sequence
    // in that case.
    
    if(!bttfn_connected())
        return false;

    // "Probe" to decide between stand-alone
    // or BTTFN-wide TT. Since the flags below
    // are not 100% reliable (due to communication
    // delays), and a stand-alone TT while
    // seemingly connected to a TCD is confusing,
    // we stop the probe here, and live with
    // a TT simply not starting if the TCD refuses.
    // Still less confusing than a Remote doing
    // a TT sequence while the TCD is not.
    if(probe)
        return true;

    if(TTrunning || tcdIsBusy)
        return false;

    BTTFNPreparePacket();

    // Trigger BTTFN-wide TT
    BTTFUDPBuf[5] = 0x80;

    BTTFNDispatch();

    return true;
}

static bool bttfn_send_command(uint8_t cmd, uint8_t p1, uint8_t p2)
{
    if(!remoteAllowed && (cmd <= BTTFN_REM_MAX_COMMAND))
        return false;
    
    if(!bttfn_connected())
        return false;

    BTTFNPreparePacket();
    
    //BTTFUDPBuf[5] = 0x00;   // already 0

    if(cmd <= BTTFN_REM_MAX_COMMAND) {
        SET32(BTTFUDPBuf, 6, bttfnSeqCnt[cmd]);
        bttfnSeqCnt[cmd]++;
        if(!bttfnSeqCnt[cmd]) bttfnSeqCnt[cmd]++;
    }

    BTTFUDPBuf[25] = cmd;
    BTTFUDPBuf[26] = p1;
    BTTFUDPBuf[27] = p2;

    BTTFNDispatch();

    lastCommandSent = millis();

    return true;
}

// Remote does not need to send KEEP_ALIVE, it
// sends combined updates on a regular basis.
/*
static void bttfn_remote_keepalive()
{
    if(!bttfn_send_command(BTTFN_REMCMD_PING, 0, 0)) {
        triggerCompleteUpdate = true;
    }
}
*/

void bttfn_remote_unregister()
{
    bttfn_send_command(BTTFN_REMCMD_BYE, 0, 0);
    // No need for "triggerCompleteUpdate" logic,
    // BYE means we reboot.
}

static void bttfn_remote_send_combined(bool powerstate, bool brakestate, uint8_t speed)
{
    if(!triggerCompleteUpdate) {
        uint8_t p1 = 0;
        if(powerstate)     p1 |= 0x01;
        if(brakestate)     p1 |= 0x02;
        if(powerMaster)    p1 |= 0x08;  // 4 tainted by buggy TCD 3.7
        if(displayGPSMode) p1 |= 0x10;
        if(!bttfn_send_command(BTTFN_REMCMD_COMBINED, p1, speed)) {
            triggerCompleteUpdate = true;
        }
    }
}

static void bttfn_setup()
{
    useBTTFN = false;

    // string empty? Disable BTTFN.
    if(!settings.tcdIP[0])
        return;

    haveTCDIP = isIp(settings.tcdIP);
    
    if(!haveTCDIP) {
        tcdHostNameHash = 0;
        unsigned char *s = (unsigned char *)settings.tcdIP;
        for ( ; *s; ++s) tcdHostNameHash = 37 * tcdHostNameHash + tolower(*s);
    } else {
        bttfnTcdIP.fromString(settings.tcdIP);
    }
    
    remUDP = &bttfUDP;
    remUDP->begin(BTTF_DEFAULT_LOCAL_PORT);

    remMcUDP = &bttfMcUDP;
    remMcUDP->beginMulticast(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 2);

    BTTFNPreparePacketTemplate();
    
    BTTFNfailCount = 0;
    useBTTFN = true;
}

void bttfn_loop()
{
    if(!useBTTFN)
        return;

    int t = 100;

    while(bttfn_checkmc() && t--) {}

    unsigned long now = millis();
        
    BTTFNCheckPacket();
    
    if(bttfnDataNotEnabled) {
        // Remote does not need to send KEEP_ALIVE, it
        // sends combined updates on a regular basis.
        if(now - bttfnLastNotData > BTTFN_DATA_TO) {
            // Return to polling if no NOT_DATA for too long
            bttfnDataNotEnabled = false;
            bttfnTCDDataSeqCnt = 1;
            // Re-do DISCOVER, TCD might have got new IP address
            if(tcdHostNameHash) {
                haveTCDIP = false;
            }
            // Avoid immediate return to stand-alone in main_loop()
            lastBTTFNpacket = now;
            #ifdef REMOTE_DBG_NET
            Serial.println("NOT_DATA timeout, returning to polling");
            #endif
        }
    } else if(!BTTFNPacketDue) {
        // If WiFi status changed, trigger immediately
        if(!BTTFNWiFiUp && (WiFi.status() == WL_CONNECTED)) {
            BTTFNUpdateNow = 0;
        }
        if((!BTTFNUpdateNow) || (millis() - BTTFNUpdateNow > bttfnRemPollInt)) {
            BTTFNSendRequest();
        }
    }
}

static void bttfn_loop_quick()
{
    if(!useBTTFN)
        return;
    
    int t = 100;

    while(bttfn_checkmc() && t--) {}
}
