#pragma once 
#include "Utils.h"

#if USE_DAISY
#include "daisy_pod.h"
#include "daisy_seed.h"
#include "Effect.h"
#endif
#include "../EffetEarth/Earth.h"
#include "../EffetDelay/Delay.h"
#include "../EffetDisto/AudioEffectDrive.h"

#if USE_DAISY

EarthEffect* earth_effects[6] = {nullptr};
DelayEffect* delay_effects[6] = {nullptr};
AudioEffectDrive* drive_effects[6] = {nullptr};

enum class EffectType {
    Mute,
    Bypass,
    Earth,
    Delay,
    Drive
};

class StringUtil{
public : 
    EffectType type;
    int index;
    Effect* active_effect;

    StringUtil(EffectType type, int index){
        this->type = type;
        this->index = index;
        this->active_effect = nullptr;
    }

    EffectType GetType() {
        return type;
    }
};

int volatile idxString = 0;

class paramUtil{
public : 
    int current_param;
    int max_params;
    bool changing = false;

    paramUtil(int max = 1){
        current_param = 0;
        max_params = max;
    }

    void setMaxParams(int max) {
        max_params = max;
        if (current_param >= max_params) current_param = 0;
    }

    // Change le paramètre sélectionné avec l'encodeur
    void changeParam(int increment) {
        if (increment == 0 || max_params <= 0) return;
        
        current_param = (current_param + increment) % max_params;
        if (current_param < 0) {
            current_param += max_params; 
        }
    }

    int GetParam() {
        return current_param;
    }
};

#endif // USE_DAISY