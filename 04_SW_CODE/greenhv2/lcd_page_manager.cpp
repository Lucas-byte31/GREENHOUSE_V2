/*
 * lcd_page_manager.cpp
 *
 * Logique de navigation entre les 5 pages (menus) du LCD
 * et gestion du rafraîchissement optimisé de l'écran.
 *
 *  Created on: Dec 30, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (logique inchangée, seul le driver lcd est passé en I2C)
 */
#include "lcd_page_manager.h"
#include "lcd.h"
#include "error_handler.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                          Fonctions internes (static)                       */
/* -------------------------------------------------------------------------- */
// Booleen pour savoir s'il faut rafraichir l'ecran LCD
static bool lcd_data_changed(lcd_state_t *a, lcd_state_t *b)
{
	// Compare les deux structures current et previous - retourne 1 si elles sont différentes
	// memcmp permet de comparer 2 structures par exemple, octet par octet
    return memcmp(a, b, sizeof(lcd_state_t)) != 0;
}

/* -------------------------------------------------------------------------- */
/*                    Fonctions publiques LCD PAGE MANAGER                    */
/* -------------------------------------------------------------------------- */

// Page principale
void lcd_page_1(lcd_state_t *curr, lcd_state_t *prev)
			// -------- PAGE 1 --------
			// Temperature     x°C
			// Humidite        x%
{
	// Si la comparaison current/previous est egale alors on ne fait rien
    if (!lcd_data_changed(curr, prev)) return;
    // Sinon on met à jour l'écran
    lcd_clear();

    lcd_position_cursor(0, 0);
    lcd_show_txt("Temp: ");
    lcd_print_int(curr->t_filt);
    lcd_show_carac(0xDF); // Symbole °
    lcd_show_carac('C'); //C pour Celcius


    lcd_position_cursor(0, 1);
    lcd_show_txt("Hum: ");
    lcd_print_int(curr->h_filt);
    lcd_show_carac('%');

    	// Print de l'etat du systeme OK ou KO affiché sur la page 1 uniquement
    	if (error_handler_has_error() == 0)
    	{
    	lcd_position_cursor(15, 0);
    	lcd_show_carac('O'); // Symbole O
    	lcd_position_cursor(15, 1);
    	lcd_show_carac('K'); // Symbole K
    	} else {
        lcd_position_cursor(15, 0);
        lcd_show_carac(0xFF); // Symbole carré plein
        lcd_position_cursor(15, 1);
        lcd_show_carac(0xFF); // Symbole carré plein
    	}

    *prev = *curr; // met à jour l'état précédent
}

// Régulation température
void lcd_page_2(lcd_state_t *curr, lcd_state_t *prev)
			// -------- PAGE 2 --------
			// Temperature     x°C
			// Seuil           x°C
{
    if (!lcd_data_changed(curr, prev)) return;
    // Sinon on met à jour l'écran
    lcd_clear();

    lcd_position_cursor(0, 0);
    lcd_show_txt("Temp: ");
    lcd_print_int(curr->t_filt);
    lcd_show_carac(0xDF); // Symbole °
    lcd_show_carac('C'); //C pour Celcius

    lcd_position_cursor(0, 1);
    lcd_show_txt("Seuil: ");
    lcd_print_int(curr->seuil_temperature);
    lcd_show_carac(0xDF);
    lcd_show_carac('C');

    *prev = *curr; // met à jour l'état précédent
}

// Régulation humidité
void lcd_page_3(lcd_state_t *curr, lcd_state_t *prev)
			// -------- PAGE 3 --------
			// Humidite        x%
			// Seuil           x%
{
    if (!lcd_data_changed(curr, prev)) return;

    lcd_clear();
    // Sinon on met à jour l'écran
    lcd_position_cursor(0, 0);
    lcd_show_txt("Hum: ");
    lcd_print_int(curr->h_filt);
    lcd_show_carac('%');

    lcd_position_cursor(0, 1);
    lcd_show_txt("Seuil: ");
    lcd_print_int(curr->seuil_humidite);
    lcd_show_carac('%');

    *prev = *curr; // met à jour l'état précédent
}

// Activation manuelle
void lcd_page_4(lcd_state_t *curr, lcd_state_t *prev)
			// -------- PAGE 4 --------
			// Pompe       ON/OFF
			// Ventilateur ON/OFF
{
    if (!lcd_data_changed(curr, prev)) return;
    // Sinon on met à jour l'écran
    lcd_clear();

    lcd_position_cursor(0, 0);
    lcd_show_txt("Pompe: ");
    lcd_show_txt(curr->pump_on ? "ON " : "OFF");

    lcd_position_cursor(0, 1);
    lcd_show_txt("Ventil: ");
    lcd_show_txt(curr->fan_on ? "ON " : "OFF");

    *prev = *curr; // met à jour l'état précédent
}

// Maintenance
void lcd_page_5(lcd_state_t *curr, lcd_state_t *prev)
			// -------- PAGE 5 --------
			// SYSTEME OK
			// FAN SYSTEM OK/KO
			// PUMP SYSTEM OK/KO
{
    if (!lcd_data_changed(curr, prev)) return;
    // Sinon on met à jour l'écran
    	lcd_clear();


    	switch (curr->code_erreur)
    	{
    		case 0: // Tout est ok
    			lcd_position_cursor(0, 0);
    			lcd_show_txt("LE SYSTEME");
    			lcd_position_cursor(0, 1);
    			lcd_show_txt("FONCTIONNE BIEN");

    		break;

    		case 1: // FAN KO et Pompe OK
    			lcd_position_cursor(0, 0);
    			lcd_show_txt("FAN SYSTEM KO");
    			lcd_position_cursor(0, 1);
    			lcd_show_txt("PUMP SYSTEM OK");

    		break;

    		case 2: // FAN OK et Pompe KO
    			lcd_position_cursor(0, 0);
    			lcd_show_txt("FAN SYSTEM OK");
    			lcd_position_cursor(0, 1);
    			lcd_show_txt("PUMP SYSTEM KO");

    		break;

    		case 3: // FAN KO et Pompe KO
    			lcd_position_cursor(0, 0);
    			lcd_show_txt("FAN SYSTEM KO");
    			lcd_position_cursor(0, 1);
    			lcd_show_txt("PUMP SYSTEM KO");

    		break;
    	}

    *prev = *curr; // met à jour l'état précédent
}
