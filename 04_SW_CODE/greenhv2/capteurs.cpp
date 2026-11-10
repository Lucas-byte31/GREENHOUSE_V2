/*
 * capteurs.cpp
 *
 * Ce module définit les interfaces de conversion pour les capteurs analogiques
 * du système Greenhouse (TMP235 pour la température et Grove 101020614 pour l'humidité).
 *
 * V2 : sur ESP32 l'ADC n'est pas linéaire, la lecture se fait directement en mV
 * avec analogReadMilliVolts() (courbe d'étalonnage usine de l'ESP32).
 * Les fonctions de transfert reçoivent donc une tension et non plus des points ADC.
 *
 *  Created on: Nov 27, 2025
 *      Author: paul
 *  Migrated on: Nov 4, 2026
 *      Author: lucas
 */

#include "capteurs.h"
#include <Arduino.h>


// Convertit la tension mesurée en température Celsius.
// Basé sur les spécifications du capteur TMP235.
// voltage_mv  : Tension lue sur la broche (mV).
// v_offset_mv : Décalage de tension mesuré au multimètre (étalonnage).
// Retourne la Température en degrés Celsius (°C).
float CAPTEURS_ComputeTemperature(uint32_t voltage_mv, float v_offset_mv)
{
    // Correction offset
    // Permet de compenser les chutes de tension dans les câbles ou l'erreur de l'ADC.
    float voltage_corr = (float)voltage_mv - v_offset_mv;
    if (voltage_corr < 0.0f)
        voltage_corr = 0.0f;

    //Application de la courbe caractéristique du TMP235
    // Loi TMP235 : T = (Vcorr - 500 mV) / 10 mV/°C
    float temperature = (voltage_corr - 500.0f) / 10.0f;

    return temperature;
}


// Convertit la tension mesurée en pourcentage d'humidité.
// Capteur capacitif (Grove 101020614). La tension diminue quand l'humidité augmente.
// voltage_mv : Tension lue sur la broche (mV).
// air_mv     : Tension lue à l'air sec (étalonnage min).
// water_mv   : Tension lue plongé dans l'eau (étalonnage max).
// Retourne Humidité relative en pourcentage (0 à 100%).
float CAPTEURS_ComputeHumidity(uint32_t voltage_mv, float air_mv, float water_mv)
{
    float Air   = air_mv;
    float Water = water_mv;

    // securite : eviter division par 0
    if (Air == Water)
        return 0.0f;

    // Formule d'interpolation linéaire :
    // Plus la tension est proche de 'Water', plus le pourcentage est élevé.
    // H(%) = (Air - V) / (Air - Water) * 100
    float H = (Air - (float)voltage_mv) * 100.0f / (Air - Water);

    // Saturation : empêche d'afficher des valeurs incohérentes (<0 ou >100)
    // dues aux variations de température ou de salinité du sol.
    if (H < 0.0f)   H = 0.0f;
    if (H > 100.0f) H = 100.0f;

    return H;
}


/* Lecture d un canal ADC1 par polling, retourne la tension en mV */
// Pas besoin d'interruption pour l'ADC car les mesures sont lentes (température et humidité)
// analogReadMilliVolts() applique la courbe d'étalonnage usine (eFuse) de l'ADC ESP32
// Sur-échantillonnage : moyenne de n_samples conversions pour réduire le bruit de l'ADC
uint32_t CAPTEURS_ReadMilliVolts(uint8_t pin, int n_samples)
{
    if (n_samples < 1) n_samples = 1;

    uint32_t sum = 0;
    for (int i = 0; i < n_samples; i++)
    {
        sum += analogReadMilliVolts(pin);
    }
    return sum / (uint32_t)n_samples;   // tension moyenne en mV
}
