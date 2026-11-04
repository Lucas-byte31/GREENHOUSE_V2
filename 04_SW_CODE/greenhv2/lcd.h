/*
 * lcd.h
 *
 * Driver LCD 2x16 - V2 : couche d'adaptation au-dessus de la librairie LCD_I2C (Blackhat)
 * On conserve l'API de la V1 (lcd_init, lcd_clear, lcd_show_txt...) afin que
 * le lcd_page_manager reste inchangé : seul le driver bas niveau change (4 bits -> I2C).
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void lcd_init(void);                                   // Init bus I2C + écran + rétroéclairage
void lcd_clear(void);                                  // Efface l'écran, curseur en (0,0)
void lcd_position_cursor(uint8_t col, uint8_t row);    // COL (0-15) et row (0-1)
void lcd_show_carac(char c);                           // Affiche un caractère (code HD44780)
void lcd_show_txt(const char *str);                    // Affiche une chaîne à la position courante
void lcd_print(uint8_t col, uint8_t row, const char *text); // Affiche à une position donnée
void lcd_print_int(int value);                         // Affiche un entier

#endif /* LCD_H */
