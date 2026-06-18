#include "include/main.h"

#if USE_DAISY
#if USE_DAISY_POD
#include "daisy_pod.h"
#else
#include "daisy_seed.h"
#endif

#if SD_CARD_DS 
#include "include/WavHexaPlayer.h"
#endif

#include "EffetEarth/Earth.h"
#include <new> // Nécessaire pour le "placement new"
#include <cstring> // Nécessaire pour memset


using namespace daisy;

StringUtil strings[] = {
    StringUtil(EffectType::Bypass, 0),
    StringUtil(EffectType::Bypass, 1),
    StringUtil(EffectType::Bypass, 2),
    StringUtil(EffectType::Bypass, 3),
    StringUtil(EffectType::Bypass, 4),
    StringUtil(EffectType::Bypass, 5)
};

#if USE_DAISY_POD
DaisyPod hardware;

bool volatile btnSwitchEffet = false;
bool volatile btnSwitchparam = false;
uint32_t last_blink;
bool led_state;

#else
DaisySeed hardware;
#endif

#if CPU_METER
CpuLoadMeter loadMeter;
#endif


#if SD_CARD_DS
SdmmcHandler   sdcard;
FatFSInterface fsi;
WavHexaPlayer  sampler;
#endif

paramUtil effectParams(3); // On indique qu'il y a 3 paramètres pour l'instant (PitchShifter)

// Allocation dans l'espace mémoire
alignas(EarthEffect) static uint8_t                 earth_mem[6 * sizeof(EarthEffect)];
alignas(DelayEffect) static uint8_t DSY_SDRAM_BSS   delay_mem[6 * sizeof(DelayEffect)];
alignas(AudioEffectDrive) static uint8_t DSY_SDRAM_BSS drive_mem[6 * sizeof(AudioEffectDrive)];



void changeEffect(){
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
            hardware.SetLed(led_state);
        
            led_state = !led_state;
        }
#endif
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
#if CPU_METER
#if !CPU_LoadEffect
    loadMeter.OnBlockStart();
#elif CPU_LoadAll
    loadMeter.OnBlockStart();
#endif
#endif

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
                    strings[j].active_effect->update(in_ptrs, out_ptrs, 0);

                    if (effectParams.changing && j == idxString && i == 0) { 
                        pot1_val = hardware.knob1.Process(); 
                        strings[j].active_effect->setParameter(effectParams.GetParam(), pot1_val);
                    }

                // Pour les effets bksheperd
                // } else if (strings[j].active_effect_module != nullptr) {
                //     strings[j].active_effect_module->ProcessStereo(in_arr[0][0], in_arr[1][0]);
                //     out_arr[0][0] = strings[j].active_effect_module->GetAudioLeft();
                //     out_arr[1][0] = strings[j].active_effect_module->GetAudioRight();
                    
                //     if (effectParams.changing && j == idxString && i == 0) { 
                //         pot1_val = hardware.knob1.Process(); 
                //         strings[j].active_effect_module->SetParameterAsMagnitude(effectParams.GetParam(), pot1_val);
                //     }
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
}

int main(void)
{
    float samplerate;

    // Initialisation du matériel cible
#if USE_DAISY_POD
    // Initialise le Daisy Pod (codec audio, contrôles, LEDs, etc.)
    hardware.Init();

    // Allume la LED en Bleu pour indiquer le début de l'allocation mémoire
    hardware.led1.Set(0.f, 0.f, 1.f); // Bleu
    hardware.UpdateLeds();
#else
    // Configure et initialise la Daisy Seed seule
    hardware.Configure();
    hardware.Init();

    hardware.SetLed(true);
#endif

#if SD_CARD_DS
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sdcard.Init(sd_cfg);
    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    f_mount(&fsi.GetSDFileSystem(), "/", 1);

    sampler.Init(fsi.GetSDPath());
    
    // Ouvre les 6 fichiers
    for(int i = 0; i < 6; i++) {
        if (i < sampler.GetNumberFiles()) {
            sampler.OpenHex(i, i);
            sampler.SetLoopingHex(i, true);
        }
    }
#endif

    // Configuration audio commune
    hardware.SetAudioBlockSize(48); // LIMITE LIBDAISY : la taille max est de 128. 48 est sûr et multiple de 6.
    samplerate = hardware.AudioSampleRate();

#if CPU_METER
    // Initialisation du module de mesure CPU
    hardware.seed.StartLog(true);
    loadMeter.Init(hardware.AudioSampleRate(), hardware.AudioBlockSize());
#endif

    // La SDRAM n'est pas mise à zéro par défaut. On la vide pour éviter des bruits stridents dans la reverb.
    memset(earth_mem, 0, 6 * sizeof(EarthEffect));
    memset(delay_mem, 0, 6 * sizeof(DelayEffect));
    memset(drive_mem, 0, 6 * sizeof(AudioEffectDrive));
    // memset(uranus_mem, 0, 6 * sizeof(UranusEffect));
    // memset(neptune_mem, 0, 6 * sizeof(NeptuneEffect));
    // memset(pitchshift_mem, 0, 6 * sizeof(PitchShiftEffect));
    // memset(pitchshiftbkshep_mem, 0, 6 * sizeof(bkshepherd::PitchShifterModule));


    // Instanciation des 6 blocs d'effets en SDRAM
    for (int j = 0; j < 6; j++) {
        earth_effects[j] = new(&earth_mem[j * sizeof(EarthEffect)]) EarthEffect(samplerate);
        delay_effects[j] = new(&delay_mem[j * sizeof(DelayEffect)]) DelayEffect(samplerate);
        drive_effects[j] = new(&drive_mem[j * sizeof(AudioEffectDrive)]) AudioEffectDrive(samplerate);
        // neptune_effects[j] = new(&neptune_mem[j * sizeof(NeptuneEffect)]) NeptuneEffect(hardware.seed, samplerate);
        // pitchshift_effects[j] = new(&pitchshift_mem[j * sizeof(PitchShiftEffect)]) PitchShiftEffect(hardware.seed, samplerate);
        // pitchshiftbkshep_effects[j] = new(&pitchshiftbkshep_mem[j * sizeof(bkshepherd::PitchShifterModule)]) bkshepherd::PitchShifterModule();
        // pitchshiftbkshep_effects[j]->Init(samplerate);
    }

    // Allume la LED en Vert pour prouver que l'allocation a réussi et que la carte n'a pas planté
#if USE_DAISY_POD
        hardware.led1.Set(0.f, 1.f, 0.f); // Vert
        hardware.UpdateLeds();
        System::Delay(500);
        hardware.led1.Set(0.f, 0.f, 0.f); // Éteint
        hardware.UpdateLeds();
#else
        hardware.SetLed(false);
#endif

    hardware.StartAdc();
    hardware.StartAudio(AudioCallback);

    led_state = true;
    last_blink = System::GetNow();

    uint32_t last_ui_update = System::GetNow();

    // Loop forever
    while(1)
    {
        // Met à jour les boutons et LEDs toutes les 1 ms sans bloquer le CPU
        if (System::GetNow() - last_ui_update >= 1) {
            last_ui_update = System::GetNow();
            updateUI();
        }
        
#if CPU_METER
        static uint32_t last_print = System::GetNow();
        if(System::GetNow() - last_print > 1000) {
            last_print = System::GetNow();
            
            float avgLoad = loadMeter.GetAvgCpuLoad();
            float maxLoad = loadMeter.GetMaxCpuLoad();
            

#if !CPU_LoadEffect
            hardware.seed.PrintLine("Charge CPU Moyenne : %d%% | Max : %d%%", 
                        (int)(avgLoad * 100.0f), 
                        (int)(maxLoad * 100.0f));
#else
            // 480 000 ticks correspondent au temps CPU max disponible pour 1 bloc audio (1 ms)
            // On divise nos ticks par ça pour avoir un pourcentage de charge CPU exact de la fonction ciblée
            float specificLoad = ((float)earth_effects[0]->last_profiled_ticks / 480000.0f) * 100.0f;
            hardware.seed.PrintLine("Charge fonction cible : %d%%", (int)specificLoad);
#if CPU_LoadAll
            hardware.seed.PrintLine("Charge CPU Moyenne : %d%% | Max : %d%%", 
                        (int)(avgLoad * 100.0f), 
                        (int)(maxLoad * 100.0f));
#endif

#endif
        }
#endif

#if SD_CARD_DS
        sampler.update();    
#endif

    }
}

#else 
#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include "EffetEarth/Earth.h"
#include "EffetDelay/Delay.h"
#include "EffetDisto/AudioEffectDrive.h"
#include "include/Utils.h"


void OnControlChange(byte channel, byte control, byte value);

#pragma region Objet audios
#if !InputTDM
AudioSynthWaveform       mesOscs[6];       // combinaison d'oscillateurs
#endif

DelayEffect              mesDelays[6];
EarthEffect              EffetEarth[6];
AudioEffectDrive         mesDistos[6];


// Comme la reverb est stéréo, il faut 2 canaux de mixage
AudioMixer4              mixerL_1a4;   
AudioMixer4              mixerL_5et6;  
AudioMixer4              masterL;      

// --- Analyseurs de pic pour le signal des cordes ---
AudioAnalyzePeak         stringPeaks[6];
// -------------------------------------------------------------

AudioMixer4              mixerR_1a4;   
AudioMixer4              mixerR_5et6;  
AudioMixer4              masterR;      

#if OutputUSB
AudioOutputUSB           usbOut;           // Sortie audio de la teensy
#endif

#if InputTDM || OutputTDM
DMAMEM AudioControlCS42448 cs42448_1;      // Contrôleur matériel CS42448
#endif

#if InputTDM
AudioInputTDM            inputTDM;         // Entrée TDM depuis la guitare
#endif

#if InputTDM || OutputTDM
AudioOutputTDM           outputTDM;       
#endif

#pragma endregion

#pragma region Connexions audio 
//Source, Port de Sortie, Desrtination, Port d'Entrée)

#if InputTDM == 0
AudioConnection p_osc_dist0(mesOscs[0], 0, mesDistos[0], 0);
AudioConnection p_osc_dist1(mesOscs[1], 0, mesDistos[1], 0);
AudioConnection p_osc_dist2(mesOscs[2], 0, mesDistos[2], 0);
AudioConnection p_osc_dist3(mesOscs[3], 0, mesDistos[3], 0);
AudioConnection p_osc_dist4(mesOscs[4], 0, mesDistos[4], 0);
AudioConnection p_osc_dist5(mesOscs[5], 0, mesDistos[5], 0);
#else
AudioConnection p_tdm_dist0(inputTDM, 10, mesDistos[0], 0);
AudioConnection p_tdm_dist1(inputTDM, 8,  mesDistos[1], 0);
AudioConnection p_tdm_dist2(inputTDM, 6,  mesDistos[2], 0);
AudioConnection p_tdm_dist3(inputTDM, 4,  mesDistos[3], 0);
AudioConnection p_tdm_dist4(inputTDM, 2,  mesDistos[4], 0);
AudioConnection p_tdm_dist5(inputTDM, 0,  mesDistos[5], 0);
#endif

AudioConnection p_dist_earth0(mesDistos[0], 0, EffetEarth[0], 0);
AudioConnection p_dist_earth1(mesDistos[1], 0, EffetEarth[1], 0);
AudioConnection p_dist_earth2(mesDistos[2], 0, EffetEarth[2], 0);
AudioConnection p_dist_earth3(mesDistos[3], 0, EffetEarth[3], 0);
AudioConnection p_dist_earth4(mesDistos[4], 0, EffetEarth[4], 0);
AudioConnection p_dist_earth5(mesDistos[5], 0, EffetEarth[5], 0);

AudioConnection p_earth_dly0(EffetEarth[0], 0, mesDelays[0], 0);
AudioConnection p_earth_dly1(EffetEarth[1], 0, mesDelays[1], 0);
AudioConnection p_earth_dly2(EffetEarth[2], 0, mesDelays[2], 0);
AudioConnection p_earth_dly3(EffetEarth[3], 0, mesDelays[3], 0);
AudioConnection p_earth_dly4(EffetEarth[4], 0, mesDelays[4], 0);
AudioConnection p_earth_dly5(EffetEarth[5], 0, mesDelays[5], 0);

// Connexions vers le Mixer Droit (Right)
AudioConnection p_dly_mixR0(mesDelays[0], 0, mixerR_1a4, 0);
AudioConnection p_dly_mixR1(mesDelays[1], 0, mixerR_1a4, 1);
AudioConnection p_dly_mixR2(mesDelays[2], 0, mixerR_1a4, 2);
AudioConnection p_dly_mixR3(mesDelays[3], 0, mixerR_1a4, 3);
AudioConnection p_dly_mixR4(mesDelays[4], 0, mixerR_5et6, 0);
AudioConnection p_dly_mixR5(mesDelays[5], 0, mixerR_5et6, 1);

// Connexions vers le Mixer Gauche (Left) pour avoir du Dual Mono
AudioConnection p_dly_mixL0(mesDelays[0], 0, mixerL_1a4, 0);
AudioConnection p_dly_mixL1(mesDelays[1], 0, mixerL_1a4, 1);
AudioConnection p_dly_mixL2(mesDelays[2], 0, mixerL_1a4, 2);
AudioConnection p_dly_mixL3(mesDelays[3], 0, mixerL_1a4, 3);
AudioConnection p_dly_mixL4(mesDelays[4], 0, mixerL_5et6, 0);
AudioConnection p_dly_mixL5(mesDelays[5], 0, mixerL_5et6, 1);


// Mixers sous-groupes dans le Masters
AudioConnection p_mastL1(mixerL_1a4, 0, masterL, 0);
AudioConnection p_mastL2(mixerL_5et6, 0, masterL, 1);
AudioConnection p_mastR1(mixerR_1a4, 0, masterR, 0);
AudioConnection p_mastR2(mixerR_5et6, 0, masterR, 1);

// Masters -> Sorties
#if OutputUSB
AudioConnection p_outL_usb(masterL, 0, usbOut, 0);
AudioConnection p_outR_usb(masterR, 0, usbOut, 1);
#endif
#if OutputTDM
AudioConnection p_outL_tdm(masterL, 0, outputTDM, 14); // Sortie Analogique Gauche
AudioConnection p_outR_tdm(masterR, 0, outputTDM, 12); // Sortie Analogique Droite
#endif
#pragma endregion

unsigned long tempsDerniereNote = 0;
int cordeCourante = 0;
float volumesCordes[6] = {0.00001f, 0.00001f, 0.00001f, 0.00001f, 0.00001f, 0.00001f};
const float frequencesGuitare[6] = {82.41, 110.00, 146.83, 196.00, 246.94, 329.63};

// --- NOUVELLES VARIABLES MUTE ET BYPASS ---
bool Bypass = false;
int effetActif[6] = {0, 0, 0, 0, 0, 0}; // 0 = Aucun, 1 = Delay, 2 = Disto, 3 = Earth
bool stringBypass[6] = {false, false, false, false, false, false};
bool globalBypassState = false;

const int reset_p = 2; 

void setup() {
    pinMode(13, OUTPUT); // NOUVEAU : LED de statut MIDI
    Serial.begin(115200);

    AudioMemory(1500); // on alloue une mémoire suffisante 

    #if InputTDM || OutputTDM
    
    //CTRL_UART.begin(115200);
    
    pinMode(reset_p, OUTPUT);                                    
    //Power-Up Sequence
    digitalWrite(reset_p, LOW);
    delay(800);
    digitalWrite(reset_p, HIGH);

    if (cs42448_1.enable()) {
        Serial.println("configured CS42448");
    } else {
        Serial.println("failed to config CS42448");
    }
    
    cs42448_1.inputLevel(1);
    cs42448_1.volume(1.0);

    #endif

    #pragma region Initialisation des effets et oscillateurs
    for (int i = 0; i < 6; i++) {
        #if !InputTDM
        mesOscs[i].begin(WAVEFORM_TRIANGLE);
        mesOscs[i].amplitude(volumesCordes[i]);
        mesOscs[i].frequency(frequencesGuitare[i]);
        #endif
        
        // Initialisation de la Disto
        mesDistos[i].begin(2048);
        mesDistos[i].setMix(0.0f); // Par défaut bypass
        mesDistos[i].setVolume(0.2f); // On abaisse le volume par défaut pour éviter de percer les tympans
        
        // Initialisation de chaque delay
        #if DelayPaulo
        mesDelays[i].begin(800);
        #else
        mesDelays[i].begin();
        #endif
        mesDelays[i].setMix(0.0f); // Par défaut bypass

        // Initialisation de la Reverb
        EffetEarth[i].setMix(0.0f);
        // --------------------------------------------------------- 
    }
    #pragma endregion

    usbMIDI.setHandleControlChange(OnControlChange); //Active la fonction OnControlChange() des qu'il y a un CC 
}

void loop() {
    unsigned long tempsActuel = millis(); // temps actuel en ms depuis le démarrage du programme
    static unsigned long lastHeartbeat = 0;

    // Heartbeat : Affichage régulier des diagnostics
    // NOUVEAU : Passé de 5ms à 500ms. 5ms saturait le port USB Série et bloquait complètement la Teensy !
    if (tempsActuel - lastHeartbeat >= 500) { 
        lastHeartbeat = tempsActuel;
        Serial.print("Charge CPU Audio Actuelle : ");
        Serial.print(AudioProcessorUsage());
        Serial.println(" %");

        Serial.print("Charge CPU Audio Max : ");
        Serial.print(AudioProcessorUsageMax());
        Serial.println(" %");
        
        digitalWrite(13, !digitalRead(13)); // Clignotement lent
    }

    while (usbMIDI.read()) {}

#if !InputTDM
    if (tempsActuel - tempsDerniereNote >= 400) { // on joue une corde toutes les 400 ms
        tempsDerniereNote = tempsActuel;  
        if (!cordeMute[cordeCourante]) {         // On ne joue la corde QUE si elle n'est pas muette
            volumesCordes[cordeCourante] = 0.3f; 
            mesOscs[cordeCourante].frequency(frequencesGuitare[cordeCourante]);
        }
        cordeCourante = (cordeCourante + 1) % 6; // Modulo 6 pour tourner en boucle
    }
    

    // Gestion du Decay (extinction du son) indépendant pour chaque corde
    for (int i = 0; i < 6; i++) {
        if (volumesCordes[i] > 0.00001f) {
            volumesCordes[i] *= 0.99f; // Extinction lente pour laisser résonner les notes ensemble
            
            if (volumesCordes[i] < 0.00001f) volumesCordes[i] = 0.00001f;
            mesOscs[i].amplitude(volumesCordes[i]);
        }
    }
#endif

    delay(2);

}

void OnControlChange(byte channel, byte control, byte value) {
    float valNorm = value / 127.0f;

    // --- TRANCHE 1 : DELAY (CC 10 à 45) ---
    if (control >= 10 && control <= 45) {
        int ccRelatif = control - 10;
        int corde = ccRelatif / 6;  // Division entière -> Donne la corde (0 à 5)
        int potard = ccRelatif % 6; // Reste -> Donne le bouton (0 à 5)

        if (corde >= 0 && corde < 6) {
            effetActif[corde] = 1; // Mémorise que Delay est l'effet de cette corde
            if (!stringBypass[corde] && !globalBypassState) {
                mesDelays[corde].setEnabled(true);
            }
            // Affichage mouchard dans la console VS Code
            Serial.print("MIDI -> Effet: DELAY | Corde: ");
            Serial.print(corde);
            Serial.print(" | Potard: P");
            Serial.print(potard + 1);
            Serial.print(" | Valeur: ");
            Serial.println(value);

            mesDelays[corde].setParameter(potard, valNorm);
        }
        EffetEarth[corde].setEnabled(false);
        mesDistos[corde].setEnabled(false);
    }
    
    // --- TRANCHE 2 : DISTORTION (CC 50 à 85) ---
    else if (control >= 50 && control <= 85) { 
        int ccRelatif = control - 50;
        int corde = ccRelatif / 6;
        int potard = ccRelatif % 6;

        if (corde >= 0 && corde < 6) {
            effetActif[corde] = 2; // Mémorise que Disto est l'effet de cette corde
            if (!stringBypass[corde] && !globalBypassState) {
                mesDistos[corde].setEnabled(true);
            }   
            // Affichage mouchard dans la console VS Code
            Serial.print("MIDI -> Effet: DISTO | Corde: ");
            Serial.print(corde);
            Serial.print(" | Potard: P");
            Serial.print(potard + 1);
            Serial.print(" | Valeur: ");
            Serial.println(value);

            // Appel de la fonction unifiée
            mesDistos[corde].setParameter(potard, valNorm);
        }
        EffetEarth[corde].setEnabled(false);
        mesDelays[corde].setEnabled(false);
    }

    // --- TRANCHE 3 : EARTH (CC 90 à 125 - Remplace la Reverb) ---
    else if (control >= 90 && control <= 125) {
        int ccRelatif = control - 90;
        int corde = ccRelatif / 6;
        int potard = ccRelatif % 6;


        if (corde >= 0 && corde < 6) {
            effetActif[corde] = 3; // Mémorise que Earth est l'effet de cette corde
            if (!stringBypass[corde] && !globalBypassState) {
                EffetEarth[corde].setEnabled(true);
            }
            // Affichage mouchard dans la console VS Code
            Serial.print("MIDI -> Effet: DELAY | Corde: ");
            Serial.print(corde);
            Serial.print(" | Potard: P");
            Serial.print(potard + 1);
            Serial.print(" | Valeur: ");
            Serial.print(value);
            Serial.print(" | Valeur Normalisé: ");
            Serial.println(valNorm);

            EffetEarth[corde].setParameter(potard, valNorm);
        }
        mesDistos[corde].setEnabled(false);
        mesDelays[corde].setEnabled(false);
    }

    // --- TRANCHE 4 : BYPASS DES CORDES INDIVIDUELLES (CC 0 à 5) ---
    else if (control >= 0 && control <= 5) {
            // La valeur > 63 suppose que 127 = Bypass ON (son coupé) et 0 = Bypass OFF.
        // (Si ton bouton envoie l'inverse pour "Allumer l'effet", remplace par "value < 64")
        bool isBypassed = (value > 63); 
        stringBypass[control] = isBypassed;
        
        if (isBypassed || globalBypassState) {
            mesDistos[control].setEnabled(false);
            mesDelays[control].setEnabled(false);
            EffetEarth[control].setEnabled(false);
        } else {
            // Si on sort du bypass, on réactive le dernier effet utilisé
            if (effetActif[control] == 1) mesDelays[control].setEnabled(true);
            else if (effetActif[control] == 2) mesDistos[control].setEnabled(true);
            else if (effetActif[control] == 3) EffetEarth[control].setEnabled(true);
        }
    }
    
    // --- TRANCHE 5 : BYPASS GLOBAL (CC 126) ---
    else if (control == 126) {
        globalBypassState = (value > 63);
        for (int i = 0; i < 6; i++) {
            if (globalBypassState || stringBypass[i]) {
                mesDistos[i].setEnabled(false);
                mesDelays[i].setEnabled(false);
                EffetEarth[i].setEnabled(false);
            } else {
                if (effetActif[i] == 1) mesDelays[i].setEnabled(true);
                else if (effetActif[i] == 2) mesDistos[i].setEnabled(true);
                else if (effetActif[i] == 3) EffetEarth[i].setEnabled(true);
            }

        }
    }
}
#endif