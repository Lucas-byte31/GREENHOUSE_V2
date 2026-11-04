/*
 * filter.h
 *
 *  Created on: Jan 5, 2026
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (inchangé, portable)
 */

#ifndef FILTER_H
#define FILTER_H

#define FILTER_MAX_SIZE 8 // Taille max du buffer = 8 échantillons

//Structure utilisée pour faire une moyenne glissante dans un filtre numérique
typedef struct
{
    int buffer[FILTER_MAX_SIZE];
    int index;
    int sum;
    int size;
} moving_avg_filter_t;

// Prototype des fonctions
void filter_init(moving_avg_filter_t *f, int size, int init_value);
int filter_update(moving_avg_filter_t *f, int new_value);

#endif /* FILTER_H */
