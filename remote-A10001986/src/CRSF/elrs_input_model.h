#ifndef _ELRS_INPUT_MODEL_H
#define _ELRS_INPUT_MODEL_H

#include <stdint.h>

#include "elrs_crsf_shared.h"

enum ELRSGimbalInput : uint8_t {
    ELRS_GIMBAL_INPUT_AILERON = 0,
    ELRS_GIMBAL_INPUT_ELEVATOR = 1,
    ELRS_GIMBAL_INPUT_RUDDER = 2,
    ELRS_GIMBAL_INPUT_THROTTLE = 3
};

struct ELRSInputAxisProfile {
    int16_t minimum;
    int16_t center;
    int16_t maximum;
    uint16_t reverse;
    uint16_t deadband;
    uint8_t expo; // Reserved for later curve shaping; Task 1 primitives do not apply expo yet.
};

struct ELRSGimbalRouting {
    uint8_t aileronChannel;
    uint8_t elevatorChannel;
    uint8_t throttleChannel;
    uint8_t rudderChannel;
};

int16_t elrsInputModelAxisToUs(const ELRSInputAxisProfile &profile, int16_t raw);
uint16_t elrsInputUsToCrsfTicks(int16_t us);
ELRSInputAxisProfile elrsDefaultInputAxisProfile();
ELRSGimbalRouting elrsDefaultGimbalRouting();
bool elrsIsValidInputAxisProfile(const ELRSInputAxisProfile &profile);
ELRSInputAxisProfile elrsSanitizeInputAxisProfile(const ELRSInputAxisProfile &profile);
bool elrsIsValidGimbalRouting(const ELRSGimbalRouting &routing);
ELRSGimbalRouting elrsSanitizeGimbalRouting(const ELRSGimbalRouting &routing);

#endif
