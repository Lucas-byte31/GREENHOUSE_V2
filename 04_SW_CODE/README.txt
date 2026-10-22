===========================================================
PROJET GREENHOUSE - SYSTÈME DE RÉGULATION d'une serre
Equipe FIPC S3E: L.JOUFFREAU en solo
===========================================================

1. PRÉSENTATION DU PROJET
-------------------------
Ce projet implémente un système de contrôle-commande pour une serre automatisée.
Il assure la gestion d'un ventilateur (température) et d'une pompe à eau (humidité)
via des lois de commande Tout-Ou-Rien (avec hystérésis) et Proportionnelle (PWM).

2. ARCHITECTURE LOGICIELLE
---------------------------------------------

[Cœur du système]
- main.c/.h             : Boucle principale, configuration des périphériques 
                          (ADC, PWM, GPIO), gestion des interruptions
						  et séquence d'initialisation.
- error_handler.c/.h    : Chien de garde (Watchdog) logiciel. Détecte les pannes
                          matérielles et gère la signalisation d'erreur.

[Traitement Capteurs]
- capteurs.c/.h         : Fonctions de transfert (Lois physiques) pour convertir
                          les données brutes ADC en °C et %.
- filter.c/.h           : Algorithme de moyenne glissante pour stabiliser les
                          mesures face au bruit électronique.

[Interface Utilisateur (HMI)]
- lcd.c/.h              : Driver pour écran LCD HD44780U 2x16 (Mode 4 bits).
- lcd_page_manager.c/.h : Logique de navigation entre les 5 pages (menus) du LCD
						  et gestion du rafraîchissement optimisé de l'écran.

[Actionneurs]
- motor_driver_fan.c/.h : Driver Pilotage du ventilateur (ON/OFF).
- motor_driver_pump.c.h : Driver Gestion de la pompe via signal PWM
						  (Vitesse variable).

3. LOGIQUES DE RÉGULATION
-------------------------
- TEMPÉRATURE : Seuil ajustable avec hystérésis de 0.5°C pour protéger le moteur.
- HUMIDITÉ    : Asservissement par rampe linéaire (PWM) pour éviter l'overshoot
               et garantir une approche douce de la consigne.

4. INSTALLATION & COMPILATION
-----------------------------
- IDE : STM32CubeIDE v1.13.0 (ou supérieur)
- SDK (development KIT) : HAL STM32G0
- Câblage : Se référer au rapport technique pour le mappage GPIO.