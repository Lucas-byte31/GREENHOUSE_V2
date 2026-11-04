/*
 * greenhv2.ino
 *
 * PROJET GREENHOUSE V2 - Système de régulation d'une serre
 * Etape 3 : migration STM32 (STM32CubeIDE / HAL) -> ESP32 (Arduino IDE)
 *
 * Ce fichier remplace le main.c de la V1 :
 *  - setup() : séquence d'initialisation (équivalent MX_xxx_Init + USER CODE 2)
 *  - loop()  : boucle principale (équivalent du while(1)), logique strictement identique à la V1
 *
 * Remarque : sur ESP32, loop() est déjà exécutée dans une tâche FreeRTOS (loopTask, cœur 1).
 * Cette version reste volontairement une super-boucle « iso-V1 » qui sert de référence
 * de non-régression (SR-GH-016) avant le découpage en tâches temps réel.
 *
 *  Created on: Nov 4, 2026
 *      Author: lucas
 */

#include "greenhv2.h"
#include "motor_driver_fan.h"
#include "motor_driver_pump.h"
#include "capteurs.h"
#include "lcd.h"
#include "lcd_page_manager.h"
#include "filter.h"
#include "error_handler.h"
#include <stdlib.h>

/* ================== DEBUG ================== */
// Pour debug sur le moniteur série (115200 bauds) -- pas besoin en opérationnel
// Utile pour l'étalonnage des capteurs (affiche les tensions brutes en mV)
#define GREENHV2_DEBUG       0
#define DEBUG_PERIOD_MS      1000

/* ================== ETALONNAGE CAPTEURS ================== */
#define V_OFFSET_MV   0.0f     // offset TMP235 (mV) - V1 : 130 mV sur STM32, à remesurer sur ESP32

// Calibration humidité Grove 101020614 (mV) - à remesurer sur la cible ESP32
// Valeurs de départ = valeurs V1 converties en mV (2675 pts et 1780 pts sur 12 bits / 3,3 V)
//#define AIR_VALUE_MV    2156.0f  // mesure en air sec - valeur sur STM32
//#define WATER_VALUE_MV  1434.0f  // mesure dans l'eau - valeur sur STM32
#define AIR_VALUE_MV    2156.0f  // mesure en air sec - valeur sur ESP32
#define WATER_VALUE_MV  1434.0f  // mesure dans l'eau - valeur sur ESP32

/* ================== ACQUISITION ================== */
// V2 : l'ADC de l'ESP32 est plus bruité que celui du STM32 (± 40 mV observés sur le capteur d'humidité)
// et la boucle tourne beaucoup plus vite qu'en V1 : le filtre sur 8 échantillons ne couvrait
// plus que quelques ms. On fixe donc la période d'acquisition (anticipation SR-GH-021)
// et on moyenne plusieurs conversions ADC à chaque acquisition (sur-échantillonnage).
#define ACQ_PERIOD_MS    100   // Période d'acquisition : 8 échantillons x 100 ms = fenêtre de 0,8 s
#define ADC_OVERSAMPLING 16    // Nombre de conversions moyennées par lecture (bruit divisé par ~4)

/* ================== IHM ================== */
#define DEBOUNCE_MS      200   // Anti-rebond logiciel des boutons (V1 : 200 ms)
#define LCD_REFRESH_MS   500   // Rafraîchissement LCD : 500 ms = confort visuel

#define TEMP_DEADBAND 1 // 1 °C de DEADBAND
#define HUM_DEADBAND  2 // 2 % de DEADBAND


//Flags d'interruption pour les boutons poussoir : mis à TRUE par IT GPIO
// Mis à FALSE par la boucle principale
volatile bool PB1_pressed = false;   // Flag PB1 - Bouton rouge - navigation dans les menus
volatile bool PB2_pressed = false;   // Flag PB2 - Bouton blanc - Incrémentation consigne ou Forcage manuel
volatile bool PB3_pressed = false;   // Flag PB3 - Bouton vert - Décrémentation consigne ou Forcage manuel


//--Paramètres de régulation par défaut--//
static int Var_page = 1; // page affichée au démarrage
static int seuil_temperature = 25;  // Seuil de température par défaut
static int seuil_humidite = 5;      // Seuil d'humidité par défaut (V1 : 18 %, abaissé à 5 % en V2 pour la démonstration)
static int code_erreur = 0; //init du code erreur à 0
static uint8_t speed = 0;  // Variable pour la vitesse de la pompe (0-100) en %

//--Calcul de la rampe linéaire à l'approche du seuil d'humidité--//
// (V2 : x1/y1 renommés car y1() est déjà déclarée par math.h côté Arduino)
static const float ramp_x1 = 0.55f;  // 0.55*seuil_humidite → speed = 100
static const float ramp_y1 = 100.0f;
static const float ramp_x2 = 1.0f;   // 1.0*seuil_humidite → speed = 50
static const float ramp_y2 = 50.0f;
// Coefficients m et b de la droite y = mx + b (calculés dans setup)
static float m = 0.0f;
static float b = 0.0f;

// Etats LCD courant / précédent
static lcd_state_t lcd_prev = {};
static lcd_state_t lcd_curr = {};
static uint32_t last_lcd_update = 0;

//- Filtrage sur moyenne glissante pour éviter le clear intempestif du LCD --//
static int t_display = 0; // température affichée
static int h_display = 0; // humidité affichée
static moving_avg_filter_t temp_filter;
static moving_avg_filter_t hum_filter;


/* ================== PROTOTYPES ================== */
uint32_t ReadADC_Channel(uint8_t pin);
void IRAM_ATTR PB1_ISR(void);
void IRAM_ATTR PB2_ISR(void);
void IRAM_ATTR PB3_ISR(void);
static void Buttons_Init(void);


/* ======================================================================== */
/*                                  SETUP                                   */
/* ======================================================================== */
void setup()
{
#if GREENHV2_DEBUG
  Serial.begin(115200);
#endif

  // Initialisation des périphériques (équivalent MX_GPIO_Init / MX_ADC1_Init / MX_TIM3_Init)
  analogReadResolution(12);   // ADC 12 bits comme en V1 (la lecture se fait en mV)
  Buttons_Init();

  // On initialise le moteur a OFF
  Motor_Init();
  // On initialise la pompe a OFF
  Pump_Init(); // init de la pompe

  //--Configuration du PWM pour la pompe--//
  // Calcul des coefficients m et b de la droite y = mx + b
  m = (ramp_y2 - ramp_y1) / (ramp_x2 - ramp_x1);
  b = ramp_y1 - m * ramp_x1;

  // Initialisation LCD
  lcd_init();
  lcd_clear();

  // Affichage
  lcd_position_cursor(0,0);
  lcd_show_txt("HELLO WORLD");

  lcd_position_cursor(0,1);
  lcd_show_txt("ESP32 GREENH V2");

  //Moyenne glissante sur 8 échantillons pour stabiliser l'affichage sur le LCD
  filter_init(&temp_filter, 8, seuil_temperature); //init du filtre au seuil_temperature
  filter_init(&hum_filter, 8, seuil_humidite); //init du filtre au seuil humidite

  // Init de l'Error Handler
  error_handler_init(seuil_temperature, seuil_humidite); // Init de l'error handler
}


/* ======================================================================== */
/*                         BOUCLE PRINCIPALE (iso-V1)                       */
/* ======================================================================== */
void loop()
{
  // Connaitre l'instant présent - utilisé dans l'error handler et dans le rafraichissement du LCD
  uint32_t now = millis();

  // Grandeurs mesurées : conservées entre deux acquisitions (static)
  static uint32_t last_acq = 0;
  static bool first_acq = true;     // force une acquisition dès le 1er passage (pas de régulation sur des mesures à 0)
  static uint32_t mv_hum = 0, mv_temp = 0;
  static float humidity = 0.0f, temperature = 0.0f;
  static int h_int = 0, t_int = 0;
  static int t_filt = 0, h_filt = 0;

  // ===================== Acquisition périodique (100 ms) =====================
  if (first_acq || (now - last_acq) >= ACQ_PERIOD_MS)
  {
    first_acq = false;
    last_acq = now;

    /* ================== HUMIDITE sur Io34 (ADC1) ================== */
    mv_hum = ReadADC_Channel(PIN_HUMIDITE);
    humidity = CAPTEURS_ComputeHumidity(mv_hum, AIR_VALUE_MV, WATER_VALUE_MV);

    int h100 = (int)(humidity * 100.0f);
    h_int = h100 / 100;


    /* ============== TEMPERATURE sur Io35 (ADC1) (TMP235) ============== */
    mv_temp = ReadADC_Channel(PIN_TEMPERATURE);
    temperature = CAPTEURS_ComputeTemperature(mv_temp, V_OFFSET_MV);

    int t100 = (int)(temperature * 100.0f);
    t_int = t100 / 100;


    // ===================== Filtrage numérique Temperature et Humidite =====================
    /* Un filtrage logiciel par moyenne glissante permet de supprimer le bruit des capteurs
    tout en conservant une information pertinente pour l’utilisateur, sans bloquer l’exécution du système. */
    t_filt = filter_update(&temp_filter, t_int);
    h_filt = filter_update(&hum_filter, h_int);
  } // fin acquisition périodique


  /* --- Température --- */
  if (abs(t_filt - t_display) >= TEMP_DEADBAND)
  {
      t_display = t_filt;
  }

  /* --- Humidité --- */
  if (abs(h_filt - h_display) >= HUM_DEADBAND)
  {
      h_display = h_filt;
  }


  // ===================== Asservissement de la Ventilation en Automatique =====================
  // Le contrôle est actif seulement si on n'est pas sur la Page 4 (Mode Manuel)
  if (Var_page != 4)
  {
	  if (temperature >= seuil_temperature)
	  {
          Motor_On();
	  } else if (temperature < (seuil_temperature - 0.5f))
		  // petite hysteresis pour éviter que
		  // le ventilateur démmarre et s'arrête en permanence quand temperature oscille sur seuil_temperature
	  {
	  	  Motor_Off();
	  }

  // ===================== Asservissement de la Pompe en Automatique =====================

	  // Asservissement linéaire
	  //Plus l'humidité est éloignée de sa consigne, plus la pompe tourne à haute vitesse
	  //Plus l'humidité s'approche de sa valeur consigne, moins la pompe tourne

	  // Calcul du ratio humidité courante / consigne pour la loi PWM
	  // Sécurité V2 : une consigne à 0 % donnerait une division par 0 -> ratio infini -> pompe arrêtée
	  float humidity_ratio = (seuil_humidite > 0) ? (humidity / (float)seuil_humidite) : 1000.0f;

	  // Application de l'équation de droite
	  if (humidity_ratio <= ramp_x1) // Zone 1 : Besoin critique (100% PWM)
	  {
	      speed = 100;  // Vitesse max si humidity <= 0.55*seuil_humidite
	  }
	  else if (humidity_ratio <= ramp_x2) // Zone 2 : Régulation douce (Approche)
	  {
	      speed = (uint8_t)(m * humidity_ratio + b);  // Rampe linéaire
	  }
	  else // Zone 3 : Consigne atteinte
	  {
	      speed = 0;  // Pompe éteinte si humidity > 0.999*seuil_humidite
	      Pump_Stop();
	  }
	  Pump_SetSpeed(speed);

  } else {
	  // Ne rien faire
  }



  // ===================== Changement de page si PB1 appuyé =====================
  if (PB1_pressed)
  {
      PB1_pressed = false;       // on consomme l'événement

      Var_page++;
      if (Var_page > 5)
          Var_page = 1;          // retour à la page 1
  }


  // ===================== Mise à jour du code erreur =====================
  // V2 (correctif) : en V1 le code erreur n'était rafraîchi que sur la page 5.
  // Sur la page 1, l'écran n'est redessiné que si lcd_state_t change : une erreur apparue
  // avec des mesures stables (capteur débranché) ne provoquait donc jamais de redessin -> "OK" figé.
  code_erreur = error_handler_has_error();

  // ===================== Mise à jour de l'etat courant de la structure lcd_state_t =====================
  lcd_curr.page = Var_page;
  lcd_curr.t_filt = t_display;
  lcd_curr.h_filt = h_display;
  lcd_curr.seuil_temperature = seuil_temperature;
  lcd_curr.seuil_humidite = seuil_humidite;
  lcd_curr.code_erreur = code_erreur;
  lcd_curr.pump_on = Pump_IsOn();
  lcd_curr.fan_on = Motor_IsOn();




  // ===================== Gestion de l'affichage (500ms) =====================
  // Boucle principale de l'affichage LCD

  // Le rafraîchissement est cadencé pour éviter les scintillements de l'écran
  if ((now - last_lcd_update) >= LCD_REFRESH_MS)   // 500 ms = confort visuel
      {
          last_lcd_update = now;

          // ===================== Mise à jour de l'écran LCD uniquement sur évênement =====================
          // On compare l'état actuel (lcd_curr) avec l'état précédent (lcd_prev)
          switch (Var_page)
          {
          case 1:
        	  // -------- PAGE 1 --------
        	  // Temperature     x°C
        	  // Humidite        x%

        	  lcd_page_1(&lcd_curr, &lcd_prev);

             break;

          case 2:
        	  // -------- PAGE 2 --------
        	  // Temperature     x°C
        	  // Seuil           x°C -- limité 0 -> 51°C

        		  if (PB2_pressed && !PB3_pressed)
        		 {
        		  PB2_pressed = false;       // on consomme l'événement
        		  if (seuil_temperature < 51) seuil_temperature  += 1;// + 1
        		 }
        		 if (PB3_pressed && !PB2_pressed)
        		 {
        		  PB3_pressed = false;       // on consomme l'événement
        		  if (seuil_temperature > 0) seuil_temperature -= 1; // - 1
        		 }
        	  lcd_page_2(&lcd_curr, &lcd_prev);

             break;

          case 3:
        	  // -------- PAGE 3 --------
        	  // Humidite        x%
        	  // Seuil           x% -- limité 0 -> 100%

        	  if (PB2_pressed && !PB3_pressed )
        	  {
        		  PB2_pressed = false;       // on consomme l'événement
        		  if (seuil_humidite < 100) seuil_humidite += 1; // +1
        	  }
        	  if (PB3_pressed && !PB2_pressed )
        	  {
        		  PB3_pressed = false;       // on consomme l'événement
        		  if (seuil_humidite > 0) seuil_humidite -= 1; // -1
        	  }

        	  lcd_page_3(&lcd_curr, &lcd_prev);

             break;

          case 4:
        	  // -------- PAGE 4 --------
        	  // Pompe       ON/OFF
        	  // Ventilateur ON/OFF

        	  // ===================== Ventilateur Manuel=====================
        	  if (PB2_pressed)
        	  {
        		  PB2_pressed = false;   // On remet PB à false
        		  if (Motor_IsOn()) // si le moteur est déjà allumé on l'éteind
        			  Motor_Off();
        		  else				// si le moteur est éteind on l'allume
        			  Motor_On();
        	  }

        	  // ===================== Pompe Manuelle=====================
        	  if (PB3_pressed)
        	  {
        		  PB3_pressed = false;   // On remet PB à false
        		  if (Pump_IsOn())		// si la pompe est déjà allumé on l'éteind
        			  Pump_Stop();
        		  else
        			  Pump_SetSpeed(100);   // si la pompe est éteinte on l'allume à 100% PWM
        	  }

        	  lcd_page_4(&lcd_curr, &lcd_prev);

             break;

          case 5:
          	  // -------- PAGE 5 -------- MAINTENANCE
  			// SYSTEME OK
  			// FAN SYSTEM OK/KO
  			// PUMP SYSTEM OK/KO
        	  	  // code_erreur est mis à jour à chaque tour de boucle (cf. correctif V2 ci-dessus)
          	      lcd_page_5(&lcd_curr, &lcd_prev);

          	 break;

          }
      }

  //Gestion des erreurs
  error_handler_update(
      t_filt,
      h_filt,
	  seuil_temperature,
	  seuil_humidite,
      Motor_IsOn(),
      Pump_IsOn(),
      now
  );

#if GREENHV2_DEBUG
  // Pour le debug : tensions brutes et valeurs calculées (étalonnage V_OFFSET / AIR / WATER)
  static uint32_t last_debug = 0;
  if ((now - last_debug) >= DEBUG_PERIOD_MS)
  {
      last_debug = now;
      Serial.printf("T: %lu mV -> %d C (filt %d) | H: %lu mV -> %d %% (filt %d, LCD %d) | Err: %d\r\n",
                    (unsigned long)mv_temp, t_int, t_filt, (unsigned long)mv_hum, h_int, h_filt, h_display,
                    error_handler_has_error());
  }
#endif
}


/* ======================================================================== */
/*                            FONCTIONS LOCALES                             */
/* ======================================================================== */

/* Lecture d un canal ADC1 par polling, retourne la tension en mV */
// Pas besoin d'interruption pour l'ADC car les mesures sont lentes (température et humidité)
// V2 : analogReadMilliVolts() applique la courbe d'étalonnage usine (eFuse) de l'ADC ESP32
// Sur-échantillonnage : moyenne de ADC_OVERSAMPLING conversions pour réduire le bruit de l'ADC
uint32_t ReadADC_Channel(uint8_t pin)
{
    uint32_t sum = 0;
    for (int i = 0; i < ADC_OVERSAMPLING; i++)
    {
        sum += analogReadMilliVolts(pin);
    }
    return sum / ADC_OVERSAMPLING;   // tension moyenne en mV
}

// Configuration des boutons poussoirs et des interruptions (équivalent EXTI V1)
static void Buttons_Init(void)
{
    // Boutons actifs à l'état bas : pull-up interne
    pinMode(PIN_PB1, INPUT_PULLUP);
    pinMode(PIN_PB2, INPUT_PULLUP);
    pinMode(PIN_PB3, INPUT_PULLUP);

    // Interruption sur front montant comme en V1 (HAL_GPIO_EXTI_Rising_Callback)
    // -> avec un pull-up, l'événement est généré au relâchement du bouton
    attachInterrupt(digitalPinToInterrupt(PIN_PB1), PB1_ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_PB2), PB2_ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_PB3), PB3_ISR, RISING);
}

// Interruption
// Anti-rebond logiciel : On ignore les impulsions parasites < 200ms
// générées par le contact mécanique des boutons (horodatage commun aux 3 boutons, comme en V1)
static volatile uint32_t last_interrupt_time = 0;

static bool IRAM_ATTR debounce_ok(void)
{
    uint32_t now = millis();
    if (now - last_interrupt_time < DEBOUNCE_MS) return false;  // 200 ms debounce
    last_interrupt_time = now;
    return true;
}

// IRAM_ATTR : la routine est placée en RAM interne (exécutable même pendant un accès flash)
void IRAM_ATTR PB1_ISR(void)
{
    if (debounce_ok()) PB1_pressed = true;   // Flag PB1
}

void IRAM_ATTR PB2_ISR(void)
{
    if (debounce_ok()) PB2_pressed = true;   // Flag PB2
}

void IRAM_ATTR PB3_ISR(void)
{
    if (debounce_ok()) PB3_pressed = true;   // Flag PB3
}
