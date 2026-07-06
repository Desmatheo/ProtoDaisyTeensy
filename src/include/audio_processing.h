#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <cmath>
#include "daisy_core.h"
#include "daisy_tdm_slave.h"

#if USE_DAISY_TDM
extern DaisyTdmSlave hardware;
#elif USE_DAISY_POD
extern DaisyPod hardware;
#else
extern DaisySeed hardware;
#endif

// ================================================================
// Diagnostics shared between the audio callback (IRQ context) and the
// main loop. Written by the callback, read + reset by main().
// Never print from the audio callback: it runs in an interrupt and
// USB logging there can block the whole audio engine.
// ================================================================
struct AudioDiagnostics
{

    volatile uint32_t callback_count = 0;
    volatile float    in_peak[DaisyTdmSlave::kNumInputs] = {0};

    void ResetPeaks()
    {
        for(size_t ch = 0; ch < DaisyTdmSlave::kNumInputs; ch++)
            in_peak[ch] = 0.f;
    }
};

static AudioDiagnostics audio_diag;

// ================================================================
// Audio callback -- runs at 48 kHz / blocksize (1500 Hz for block 32),
// clocked by the Teensy TDM master.
//
//   in[0..5]  : the 6 hexaphonic channels sent by the Teensy (slots 0..5)
//   in[6..7]  : unused slots (silence as long as the Teensy sends nothing)
//   out[0..7] : the 8 channels sent back to the Teensy (slots 0..7)
//
// Routage actuel : Pass-through Stéréo depuis l'USB (Teensy)
//   out[0] = in[0] (Canal Gauche)
//   out[2] = in[2] (Canal Droit)
//   Les autres sorties sont mises au silence.
// ================================================================

#define HexaTDM 1


#if USE_DAISY_TDM
static void AudioCallback(daisy::AudioHandle::InputBuffer  in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t                           size)
{

    audio_diag.callback_count++;

    bool signal_present = false;

    #if CPU_METER && (!CPU_LoadEffect || CPU_LoadAll)
        loadMeter.OnBlockStart();
    #endif




    const float** in2 = const_cast<const float**>(in);
    float** out2 = const_cast<float**>(out);
    //parcourt les echantillons du buffer
    for (size_t echant = 0; echant < size; echant++){

        for(size_t ch = 0; ch < DaisyTdmSlave::kNumInputs; ch++)
        {
            const float s   = in[ch][echant];
            const float mag = fabsf(s);
            if(mag > audio_diag.in_peak[ch])
                audio_diag.in_peak[ch] = mag;
            if(mag > 0.05f)
                signal_present = true;
        }



        // Audio de sortie (6 entrées)
        for (size_t ch = 0; ch <  DaisyTdmSlave::kNumInputs; ch++){

            // Si la corde est mute on met a 0 sans chercher le sample d'entrée
            if (strings[ch].type == EffectType::Mute) {
                out[ch][echant] = 0;
            }
            else {

                if (strings[ch].type == EffectType::Bypass) {
                    out[ch][echant] = in[ch][echant];
                } else if (strings[ch].active_effect != nullptr) {
                    strings[ch].active_effect->update(in2, out2, echant, ch);
                }
            }
        }
    };


    #if CPU_METER
    #if !CPU_LoadEffect
        loadMeter.OnBlockEnd();
    #elif CPU_LoadAll
        // À la fin du bloc audio, on sauvegarde la somme des cycles pour l'affichage, et on remet à 0
        if (earth_effects[0] != nullptr) {
            earth_effects[0]->last_profiled_ticks = earth_effects[0]->profiled_ticks;
            earth_effects[0]->profiled_ticks = 0;
        }
        loadMeter.OnBlockEnd();
    #else 
        // À la fin du bloc audio, on sauvegarde la somme des cycles pour l'affichage, et on remet à 0
        if (earth_effects[0] != nullptr) {
            earth_effects[0]->last_profiled_ticks = earth_effects[0]->profiled_ticks;
            earth_effects[0]->profiled_ticks = 0;
        }
    #endif
    #endif
    /*

    bool signal_present = false;

    for(size_t i = 0; i < size; i++)
    {
        // 1. Analyse des signaux entrants pour les diagnostics (pics sur le port Série)
        for(size_t ch = 0; ch < DaisyTdmSlave::kNumInputs; ch++)
        {
            const float s   = in[ch][i];
            const float mag = fabsf(s);
            if(mag > audio_diag.in_peak[ch])
                audio_diag.in_peak[ch] = mag;
            if(mag > 0.05f)
                signal_present = true;
        }

        // 2. Routage avec protection "anti-wrap-around" (0.95f au lieu de 1.0f)
        // Empêche la distorsion extrême si un pic de son se convertit mal en int32_t
        out[0][i] = in[0][i] * 0.95f; 
        out[1][i] = in[1][i] * 0.95f; 

        // 3. Mute des autres canaux pour garder un signal audio propre et éviter du bruit
        for(size_t ch = 0; ch < DaisyTdmSlave::kNumOutputs; ch++) {
            if(ch != 0 && ch != 1) {
                out[ch][i] = 0.0f;
            }
        }
    }

    // On-board LED lights up while audio is arriving from the Teensy.
    hw.seed.SetLed(signal_present); */
}



#else
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    #pragma region Boucle Audio
#if CPU_METER
#if !CPU_LoadEffect
    loadMeter.OnBlockStart();
#elif CPU_LoadAll
    loadMeter.OnBlockStart();
#endif
#endif

    // hardware.seed.PrintLine("test entrée boucle");

    float pot1_val = 0.0f;

    //parcourt les echantillons du buffer
    for (int i = 0; i < (int)size; i++){
        // Audio de sortie (cumulé des 6 entrées)
        float mixed_out_l = 0.0f;
        float mixed_out_r = 0.0f;

        for (int j = 0; j < 6; j++){
            float out_arr[2][1] = {{0.0f}, {0.0f}};
            float* out_ptrs[2] = {out_arr[0], out_arr[1]};

            // Si la corde est mute on met a 0 sans chercher le sample d'entrée
            if (strings[j].type == EffectType::Mute) {
                out_arr[0][0] = 0;
                out_arr[1][0] = 0;
            }
            else {

#if SD_CARD_DS
                float sample = s162f(sampler.StreamHex(j));
                float in_arr[2][1] = {{sample}, {sample}};
#else 
                float in_arr[2][1] = {{in[0][i]}, {in[1][i]}};    
#endif
                const float* in_ptrs[2] = {in_arr[0], in_arr[1]};


                if (strings[j].type == EffectType::Bypass) {
                    out_arr[0][0] = in_arr[0][0];
                    out_arr[1][0] = in_arr[1][0];
                } else if (strings[j].type == EffectType::Mute) {
                    out_arr[0][0] = 0;
                    out_arr[1][0] = 0;
                } else if (strings[j].active_effect != nullptr) {
                    strings[j].active_effect->update(in_ptrs, out_ptrs, 0, 0);

                    if (effectParams.changing && j == idxString && i == 0) { 
#if USE_DAISY_POD
                        pot1_val = hardware.knob1.Process(); 
                        strings[j].active_effect->setParameter(effectParams.GetParam(), pot1_val);
#endif
                    }
                }
            }

#if Padding_on
            mixed_out_l += out_arr[0][0] * ((j + 1) / 6.0f);
            mixed_out_r += out_arr[1][0] * (1 - ((j + 1) / 6.0f));
#else 
            mixed_out_l += out_arr[0][0];
            mixed_out_r += out_arr[1][0];
#endif
        }
        out[0][i] = mixed_out_l ;// / 6.0f;
        out[1][i] = mixed_out_r ;// / 6.0f;
    };
#if CPU_METER
#if !CPU_LoadEffect
    loadMeter.OnBlockEnd();
#elif CPU_LoadAll
    // À la fin du bloc audio, on sauvegarde la somme des cycles pour l'affichage, et on remet à 0
    if (earth_effects[0] != nullptr) {
        earth_effects[0]->last_profiled_ticks = earth_effects[0]->profiled_ticks;
        earth_effects[0]->profiled_ticks = 0;
    }
    loadMeter.OnBlockEnd();
#else 
    // À la fin du bloc audio, on sauvegarde la somme des cycles pour l'affichage, et on remet à 0
    if (earth_effects[0] != nullptr) {
        earth_effects[0]->last_profiled_ticks = earth_effects[0]->profiled_ticks;
        earth_effects[0]->profiled_ticks = 0;
    }
#endif
#endif

    #pragma endregion
}
#endif

#endif // AUDIO_PROCESSING_H
