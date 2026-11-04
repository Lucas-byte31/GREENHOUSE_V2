===========================================================
PROJET GREENHOUSE V2 - SYSTÈME DE RÉGULATION d'une serre
Equipe FIPC S3E : L. JOUFFREAU (solo)
===========================================================

1. PRÉSENTATION DU PROJET
-------------------------
Ce projet implémente un système de contrôle-commande pour une serre automatisée.
Il assure la gestion d'un ventilateur (température) et d'une pompe à eau (humidité)
via des lois de commande Tout-Ou-Rien (avec hystérésis) et Proportionnelle (PWM).
La V2 porte la V1 (STM32G031 / STM32CubeIDE) sur ESP32 (Arduino IDE) puis introduit
le temps réel avec FreeRTOS (cf. 02_SW_SPEC/System_spec_v2.docx).

2. ARCHITECTURE LOGICIELLE (greenhv2/)
---------------------------------------------

[Cœur du système]
- greenhv2.ino          : Séquence d'initialisation (setup) et boucle principale (loop).
                          Remplace main.c. Gestion des interruptions des boutons.
- greenhv2.h            : Mapping GPIO ESP32 et structure lcd_state_t (ex main.h).
- error_handler.cpp/.h  : Diagnostic fonctionnel. Détecte les pannes capteurs et
                          actionneurs (consigne non rejointe en 15 s, valeurs aberrantes).

[Traitement Capteurs]
- capteurs.cpp/.h       : Fonctions de transfert (Lois physiques) pour convertir
                          les tensions mesurées (mV) en °C et %.
- filter.cpp/.h         : Algorithme de moyenne glissante pour stabiliser les
                          mesures face au bruit électronique.

[Interface Utilisateur (HMI)]
- lcd.cpp/.h            : Driver LCD 2x16 I2C (PCF8574, adresse 0x27). Couche
                          d'adaptation sur la librairie LCD_I2C (Blackhat) qui
                          conserve l'API de la V1.
- lcd_page_manager.cpp/.h : Logique de navigation entre les 5 pages (menus) du LCD
                          et gestion du rafraîchissement optimisé de l'écran.

[Actionneurs]
- motor_driver_fan.cpp/.h  : Driver Pilotage du ventilateur (ON/OFF).
- motor_driver_pump.cpp/.h : Driver Gestion de la pompe via signal PWM LEDC 20 kHz
                          (Vitesse variable).

3. LOGIQUES DE RÉGULATION
-------------------------
- TEMPÉRATURE : Seuil ajustable avec hystérésis de 0.5°C pour protéger le moteur.
- HUMIDITÉ    : Asservissement par rampe linéaire (PWM) pour éviter l'overshoot
               et garantir une approche douce de la consigne.

4. INSTALLATION & COMPILATION
-----------------------------
- IDE : Arduino IDE 2.x
- Carte : core "esp32" by Espressif (2.x ou 3.x supportés), carte "ESP32 Dev Module"
- Librairie : LCD_I2C by Blackhat (gestionnaire de bibliothèques)
- Ouvrir greenhv2/greenhv2.ino (le dossier et le .ino doivent avoir le même nom)
- Câblage : 02_SW_SPEC/Schema de cablage.pptx (référence)
  Température Io35, Humidité Io34, PB1/PB2/PB3 Io18/Io19/Io23,
  Ventilateur IN1/IN2 Io26/Io25, Pompe IN3 (PWM)/IN4 Io17/Io16, LCD SDA 21 / SCL 22.
- Alimentation : ESP32 par USB, carte moteur par 5 VDC (masse commune).

5. ÉTALONNAGE
-------------
Passer GREENHV2_DEBUG à 1 dans greenhv2.ino, ouvrir le moniteur série (115200) :
- V_OFFSET_MV    : écart entre la température affichée et un thermomètre (x 10 mV/°C)
- AIR_VALUE_MV   : tension du capteur d'humidité à l'air sec
- WATER_VALUE_MV : tension du capteur d'humidité plongé dans l'eau
Remettre GREENHV2_DEBUG à 0 ensuite.

6. HISTORIQUE
-------------
- V1   : STM32G031K8, STM32CubeIDE, LCD parallèle 4 bits, super-boucle.
- V2.0 : migration ESP32 iso-V1 (super-boucle dans loop()) - référence de non-régression.
         Écart assumé vs V1 : acquisition cadencée à 100 ms + sur-échantillonnage ADC x16
         (l'ADC ESP32 est plus bruité et la boucle plus rapide : affichage instable sinon).
         Correctifs V1 : code erreur rafraîchi à chaque tour (et plus seulement sur la page 5),
         détection de la sonde de température débranchée (T < -20 °C), cf. SR-GH-011 rév. 01.
         Consigne d'humidité par défaut : 5 % (V1 : 18 %), cf. SR-GH-007.
- V2.1 : (à venir) découpage en tâches FreeRTOS, synchronisation, mesures WCET.
