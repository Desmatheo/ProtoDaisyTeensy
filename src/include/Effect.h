#pragma once

#include "daisy_seed.h"

#define CPU_LoadEffect 1
#define CPU_LoadAll 1

class Effect {
public:

#if CPU_LoadEffect
    uint32_t profiled_ticks = 0;
    uint32_t last_profiled_ticks = 0;
#endif


    virtual ~Effect() = default;
    virtual void update(const float** in, float** out, int idx, int ch) = 0;
    virtual float updateTest(const float in, float out, int idx, int ch) = 0;
    virtual void setParameter(int param_id, float value) = 0;
};