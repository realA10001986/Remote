/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * RotEnc Class, RemButton Class: I2C-RotEnc and Button handling
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
 * -------------------------------------------------------------------
 */

#ifndef _REMOTEINPUT_H
#define _REMOTEINPUT_H

#include <Wire.h>

/*
 * REMRotEnc class
 */

#define REM_RE_TYPE_ADA4991    0     // Adafruit 4991            https://www.adafruit.com/product/4991
#define REM_RE_TYPE_DUPPAV2    1     // DuPPA I2CEncoder V2.1    https://www.duppa.net/shop/i2cencoder-v2-1/
#define REM_RE_TYPE_DFRGR360   2     // DFRobot Gravity 360      https://www.dfrobot.com/product-2575.html
#define REM_RE_TYPE_CS         3     // CircuitSetup             <yet to be designed>
#define REM_RE_TYPE_ADS1X15    4     // ADS1015                  ADC (not really a rotary encoder)

class REMRotEnc {
  
    public:
        REMRotEnc(int numTypes, const uint8_t *addrArr);
        bool    begin(bool forSpeed = true, bool newBoard = false);
        
        void    zeroPos(bool calibMode = false);
        
        bool    setMaxStepsUp(int32_t num = 0);
        bool    setMaxStepsDown(int32_t num = 0);

        bool    dynZeroPos();
        void    setZeroPos(int32_t num);

        int32_t getMaxStepsUp();
        int32_t getMaxStepsDown();
        int32_t getZeroPos();
        
        int32_t updateThrottlePos(bool force = false);

        int     updateVolume(int curVol, bool force = false);

    private:
        int32_t getEncPos();
        int     read(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num);
        void    write(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num);

        bool    zeroEnc();

        int           _numTypes = 0;
        const uint8_t *_addrArr;
        int8_t        _st = -1;
        int8_t        _type = 0;        // 0=throttle; 1=vol
        
        int           _i2caddr;

        union {
            int32_t   throttlePos = 0;
            int32_t   rotEncPos;
        };
        int32_t       rotEncZeroPos = 0;
        int32_t       throttlePositionsUp = 5;
        int32_t       throttlePositionsDown = -5;
        unsigned long lastUpd = 0;

        bool          _dynZeroPos = false;

        int           dfrgain;
        int           dfroffslots;

        bool          scaleThrottlePos = false;
};


/*
 * RemButton class
 */
 
typedef enum {
    REMBUS_IDLE,
    REMBUS_PRESSED,
    REMBUS_HOLD,
    REMBUS_RELEASED,
    REMBUS_HOLDEND,
    REMBUS_EHOLD,
    REMBUS_EHOLDEND
} ButState;

struct ButStruct {
    ButState      bState;
    unsigned long startTime;
};

class RemButton {
  
    public:
        RemButton();

        void begin(const int pin, const bool activeLow = true, const bool pullupActive = true, const bool pulldownActive = false);
      
        void setTiming(const int debounceDur, const int lPressDur, const int elPressDur = 0);
      
        void attachPressDown(void (*newFunction)());            // pressed down
        void attachPressEnd(void (*newFunction)());             // released after "press" (=short)
        void attachLongPressStart(void (*newFunction)());       // "Hold" started (pressed > holdTime)
        void attachLongPressStop(void (*newFunction)());        // released after "hold" (=long)
        void attachELongPressStart(void (*newFunction)(void));
        void attachELongPressStop(void (*newFunction)(void));

        void scan();
        void reset(void);

    private:

        void transitionTo(ButState nextState);

        void (*_pressDownFunc)() = NULL;
        void (*_pressEndFunc)() = NULL;
        void (*_longPressStartFunc)() = NULL;
        void (*_longPressStopFunc)() = NULL;
        void (*_elongPressStartFunc)(void) = NULL;
        void (*_elongPressStopFunc)(void) = NULL;

        int _pin;
        
        unsigned int _debounceDur = 50;
        unsigned int _longPressDur = 800;
        unsigned int _elongPressDur = 5000;
      
        int _buttonPressed;
      
        ButState _state     = REMBUS_IDLE;
        ButState _lastState = REMBUS_IDLE;
      
        unsigned long _startTime = 0;
        bool    _wasPressed = false;
};

/*
 * ButtonPack class
 */
 
#define REM_BP_TYPE_PCA8574 0
#define REM_BP_TYPE_PCA9554 1

#define PACK_SIZE 8

/*
 * RemButton class
 */

class ButtonPack {
  
    public:
        ButtonPack(int numTypes, const uint8_t *addrArr);

        bool begin();

        void setScanInterval(const unsigned long scanInterval);
        void setTiming(int idx, const int debounceTs, const int lPressTs);
      
        void attachPressDown(void (*newFunction)(int));       // pressed down
        void attachPressEnd(void (*newFunction)(int));        // released after "press" (=short)
        void attachLongPressStart(void (*newFunction)(int));  // "Hold" started (pressed > holdTime)
        void attachLongPressStop(void (*newFunction)(int));   // released after "hold" (=long)

        int  getPackSize();
        void scan();
        #ifdef HAVE_CRSF
        bool    sampleStates(uint8_t &states);
        uint8_t readStates();
        #endif

    private:

        void reset(int);
        void transitionTo(int, ButState nextState);

        void port_write(uint8_t reg, uint8_t val);
        int  port_read(uint8_t *buf);

        void (*_pressDownFunc)(int) = NULL;
        void (*_pressEndFunc)(int) = NULL;
        void (*_longPressStartFunc)(int) = NULL;
        void (*_longPressStopFunc)(int) = NULL;

        int _pack_size = PACK_SIZE;

        int           _numTypes = 0;
        const uint8_t *_addrArr;
        int8_t        _st = -1;
        
        int           _i2caddr;
        
        unsigned int _debounceDur[PACK_SIZE];
        unsigned int _longPressDur[PACK_SIZE];

        unsigned long _scanInterval = 20;
        unsigned long _lastScan = 0;
      
        int _buttonPressed;
      
        ButState _state[PACK_SIZE];
        ButState _lastState[PACK_SIZE];
      
        unsigned long _startTime[PACK_SIZE] = { 0 };
        bool          _wasPressed[PACK_SIZE] = { false };
};
#endif
