/*
 * filter.cpp
 *
 * Ce module implémente un filtre passe-bas de type Moyenne Glissante.
 * Il lisse les mesures des capteurs Température/Humidité en atténuant
 * les variations brusques et le bruit de numérisation de l'ADC.
 *
 * Created on: Jan 5, 2026
 * Author: lucas
 * Migrated on: Nov 4, 2026 (algorithme inchangé, ajout d'une saturation de size)
 */


#include "filter.h"

// Initialisation du Filtre numérique avec valeur de départ
// f: Pointeur vers la structure du filtre
// size: Nombre d'achantillons (saturé à FILTER_MAX_SIZE pour ne pas déborder du buffer)
// init_value : valeur initiale
void filter_init(moving_avg_filter_t *f, int size, int init_value)
{
    if (size > FILTER_MAX_SIZE) size = FILTER_MAX_SIZE;
    if (size < 1) size = 1;

    f->size = size;
    f->index = 0;
    f->sum = init_value * size;

    // On remplit le buffer avec la valeur initiale pour stabiliser le filtre immédiatement
    for (int i = 0; i < size; i++)
        f->buffer[i] = init_value;
}

// Filtre numérique à moyenne glissante
// f: Pointeur vers la structure du filtre
// new_value: Mesure brute venant de l'ADC
int filter_update(moving_avg_filter_t *f, int new_value)
{
	// Soustraire la valeur la plus ancienne du cumul (celle qui va être écrasée)
    f->sum -= f->buffer[f->index];

    // Enregistrer la nouvelle valeur dans le buffer
    f->buffer[f->index] = new_value;

    // Ajouter la nouvelle valeur au cumul
    f->sum += new_value;

    // Incrémenter l'indice et boucler s'il dépasse la taille du buffer
    f->index++;
    if (f->index >= f->size)
        f->index = 0;

    // Retour de la moyenne
    return f->sum / f->size;
}
