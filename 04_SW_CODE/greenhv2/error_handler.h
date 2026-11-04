/*
 * error_handler.h
 *
 *  Created on: Jan 5, 2026
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (API inchangée)
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/* Initialisation */
void error_handler_init(int temp_init, int hum_init);

/* Mise à jour périodique */
void error_handler_update(int temp,
                          int hum,
						  int seuil_temp,
						  int seuil_hum,
                          bool fan_on,
                          bool pump_on,
                          uint32_t now_ms);

/* État global erreur */
int error_handler_has_error(void);

#endif /* ERROR_HANDLER_H */
