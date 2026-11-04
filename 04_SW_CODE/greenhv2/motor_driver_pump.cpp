/*
 * motor_driver_pump.cpp
 *
 * Driver Gestion de la pompe via signal PWM (Vitesse variable)
 * V2 : le Timer TIM3_CH2 du STM32 est remplacé par le périphérique LEDC de l'ESP32.
 * Fréquence conservée : 20 kHz (V1 : 16 MHz / (799 + 1)), hors bande audible.
 *
 *  Created on: Nov 27, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026
 */

#include "motor_driver_pump.h"
#include "greenhv2.h"

#define PUMP_PWM_FREQ_HZ     20000  // 20 kHz comme en V1
#define PUMP_PWM_RES_BITS    10     // Résolution 10 bits -> 0..1023
#define PUMP_PWM_MAX_DUTY    ((1 << PUMP_PWM_RES_BITS) - 1)
#define PUMP_PWM_CHANNEL     0      // Canal LEDC (utilisé uniquement avec le core ESP32 2.x)

static bool pump_state = false;   // Mémorise l'état de la pompe


// Ecriture du rapport cyclique brut (0..PUMP_PWM_MAX_DUTY)
// Compatibilité core arduino-esp32 2.x (canaux) et 3.x (broches)
static void pump_write_duty(uint32_t duty)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(PIN_PUMP_PWM, duty);
#else
    ledcWrite(PUMP_PWM_CHANNEL, duty);
#endif
}


//Initialisation de la pompe : direction + PWM
void Pump_Init(void)
{
    // IN4 = Io16 = direction -> LOW (sens normal)
    pinMode(PIN_PUMP_DIR, OUTPUT);
    digitalWrite(PIN_PUMP_DIR, LOW);

    // Lancer le PWM sur IN3 = Io17 avec un duty = 0%
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(PIN_PUMP_PWM, PUMP_PWM_FREQ_HZ, PUMP_PWM_RES_BITS);
#else
    ledcSetup(PUMP_PWM_CHANNEL, PUMP_PWM_FREQ_HZ, PUMP_PWM_RES_BITS);
    ledcAttachPin(PIN_PUMP_PWM, PUMP_PWM_CHANNEL);
#endif
    pump_write_duty(0);

    pump_state = false;
}


//Définit la vitesse de la pompe en % (0-100)
void Pump_SetSpeed(uint8_t duty)
{
    if (duty > 100) duty = 100;

    uint32_t pulse = ((uint32_t)PUMP_PWM_MAX_DUTY * duty) / 100;   // Calcul du duty cycle

    pump_write_duty(pulse);

    pump_state = (duty > 0);
}


// Arrêt de la pompe
void Pump_Stop(void)
{
    pump_write_duty(0);
    pump_state = false;
}


//  Retourne TRUE si la pompe tourne
bool Pump_IsOn(void)
{
    return pump_state;
}
