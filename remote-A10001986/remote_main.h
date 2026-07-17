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
 * License: Modified MIT NON-AI
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
 * Links inside the Software pointing to the original source must not 
 * be changed or removed.
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

#ifndef _REMOTE_MAIN_H
#define _REMOTE_MAIN_H

#include "display.h"
#include "input.h"
#ifdef HAVE_PM
#include "power.h"
#endif

void main_boot();
void main_boot2();
void main_setup();
void main_loop();

void flushDelayedSave();
void increaseVolume();
void decreaseVolume();

void disectOldVisMode();
void updateVisMode();

void setAutoThrottle(bool isOn);
void setCoast(bool isOn);
void setMovieMode(bool isOn);
void setDisplayGPS(bool isOn);

void showWaitSequence();
void endWaitSequence();
void showCopyError();
void showNumber(int num);

void allOff();
void prepareReboot();

bool switchMusicFolder(uint8_t nmf, bool isSetup = false);
void waitAudioDone(bool withBTTFN = false);

void mydelay(unsigned long mydel, bool withBTTFN = false);
unsigned long millisNonZero();

void addCmdQueue(uint32_t command);
void bttfn_loop();
void bttfn_remote_unregister();

extern unsigned long powerupMillis;

extern bool haveNewBoard;

#ifdef HAVE_CRSF
extern bool opModeCRSF;
#endif

extern uint32_t csf;
#define CSF_OFF           0x00000001
#define CSF_TCDINP0       0x00000002
#define CSF_TCDINP0O      0x00000004
#define CSF_INTP0         0x00000008
#define CSF_TT            0x00000010
#define CSF_CALIBMD       0x00000020
#define CSF_KEEPCOUNTING  0x00000040

#define CSF_TCDINP0T      0x00001000
#define CSF_TUFIRST    0x00002000

#define CSF_TTP0          0x01000000
#define CSF_TTP1          0x02000000
#define CSF_TTP2          0x04000000
#define CSF_BLOCKSCAN     0x40000000
#define CSF_BUSY          0x80000000

extern uint32_t myRemID;
extern bool     remoteAllowed;

extern REMRotEnc rotEnc;

extern remDisplay remdisplay;
extern remLED remledPwr;
extern remLED remledStop;

extern bool showUpdAvail;

extern bool useRotEnc;

extern bool havePwrMon;

extern uint16_t visMode;
extern bool autoThrottle;
extern bool doCoast;
extern bool movieMode;
extern bool displayGPSMode;
extern bool powerMaster;

extern bool bttfnTT;

extern bool networkTimeTravel;
extern bool networkReentry;
extern bool networkAbort;
extern bool networkAlarm;
extern uint16_t networkLead;
extern uint16_t networkP1;

extern bool doPrepareTT;
extern bool doWakeup;

extern int32_t  throttlePos;

extern int     bttfnHaveTCDSSID;
extern char    TCDSSID[];
extern uint8_t TCDpwMarker;

#endif
