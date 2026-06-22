#pragma once

#include "../include/Utils.h"

#if USE_DAISY
#include "daisy_seed.h"
#include "daisysp.h"
#include "delayline_oct.h"
#include "../../DaisySP/DaisySP-LGPL/Source/Filters/tone.h"
#include "../include/Effect.h"

using namespace daisy;
using namespace daisysp;

#else
#include <Arduino.h>
#include "AudioStream.h"
#include "arm_math.h"
#endif


#if USE_DAISY
class DelayEffect : public Effect {
#else 
class DelayEffect : public AudioStream{
#endif
public:

#if USE_DAISY
    static constexpr size_t MAX_DELAY = static_cast<size_t>(48000 * 4.0f); // 4 secondes de delay max
    // Lignes de delay
    DelayLineOct<float, MAX_DELAY> delayLineOctLeft;
    DelayLineOct<float, MAX_DELAY> delayLineOctRight;
#else
    // Sur Teensy, avec 44100Hz
    static constexpr size_t MAX_DELAY = static_cast<size_t>(44100 * 4.0f);
#endif

    // Paramètres internes
    float dryMix = 0.5f;
    float wetMix = 0.5f;
    float volume = 1;
    float vdelayTime = 0.5f;
    float vdelayFDBK = 0.7f;


    // Structure interne pour gérer proprement un canal de delay (gauche/droite)
    struct DelayChannel {
#if USE_DAISY
        DelayLineOct<float, MAX_DELAY>* del;
        Tone tone;
#else
        float* buffer = nullptr;
        uint32_t buf_len = 0;
        uint32_t write_idx = 0;
        bool use_extmem = false;
        
        float tone_z1 = 0.0f;
        float tone_a0 = 1.0f;
        float tone_b1 = 0.0f;
#endif
        float muteFade = 1.0f;
        uint32_t standbyTimer = 0;
        float lastTarget = 0.0f;
        float currentDelay = 0.0f;
        float delayTarget = 0.0f;
        float feedback = 0.0f;
        bool active = false;

#if USE_DAISY
        void Init(DelayLineOct<float, MAX_DELAY>* delayLine, float sampleRate);
#else
        void Init(float sampleRate, uint32_t max_delay_samples);
        void Free();
#endif
        float Process(float in);
    };

    DelayChannel delayL;
    DelayChannel delayR;  

#if USE_DAISY
    DelayEffect(float sampleRate);
#else
    DelayEffect();
    ~DelayEffect();
    bool begin();
#endif

#if USE_DAISY
    void update(const float** in, float** out, int idx, int ch) override;
    float updateTest(const float in, float out, int idx, int ch ) override;
#else 
    virtual void update() override;

    // ON/OFF soft
    void setEnabled(bool e) { active = e; }
    bool isEnabled() const { return active; }
#endif

    // Setters pour personnalisation
    void setMix(float mix);
    void setVolume(float vol);
    void setDelayTime(float time);
    void setFeedback(float fdbk);
    float getMix() {return wetMix;};
#if USE_DAISY
    void setParameter(int param_id, float value) override;
#else
    void setParameter(int param_id, float value);
private:
    bool active = false;
    audio_block_t* inputQueueArray_[2];
#endif
};