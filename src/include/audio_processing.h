#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <cmath>
#include "daisy_core.h"
#include "daisy_tdm_slave.h"

extern DaisyTdmSlave hardware;

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

static void AudiotestCallback(daisy::AudioHandle::InputBuffer  in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t                           size)
{
    const float** in2 = const_cast<const float**>(in);
    float** out2 = const_cast<float**>(out);
    //parcourt les echantillons du buffer
    for (int i = 0; i < (int)size; i++){


        // Audio de sortie (6 entrées)
        for (int j = 0; j < 6; j++){
            // Si la corde est mute on met a 0 sans chercher le sample d'entrée
            if (strings[j].type == EffectType::Mute) {
                out[j][i] = 0;
            }
            else {

                if (strings[j].type == EffectType::Bypass) {
                    out[j][i] = in[j][i];
                } else if (strings[j].active_effect != nullptr) {
                    strings[j].active_effect->update(in2, out2, i, j);
                }
            }
        }
    };

    /*
    audio_diag.callback_count++;

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

#endif // AUDIO_PROCESSING_H
