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

2. ARCHITECTURE LOGICIELLE (greenhv2/) - V2.1 FreeRTOS
---------------------------------------------
Conception détaillée : 02_SW_SPEC/Conception_TR_v2.md

[Cœur du système - temps réel]
- greenhv2.ino          : Séquenceur. setup() crée les objets FreeRTOS et 5 tâches
                          épinglées sur le cœur 1 ; corps des tâches :
                          T_ACQ (prio 6, 100 ms)  acquisition + filtrage
                          T_REG (prio 5, événement) régulation, seule à piloter les actionneurs
                          T_IHM (prio 4, événement) traitement des appuis boutons
                          T_LCD (prio 3, 500 ms ou sur demande) seule à piloter l'écran
                          T_DIAG (prio 2, 500 ms) diagnostic
                          Chien de garde des tâches (Task WDT, 5 s).
- greenhv2.h            : Mapping GPIO ESP32, lcd_state_t, événements de régulation.
- shared_data.cpp/.h    : Données partagées protégées par un mutex (héritage de priorité).
- rt_trace.cpp/.h       : Instrumentation WCET (RT_TRACE = 1) : durées, gigue, latences.
- error_handler.cpp/.h  : Diagnostic fonctionnel (consigne non rejointe en 15 s,
                          sondes hors service). Utilisé par T_DIAG uniquement.

[Traitement Capteurs]
- capteurs.cpp/.h       : Lecture ADC en mV (sur-échantillonnage) et fonctions de
                          transfert (Lois physiques) en °C et %.
- filter.cpp/.h         : Algorithme de moyenne glissante.

[Interface Utilisateur (HMI)]
- buttons.cpp/.h        : Interruptions boutons (anti-rebond par bouton) -> file q_boutons.
- lcd.cpp/.h            : Driver LCD 2x16 I2C (PCF8574, 0x27) sur la librairie LCD_I2C.
- lcd_page_manager.cpp/.h : Les 5 pages (menus) et le rafraîchissement optimisé.

[Actionneurs]
- regulation.cpp/.h     : Lois TOR (hystérésis) et PWM, mode manuel, mise en sécurité.
- motor_driver_fan.cpp/.h  : Driver Pilotage du ventilateur (ON/OFF).
- motor_driver_pump.cpp/.h : Driver Gestion de la pompe via signal PWM LEDC 20 kHz.

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
Version v2.0-migration : passer GREENHV2_DEBUG à 1 dans greenhv2.ino, moniteur série (115200) :
- V_OFFSET_MV    : écart entre la température affichée et un thermomètre (x 10 mV/°C)
- AIR_VALUE_MV   : tension du capteur d'humidité à l'air sec
- WATER_VALUE_MV : tension du capteur d'humidité plongé dans l'eau
Remettre GREENHV2_DEBUG à 0 ensuite. (V2.1 : valeurs dans greenhv2.ino, rapport de mesures
activé par RT_TRACE = 1 dans rt_trace.h.)

6. HISTORIQUE
-------------
- V1   : STM32G031K8, STM32CubeIDE, LCD parallèle 4 bits, super-boucle.
- V2.0 : migration ESP32 iso-V1 (super-boucle dans loop()) - référence de non-régression.
         Écart assumé vs V1 : acquisition cadencée à 100 ms + sur-échantillonnage ADC x16
         (l'ADC ESP32 est plus bruité et la boucle plus rapide : affichage instable sinon).
         Correctifs V1 : code erreur rafraîchi à chaque tour (et plus seulement sur la page 5),
         détection de la sonde de température débranchée (T < -20 °C), cf. SR-GH-011 rév. 01.
         Consigne d'humidité par défaut : 5 % (V1 : 18 %), cf. SR-GH-007.
- V2.1 : (branche feature/freertos) découpage en 5 tâches FreeRTOS sur le cœur 1,
         mutex / sémaphore binaire / files, chien de garde, mise en sécurité (SR-GH-031),
         boutons détectés à l'appui avec anti-rebond par bouton, instrumentation rt_trace.
