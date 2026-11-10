/*
 * shared_data.h
 *
 * Données partagées entre les tâches FreeRTOS (SR-GH-026).
 * La structure g_data n'est jamais accédée directement : on passe par les fonctions
 * ci-dessous qui prennent / rendent le mutex mtx_data.
 *
 * Règle "un seul écrivain par champ" :
 *  - Mesures      : écrites par T_REG (à réception d'une mesure de T_ACQ)
 *  - IHM          : écrites par T_IHM (page, consignes, demandes manuelles)
 *  - Actionneurs  : écrits par T_REG (état réel du ventilateur et de la pompe)
 *  - Diagnostic   : écrit par T_DIAG (code erreur, défaut sonde de température)
 *  Toutes les tâches peuvent lire une copie cohérente (instantané) avec shared_data_get().
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>
#include <stdbool.h>

// Délai max d'attente du mutex : une tâche ne reste jamais bloquée indéfiniment
#define SHARED_DATA_MUTEX_TIMEOUT_MS   10

typedef struct
{
    /* ---------- Mesures (T_REG) ---------- */
    float temperature;      // Température non filtrée (°C) - utilisée par la régulation (comme en V1)
    float humidity;         // Humidité non filtrée (%)
    int   t_filt;           // Température filtrée (moyenne glissante) - diagnostic / affichage
    int   h_filt;           // Humidité filtrée

    /* ---------- IHM (T_IHM) ---------- */
    int   page;                 // Page LCD affichée (1 à 5) - page 4 = mode manuel
    int   seuil_temperature;    // Consigne de température (0..51 °C)
    int   seuil_humidite;       // Consigne d'humidité (0..100 %)
    bool  manual_fan;           // Demande manuelle ventilateur (page 4)
    bool  manual_pump;          // Demande manuelle pompe (page 4)

    /* ---------- Actionneurs (T_REG) ---------- */
    bool  fan_on;           // Etat réel du ventilateur
    bool  pump_on;          // Etat réel de la pompe

    /* ---------- Diagnostic (T_DIAG) ---------- */
    int   code_erreur;          // 0: OK, 1: Erreur Temp, 2: Erreur Hum, 3: Double Erreur
    bool  temp_sensor_fault;    // Sonde de température hors service confirmée (SR-GH-031)
} greenhouse_data_t;


// Création du mutex et initialisation des valeurs par défaut
bool shared_data_init(int seuil_temperature, int seuil_humidite);

// Lecture d'un instantané cohérent de toutes les données (copie sous mutex)
bool shared_data_get(greenhouse_data_t *copy);

// Ecritures, une fonction par écrivain
bool shared_data_set_mesures(float temperature, float humidity, int t_filt, int h_filt);
bool shared_data_set_ihm(int page, int seuil_temperature, int seuil_humidite, bool manual_fan, bool manual_pump);
bool shared_data_set_actionneurs(bool fan_on, bool pump_on);
bool shared_data_set_diag(int code_erreur, bool temp_sensor_fault);

#endif /* SHARED_DATA_H */
