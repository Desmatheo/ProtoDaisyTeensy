#pragma once

#include "../include/Utils.h"

#if USE_DAISY
#include "daisy_seed.h"
#include "daisysp.h"

#include <q/fx/biquad.hpp>
#include "../include/Effect.h"
namespace q = cycfi::q;
using namespace daisy;
using namespace daisysp;
#else
#include <Arduino.h>
#include "AudioStream.h"
#endif

#include "Util/Multirate.h"
#include "Util/OctaveGenerator.h"

#if USE_DAISY
class EarthEffect : public Effect {
#else
class EarthEffect : public AudioStream {
#endif

    public:

    float dryMix;
    float wetMix;
    float volume = 1;


    Decimator2 decimate2;
    Interpolator interpolate;
    OctaveGenerator octave;
#if USE_DAISY
    q::highshelf eq1;
    q::lowshelf eq2;
    Overdrive overdrive;
#endif
    float buff[6];
    float buff_out[6];
    int bin_counter = 0;

    float current_ODswell;

    int effect_mode = 0;

    bool odOn = false;
    bool bypass = false;

#if USE_DAISY
    EarthEffect(float sampleRate); 
#else
    EarthEffect(); 
    virtual void update() override;
    void midiUpdate();
#endif


#if USE_DAISY
    void update(const float** in, float** out, int idx) override;
    float updateTest(const float in, float out, int idx) override;
#endif

    void setMix(float mix);                // Ctrl 2 (0.0 -> 1.0)
    void setVolume(float vol);             // Ctrl 1 (0.0 -> 1.0
    
    void setOctaveMode(int mode);          // 3-Way Switch 2 (0, 1, 2)

#if USE_DAISY
    void setParameter(int param_id, float value) override;
#else
    void setParameter(int param_id, float value);
#endif

    // --- METHODES ET VARIABLES PARTAGEES ---
    void setEnabled(bool e) { active = e; }
    bool isEnabled() const  { return active; }

private:
    bool active = false; // effet actif ou non
    
#if !USE_DAISY
    // --- SPECIFIQUE TEENSY ---
    audio_block_t* inputQueueArray_[1];
#endif
};
