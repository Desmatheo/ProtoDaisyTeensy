#pragma once

#ifndef USE_DAISY
#define USE_DAISY 0
#endif

#define MODE_HEXAPHONIQUE

#define InputTDM 1    // 1: Entrée Guitare (TDM), 0: Séquenceur d'Oscillateurs
#if InputTDM
#define OutputTDM 1   // 1: Sortie Jack CS42448 (TDM), 0: Désactivé
#endif
#define OutputUSB 0   // 1: Sortie Casque/PC (USB), 0: Désactivé

#define USE_DAISY_POD 0

#if !USE_DAISY_POD  //eviter les conflits
#define USE_DAISY_TDM 1
#endif

#define SD_CARD_DS 0
#define Padding_on 0


#define CPU_METER 1
#define CPU_LoadEffect 0
#define CPU_LoadAll 1


static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}