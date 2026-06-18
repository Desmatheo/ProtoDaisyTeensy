// Earth Reverbscape

#include "Earth.h"

#include <span>

#if USE_DAISY
#include "daisy_seed.h"
#include "daisysp.h"
#include <q/support/literals.hpp>
namespace q = cycfi::q;
using namespace q::literals;

using namespace daisy;
using namespace daisysp;

#endif

#define eq_ON 0
#define od_ON 0

#if USE_DAISY
EarthEffect::EarthEffect(float sampleRate)
    : octave(sampleRate / resample_factor),
      eq1(-11, 140_Hz, sampleRate),
      eq2(5, 160_Hz, sampleRate)
#else
EarthEffect::EarthEffect()
    : AudioStream(1, inputQueueArray_),
      octave(AUDIO_SAMPLE_RATE_EXACT / resample_factor)
#endif
{
    for (int j = 0; j < 6; ++j) {
        buff[j] = 0.0f;
        buff_out[j] = 0.0f;
    }

    setMix(1.0f);
    setOctaveMode(1);

#if od_ON
    overdrive.Init();
    overdrive.SetDrive(0.4f);
    current_ODswell = 0.4f;
#endif
}

#if USE_DAISY
void EarthEffect::update(const float** in, float** out, int idx) {
    float inputL;
    float inputR;

    inputL = inputR = in[0][idx] + 1e-9f; // Anti-denormal

    buff[bin_counter] = inputL;
    
    if (bin_counter > 4) {
        if (1) {
            std::span<const float, resample_factor> in_chunk(&(buff[0]), resample_factor);
            const auto sample = decimate2(in_chunk); 

            float octave_mix = 0.0;

#if CPU_LoadEffect
            uint32_t start_ticks = daisy::System::GetTick();
#endif
            octave.update(sample, effect_mode);
#if CPU_LoadEffect
            profiled_ticks += (daisy::System::GetTick() - start_ticks);
#endif

            if (effect_mode == 1) octave_mix += octave.up1() * 6.0;
            if (effect_mode == 2) octave_mix += octave.down1() * 6.0;
            if (effect_mode == 3) octave_mix += octave.down2() * 6.0;

            auto out_chunk = interpolate(octave_mix);
            for (size_t j = 0; j < out_chunk.size(); ++j) {
#if eq_ON
                buff_out[j] = eq2(eq1(out_chunk[j]));
#else
                buff_out[j] = out_chunk[j];
#endif
            }
        } 
        else {
            // Optimisation CPU : on ne calcule pas l'octaver s'il est coupé
            for (size_t j = 0; j < 6; ++j) {
                buff_out[j] = 0.0f;
            }
        }
    }

    // Avance le compteur de temps (0 à 5)
    bin_counter += 1;
    if (bin_counter > 5) bin_counter = 0;

    // Le signal d'effet est le son de l'octaver (ou 0 si désactivé)
    float octave_signal = buff_out[bin_counter];

    float effect_output = octave_signal;

    // Application de l'overdrive si activé
#if od_ON
    // Really cool sound when the low octave is overdriven, like epic sci fi blade runner
    effect_output = overdrive.Process(effect_input * 0.25f) * (1.0f - (current_ODswell * current_ODswell * 2.8f - 0.1296f));
#endif

    // Mixage final dry/wet pour cet effet de corde
    out[0][idx] = inputL * dryMix + effect_output * wetMix * volume;
    out[1][idx] = out[0][idx];
}

float EarthEffect::updateTest(const float in, float out, int idx) {
    return in; // Implémentation factice pour satisfaire le compilateur
}
#else
void EarthEffect::update() {

    audio_block_t* in = receiveReadOnly(0);
    if (!in) return;

    // Pour éviter de faire le gros calcul DSP quand l'effet n'est pas actif, bypass complet
    if (!active) {
        transmit(in, 0);
        release(in);
        return;
    }

    audio_block_t* out = allocate();
    if (!out) {
        release(in);
        return;
    }

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float inputL = (float)in->data[i] / 32768.0f;
        inputL += 1e-9f; // Anti-denormal

        buff[bin_counter] = inputL;
        
        if (bin_counter > 4) {
            if (1) {
                std::span<const float, resample_factor> in_chunk(&(buff[0]), resample_factor);
                const auto sample = decimate2(in_chunk); 

                float octave_mix = 0.0f;
                octave.update(sample, effect_mode);

                if (effect_mode == 1) octave_mix += octave.up1() * 1.0f;
                if (effect_mode == 2) octave_mix += octave.down1() * 2.0f;
                if (effect_mode == 3) octave_mix += octave.down2() * 4.0f;

                auto out_chunk = interpolate(octave_mix);
                for (size_t j = 0; j < out_chunk.size(); ++j) {
                    buff_out[j] = out_chunk[j];
                }
            } 
            else {
                for (size_t j = 0; j < 6; ++j) {
                    buff_out[j] = 0.0f;
                }
            }
        }

        bin_counter += 1;
        if (bin_counter > 5) bin_counter = 0;

        float octave_signal = buff_out[bin_counter];

        float output = (inputL * dryMix) + (octave_signal * wetMix * volume);

        if (output > 1.0f) output = 1.0f;
        if (output < -1.0f) output = -1.0f;
        out->data[i] = (int16_t)(output * 32767.0f);
    }

    transmit(out, 0);
    release(out);
    release(in);
}
#endif

// --- Implémentation des Setters Spécifiques ---

void EarthEffect::setMix(float mix) {
    dryMix = 1.0f - mix;
    wetMix = mix;
}

void EarthEffect::setVolume(float vol)
{
    volume = clampf(vol, 0.0f, 1.0f);
}

void EarthEffect::setOctaveMode(int mode) {
    effect_mode = mode;
}


void EarthEffect::setParameter(int param_id, float value) {
    switch (param_id){
        case 0 : 
            setMix(value);
            break;
        case 1 :  
            if (value > 0.66f) setOctaveMode(1);
            else if ((0.66f > value) && (value > 0.33f)) setOctaveMode(2);
            else setOctaveMode(3);
            break; 
        case 5 : 
            setVolume(value);
            break;
        default:
#if !USE_DAISY
            Serial.print("Parametre invalide: ");
            Serial.println(param_id);
#endif
            break;
    };
}