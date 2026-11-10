/*
 * buttons.cpp
 *
 * Gestion des boutons poussoirs par interruption.
 *
 * Choix V2 (SR-GH-025) :
 *  - Un anti-rebond PAR bouton (V1 : horodatage commun aux 3 boutons).
 *  - Détection à l'APPUI (front descendant, boutons actifs à l'état bas) pour réduire la latence.
 *  - L'interruption est armée sur les DEUX fronts (CHANGE) : chaque front, y compris les
 *    rebonds, met à jour la date du dernier front. Un appui n'est validé que si :
 *      1. la broche est à l'état bas (bouton enfoncé),
 *      2. la ligne était stable (aucun front) depuis BTN_STABLE_MS -> rejette les rebonds
 *         de l'appui ET ceux du relâchement (qui contiennent aussi des fronts descendants),
 *      3. le dernier appui validé date d'au moins BTN_MIN_INTERVAL_MS (200 ms comme en V1).
 *  - Bouton maintenu enfoncé : la ligne reste à l'état bas, il n'y a plus aucun front,
 *    donc plus aucune interruption -> un seul événement par appui, pas de répétition.
 *
 * L'ISR est courte (quelques µs) : elle dépose l'événement dans la file q_boutons
 * (xQueueSendFromISR) et demande un changement de contexte si T_IHM doit préempter
 * la tâche en cours (portYIELD_FROM_ISR).
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "buttons.h"
#include "greenhv2.h"
#include "esp_timer.h"

#define BTN_STABLE_MS          30     // Durée de stabilité exigée avant un appui (rebonds < 10 ms typ.)
#define BTN_MIN_INTERVAL_MS    200    // Intervalle mini entre 2 appuis validés d'un même bouton (V1 : 200 ms)

static QueueHandle_t q_boutons = NULL;

// Broches associées à chaque bouton
static const uint8_t btn_pin[BTN_COUNT] = { PIN_PB1, PIN_PB2, PIN_PB3 };

// Etat de l'anti-rebond, un par bouton (accédé uniquement depuis l'ISR du bouton)
static volatile uint32_t last_edge_us[BTN_COUNT]   = {0, 0, 0};  // date du dernier front (appui, relâchement ou rebond)
static volatile uint32_t last_accept_us[BTN_COUNT] = {0, 0, 0};  // date du dernier appui validé


// Traitement commun aux 3 interruptions
static void IRAM_ATTR button_isr(button_id_t id)
{
    uint32_t now = (uint32_t)esp_timer_get_time();     // horloge µs
    uint32_t since_edge = now - last_edge_us[id];
    last_edge_us[id] = now;                             // tout front compte, y compris les rebonds

    // 1. Bouton enfoncé ?  2. Ligne stable avant ce front ?  3. Intervalle mini respecté ?
    if (digitalRead(btn_pin[id]) != LOW) return;
    if (since_edge < (BTN_STABLE_MS * 1000UL)) return;
    if ((now - last_accept_us[id]) < (BTN_MIN_INTERVAL_MS * 1000UL)) return;

    last_accept_us[id] = now;

    // Dépôt de l'événement dans la file (jamais bloquant en ISR)
    button_event_t evt = { id, now };
    BaseType_t higher_prio_woken = pdFALSE;
    xQueueSendFromISR(q_boutons, &evt, &higher_prio_woken);

    // Si T_IHM est plus prioritaire que la tâche interrompue : commutation dès la sortie de l'ISR
    if (higher_prio_woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

// IRAM_ATTR : les routines sont placées en RAM interne
static void IRAM_ATTR PB1_ISR(void) { button_isr(BTN_PB1); }
static void IRAM_ATTR PB2_ISR(void) { button_isr(BTN_PB2); }
static void IRAM_ATTR PB3_ISR(void) { button_isr(BTN_PB3); }


// Configuration des boutons poussoirs et des interruptions
// Appelée depuis setup() : les interruptions sont donc servies par le cœur 1 (comme les tâches)
void Buttons_Init(QueueHandle_t queue)
{
    q_boutons = queue;

    // Boutons actifs à l'état bas : pull-up interne
    pinMode(PIN_PB1, INPUT_PULLUP);
    pinMode(PIN_PB2, INPUT_PULLUP);
    pinMode(PIN_PB3, INPUT_PULLUP);

    // Interruption sur les deux fronts (voir en-tête : validation de l'appui dans l'ISR)
    attachInterrupt(digitalPinToInterrupt(PIN_PB1), PB1_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_PB2), PB2_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_PB3), PB3_ISR, CHANGE);
}
