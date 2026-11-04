/*
 * lcd_page_manager.h
 *
 *  Created on: Dec 30, 2025
 *      Author: lucas
 *  Migrated on: Nov 4, 2026 (inchangé)
 */

#ifndef LCD_PAGE_MANAGER_H
#define LCD_PAGE_MANAGER_H

#include "greenhv2.h"

/* -------------------------------------------------------------------------- */
/*                    Fonctions publiques LCD PAGE MANAGER                    */
/* -------------------------------------------------------------------------- */

void lcd_page_1(lcd_state_t *curr, lcd_state_t *prev);
void lcd_page_2(lcd_state_t *curr, lcd_state_t *prev);
void lcd_page_3(lcd_state_t *curr, lcd_state_t *prev);
void lcd_page_4(lcd_state_t *curr, lcd_state_t *prev);
void lcd_page_5(lcd_state_t *curr, lcd_state_t *prev);

#endif /* LCD_PAGE_MANAGER_H */
