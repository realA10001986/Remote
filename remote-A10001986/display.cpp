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

#include "remote_global.h"

#include <Arduino.h>
#include <math.h>
#include "display.h"
#include <Wire.h>

/* remLED class */

// Store i2c address
remLED::remLED()
{
}

void remLED::begin(uint8_t pin, bool doUse)
{
    _lpin = pin;
    _doUse = doUse;
    
    pinMode(_lpin, OUTPUT);
    digitalWrite(_lpin, LOW);
    _state = false;
}

void remLED::setState(bool state)
{   
    _state = state;
    if(_doUse) {
        digitalWrite(_lpin, state ? HIGH : LOW);
    }
}

bool remLED::getState()
{
    return _state;
}


/* remDisplay class */

// The segments' wiring to buffer bits
// This reflects the actual hardware wiring

// generic
#define S7G_T   0b00000001    // A  top
#define S7G_TR  0b00000010    // B  top right
#define S7G_BR  0b00000100    // C  bottom right
#define S7G_B   0b00001000    // D  bottom
#define S7G_BL  0b00010000    // E  bottom left
#define S7G_TL  0b00100000    // F  top left
#define S7G_M   0b01000000    // G  middle
#define S7G_DOT 0b10000000    // DP dot

static const uint16_t font7segGeneric[48] = {
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR,
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_M,
    S7G_TR|S7G_BR|S7G_TL|S7G_M,
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_M,
    S7G_T|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL,
    S7G_T|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_BL,
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_B|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,
    S7G_DOT,
    S7G_M,        // 37 minus
    S7G_T,        // 38 coded as  1
    S7G_TR,       // 39           2
    S7G_BR,       // 40           3
    S7G_B,        // 41           4
    S7G_BL,       // 42           5
    S7G_TL,       // 43           6
    S7G_T|S7G_B,  // 44           7
    S7G_T|S7G_BR|S7G_B|S7G_BL|S7G_M,  // 45 8
    S7G_BR|S7G_B|S7G_BL|S7G_M,        // 46 9
    S7G_T|S7G_B|S7G_M                 // 47 % = top middle bottom
};

static const struct dispConf {
    uint8_t  num_digs;       //   total number of digits/letters
    uint8_t  max_bufPos;     //   highest buffer position to update
    uint8_t  bufPosArr[4];   //   The buffer positions of each of the digits from left to right
    uint8_t  bufShftArr[4];  //   Shift-value for each digit from left to right
    const uint16_t *fontSeg; //   Pointer to font
} displays[REM_DISP_NUM_TYPES] = {  
    { 4, 2, { 0, 0, 1, 2 }, { 0, 8, 0, 0 }, font7segGeneric }
};

// Store i2c address
remDisplay::remDisplay(uint8_t address)
{
    _address = address;
}

// Start the display
bool remDisplay::begin()
{
    bool ret = true;

    // Check for display on i2c bus
    Wire.beginTransmission(_address);
    if(Wire.endTransmission(true)) {

        ret = false;

    } else {

        _dispType = 0;
        _num_digs = displays[_dispType].num_digs;
        _buf_max = displays[_dispType].max_bufPos;
        _bufPosArr = displays[_dispType].bufPosArr;
        _bufShftArr = displays[_dispType].bufShftArr;

        _fontXSeg = displays[_dispType].fontSeg;

        _speed_pos10 = *(_bufPosArr + 0);
        _speed_pos01 = *(_bufPosArr + 1);
        _speed_pos_1 = *(_bufPosArr + 2);
        _dig10_shift = *(_bufShftArr + 0);
        _dig01_shift = *(_bufShftArr + 1);
        _dig_1_shift = *(_bufShftArr + 2);
        _dot_pos01   = *(_bufPosArr + 1);
        _dot01_shift = _dig01_shift = *(_bufShftArr + 1);
    }

    directCmd(0x20 | 1); // turn on oscillator

    clearBuf();          // clear buffer
    setBrightness(15);   // setup initial brightness
    clearDisplay();      // clear display RAM
    on();                // turn it on

    return ret;
}

// Turn on the display
void remDisplay::on()
{
    if(_onCache == (1 | _blink)) return;
    directCmd(0x80 | 1 | _blink);
    _onCache = 1 | _blink;
    _briCache = 0xfe;
}

// Turn off the display
void remDisplay::off()
{   
    if(!_onCache) return;
    directCmd(0x80);
    _onCache = 0;
}

void remDisplay::blink(bool bl)
{
    _blink = bl ? 0x02: 0;
    if(_onCache) on();
}

// Clear the buffer
void remDisplay::clearBuf()
{
    // must call show() to actually clear display

    for(int i = 0; i < 8; i++) {
        _displayBuffer[i] = 0;
    }
}

// Set display brightness
// Valid brighness levels are 0 to 15. Default is 15.
// 255 sets it to previous level
uint8_t remDisplay::setBrightness(uint8_t level, bool isInitial)
{
    if(level == 255)
        level = _brightness;    // restore to old val

    _brightness = setBrightnessDirect(level);

    if(isInitial)
        _origBrightness = _brightness;

    return _brightness;
}

uint8_t remDisplay::setBrightnessDirect(uint8_t level)
{
    if(level > 15)
        level = 15;

    if(level != _briCache) {
        directCmd(0xE0 | level);  // Dimming command
        _briCache = level;
    }

    return level;
}

uint8_t remDisplay::getBrightness()
{
    return _brightness;
}


// Show data in display --------------------------------------------------------


// Show the buffer
void remDisplay::show()
{
    int i;

    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(0x00);  // start address
    
        for(i = 0; i <= _buf_max; i++) {
            Wire.write(_displayBuffer[i] & 0xFF);
            Wire.write(_displayBuffer[i] >> 8);
        }
    
        Wire.endTransmission();
    }
}


// Set data in buffer --------------------------------------------------------


// Write given text to buffer
void remDisplay::setText(const char *text)
{
    int idx = 0, pos = 0;
    int temp = 0;

    clearBuf();

    if(_dispType >= 0) {
        while(text[idx] && (pos < _num_digs)) {
            temp = getLEDChar(text[idx]) << (*(_bufShftArr + pos));
            idx++;
            if(text[idx] == '.') {
                temp |= (getLEDChar('.') << (*(_bufShftArr + pos)));
                idx++;
            }
            _displayBuffer[*(_bufPosArr + pos)] |= temp;
            pos++;
        }
    }
}

void remDisplay::setSpeed(int speed)    // times 10
{
    uint16_t b1, b2, b3;
    int temp;
    
    _speed = speed;

    if(_dispType >= 0) {

        clearBuf();
    
        if(speed < 0) {
            b1 = b2 = b3 = *(_fontXSeg + 37);
            _spdpd = 0;
        } else if(speed > 990) {
            b1 = *(_fontXSeg + ('H' - 'A' + 10));
            b2 = *(_fontXSeg + ('I' - 'A' + 10));
            b3 = _spdpd = 0;
        } else {
            temp = speed / 100;
            b1 = temp ? *(_fontXSeg + temp) : 0;
            speed -= (temp * 100);
            b2 = *(_fontXSeg + (speed / 10));
            _spdpd = speed % 10;
            b3 =  *(_fontXSeg + _spdpd);
        }
    
        _displayBuffer[_speed_pos10] |= (b1 << _dig10_shift);
        _displayBuffer[_speed_pos01] |= (b2 << _dig01_shift);
        _displayBuffer[_speed_pos_1] |= (b3 << _dig_1_shift);
        
        _displayBuffer[_dot_pos01] |= (*(_fontXSeg + 36) << _dot01_shift);

    }
}

// Query data ------------------------------------------------------------------

int remDisplay::getSpeed()    // times 10
{
    return (int)_speed;
}

int remDisplay::getSpeedPostDot()
{
    return (int)_spdpd;
}

// Private functions ###########################################################

// Returns bit pattern for provided character
uint16_t remDisplay::getLEDChar(uint8_t value)
{
    if(value >= '0' && value <= '9') {
        return *(_fontXSeg + (value - '0'));
    } else if(value >= 'A' && value <= 'Z') {
        return *(_fontXSeg + (value - 'A' + 10));
    } else if(value >= 'a' && value <= 'z') {
        return *(_fontXSeg + (value - 'a' + 10));
    } else if(value == '.') {
        return *(_fontXSeg + 36);
    } else if(value == '-') {
        return *(_fontXSeg + 37);
    } else if(value == '&') {       // "percent" sign
        return *(_fontXSeg + 47); 
    } else if(value >= 1 && value <= 9) {
        return *(_fontXSeg + 38 + (value - 1));
    } 

    return 0;
}

// Directly clear the display
void remDisplay::clearDisplay()
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(0x00);  // start address
    
        for(int i = 0; i <= _buf_max; i++) {
            Wire.write(0x00);
            Wire.write(0x00);
        }
    
        Wire.endTransmission();
    }
}

void remDisplay::directCmd(uint8_t val)
{
    if(_dispType >= 0) {
        Wire.beginTransmission(_address);
        Wire.write(val);
        Wire.endTransmission();
    }
}
