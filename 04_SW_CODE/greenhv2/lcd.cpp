/*
 * lcd.cpp
 *
 * Driver LCD 2x16 I2C (module PCF8574) basé sur la librairie LCD_I2C de Blackhat.
 * Remplace le driver parallèle 4 bits de la V1 (lcd.c) en conservant la même API.
 *
 * Remarque temps réel : la librairie attend la fin de chaque commande par
 * delayMicroseconds() (attente active) : ~1,6 ms pour un clear, ~40 µs par caractère
 * en plus de la trame I2C. Ces durées seront mesurées à l'étape WCET.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "lcd.h"
#include "greenhv2.h"
#include <Wire.h>
#include <LCD_I2C.h>

// Instance unique de l'écran (adresse I2C 0x27, 16 colonnes, 2 lignes)
static LCD_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);


// Initialise l'écran LCD : démarre le bus I2C (Wire) puis la séquence d'init HD44780
void lcd_init(void)
{
    lcd.begin();       // Wire.begin() + init mode 4 bits via PCF8574
    lcd.backlight();   // Rétroéclairage ON
}

// Efface l'écran complet et repositionne le curseur en (0,0)
void lcd_clear(void)
{
    lcd.clear();
}

// Positionne le curseur à une coordonnée précise. COL (0-15) et row (0-1)
void lcd_position_cursor(uint8_t col, uint8_t row)
{
    lcd.setCursor(col, row);
}

// Affiche un caractère ASCII (ou un code HD44780 : 0xDF = °, 0xFF = carré plein)
void lcd_show_carac(char c)
{
    lcd.write((uint8_t)c);
}

// Affiche une chaîne de caractères à la position actuelle
void lcd_show_txt(const char *str)
{
    lcd.print(str);
}

// Affiche un texte à une position ligne / colonne
void lcd_print(uint8_t col, uint8_t row, const char *text)
{
    lcd.setCursor(col, row);
    lcd.print(text);
}

// Affiche un entier sur le LCD (la classe Print gère la conversion, sans sprintf)
void lcd_print_int(int value)
{
    lcd.print(value);
}
