/*
 * motor_driver_fan.h
 *
 *  Created on: Nov 14, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (API inchangée)
 */

#ifndef MOTOR_DRIVER_FAN_H
#define MOTOR_DRIVER_FAN_H

#include <stdint.h>

// Initialise les GPIO IN1/IN2 du ventilateur (moteur arrêté)
void Motor_Init(void);

//Activation du ventilateur
void Motor_On(void);

//Arrêt du ventilateur
void Motor_Off(void);

//Retour sur l'état du ventilateur (0 = OFF, 1 = ON)
uint8_t Motor_IsOn(void);

#endif /* MOTOR_DRIVER_FAN_H */
