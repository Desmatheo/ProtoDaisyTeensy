#pragma once 
#include "Utils.h"


#if USE_DAISY
#if USE_DAISY_POD
#include "daisy_pod.h"
#elif USE_DAISY_TDM
#include "daisy_tdm_slave.h"
#else
#include "daisy_seed.h"
#endif

#include "Effect.h"
#endif
#include "../EffetEarth/Earth.h"
#include "../EffetDelay/Delay.h"
#include "../EffetDisto/AudioEffectDrive.h"

#if USE_DAISY

#if USE_DAISY_POD
DaisyPod hardware;

bool volatile btnSwitchEffet = false;
bool volatile btnSwitchparam = false;
#elif USE_DAISY_TDM
DaisyTdmSlave hardware;
#else
DaisySeed hardware;
#endif

bool led_state;
uint32_t last_blink;

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

StringUtil strings[] = {
    StringUtil(EffectType::Bypass, 0),
    StringUtil(EffectType::Bypass, 1),
    StringUtil(EffectType::Bypass, 2),
    StringUtil(EffectType::Bypass, 3),
    StringUtil(EffectType::Bypass, 4),
    StringUtil(EffectType::Bypass, 5)
};

#include "audio_processing.h"

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

extern paramUtil effectParams;

void changeEffect(){
    #pragma region Changement des effets
    EffectType current = strings[idxString].type;
    if(current == EffectType::Bypass) {
        strings[idxString].type                     = EffectType::Mute;
        strings[idxString].active_effect            = nullptr;
    } else if (current == EffectType::Mute) {
        strings[idxString].type                     = EffectType::Earth;
        strings[idxString].active_effect            = earth_effects[idxString];
    } else if(current == EffectType::Earth) {
        strings[idxString].type                     = EffectType::Delay;
        strings[idxString].active_effect            = delay_effects[idxString];
    } else if(current == EffectType::Delay) {
        strings[idxString].type                     = EffectType::Drive;
        strings[idxString].active_effect            = drive_effects[idxString];
    // } else if(current == EffectType::PitchShift) {
    //     strings[idxString].type                  = EffectType::pitchShiftbkshep;
    //     strings[idxString].active_effect_module  = pitchshiftbkshep_effects[idxString];
    //     strings[idxString].active_effect         = nullptr;
    // } else if(current == EffectType::pitchShiftbkshep) {
    // //     strings[idxString].type               = EffectType::Uranus;
    // //     strings[idxString].active_effect      = uranus_effects[idxString];
    // // } else if(current == EffectType::Uranus) {
    //     strings[idxString].type                  = EffectType::Neptune;
    //     strings[idxString].active_effect         = neptune_effects[idxString];
    //     strings[idxString].active_effect_module  = nullptr;
    } else {
        strings[idxString].type                     = EffectType::Bypass;
        strings[idxString].active_effect            = nullptr;
        //strings[idxString].active_effect_module   = nullptr;
    }
    #pragma endregion
}


void updateUI(){

#if USE_DAISY_POD
        hardware.ProcessAnalogControls();
        hardware.ProcessDigitalControls();
        
        bool button_pressed = false;

        // Si on tourne l'encodeur
        int enc_inc = hardware.encoder.Increment(); 
        if (enc_inc != 0) {
            effectParams.changeParam(enc_inc);
            effectParams.changing = false;
            button_pressed = true; // Force la mise à jour des LEDs sans attendre
        }

        // Si on appuis sur l'encodeur
        if (hardware.encoder.RisingEdge()) {
            effectParams.changing = !effectParams.changing; // Toggle On/Off du mode édition
            button_pressed = true; 
        }

        // Si on clique sur le bouton 1, on change d'effet
        if(hardware.button1.RisingEdge()) {
            changeEffect();
            effectParams.changing = false; // Quitte le mode édition par sécurité
            button_pressed = true;
        }
        
        // Si on clique sur le bouton 2, on sélectionne la corde suivante
        if(hardware.button2.RisingEdge()) {
            idxString = (idxString + 1) % 6;
            effectParams.changing = false; // Quitte le mode édition par sécurité
            button_pressed = true;
        }

        // Timer non-bloquant : on fait clignoter la LED toutes les 500ms
        if(System::GetNow() - last_blink > 500 || button_pressed) {
            
            if (button_pressed) {
                led_state = true; // Allume tout de suite pour voir le changement
            }
            last_blink = System::GetNow();


            // Affiche la couleur de l'effet assigné à la CORDE ACTUELLE
            EffectType current = strings[idxString].type;
            float r = 0.f, g = 0.f, b = 0.f;
            
            if (led_state) {
                if (current == EffectType::Bypass) {
                    r = 1.f;                        // Rouge
                } else if (current == EffectType::Earth) {
                    g = 1.f;                        // Vert classique
                } else if (current == EffectType::Delay) {
                    b = 1.f;                        // Bleu
                } else if (current == EffectType::Drive) {
                    r = 1.f; g = 1.f;               // Jaune (Rouge + Vert)
                // } else if (current == EffectType::PitchShift) {
                //     r = 1.f; g = 1.f; // Jaune
                // } else if (current == EffectType::pitchShiftbkshep) {
                //     r = 1.f; g = 1.f; b = 1.f;   // Blanc
                // } else if (current == EffectType::Uranus) {
                //     b = 1.f;                     // Bleu
                // } else if (current == EffectType::Neptune) {
                //     g = 1.f; b = 1.f;            // Cyan
                // }
                }
            }

            hardware.led1.Set(r, g, b); 


            led_state = !led_state;
        }

        // --- Affiche la couleur du paramètre sélectionné sur la LED 2 ---
        int current_param = effectParams.GetParam();
        float r2 = 0.f, g2 = 0.f, b2 = 0.f;
        
        // Si on édite le paramètre, on fait clignoter la LED en synchro avec la LED 1
        // led_state est inversé à la fin du timer, on regarde donc !led_state
        bool led2_active = !effectParams.changing || !led_state;

        if (led2_active) {
            if (current_param == 0) {
                r2 = 1.f; // Rouge pour le Paramètre 0
            } else if (current_param == 1) {
                b2 = 1.f; // Bleu pour le Paramètre 1
            } else if (current_param == 2) {
                g2 = 1.f; // Vert pour le Paramètre 2
            }
        }
        hardware.led2.Set(r2, g2, b2);

        hardware.UpdateLeds();
#else
        if(System::GetNow() - last_blink > 500) {
            last_blink = System::GetNow();
            hardware.seed.SetLed(led_state);
        
            led_state = !led_state;
        }
#endif
}




#endif // USE_DAISY