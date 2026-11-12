/*
 * rt_trace.cpp
 *
 * Instrumentation temps réel (voir rt_trace.h).
 *
 * Concurrence : chaque tâche n'écrit que dans ses propres statistiques. La lecture par
 * T_DIAG et les compteurs communs sont protégés par une section critique courte
 * (portENTER_CRITICAL / spinlock) car ces fonctions peuvent être appelées depuis des
 * tâches de priorités différentes et le rapport doit lire des valeurs cohérentes.
 *
 * la durée mesurée entre rt_trace_begin et rt_trace_end
 * inclut les éventuelles préemptions par des tâches plus prioritaires. Pour T_ACQ
 * (priorité max) c'est directement son temps d'exécution C ; pour les autres tâches
 * c'est un temps de réponse depuis le démarrage, qui majore C.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "rt_trace.h"

#if RT_TRACE

#include <Arduino.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// GPIO de trace (libres sur le schéma de câblage) - un par tâche
static const uint8_t trace_pin[RT_TASK_COUNT] = { 4, 5, 13, 14, 27 };
static const char   *task_name[RT_TASK_COUNT] = { "T_ACQ", "T_REG", "T_IHM", "T_LCD", "T_DIAG" };

typedef struct
{
    uint32_t n;          // nombre d'activations
    uint32_t c_min;      // durée min (µs)
    uint32_t c_max;      // durée max (µs)
    uint64_t c_sum;      // cumul pour la moyenne
    uint32_t last_act;   // date de la dernière activation (µs)
    uint32_t jitter_max; // gigue d'activation max (µs)
} rt_stat_t;

static rt_stat_t stats[RT_TASK_COUNT];
static uint32_t  press_lat_max  = 0;   // appui -> prise en compte (µs)
static uint32_t  screen_lat_max = 0;   // appui -> écran à jour (µs)
static uint32_t  last_press_us  = 0;   // date du dernier appui pris en compte (0 = aucun en attente)
static uint32_t  queue_full_cnt = 0;
static uint32_t  mutex_to_cnt   = 0;

static portMUX_TYPE trace_mux = portMUX_INITIALIZER_UNLOCKED;   // spinlock de la section critique


void rt_trace_init(void)
{
    for (int i = 0; i < RT_TASK_COUNT; i++)
    {
        stats[i].n = 0;
        stats[i].c_min = UINT32_MAX;
        stats[i].c_max = 0;
        stats[i].c_sum = 0;
        stats[i].last_act = 0;
        stats[i].jitter_max = 0;
#if RT_TRACE_GPIO
        pinMode(trace_pin[i], OUTPUT);
        digitalWrite(trace_pin[i], LOW);
#endif
    }
}

uint32_t rt_trace_begin(rt_task_id_t id)
{
#if RT_TRACE_GPIO
    digitalWrite(trace_pin[id], HIGH);
#endif
    return (uint32_t)esp_timer_get_time();
}

void rt_trace_end(rt_task_id_t id, uint32_t t_begin)
{
    uint32_t c = (uint32_t)esp_timer_get_time() - t_begin;
#if RT_TRACE_GPIO
    digitalWrite(trace_pin[id], LOW);
#endif
    portENTER_CRITICAL(&trace_mux);
    rt_stat_t *s = &stats[id];
    s->n++;
    s->c_sum += c;
    if (c < s->c_min) s->c_min = c;
    if (c > s->c_max) s->c_max = c;
    portEXIT_CRITICAL(&trace_mux);
}

// Gigue = |intervalle entre deux activations - période nominale|
void rt_trace_activation(rt_task_id_t id, uint32_t period_us)
{
    uint32_t now = (uint32_t)esp_timer_get_time();
    portENTER_CRITICAL(&trace_mux);
    rt_stat_t *s = &stats[id];
    if (s->last_act != 0)
    {
        int32_t delta = (int32_t)(now - s->last_act) - (int32_t)period_us;
        uint32_t jitter = (delta < 0) ? (uint32_t)(-delta) : (uint32_t)delta;
        if (jitter > s->jitter_max) s->jitter_max = jitter;
    }
    s->last_act = now;
    portEXIT_CRITICAL(&trace_mux);
}

void rt_trace_press(uint32_t t_press_us)
{
    uint32_t lat = (uint32_t)esp_timer_get_time() - t_press_us;
    portENTER_CRITICAL(&trace_mux);
    if (lat > press_lat_max) press_lat_max = lat;
    last_press_us = t_press_us;      // l'écran devra se mettre à jour suite à cet appui
    portEXIT_CRITICAL(&trace_mux);
}

void rt_trace_screen(void)
{
    uint32_t now = (uint32_t)esp_timer_get_time();
    portENTER_CRITICAL(&trace_mux);
    if (last_press_us != 0)
    {
        uint32_t lat = now - last_press_us;
        if (lat > screen_lat_max) screen_lat_max = lat;
        last_press_us = 0;
    }
    portEXIT_CRITICAL(&trace_mux);
}

void rt_trace_queue_full(void)
{
    portENTER_CRITICAL(&trace_mux);
    queue_full_cnt++;
    portEXIT_CRITICAL(&trace_mux);
}

void rt_trace_mutex_timeout(void)
{
    portENTER_CRITICAL(&trace_mux);
    mutex_to_cnt++;
    portEXIT_CRITICAL(&trace_mux);
}

// Impression du rapport - appelée par T_DIAG (priorité la plus basse) toutes les RT_TRACE_REPORT_MS
void rt_trace_report(void)
{
    static uint32_t last_report_ms = 0;
    uint32_t now_ms = millis();
    if ((now_ms - last_report_ms) < RT_TRACE_REPORT_MS) return;
    last_report_ms = now_ms;

    // Copie cohérente sous section critique, impression hors section critique
    rt_stat_t copy[RT_TASK_COUNT];
    uint32_t p_lat, s_lat, qf, mto;
    portENTER_CRITICAL(&trace_mux);
    memcpy(copy, stats, sizeof(stats));
    p_lat = press_lat_max; s_lat = screen_lat_max; qf = queue_full_cnt; mto = mutex_to_cnt;
    portEXIT_CRITICAL(&trace_mux);

    Serial.printf("\r\n===== RT_TRACE @ %lu ms =====\r\n", (unsigned long)now_ms);
    Serial.printf("%-7s %8s %8s %8s %8s %10s\r\n", "Tache", "N", "Cmin_us", "Cmoy_us", "Cmax_us", "Gigue_us");
    for (int i = 0; i < RT_TASK_COUNT; i++)
    {
        uint32_t moy = copy[i].n ? (uint32_t)(copy[i].c_sum / copy[i].n) : 0;
        Serial.printf("%-7s %8lu %8lu %8lu %8lu %10lu\r\n", task_name[i],
                      (unsigned long)copy[i].n,
                      (unsigned long)(copy[i].n ? copy[i].c_min : 0),
                      (unsigned long)moy,
                      (unsigned long)copy[i].c_max,
                      (unsigned long)copy[i].jitter_max);
    }
    Serial.printf("Latence appui->prise en compte max : %lu us\r\n", (unsigned long)p_lat);
    Serial.printf("Latence appui->ecran a jour max    : %lu us\r\n", (unsigned long)s_lat);
    Serial.printf("File pleine : %lu | Mutex timeout : %lu\r\n", (unsigned long)qf, (unsigned long)mto);
}

#else   /* RT_TRACE == 0 : fonctions vides, aucun surcoût */

void     rt_trace_init(void) {}
uint32_t rt_trace_begin(rt_task_id_t id) { (void)id; return 0; }
void     rt_trace_end(rt_task_id_t id, uint32_t t_begin) { (void)id; (void)t_begin; }
void     rt_trace_activation(rt_task_id_t id, uint32_t period_us) { (void)id; (void)period_us; }
void     rt_trace_press(uint32_t t_press_us) { (void)t_press_us; }
void     rt_trace_screen(void) {}
void     rt_trace_queue_full(void) {}
void     rt_trace_mutex_timeout(void) {}
void     rt_trace_report(void) {}

#endif
