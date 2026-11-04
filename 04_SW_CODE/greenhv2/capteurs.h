/*
 * capteurs.h
 *
 *  Created on: Nov 27, 2025
 *      Author: paul
 *  Migrated on: Nov 4, 2026 (ESP32 - entrées en mV)
 *      Author: lucas
 */

#ifndef CAPTEURS_H
#define CAPTEURS_H

#include <stdint.h>

/**
 * @brief Calcule la temperature en °C a partir de la tension mesurée.
 * @param voltage_mv : tension lue par analogReadMilliVolts() (mV)
 * @param v_offset_mv: offset a retirer (en mV), étalonnage
 * @return           : temperature en degres Celsius (float)
 *
 * Formule utilisee (TMP235 : 10 mV/°C, 500 mV à 0 °C) :
 *   Vcorr = max(V - v_offset, 0)
 *   T(°C) = (Vcorr - 500) / 10     <=> T = 100 * Vcorr(V) - 50 (formule V1)
 */
float CAPTEURS_ComputeTemperature(uint32_t voltage_mv, float v_offset_mv);

/**
 * @brief Calcule l'humidite en % a partir de la tension mesurée.
 * @param voltage_mv : tension lue par analogReadMilliVolts() (mV)
 * @param air_mv     : tension mesurée en air sec (0 %)
 * @param water_mv   : tension mesurée dans l'eau (100 %)
 * @return           : humidite en %, saturee entre 0 et 100
 *
 * Formule (capteur capacitif Grove 101020614, la tension diminue quand l'humidite augmente) :
 *   H(%) = (Air - V) / (Air - Water) * 100
 */
float CAPTEURS_ComputeHumidity(uint32_t voltage_mv, float air_mv, float water_mv);

#endif // CAPTEURS_H
