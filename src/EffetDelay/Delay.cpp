#include "Delay.h"

#if USE_DAISY
using namespace daisy;
using namespace daisysp;
#endif

#if USE_DAISY

// ------ Partie Daisy --------

void DelayEffect::DelayChannel::Init(DelayLineOct<float, MAX_DELAY>* delayLine, float sampleRate) {
    del = delayLine;
    del->Init();
    del->setOctave(false);
    
    tone.Init(sampleRate);
    tone.SetFreq(3000.0f);
    
    currentDelay = 2400.0f;
    delayTarget = 2400.0f;
    feedback = 0.5f;
    active = true;
    muteFade = 1.0f;
    standbyTimer = 0;
    lastTarget = 2400.0f;
    del->SetDelay(currentDelay);
}

float DelayEffect::DelayChannel::Process(float in) {
    // --- Mode Standby Amélioré (Anti-Scratching) ---
    // Si le potard est en train d'être tourné (la cible change)
    if (fabsf(delayTarget - lastTarget) > 0.1f) {
        lastTarget = delayTarget;
        standbyTimer = 10000; // Maintient le standby pendant ~200ms après le dernier mouvement (à 48kHz)
    }

    if (standbyTimer > 0) {
        standbyTimer--;
        
        // Fade-out ultra-rapide
        muteFade -= 0.01f;
        if (muteFade <= 0.0f) {
            muteFade = 0.0f;
            // Dès qu'on est sous silence, le pointeur saute instantanément (aucun pitch-shift)
            currentDelay = delayTarget;
            del->SetDelay(currentDelay);
        }
    } else {
        // Le potard ne bouge plus, on sort du standby (Fade-in)
        muteFade += 0.01f;
        if (muteFade > 1.0f) {
            muteFade = 1.0f;
        }
    }

    // Lecture du son retardé
    float del_read = del->Read();

    // Application du fade pour le mode standby
    del_read *= muteFade;

    // Application du filtre passe-bas
    float read = tone.Process(del_read); 

    // Écriture dans la ligne de delay (avec feedback)
    if (active) {
        del->Write((feedback * read) + in);
    } else {
        del->Write(feedback * read);                // Si inactif, seul le feedback est réinjecté
    }

    return read;
}

DelayEffect::DelayEffect(float sampleRate) {
    // Initialisation des canaux de delay
    delayL.Init(&delayLineOctLeft, sampleRate);
    delayR.Init(&delayLineOctRight, sampleRate);

    // Valeurs par défaut
    setMix(0.5f);
    setDelayTime(0.5f);
    setFeedback(0.7f);
}

void DelayEffect::update(const float** in, float** out, int idx) {
    float inputL = in[0][idx];
    float inputR = in[1][idx];


    // Traitement du son par les lignes de delay
    float delay_outL = delayL.Process(inputL);
    float delay_outR = delayR.Process(inputR);

    // Mixage du son original (dry) et du son traité (wet)
    out[0][idx] = inputL * dryMix + delay_outL * wetMix * volume;
    out[1][idx] = inputR * dryMix + delay_outR * wetMix * volume;
}

float DelayEffect::updateTest(const float in, float out, int idx) {
    return in; // Implémentation factice pour satisfaire le compilateur
}


#else

// ------ Partie TEENSY -------

void DelayEffect::DelayChannel::Init(float sampleRate, uint32_t max_delay_samples) {
    buf_len = max_delay_samples;
    
    // Allocation en priorité sur la PSRAM (extmem_malloc)
    // 4s * 2 (stéréo) * 6 cordes = ~8.4 Mo, impossible sur la RAM interne !
    buffer = (float*)extmem_malloc(buf_len * sizeof(float));
    if (buffer) {
        use_extmem = true;
    } else {
        buffer = (float*)malloc(buf_len * sizeof(float));
        use_extmem = false;
    }

    if (buffer) {
        memset(buffer, 0, buf_len * sizeof(float));
    }
    write_idx = 0;
    
    // Initialisation du filtre Tone (Low-pass 1-pole) à 3000 Hz
    float tone_hz = 3000.0f;
    float alpha = expf(-2.0f * (float)PI * tone_hz / sampleRate);
    tone_a0 = 1.0f - alpha;
    tone_b1 = alpha;
    tone_z1 = 0.0f;

    currentDelay = 2400.0f;
    delayTarget = 2400.0f;
    feedback = 0.5f;
    active = true;
    muteFade = 1.0f;
    standbyTimer = 0;
    lastTarget = 2400.0f;
}

void DelayEffect::DelayChannel::Free() {
    if (buffer) {
        if (use_extmem) {
            extmem_free(buffer);
        } else {
            free(buffer);
        }
        buffer = nullptr;
    }
}

float DelayEffect::DelayChannel::Process(float in) {
    if (!buffer) return in;

    // Si le potard est en train d'être tourné (la cible change)
    if (fabsf(delayTarget - lastTarget) > 0.1f) {
        lastTarget = delayTarget;
        standbyTimer = 10000; // Maintient le standby pendant ~220ms après le dernier mouvement
    }

    if (standbyTimer > 0) {
        standbyTimer--;
        
        // Fade-out ultra-rapide
        muteFade -= 0.01f;
        if (muteFade <= 0.0f) {
            muteFade = 0.0f;
            // Dès qu'on est sous silence, le pointeur saute instantanément (aucun pitch-shift)
            currentDelay = delayTarget;
        }
    } else {
        // Le potard ne bouge plus, on sort du standby (Fade-in)
        muteFade += 0.01f;
        if (muteFade > 1.0f) {
            muteFade = 1.0f;
        }
    }

    // Lecture du son retardé avec interpolation linéaire
    float read_idx_f = (float)write_idx - currentDelay;
    while (read_idx_f < 0.0f) read_idx_f += (float)buf_len;
    while (read_idx_f >= (float)buf_len) read_idx_f -= (float)buf_len;

    uint32_t r0 = (uint32_t)read_idx_f;
    uint32_t r1 = r0 + 1; 
    if (r1 >= buf_len) r1 = 0;
    float frac = read_idx_f - (float)r0;

    float del_read = buffer[r0] + (buffer[r1] - buffer[r0]) * frac;

    // Application du fade pour le mode standby
    del_read *= muteFade;

    // Application du filtre passe-bas
    float read = tone_a0 * del_read + tone_b1 * tone_z1;
    tone_z1 = read;

    // Écriture dans la ligne de delay (avec feedback)
    float write_val = feedback * read;
    if (active) {
        write_val += in;
    }
    
    // Limiteur de saturation interne
    if (write_val > 1.0f) write_val = 1.0f;
    if (write_val < -1.0f) write_val = -1.0f;
    
    buffer[write_idx] = write_val;

    write_idx++;
    if (write_idx >= buf_len) write_idx = 0;

    return read;
}

DelayEffect::DelayEffect() : AudioStream(2, inputQueueArray_) {
    setMix(0.5f);
    setDelayTime(0.5f);
    setFeedback(0.7f);
}

DelayEffect::~DelayEffect() {
    delayL.Free();
    delayR.Free();
}

bool DelayEffect::begin() {
    delayL.Free();
    delayR.Free();
    
    delayL.Init(AUDIO_SAMPLE_RATE_EXACT, MAX_DELAY);
    delayR.Init(AUDIO_SAMPLE_RATE_EXACT, MAX_DELAY);
    
    return (delayL.buffer != nullptr && delayR.buffer != nullptr);
}

void DelayEffect::update() {
    audio_block_t* inL = receiveReadOnly(0);
    audio_block_t* inR = receiveReadOnly(1);

    if (!inL && !inR) return;
    
    if (!active || (!delayL.buffer && !delayR.buffer)) {
        if (inL) { transmit(inL, 0); release(inL); }
        if (inR) { transmit(inR, 1); release(inR); }
        return;
    }

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

        float delay_outL = delayL.Process(inputL);
        float delay_outR = delayR.Process(inputR);

        float outputL = (inputL * dryMix) + (delay_outL * wetMix * volume);
        float outputR = (inputR * dryMix) + (delay_outR * wetMix * volume);

        // Saturation douce (clamp final)
        if (outputL > 1.0f) outputL = 1.0f;
        if (outputL < -1.0f) outputL = -1.0f;
        if (outputR > 1.0f) outputR = 1.0f;
        if (outputR < -1.0f) outputR = -1.0f;

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
#endif

void DelayEffect::setMix(float mix) {
    wetMix = clampf(mix, 0.0f, 1.0f);
    dryMix = 1.0f - wetMix;
}

void DelayEffect::setVolume(float vol)
{
    volume = clampf(vol, 0.0f, 1.0f);
}

void DelayEffect::setDelayTime(float time) {
    vdelayTime = clampf(time, 0.0f, 1.0f);
    // Mise à jour de l'état actif et de la cible du delay uniquement quand la valeur change
    bool isActive = (vdelayTime > 0.01f);
    delayL.active = isActive;
    delayR.active = isActive;

#if USE_DAISY
    float target = fmap(vdelayTime, 2400.0f, static_cast<float>(MAX_DELAY), Mapping::LOG);
#else
    // Mapping logarithmique (équivalent de fmap Mapping::LOG)
    float log_min = logf(2400.0f);
    float log_max = logf(static_cast<float>(MAX_DELAY));
    float target = expf(log_min + vdelayTime * (log_max - log_min));
#endif

    delayL.delayTarget = target;
    delayR.delayTarget = target;
}

void DelayEffect::setFeedback(float fdbk) {
    vdelayFDBK = clampf(fdbk, 0.0f, 0.99f);

    delayL.feedback = vdelayFDBK;
    delayR.feedback = vdelayFDBK;
}


void DelayEffect::setParameter(int param_id, float value) {
    switch (param_id) {
        case 0:
            setMix(value);
            break;
        case 1:
            setDelayTime(value);
            break;
        case 2:
            setFeedback(value);
            break;
        case 5: 
            setVolume(value);
            break;
        default:
            break;
    }
}
