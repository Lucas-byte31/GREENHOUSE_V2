/*
 * motor_driver_pump.h
 *
 *  Created on: Nov 27, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (API inchangée)
 */

#ifndef MOTOR_DRIVER_PUMP_H
#define MOTOR_DRIVER_PUMP_H

#include <stdint.h>
#include <stdbool.h>

// Initialise la pompe (PWM + Direction)
void Pump_Init(void);

// Définit la vitesse en % (0–100)
void Pump_SetSpeed(uint8_t duty);

// Arrête totalement la pompe
void Pump_Stop(void);

// Retourne l'état (ON/OFF)
bool Pump_IsOn(void);

#endif /* MOTOR_DRIVER_PUMP_H */
