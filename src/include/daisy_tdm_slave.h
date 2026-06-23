#pragma once
#ifndef DAISY_TDM_SLAVE_H
#define DAISY_TDM_SLAVE_H

#include "daisy_seed.h"

/** Daisy Seed configured as a TDM slave on SAI2, clocked by an external
 *  TDM master (Teensy 4.x Audio Library, AudioOutputTDM/AudioInputTDM).
 *
 *  Frame format (fixed by the Teensy master):
 *    - 8 slots x 32 bits = 256 BCLK per frame
 *    - FS: 1-BCLK pulse, active HIGH, asserted one bit before slot 0
 *    - MSB first, data changes on BCLK falling edge, sampled on rising edge
 *    - sample rate 48 kHz -> BCLK = 12.288 MHz
 *      (the Teensy must be built with AUDIO_SAMPLE_RATE_EXACT=48000.0f;
 *       its Audio Library defaults to 44.1 kHz otherwise)
 *
 *  Wiring (Daisy Seed <-> Teensy 4.x):
 *    D28 (SAI2_SCK_B, PA2)  <- Teensy pin 21 (BCLK1)
 *    D27 (SAI2_FS_B,  PG9)  <- Teensy pin 20 (LRCLK1)
 *    D25 (SAI2_SD_B,  PA0)  <- Teensy pin 7  (OUT1A, TDM data Teensy -> Daisy)
 *    D26 (SAI2_SD_A,  PD11) -> Teensy pin 8  (IN1,   TDM data Daisy -> Teensy)
 *    GND                    -- GND (mandatory common ground)
 *    D24 (MCLK) is NOT used: the Daisy is a pure slave and the Teensy
 *    generates its own MCLK internally.
 *
 *  Channel mapping in the audio callback:
 *    in[0..7]  = TDM slots 0..7 received from the Teensy (6 used for hexa)
 *    out[0..7] = TDM slots 0..7 transmitted to the Teensy
 */
class DaisyTdmSlave
{
  public:
    static constexpr size_t kTdmSlots  = 8; // Retour à 8 slots
    static constexpr size_t kNumInputs = 8; // On gère 8 canaux via libDaisy
    static constexpr size_t kNumOutputs = 8;
    static constexpr size_t kBlockSize  = 32;

    /** The Daisy is a clock slave: the real rate is whatever the Teensy
     *  master generates -- 48 kHz in this project (Teensy built with
     *  AUDIO_SAMPLE_RATE_EXACT=48000.0f). Use this for DSP coefficients,
     *  NOT seed.AudioSampleRate(), which reports the nominal SAI config
     *  and is meaningless in slave mode. */
    // static constexpr float kSampleRate = 48000.f;
    static constexpr float kSampleRate = 44117.647f; // Fréquence par défaut EXACTE de la Teensy 4.x
    daisy::DaisySeed seed;

    void Init(bool boost = true)
    {
        seed.Init(boost);
        // seed.Init() configured SAI1 + the on-board codec; re-init the
        // audio engine on top of it with our SAI2 TDM slave stream only.
        InitTdmAudio();
    }

    void StartAudio(daisy::AudioHandle::AudioCallback cb)
    {
        seed.audio_handle.Start(cb);
    }

  private:
    void InitTdmAudio()
    {
        using daisy::SaiHandle;
        using daisy::AudioHandle;

        SaiHandle::Config cfg;
        cfg.periph = SaiHandle::Config::Peripheral::SAI_2;
        // Nominal only: in slave mode every clock comes from the master.
        cfg.sr        = SaiHandle::Config::SampleRate::SAI_44_1KHZ;
        cfg.bit_depth = SaiHandle::Config::BitDepth::SAI_32BIT; // OBLIGATOIRE en TDM pour lire les 32 bits correctement
        cfg.tdm_slots = kTdmSlots;

        // Both blocks are slaves of the Teensy. The external FS/SCK pins of
        // SAI2 on the Seed (D27/PG9, D28/PA2) are block-B signals, so block B
        // taps the pins (ASYNCHRONOUS) and block A syncs to it internally.
        cfg.a_sync          = SaiHandle::Config::Sync::SLAVE;
        cfg.b_sync          = SaiHandle::Config::Sync::SLAVE;
        cfg.ext_clock_block = SaiHandle::Config::ExtClockBlock::BLOCK_B;

        // Teensy/i.MX SAI frame sync: 1-BCLK pulse, active high, early.
        cfg.tdm_fs_polarity = SaiHandle::Config::TdmFsPolarity::ACTIVE_HIGH;

        cfg.a_dir = SaiHandle::Config::Direction::TRANSMIT; // D26 -> Teensy
        cfg.b_dir = SaiHandle::Config::Direction::RECEIVE;  // D25 <- Teensy

        cfg.pin_config.mclk = {DSY_GPIOX, 0}; // unused: pure slave
        cfg.pin_config.fs   = daisy::seed::D27;
        cfg.pin_config.sck  = daisy::seed::D28;
        cfg.pin_config.sa   = daisy::seed::D26;
        cfg.pin_config.sb   = daisy::seed::D25;

        SaiHandle sai;
        sai.Init(cfg);

        AudioHandle::Config audio_cfg;
        audio_cfg.blocksize  = kBlockSize;
        audio_cfg.samplerate = SaiHandle::Config::SampleRate::SAI_44_1KHZ; // Synchronisé avec la Teensy !
        audio_cfg.postgain   = 1.0f;
        seed.audio_handle.Init(audio_cfg, sai);
    }
};

#endif // DAISY_TDM_SLAVE_H