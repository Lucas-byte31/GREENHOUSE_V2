/*
 * greenhv2.h
 *
 * Définitions communes du projet GREENHOUSE V2 (équivalent du main.h de la V1)
 * - Mapping des GPIO de l'ESP32 (cf. Schema de cablage.pptx V2)
 * - Structure de l'état du LCD partagée entre le séquenceur et le lcd_page_manager
 * - Type des événements de la file q_reg (V2.1 FreeRTOS)
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef GREENHV2_H
#define GREENHV2_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/* ================== MAPPING GPIO ESP32 (Keyestudio ESP32 Plus) ================== */
// Capteurs analogiques - ADC1 uniquement (l'ADC2 est partagé avec le WiFi)
#define PIN_TEMPERATURE     35  // TMP235 - SIG -> Io35
#define PIN_HUMIDITE        34  // Grove 101020614 - OUT -> Io34

// Boutons poussoirs (module bouton, actif à l'état bas avec INPUT_PULLUP)
#define PIN_PB1             18  // Bouton rouge - navigation dans les menus
#define PIN_PB2             19  // Bouton blanc - Incrémentation consigne ou Forçage ventilateur
#define PIN_PB3             23  // Bouton vert  - Décrémentation consigne ou Forçage pompe

// Carte moteur SBC-MotorDriver2
#define PIN_MOTOR_IN1       26  // Ventilateur IN1
#define PIN_MOTOR_IN2       25  // Ventilateur IN2
#define PIN_PUMP_PWM        17  // Pompe IN3 - signal PWM (vitesse)
#define PIN_PUMP_DIR        16  // Pompe IN4 - direction (LOW = sens normal)

// Ecran LCD 2x16 I2C (PCF8574) - bus I2C par défaut de l'ESP32 : SDA = 21, SCL = 22
#define LCD_I2C_ADDRESS     0x27
#define LCD_COLUMNS         16
#define LCD_ROWS            2


/* ================== STRUCTURE DE L'ETAT DU LCD ================== */
//Definition de la structure de l'etat du LCD
//On viendra la comparer avec l'etat precedent pour voir si on a besoin de rafraîchir la page
typedef struct
{
    int page;
    int t_filt;
    int h_filt;
    int seuil_temperature;
    int seuil_humidite;
    int code_erreur;
    bool pump_on;
    bool fan_on;
} lcd_state_t;

/* ================== EVENEMENTS DE LA REGULATION ================== */
// Message déposé dans la file q_reg pour réveiller la tâche T_REG
typedef enum
{
    EVT_MESURE = 0,     // Nouvelle mesure publiée par T_ACQ (toutes les 100 ms)
    EVT_COMMANDE        // Changement de page ou commande manuelle par T_IHM
} reg_event_type_t;

typedef struct
{
    reg_event_type_t type;
    float temperature;  // Valeurs valides uniquement pour EVT_MESURE
    float humidity;
    int   t_filt;
    int   h_filt;
} reg_event_t;

#endif /* GREENHV2_H */
