#pragma once

#ifndef _POWERVM_H_
#define _POWERVM_H_
#endif

#include <Arduino.h>
#include "DateTime.h"

typedef struct
{
    DateTime lastChange;
    float actValue;
    float lastValue;
}SampleValueSet;

class PowerVM{
    public:
    // constructor
    PowerVM();
};

