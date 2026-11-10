/*
 * buttons.h
 *
 * Gestion des boutons poussoirs par interruption (SR-GH-025)
 * L'ISR ne fait que déposer un événement dans la file q_boutons ; le traitement
 * est fait par la tâche T_IHM dans le contexte de la page active.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BUTTONS_QUEUE_LENGTH   8    // Profondeur de la file d'événements boutons

// Identifiant du bouton
typedef enum
{
    BTN_PB1 = 0,    // Bouton rouge - navigation dans les menus
    BTN_PB2,        // Bouton blanc - Incrémentation consigne ou Forçage ventilateur
    BTN_PB3,        // Bouton vert  - Décrémentation consigne ou Forçage pompe
    BTN_COUNT
} button_id_t;

// Evénement déposé par l'ISR dans la file
typedef struct
{
    button_id_t id;     // Quel bouton
    uint32_t    t_us;   // Date de l'appui (µs) - sert à mesurer la latence appui -> prise en compte
} button_event_t;

// Configure les GPIO et les interruptions, les événements seront envoyés dans 'queue'
void Buttons_Init(QueueHandle_t queue);

#endif /* BUTTONS_H */
