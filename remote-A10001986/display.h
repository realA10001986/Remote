/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * remLED Class:     LEDs
 * remDisplay Class: 3-digit 7-segment Display (HT16K33; addr 0x70)
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

#ifndef _remoteDisplay_H
#define _remoteDisplay_H

/* remLED Class */

class remLED {

    public:
        remLED();
        void begin(uint8_t pin, bool doUse = true);

        void setState(bool state);
        bool getState();

    private:
        uint8_t _lpin;
        bool    _doUse = false;
        bool    _state = false;
};

/* remDisplay Class */

// The supported display types: (Selection disabled by default)
// The display is a 3-digit 7-segment display
// For 4-digit displays, there are two entries in this list, one to display
// the number left-aligned, one for right-aligned.
// The idea is to remove the 4-tube display from the pcb and connect a 3-tube 
// one. Unfortunately, the pin assignments usually do not match, which makes 
// some re-wiring necessary.
//
// The display's i2c slave address is 0x70.

#define REM_DISP_NUM_TYPES 1
//
enum dispTypes : uint8_t {
    SP_CS,             // Native
};

class remDisplay {

    public:

        remDisplay(uint8_t address);
        bool begin();
        void on();
        void off();
        void blink(bool bl);

        void clearBuf();

        uint8_t setBrightness(uint8_t level, bool isInitial = false);
        uint8_t setBrightnessDirect(uint8_t level) ;
        uint8_t getBrightness();

        void show();

        void setText(const char *text);
        
        void setSpeed(int speed);
        int  getSpeed();
        int  getSpeedPostDot();

    private:

        uint16_t getLEDChar(uint8_t value);

        void clearDisplay();                    // clears display RAM
        void directCmd(uint8_t val);

        uint8_t _address;
        uint16_t _displayBuffer[8];

        int8_t _onCache = -1;                   // Cache for on/off
        uint8_t _briCache = 0xfe;               // Cache for brightness
        uint8_t _blink = 0;

        int      _speed = 0;
        uint16_t _spdpd = 0;

        uint8_t _brightness = 15;
        uint8_t _origBrightness = 15;

        int8_t   _dispType = -1;
        unsigned int _buf_max = 0;  //      number of buffer positions used
        unsigned int _num_digs;     //      total number of digits/letters (max 4)
        const uint8_t *_bufPosArr;  //      Array of buffer positions for digits left->right
        const uint8_t *_bufShftArr; //      Array of shift values for each digit

        int  _speed_pos10;          //      Speed's 10s position in 16bit buffer
        int  _speed_pos01;          //      Speed's 1s position in 16bit buffer
        int  _speed_pos_1;          //      Speed's .1s position in 16bit buffer
        int  _dig10_shift;          //      Shift 10s to align in buffer
        int  _dig01_shift;          //      Shift 1s to align in buffer
        int  _dig_1_shift;          //      Shift .1s to align in buffer
        int  _dot_pos01;            //      1s dot position in 16bit buffer
        int  _dot01_shift;          //      1s dot shift to align in buffer

        const uint16_t *_fontXSeg;
};

#endif
