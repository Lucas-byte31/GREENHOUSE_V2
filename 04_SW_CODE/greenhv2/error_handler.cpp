/*
 * error_handler.cpp
 *
 * Ce module assure la surveillance du système
 * Il détecte les pannes matérielles (capteurs débranchés) ou les pannes
 * d'actionneurs (ventilateur bloqué, réservoir vide) par une analyse temporelle
 *
 * Gestion des erreurs si:
 * Ventilateur HS
 *  ou Pompe HS
 *   ou Capteur Température HS
 *    ou Capteur Humidité HS
 *
 * V2 : logique de détection V1 + borne basse de température (sonde débranchée, SR-GH-011 rév. 01).
 *      Module appelé uniquement par la tâche T_DIAG (pas d'accès concurrent à son état interne).
 *      La LED de signalisation est supprimée (décision du 04/11/2026) :
 *      le défaut est signalé par le LCD uniquement (page 1 et page 5).
 *
 *  Created on: Jan 5, 2026
 *      Author: lucas
 *  Migrated on: Nov 4, 2026
 */


#include "error_handler.h"

/* ================== CONFIGURATION DES SEUILS================== */
// Temps d'attente maximum (15s) avant de déclarer une panne si la consigne n'est pas rejointe
#define TEMP_NO_DROP_TIMEOUT_MS     15000

// Seuils de détection de capteur défaillant (valeurs aberrantes)
#define TEMP_MAX_JUMP               80     // °C instantané à confirmer pendant TEMP_NO_DROP_TIMEOUT_MS
#define TEMP_MIN_VALID             -20     // °C - V2 (SR-GH-011 rév. 01) : sonde débranchée -> tension ~0 V -> T ~ -50 °C
#define HUM_MAX_JUMP                98     // % instantané à confirmer pendant TEMP_NO_DROP_TIMEOUT_MS
/* ============================================ */

/* État interne - Init à 0*/
static int last_temp = 0;
static int last_hum  = 0;

static uint32_t temp_timer = 0;
static uint32_t hum_timer  = 0;
static uint32_t error_code = 0;

static bool temp_error = false;
static bool hum_error  = false;

/* ================== API ================== */

// Initialisation de l'error handler
void error_handler_init(int temp_init, int hum_init)
{
    last_temp = temp_init;
    last_hum  = hum_init;
    temp_timer = 0;
    hum_timer  = 0;
    temp_error = false;
    hum_error  = false;
}

//Analyse les mesures et déclenche les alertes si nécessaire.
// Si le système est hors consigne, on lance un timer.
// Si le timer expire sans retour à la normale, on lève une erreur.
void error_handler_update(int temp,
                          int hum,
						  int seuil_temp,
						  int seuil_hum,
                          bool fan_on,
                          bool pump_on,
                          uint32_t now_ms)
{
    (void)fan_on;   // non utilisé (logique V1 conservée)
    (void)pump_on;  // non utilisé (logique V1 conservée)

    /* ---------- Température ne baisse pas malgré la consigne---------- */
    	/* ---------- Capteur de température HS ( >80°C ou < -20°C )---------- */
        // V2 : ajout de la borne basse, en V1 une sonde débranchée était vue comme "température sous la consigne"
        if (temp <= seuil_temp && temp <= TEMP_MAX_JUMP && temp >= TEMP_MIN_VALID) // Si la température est OK (sous le seuil) et cohérente
        {
            temp_timer = now_ms;  // OK → reset timer
            temp_error = false;
        }
        else if ((now_ms - temp_timer) > TEMP_NO_DROP_TIMEOUT_MS) // Si on dépasse le délai de 15s sans retour au seuil
        {
            temp_error = true;
        }


    /* ---------- Humidité n'augmente pas malgré la consigne---------- */
        /* ---------- Capteur d'humidité HS ( >98% )---------- */
        if (hum >= seuil_hum && hum <= HUM_MAX_JUMP) // Si l'humidité est OK (au-dessus du seuil) et cohérente
        {
        	hum_timer = now_ms;  // OK → reset timer
        	hum_error = false;
        }
        else if ((now_ms - hum_timer) > TEMP_NO_DROP_TIMEOUT_MS) // Si on dépasse le délai de 15s sans retour au seuil
        {
        	hum_error = true;
        }

    /* ---------- Historique ---------- */
    last_temp = temp;
    last_hum  = hum;
}

// Generation des codes erreur
// Retourne retval 0: OK, 1: Erreur Temp, 2: Erreur Hum, 3: Double Erreur
int error_handler_has_error(void)
{
	if (!temp_error && !hum_error) error_code = 0;
	else if (temp_error && !hum_error) error_code = 1;
	else if  (!temp_error && hum_error) error_code = 2;
	else if  (temp_error && hum_error) error_code = 3;
    return (error_code);
}

// Sonde de température hors service confirmée (SR-GH-031)
// L'erreur température est confirmée (15 s) ET la dernière valeur est hors de la plage physique
// -> on distingue "sonde HS" (ventilateur forcé ON) de "consigne non rejointe".
bool error_handler_temp_sensor_fault(void)
{
    return temp_error && (last_temp > TEMP_MAX_JUMP || last_temp < TEMP_MIN_VALID);
}
