/*
 * rt_trace.h
 *
 * Instrumentation temps réel pour l'approche WCET (SR-GH-029)
 * Activée par RT_TRACE = 1, sinon toutes les fonctions sont vides (aucun surcoût).
 *
 * Mesures réalisées :
 *  - durée de chaque activation de tâche (min / max / moyenne) avec esp_timer_get_time() (1 µs)
 *  - gigue d'activation de T_ACQ (écart à la période de 100 ms)
 *  - latence appui bouton -> prise en compte par T_IHM, et appui -> écran à jour
 *  - anomalies : file pleine, échec de prise du mutex
 *  - option : basculement d'un GPIO par tâche pour une mesure à l'oscilloscope
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef RT_TRACE_H
#define RT_TRACE_H

#include <stdint.h>

/* ================== CONFIGURATION ================== */
#define RT_TRACE            0       // 1 = instrumentation active (rapport sur le moniteur série)
#define RT_TRACE_GPIO       0       // 1 = un GPIO passe à 1 pendant l'exécution de chaque tâche
#define RT_TRACE_REPORT_MS  10000   // Période d'impression du rapport (par T_DIAG)
#define RT_STIMULUS         0       // 1 = banc de stimulation : la tâche T_STIM injecte des appuis PB1
#define RT_STIMULUS_MS      250     //     toutes les 250 ms (changement de page -> redessin complet du LCD)

// Identifiants des tâches instrumentées
typedef enum
{
    RT_TASK_ACQ = 0,
    RT_TASK_REG,
    RT_TASK_IHM,
    RT_TASK_LCD,
    RT_TASK_DIAG,
    RT_TASK_COUNT
} rt_task_id_t;

void     rt_trace_init(void);
uint32_t rt_trace_begin(rt_task_id_t id);                    // début d'exécution, retourne la date (µs)
void     rt_trace_end(rt_task_id_t id, uint32_t t_begin);    // fin d'exécution
void     rt_trace_activation(rt_task_id_t id, uint32_t period_us); // gigue d'activation (tâche périodique)
void     rt_trace_press(uint32_t t_press_us);                // appui pris en compte par T_IHM
void     rt_trace_screen(void);                              // écran mis à jour par T_LCD
void     rt_trace_queue_full(void);
void     rt_trace_mutex_timeout(void);
void     rt_trace_report(void);                              // impression du rapport (T_DIAG)

#endif /* RT_TRACE_H */
