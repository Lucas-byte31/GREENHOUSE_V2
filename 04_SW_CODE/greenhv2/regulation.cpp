/*
 * regulation.cpp
 *
 * Lois de commande de la serre - logique identique à la boucle principale V1,
 * restructurée en 3 temps : décision -> mise en sécurité -> application.
 * Les commandes ne sont appliquées qu'une seule fois par cycle (pas d'impulsion parasite
 * de la pompe entre la loi automatique et la mise en sécurité).
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "regulation.h"
#include "motor_driver_fan.h"
#include "motor_driver_pump.h"

#define FAN_HYSTERESIS   0.5f   // Hystérésis du ventilateur (°C)

//--Calcul de la rampe linéaire à l'approche du seuil d'humidité--//
static const float ramp_x1 = 0.55f;  // 0.55*seuil_humidite → speed = 100
static const float ramp_y1 = 100.0f;
static const float ramp_x2 = 1.0f;   // 1.0*seuil_humidite → speed = 50
static const float ramp_y2 = 50.0f;
// Coefficients m et b de la droite y = mx + b
static float m = 0.0f;
static float b = 0.0f;


// Calcul des coefficients m et b de la droite y = mx + b
void Regulation_Init(void)
{
    m = (ramp_y2 - ramp_y1) / (ramp_x2 - ramp_x1);
    b = ramp_y1 - m * ramp_x1;
}


// Calcule puis applique les commandes du ventilateur et de la pompe
void Regulation_Step(const regulation_input_t *in)
{
    bool    fan_cmd    = (Motor_IsOn() != 0);   // par défaut on garde l'état courant (hystérésis)
    uint8_t pump_speed = 0;                     // vitesse de la pompe (0-100) en %

    if (in->manual_mode)
    {
        // ===================== Mode Manuel (page 4) =====================
        // Les consignes automatiques ne sont pas prises en compte
        fan_cmd    = in->manual_fan;
        pump_speed = in->manual_pump ? 100 : 0;     // pompe manuelle à 100% PWM
    }
    else
    {
        // ===================== Asservissement de la Ventilation en Automatique =====================
        if (in->temperature >= in->seuil_temperature)
        {
            fan_cmd = true;
        }
        else if (in->temperature < (in->seuil_temperature - FAN_HYSTERESIS))
            // petite hysteresis pour éviter que
            // le ventilateur démarre et s'arrête en permanence quand temperature oscille sur seuil_temperature
        {
            fan_cmd = false;
        }

        // ===================== Asservissement de la Pompe en Automatique =====================
        // Asservissement linéaire
        //Plus l'humidité est éloignée de sa consigne, plus la pompe tourne à haute vitesse
        //Plus l'humidité s'approche de sa valeur consigne, moins la pompe tourne

        // Calcul du ratio humidité courante / consigne (consigne à 0 % -> ratio infini -> pompe arrêtée)
        float humidity_ratio = (in->seuil_humidite > 0) ? (in->humidity / (float)in->seuil_humidite) : 1000.0f;

        if (humidity_ratio <= ramp_x1)          // Zone 1 : Besoin critique (100% PWM)
        {
            pump_speed = 100;
        }
        else if (humidity_ratio <= ramp_x2)     // Zone 2 : Régulation douce (Approche)
        {
            pump_speed = (uint8_t)(m * humidity_ratio + b);   // Rampe linéaire
        }
        else                                    // Zone 3 : Consigne atteinte
        {
            pump_speed = 0;
        }
    }

    // ===================== Mise en sécurité (SR-GH-031) - prioritaire =====================
    if (in->pump_fault)
    {
        pump_speed = 0;     // protection marche à sec / inondation : pompe maintenue arrêtée
    }
    if (in->temp_sensor_fault)
    {
        fan_cmd = true;     // sonde HS : état sûr côté thermique = ventilation
    }

    // ===================== Application des commandes =====================
    if (fan_cmd) Motor_On();
    else         Motor_Off();

    Pump_SetSpeed(pump_speed);      // 0 % -> pompe arrêtée (Pump_IsOn() = false)
}
