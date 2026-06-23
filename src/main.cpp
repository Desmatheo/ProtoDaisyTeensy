#include "include/main.h"

#if USE_DAISY

#if SD_CARD_DS 
#include "include/WavHexaPlayer.h"
#endif

#include <new> // Nécessaire pour le "placement new"
#include <cstring> // Nécessaire pour memset


#pragma region initiallisation  des variables

using namespace daisy;


// Allocation dans l'espace mémoire
alignas(EarthEffect) static uint8_t DSY_SDRAM_BSS       earth_mem[6 * sizeof(EarthEffect)];
alignas(DelayEffect) static uint8_t DSY_SDRAM_BSS       delay_mem[6 * sizeof(DelayEffect)];
alignas(AudioEffectDrive) static uint8_t DSY_SDRAM_BSS  drive_mem[6 * sizeof(AudioEffectDrive)];

#pragma endregion

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
#elif USE_DAISY_TDM

    hardware.Init(true);

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

    auto blocksize = DaisyTdmSlave::kBlockSize;

#if !USE_DAISY_TDM
    samplerate = hardware.AudioSampleRate();
    hardware.SetAudioBlockSize(48); // LIMITE LIBDAISY : la taille max est de 128. 48 est sûr et multiple de 6.
#else 
    samplerate = DaisyTdmSlave::kSampleRate;
    hardware.seed.SetAudioBlockSize(blocksize);
#endif

#if CPU_METER
    // Initialisation du module de mesure CPU
    hardware.seed.StartLog(true);
    loadMeter.Init(samplerate, blocksize);
#endif

    // La SDRAM n'est pas mise à zéro par défaut. On la vide pour éviter des bruits stridents dans la reverb.
    memset(earth_mem, 0, 6 * sizeof(EarthEffect));
    memset(delay_mem, 0, 6 * sizeof(DelayEffect));
    memset(drive_mem, 0, 6 * sizeof(AudioEffectDrive));


    // Instanciation des 6 blocs d'effets en SDRAM
    for (int j = 0; j < 6; j++) {
        earth_effects[j] = new(&earth_mem[j * sizeof(EarthEffect)]) EarthEffect(samplerate);
        delay_effects[j] = new(&delay_mem[j * sizeof(DelayEffect)]) DelayEffect(samplerate);
        drive_effects[j] = new(&drive_mem[j * sizeof(AudioEffectDrive)]) AudioEffectDrive(samplerate);
    }

    // Allume la LED en Vert pour prouver que l'allocation a réussi et que la carte n'a pas planté
#if USE_DAISY_POD
        hardware.led1.Set(0.f, 1.f, 0.f); // Vert
        hardware.UpdateLeds();
        System::Delay(500);
        hardware.led1.Set(0.f, 0.f, 0.f); // Éteint
        hardware.UpdateLeds();
#else
        hardware.seed.SetLed(false);
#endif

#if USE_DAISY_POD
    hardware.StartAdc();
#else 
    // hardware.StartAdc();
#endif

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
            float specificLoad = ((float)earth_effects[0]->last_profiled_ticks / samplerate) * 100.0f;
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

#else // Use Teensy

#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include "EffetEarth/Earth.h"
#include "EffetDelay/Delay.h"
#include "EffetDisto/AudioEffectDrive.h"
#include "include/Utils.h"
#include "EffetTempBypass/Bypass.h"

#pragma region Objet audios
#if !InputTDM
AudioSynthWaveform       mesOscs[6];       // combinaison d'oscillateurs
#endif

DelayEffect              mesDelays[6];
EarthEffect              EffetEarth[6];
AudioEffectDrive         mesDistos[6];
BypassEffect             MesBypass[6];


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
AudioConnection p_tdm_bypas1(inputTDM, 10, MesBypass[0], 0);
AudioConnection p_tdm_bypas2(inputTDM, 8,  MesBypass[1], 0);
AudioConnection p_tdm_bypas3(inputTDM, 6,  MesBypass[2], 0);
AudioConnection p_tdm_bypas4(inputTDM, 4,  MesBypass[3], 0);
AudioConnection p_tdm_bypas5(inputTDM, 2,  MesBypass[4], 0);
AudioConnection p_tdm_bypas6(inputTDM, 0,  MesBypass[5], 0);
#endif

AudioConnection p_tdm_dist0(MesBypass[0], 0,  mesDistos[0], 0);
AudioConnection p_tdm_dist1(MesBypass[1], 0,  mesDistos[1], 0);
AudioConnection p_tdm_dist2(MesBypass[2], 0,  mesDistos[2], 0);
AudioConnection p_tdm_dist3(MesBypass[3], 0,  mesDistos[3], 0);
AudioConnection p_tdm_dist4(MesBypass[4], 0,  mesDistos[4], 0);
AudioConnection p_tdm_dist5(MesBypass[5], 0,  mesDistos[5], 0);

AudioConnection p_tdm_oct0(mesDistos[0], 0,  EffetEarth[0], 0);
AudioConnection p_tdm_oct1(mesDistos[1], 0,  EffetEarth[1], 0);
AudioConnection p_tdm_oct2(mesDistos[2], 0,  EffetEarth[2], 0);
AudioConnection p_tdm_oct3(mesDistos[3], 0,  EffetEarth[3], 0);
AudioConnection p_tdm_oct4(mesDistos[4], 0,  EffetEarth[4], 0);
AudioConnection p_tdm_oct5(mesDistos[5], 0,  EffetEarth[5], 0);

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

void OnControlChange(byte channel, byte control, byte value);

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
        if (!stringBypass[cordeCourante]) {      // On ne joue la corde QUE si elle n'est pas muette
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
    #pragma region Control MIDI
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


        int ccRelatif = control - 90;
        int corde = ccRelatif / 6;
        int potard = ccRelatif % 6;
        
        // Affichage mouchard dans la console VS Code
        Serial.print("MIDI -> Effet: DELAY | Corde: ");
        Serial.print(corde);
        Serial.print(" | Potard: P");
        Serial.print(potard + 1);
        Serial.print(" | Valeur: ");
        Serial.print(value);
        Serial.print(" | Valeur Normalisé: ");
        Serial.println(valNorm);

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
    #pragma endregion
}
#endif