/*
 * motor_driver_fan.cpp
 *
 * Driver Pilotage du ventilateur (ON/OFF) via la carte SBC-MotorDriver2 (IN1/IN2)
 *
 *  Created on: Nov 14, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (HAL_GPIO_WritePin -> digitalWrite)
 */

// Include de motor_driver
#include "motor_driver_fan.h"
#include "greenhv2.h"

// Global data declaration et affectation
// Etat du moteur: OFF =0 et ON =1
static uint8_t G_MotorState = 0;

// Initialisation des GPIO du ventilateur
// V2 : sur STM32 cette configuration était générée par CubeMX (MX_GPIO_Init)
void Motor_Init(void)
{
    pinMode(PIN_MOTOR_IN1, OUTPUT);
    pinMode(PIN_MOTOR_IN2, OUTPUT);
    Motor_Off();
}

//Activation du ventilateur
void Motor_On(void)
{
    // Rotation du Moteur : IN1 = 1 et IN2 = 0
    digitalWrite(PIN_MOTOR_IN1, HIGH);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    G_MotorState = 1;
}

//Arrêt du ventilateur
void Motor_Off(void)
{
    // Arrêt du Moteur : IN1 = 0 et IN2 = 0
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    G_MotorState = 0;
}

//Retour sur l'état du ventilateur
uint8_t Motor_IsOn(void)
{
    return G_MotorState;
}
