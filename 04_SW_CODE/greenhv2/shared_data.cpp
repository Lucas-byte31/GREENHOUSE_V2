/*
 * shared_data.cpp
 *
 * Protection des données partagées par un MUTEX FreeRTOS (et non un sémaphore binaire) :
 *  - le mutex a un propriétaire : seule la tâche qui l'a pris peut le rendre ;
 *  - il implémente l'héritage de priorité : si T_REG (prio 5) attend le mutex détenu par
 *    T_LCD (prio 3), T_LCD hérite temporairement de la priorité 5 et ne peut pas être
 *    préemptée par T_IHM (prio 4) -> l'inversion de priorité est bornée.
 *
 * Règle de codage : sous mutex on ne fait QUE copier des données (quelques µs),
 * jamais d'accès I2C, ADC ou Serial.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "shared_data.h"
#include "rt_trace.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t mtx_data = NULL;   // Mutex de protection de g_data
static greenhouse_data_t g_data;            // Données partagées (privées au module)


// Prise du mutex avec délai max - comptabilise les échecs pour l'analyse temps réel
static bool lock(void)
{
    if (xSemaphoreTake(mtx_data, pdMS_TO_TICKS(SHARED_DATA_MUTEX_TIMEOUT_MS)) == pdTRUE)
        return true;

    rt_trace_mutex_timeout();   // ne doit jamais arriver si les sections critiques restent courtes
    return false;
}

// Libération du mutex par la tâche propriétaire
static void unlock(void)
{
    xSemaphoreGive(mtx_data);
}


// Création du mutex et initialisation des valeurs par défaut
bool shared_data_init(int seuil_temperature, int seuil_humidite)
{
    mtx_data = xSemaphoreCreateMutex();   // mutex = héritage de priorité
    if (mtx_data == NULL) return false;

    memset(&g_data, 0, sizeof(g_data));
    g_data.page = 1;                                // page affichée au démarrage
    g_data.seuil_temperature = seuil_temperature;
    g_data.seuil_humidite    = seuil_humidite;
    g_data.t_filt = seuil_temperature;              // comme en V1 : filtre initialisé aux consignes
    g_data.h_filt = seuil_humidite;
    g_data.temperature = (float)seuil_temperature;
    g_data.humidity    = (float)seuil_humidite;
    return true;
}

// Lecture d'un instantané cohérent de toutes les données
bool shared_data_get(greenhouse_data_t *copy)
{
    if (!lock()) return false;
    *copy = g_data;     // copie de la structure complète (~50 octets)
    unlock();
    return true;
}

// Ecriture des mesures (T_REG)
bool shared_data_set_mesures(float temperature, float humidity, int t_filt, int h_filt)
{
    if (!lock()) return false;
    g_data.temperature = temperature;
    g_data.humidity    = humidity;
    g_data.t_filt      = t_filt;
    g_data.h_filt      = h_filt;
    unlock();
    return true;
}

// Ecriture des paramètres IHM (T_IHM)
bool shared_data_set_ihm(int page, int seuil_temperature, int seuil_humidite, bool manual_fan, bool manual_pump)
{
    if (!lock()) return false;
    g_data.page              = page;
    g_data.seuil_temperature = seuil_temperature;
    g_data.seuil_humidite    = seuil_humidite;
    g_data.manual_fan        = manual_fan;
    g_data.manual_pump       = manual_pump;
    unlock();
    return true;
}

// Ecriture de l'état des actionneurs (T_REG)
bool shared_data_set_actionneurs(bool fan_on, bool pump_on)
{
    if (!lock()) return false;
    g_data.fan_on  = fan_on;
    g_data.pump_on = pump_on;
    unlock();
    return true;
}

// Ecriture du diagnostic (T_DIAG)
bool shared_data_set_diag(int code_erreur, bool temp_sensor_fault)
{
    if (!lock()) return false;
    g_data.code_erreur       = code_erreur;
    g_data.temp_sensor_fault = temp_sensor_fault;
    unlock();
    return true;
}
