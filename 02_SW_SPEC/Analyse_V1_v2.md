# Analyse de GREENHOUSE V1 – base de la migration V2

*Étape 1 – 04/11/2026. Sources : 04_SW_CODE (Src/Inc), System_spec, Rapport technique, NOTICE, Note de cadrage, Matériel.xlsx, schémas de câblage V1/V2.*

## 1. Architecture logicielle V1

| Module | Rôle | Remarque pour la V2 |
|---|---|---|
| `main.c` | Init HAL, super-boucle `while(1)` : acquisition → filtre → régulation → IHM (fenêtre 500 ms) → diagnostic. ISR boutons (EXTI), `ReadADC_Channel`, `LED_BLINK` | Devient `greenhv2.ino` : séquencement puis création des tâches |
| `capteurs.c` | Fonctions de transfert TMP235 et capteur d'humidité | Portable, à passer en mV (`analogReadMilliVolts`) |
| `filter.c` | Moyenne glissante, 8 échantillons, entiers | Portable tel quel |
| `lcd.c` | Driver HD44780 parallèle 4 bits | **Supprimé** : remplacé par LCD_I2C (Blackhat) |
| `lcd_page_manager.c` | 5 menus, redessin uniquement si `lcd_state_t` a changé (`memcmp`) | Portable, appels LCD à adapter |
| `motor_driver_fan.c` | Ventilateur TOR (IN1/IN2) | Portable (GPIO 26/25) |
| `motor_driver_pump.c` | Pompe PWM (TIM3_CH2) + direction (IN4) | PWM → `ledcAttach`/`ledcWrite` (IN3 = GPIO 17, IN4 = GPIO 16) |
| `error_handler.c` | Diagnostic : consigne non rejointe en 15 s, valeurs aberrantes | Portable, à corriger (voir §3) |

Style de code à conserver : un module `.c/.h` par fonction, API préfixée (`Pump_`, `Motor_`, `filter_`, `error_handler_`, `lcd_page_`), commentaires en français, bandeaux `// ===== Titre =====`, en-tête de fichier avec rôle, auteur et date, commentaire par ligne non triviale.

## 2. Comportement de référence (non-régression)

- Consignes par défaut : T = 25 °C (0 à 51), H = 18 % (0 à 100), pas de 1.
- Ventilateur : ON si T ≥ Tcons, OFF si T < Tcons − 0,5 °C (valeur **non filtrée**).
- Pompe : r = H/Hcons ; r ≤ 0,55 → 100 % ; r ≤ 1 → −111,1·r + 161,1 ; r > 1 → 0 % (valeur **non filtrée**).
- Affichage : moyenne glissante sur 8 échantillons, puis bande morte de 1 °C / 2 %, rafraîchissement toutes les 500 ms si changement.
- Menu 4 : BP2 bascule le ventilateur, BP3 bascule la pompe à 100 % ; la régulation automatique est suspendue tant que le menu 4 est affiché.
- Diagnostic : erreur T si T > Tcons ou T > 80 pendant 15 s ; erreur H si H < Hcons ou H > 98 pendant 15 s ; codes 0/1/2/3 ; LED qui change d'état toutes les 200 ms.

## 3. Constats techniques (arguments pour le temps réel)

1. **Pas de période d'exécution.** La boucle tourne « aussi vite que possible » : la période d'échantillonnage dépend de la durée de l'affichage. Le filtre sur 8 échantillons n'a donc pas de constante de temps définie. → SR-GH-021
2. **Attente active dans le driver LCD.** `lcd_wait_ms()` est commentée « non bloquante » mais boucle sur `HAL_GetTick()` : environ 4 ms de CPU par caractère, soit plusieurs dizaines de ms par page redessinée, pendant lesquelles aucune régulation n'est faite. → SR-GH-027
3. **Latence des boutons.** Les appuis + / − et le mode manuel ne sont traités que dans la fenêtre LCD de 500 ms. → SR-GH-023
4. **Événements fantômes.** Les drapeaux PB2/PB3 ne sont pas consommés sur les menus 1 et 5 : un appui fait sur le menu 1 modifie la consigne à l'arrivée sur le menu 2. Anti-rebond de 200 ms commun aux trois boutons. Callback sur front montant, c'est-à-dire au relâchement avec le pull-up. → SR-GH-025
5. **Diagnostic.** Les paramètres `fan_on`/`pump_on` ne sont pas utilisés (faux défaut possible en mode manuel). Une sonde TMP235 débranchée donne −50 °C, ce qui est vu comme « température OK » : défaut non détecté. Pas de mise en sécurité malgré la notice. Les variables `last_temp` et `*_error_reset_timer` ne servent pas. → SR-GH-011, SR-GH-031
6. **Incohérence de données.** La régulation utilise les mesures brutes, le diagnostic et l'affichage les mesures filtrées (tronquées en entier).
7. **« Watchdog »** : c'est un diagnostic fonctionnel, pas un chien de garde matériel. → SR-GH-030

## 4. Incohérences documentaires V1 (à ne pas reconduire)

- Rapport : température sur PA0 / humidité sur PA1 et PWM sur TIM16_CH1/PB4 ; code : humidité sur PA0, température sur PA1, PWM sur TIM3_CH2/PB5.
- Capteur d'humidité nommé SEN0193 dans le code et le rapport, 101020614 dans les références. Le **Grove 101020614 est capacitif** (NE555) : la loi « tension qui baisse quand l'humidité monte » reste valable.
- Notice : pompe manuelle à 99 % (code : 100 %) ; LED à « 5 fois par seconde » (code : changement d'état toutes les 200 ms, soit 2,5 Hz) ; « mise en sécurité » non implémentée.
- Rapport §7.4.1 : « zone d'approche entre 55 % et 50 % » (incohérent avec la loi implémentée).
- L'état de l'art V1 présente l'absence d'Arduino comme un atout : le rapport V2 doit justifier le choix Arduino-ESP32 (couche fine sur ESP-IDF, FreeRTOS natif, API FreeRTOS utilisée directement). → SR-GH-013

## 5. Brochage V2 (d'après Schema de cablage.pptx, référence)

| Fonction | GPIO ESP32 |
|---|---|
| Capteur température TMP235 | 35 (ADC1) |
| Capteur humidité Grove | 34 (ADC1) |
| PB1 / PB2 / PB3 | 18 / 19 / 23 |
| Ventilateur IN1 / IN2 | 26 / 25 |
| Pompe IN3 (PWM) / IN4 (direction) | 17 / 16 |
| LCD I2C | SDA 21 / SCL 22 (connecteur I2C de la carte) |

LED de diagnostic : supprimée en V2 (décision du 04/11/2026). Points à vérifier : la tension VCC des connecteurs capteurs (3,3 V ou 5 V) ; les pull-ups I2C du module LCD à 5 V.
