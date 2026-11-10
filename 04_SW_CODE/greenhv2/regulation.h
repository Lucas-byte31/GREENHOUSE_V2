/*
 * regulation.h
 *
 * Lois de commande de la serre (extraites de la boucle principale V1)
 *  - Ventilateur : Tout-Ou-Rien avec hystérésis (SR-GH-006)
 *  - Pompe       : rampe PWM en fonction de H / Hcons (SR-GH-007)
 *  - Mode manuel : page 4 (SR-GH-012)
 *  - Mise en sécurité : prioritaire sur l'automatique et le manuel (SR-GH-031)
 *
 * Ce module est appelé par la seule tâche T_REG : c'est elle qui possède les actionneurs.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef REGULATION_H
#define REGULATION_H

#include <stdint.h>
#include <stdbool.h>

// Entrées de la régulation (instantané préparé par T_REG)
typedef struct
{
    float temperature;          // Température mesurée (°C)
    float humidity;             // Humidité mesurée (%)
    int   seuil_temperature;    // Consigne de température
    int   seuil_humidite;       // Consigne d'humidité
    bool  manual_mode;          // true si la page 4 est affichée
    bool  manual_fan;           // Demande manuelle ventilateur
    bool  manual_pump;          // Demande manuelle pompe
    bool  pump_fault;           // Défaut pompe confirmé (code erreur 2 ou 3)
    bool  temp_sensor_fault;    // Sonde de température hors service confirmée
} regulation_input_t;

// Calcul des coefficients de la rampe PWM
void Regulation_Init(void);

// Calcule puis applique les commandes du ventilateur et de la pompe
void Regulation_Step(const regulation_input_t *in);

#endif /* REGULATION_H */
