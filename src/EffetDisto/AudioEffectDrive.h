
/*
  AudioEffectDrive.h
  Interface de la classe AudioEffectDrive (choix de courbes, setParams(), setEnabled()).

  Remarques importantes :
  - Les allocations en PSRAM/EXTMEM utilisent extmem_malloc/extmem_free quand c'est nécessaire.
  - Les paramètres sont conçus pour être modifiables à chaud ; certaines transitions sont lissées pour éviter des clics.
*/

#pragma once

#include "../include/Utils.h"

#if USE_DAISY
#include "daisy_seed.h"
#include "daisysp.h"
#include "../../DaisySP/DaisySP-LGPL/Source/Filters/tone.h"
#include "../include/Effect.h"
using namespace daisy;
using namespace daisysp;
#else
#include <Arduino.h>
#include "AudioStream.h"
#include "arm_math.h"
#endif

/*
 * AudioEffectDrive
 *
 * Overdrive par waveshaper avec :
 *  - drive en dB (0..30 dB) => pré-gain avant waveshaper
 *  - LUT de waveshaper stockée en PSRAM (EXTMEM via extmem_malloc)
 *  - filtre low-pass post-drive (tone 800..8000 Hz)
 *  - mix dry/wet
 *  - on/off soft via setEnabled (mix effectif = 0 quand OFF)
 *
 * 1 entrée, 1 sortie.
 *
 * Usage typique :
 *
 *   AudioEffectDrive drive0;
 *   // dans setup():
 *   drive0.begin(2048); // alloue la LUT en PSRAM
 *   drive0.setParams(15.0f, 2000.0f, 0.6f, AudioEffectDrive::CURVE_SOFT);
 *   drive0.setEnabled(true);
 */

#if USE_DAISY
class AudioEffectDrive : public Effect
#else
class AudioEffectDrive : public AudioStream
#endif
{
public:
    // Types de courbes de waveshaper
    enum curve_t : uint8_t {
        CURVE_SOFT = 0,   // soft clip polynomial
        CURVE_HARD = 1,   // hard clip
        CURVE_TANH = 2,   // tanh(x)
        CURVE_ASYM = 3    // asymétrique
    };

#if USE_DAISY
    AudioEffectDrive(float sampleRate);
#else
    AudioEffectDrive();
#endif
    virtual ~AudioEffectDrive();

    // 1 entrée, 1 sortie
#if USE_DAISY
    void update(const float** in, float** out, int idx, int ch) override;
    float updateTest(const float in, float out, int idx, int ch) override { return in; }
#else
    virtual void update() override;

    // --- Initialisation PSRAM ---
    // Alloue la LUT du waveshaper en PSRAM via extmem_malloc.
    // table_len : nombre d'échantillons de la courbe (-1..+1 mappé sur 0..table_len-1)
    // Retourne true si l'allocation a réussi.
    bool begin(uint32_t table_len = 2048);
#endif

    // --- Paramètres individuels ---

    // drive en dB (0..30). Interne : gain linéaire = 10^(dB/20)
    void setDriveDb(float dB);

    // tone en Hz (800..8000). Contrôle un low-pass post-drive.
    void setToneHz(float hz);

    // mix (0..1) : 0 = 100% dry, 1 = 100% wet
    void setMix(float mix);

    // Volume de sortie du signal distordu (Level)
    void setVolume(float vol);

    // Paramétrage générique unifié (0 à 1)
#if USE_DAISY
    void setParameter(int param_id, float value) override;
#else
    void setParameter(int param_id, float value);
#endif

    // choix de la courbe
    void setCurve(curve_t c);
    void setCurve(uint8_t c) { setCurve((curve_t)(c & 0x03)); }

    // setter groupé pratique
    void setParams(float drive_dB, float tone_hz, float mix, curve_t curve);

    // getters (pour debug / UI)
    float   getDriveDb() const { return drive_dB_; }
    float   getToneHz()  const { return tone_hz_; }
    float   getMix()     const { return mix_; }
    float   getVolume()  const { return volume_; }
    curve_t getCurve()   const { return curve_; }

    // ON/OFF soft basé sur le mix : OFF => active_mix_ = 0
    void setEnabled(bool e);
    bool isEnabled() const { return enabled_; }

#if !USE_DAISY
    // Infos PSRAM
    bool      usingExtmem()  const { return use_extmem_; }
    uintptr_t tableAddress() const { return (uintptr_t)table_; }
    uint32_t  tableLength()  const { return table_len_; }
    bool      ready()        const { return ready_; }
#endif

private:
#if !USE_DAISY
    audio_block_t* inputQueueArray_[1];
#endif

    // --- Paramètres ---
    float   drive_dB_   = 0.0f;   // 0..30
    float   drive_gain_ = 1.0f;   // linéaire
    float   tone_hz_    = 2000.0f; // 800..8000
    float   mix_        = 0.5f;   // mix utilisateur (0..1)
    float   active_mix_ = 0.5f;   // mix effectif (0..1) (ON/OFF)
    float   volume_     = 0.5f;   // volume de sortie du signal wet
    curve_t curve_      = CURVE_SOFT;
    bool    enabled_    = true;

#if USE_DAISY
    daisysp::Overdrive drive_;
    Tone               tone_;
#else
    // --- LUT waveshaper en PSRAM ---
    int16_t* table_      = nullptr;  // LUT en int16[-1..+1]
    uint32_t table_len_  = 0;
    bool     use_extmem_ = false;
    bool     ready_      = false;

    // --- Filtre low-pass 1er ordre post-drive :
    // y[n] = a0 * x[n] + b1 * y[n-1]
    float tone_a0_ = 1.0f;
    float tone_b1_ = 0.0f;
    float tone_z1_ = 0.0f;    // état du filtre

    void updateToneCoeffs();
    void rebuildTable();              // reconstruit la LUT en PSRAM selon curve_

    // Évalue la courbe théorique (float) pour construire la LUT
    static float evalCurve(curve_t c, float x);

    // waveshaper via LUT : x ∈ [-1,1] -> y ∈ [-1,1]
    inline float shapeSampleLut(float x) const;
#endif
};
