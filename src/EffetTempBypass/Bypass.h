#include <Arduino.h>
#include "AudioStream.h"
#include "arm_math.h"


class BypassEffect : public AudioStream {
public:
    BypassEffect() : AudioStream(2, inputQueueArray_) {}

    virtual void update() override {
        audio_block_t* inL = receiveReadOnly(0);
        audio_block_t* inR = receiveReadOnly(1);

        if (!inL && !inR) return;

        audio_block_t* outL = allocate();
        audio_block_t* outR = allocate();

        if (!outL || !outR) {
            if (outL) release(outL);
            if (outR) release(outR);
            if (inL) release(inL);
            if (inR) release(inR);
            return;
        }

        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float inputL = inL ? ((float)inL->data[i] / 32768.0f) : 0.0f;
            float inputR = inR ? ((float)inR->data[i] / 32768.0f) : inputL; 

            float outputL = (inputL * 1.0f);
            float outputR = (inputR * 1.0f);

            outL->data[i] = (int16_t)(outputL * 32767.0f);
            outR->data[i] = (int16_t)(outputR * 32767.0f);
        }

        transmit(outL, 0);
        transmit(outR, 1);
        
        release(outL);
        release(outR);
        if (inL) release(inL);
        if (inR) release(inR);
    }

private:
    audio_block_t* inputQueueArray_[2];
};