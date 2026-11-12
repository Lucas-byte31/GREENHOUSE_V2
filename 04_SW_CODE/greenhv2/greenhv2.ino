/*
 * greenhv2.ino
 *
 * PROJET GREENHOUSE V2 - Système de régulation d'une serre
 * Etape 5 : version temps réel FreeRTOS (V2.1 - branche feature/freertos)
 *
 * Ce fichier séquence le système :
 *  - setup() : initialise le matériel, crée les objets de synchronisation puis les 5 tâches
 *  - loop()  : la tâche Arduino loopTask n'est plus utilisée, elle se supprime
 *  - corps des 5 tâches : ils ne font que séquencer les appels aux composants
 *
 * Architecture (cf. 02_SW_SPEC/Conception_TR_v2.md) - toutes les tâches sur le cœur 1 :
 *
 *   Tâche    Prio  Activation                         Rôle
 *   T_ACQ     6    périodique 100 ms                  acquisition + filtrage -> q_reg
 *   T_REG     5    événement q_reg                    régulation, seule à piloter les actionneurs
 *   T_IHM     4    événement q_boutons (ISR)          traitement des appuis selon la page
 *   T_LCD     3    sem_lcd ou 500 ms                  seule à piloter l'écran
 *   T_DIAG    2    périodique 500 ms                  diagnostic (error_handler)
 *
 *   Synchronisation : mutex mtx_data (shared_data), sémaphore binaire sem_lcd,
 *                     files q_boutons (ISR -> T_IHM) et q_reg (T_ACQ / T_IHM -> T_REG)
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
#include "shared_data.h"
#include "buttons.h"
#include "regulation.h"
#include "rt_trace.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

/* ================== ETALONNAGE CAPTEURS ================== */
#define V_OFFSET_MV   0.0f     // offset TMP235 (mV) - nul sur ESP32 (ADC étalonné usine via analogReadMilliVolts)

// Calibration humidité Grove 101020614 (mV)
#define AIR_VALUE_MV    2156.0f  // mesure en air sec - valeur sur ESP32
#define WATER_VALUE_MV  1434.0f  // mesure dans l'eau - valeur sur ESP32

/* ================== PARAMETRES PAR DEFAUT ================== */
#define SEUIL_TEMPERATURE_DEFAUT  25   // °C
#define SEUIL_HUMIDITE_DEFAUT     5    // % (V1 : 18 %, abaissé à 5 % en V2 - SR-GH-007)

/* ================== ACQUISITION ================== */
#define ACQ_PERIOD_MS    100   // Période d'acquisition (SR-GH-021) : 8 échantillons x 100 ms = fenêtre de 0,8 s
#define ADC_OVERSAMPLING 16    // Nombre de conversions moyennées par lecture (bruit divisé par ~4)
#define FILTER_SIZE      8     // Moyenne glissante sur 8 échantillons

/* ================== IHM ================== */
#define LCD_REFRESH_MS   500   // Rafraîchissement LCD périodique : 500 ms = confort visuel (SR-GH-024)
#define SPLASH_MS        2000  // Durée d'affichage du message de démarrage (lisible à l'oeil nu)
#define NB_PAGES         5
#define PAGE_MANUEL      4
#define SEUIL_T_MAX      51
#define SEUIL_H_MAX      100

#define TEMP_DEADBAND 1 // 1 °C de DEADBAND
#define HUM_DEADBAND  2 // 2 % de DEADBAND

/* ================== DIAGNOSTIC ================== */
#define DIAG_PERIOD_MS   500

/* ================== CONFIGURATION FreeRTOS ================== */
#define RT_CORE          1     // Cœur applicatif (APP_CPU) - un seul cœur utilisé (SR-GH-020)

// Priorités (Deadline Monotonic, cf. Conception_TR_v2 §4) - au-dessus de loopTask/Timer (1) et IDLE (0)
#define PRIO_ACQ         6
#define PRIO_REG         5
#define PRIO_IHM         4
#define PRIO_LCD         3
#define PRIO_DIAG        2

// ailles de pile en octets, validées par uxTaskGetStackHighWaterMark (pile libre min ≥ 50 %, voir Mesures_WCET_v2.md)
#define STACK_ACQ        3072
#define STACK_REG        3072
#define STACK_IHM        3072
#define STACK_LCD        4096
#define STACK_DIAG       4096

// 1 mesure / 100 ms (T_ACQ) + commandes IHM. T_REG (prio 5) vide la file en < 0,1 ms,
// il n'y a jamais plus d'un message en attente dnc 4 couvre une rafale mesure + 3 boutons.
// Vérifié : compteur "File pleine" à 0 sur tous les essais E1 à E4.
#define REG_QUEUE_LENGTH 4

// Délais max d'attente des tâches événementielles : elles se réveillent toujours
// à temps pour rafraîchir le chien de garde (jamais portMAX_DELAY)
#define REG_WAIT_MS      500
#define IHM_WAIT_MS      1000

// Chien de garde des tâches (SR-GH-030)
#define WDT_TIMEOUT_MS   5000


/* ================== OBJETS FreeRTOS ================== */
static QueueHandle_t     q_boutons = NULL;   // File ISR boutons -> T_IHM
static QueueHandle_t     q_reg     = NULL;   // File d'événements T_ACQ / T_IHM -> T_REG
static SemaphoreHandle_t sem_lcd   = NULL;   // Sémaphore binaire T_IHM / T_REG / T_DIAG -> T_LCD

static TaskHandle_t h_acq = NULL, h_reg = NULL, h_ihm = NULL, h_lcd = NULL, h_diag = NULL;


/* ================== PROTOTYPES ================== */
static void T_ACQ(void *pvParameters);
static void T_REG(void *pvParameters);
static void T_IHM(void *pvParameters);
static void T_LCD(void *pvParameters);
static void T_DIAG(void *pvParameters);
#if RT_STIMULUS
static void T_STIM(void *pvParameters);
#endif
static void Watchdog_Init(void);
static void Fatal_Error(const char *msg);


/* ======================================================================== */
/*                                  SETUP                                   */
/* ======================================================================== */
void setup()
{
#if RT_TRACE
  Serial.begin(115200);
#endif
  rt_trace_init();

  // ===================== Initialisation du matériel =====================
  analogReadResolution(12);   // ADC 12 bits comme en V1 (la lecture se fait en mV)

  // On initialise le moteur a OFF
  Motor_Init();
  // On initialise la pompe a OFF
  Pump_Init();
  // Coefficients de la rampe PWM
  Regulation_Init();

  // Initialisation LCD
  lcd_init();
  lcd_clear();
  lcd_position_cursor(0,0);
  lcd_show_txt("BONJOUR LUCAS");
  lcd_position_cursor(0,1);
  lcd_show_txt("GREENH V2 RTOS");

  // Init de l'Error Handler (utilisé uniquement par T_DIAG)
  error_handler_init(SEUIL_TEMPERATURE_DEFAUT, SEUIL_HUMIDITE_DEFAUT);

  // ===================== Création des objets de synchronisation =====================
  // Ils doivent exister Avantles tâches et les interruptions qui les utilisent
  if (!shared_data_init(SEUIL_TEMPERATURE_DEFAUT, SEUIL_HUMIDITE_DEFAUT)) Fatal_Error("mtx_data");

  q_boutons = xQueueCreate(BUTTONS_QUEUE_LENGTH, sizeof(button_event_t));
  q_reg     = xQueueCreate(REG_QUEUE_LENGTH, sizeof(reg_event_t));
  sem_lcd   = xSemaphoreCreateBinary();     // créé "vide" : la 1re prise attend un don
  if (q_boutons == NULL || q_reg == NULL || sem_lcd == NULL) Fatal_Error("objets RTOS");

  // Boutons : interruptions armées depuis setup() -> servies par le cœur 1
  Buttons_Init(q_boutons);

  Watchdog_Init();

  // ===================== Création des tâches toutes sur le cœur 1 =====================
  // Remarque : setup() tourne dans loopTask (prio 1) et chaque tâche créée, plus prioritaire,
  // démarre immédiatement. Les objets RTOS étant déjà créés, l'ordre de création est sans risque.
  xTaskCreatePinnedToCore(T_DIAG, "T_DIAG", STACK_DIAG, NULL, PRIO_DIAG, &h_diag, RT_CORE);
  xTaskCreatePinnedToCore(T_LCD,  "T_LCD",  STACK_LCD,  NULL, PRIO_LCD,  &h_lcd,  RT_CORE);
  xTaskCreatePinnedToCore(T_IHM,  "T_IHM",  STACK_IHM,  NULL, PRIO_IHM,  &h_ihm,  RT_CORE);
  xTaskCreatePinnedToCore(T_REG,  "T_REG",  STACK_REG,  NULL, PRIO_REG,  &h_reg,  RT_CORE);
  xTaskCreatePinnedToCore(T_ACQ,  "T_ACQ",  STACK_ACQ,  NULL, PRIO_ACQ,  &h_acq,  RT_CORE);
  if (!h_acq || !h_reg || !h_ihm || !h_lcd || !h_diag) Fatal_Error("creation taches");

#if RT_STIMULUS
  // Banc de stimulation pour les mesures WCET - priorité 1 : ne perturbe pas les tâches mesurées
  xTaskCreatePinnedToCore(T_STIM, "T_STIM", 2048, NULL, 1, NULL, RT_CORE);
#endif
}


/* ======================================================================== */
/*         LOOP : la super-boucle est remplacée par les tâches FreeRTOS      */
/* ======================================================================== */
void loop()
{
  // loopTask n'a plus de rôle je la supprime pour libérer sa pile et le coeur 1
  vTaskDelete(NULL);
}


/* ======================================================================== */
/*                  T_ACQ - Acquisition (périodique 100 ms, prio 6)          */
/* ======================================================================== */
// Priorité maximale : seule tâche avec une contrainte de gigue (SR-GH-021).
// Elle ne prend jamais le mutex et n'est jamais bloquée en envoi (délai 0) :
// rien d'autre qu'une interruption ne peut retarder son démarrage.
static void T_ACQ(void *pvParameters)
{
  (void)pvParameters;
  esp_task_wdt_add(NULL);   // surveillance par le chien de garde

  //Moyenne glissante sur 8 échantillons pour stabiliser l'affichage sur le LCD
  moving_avg_filter_t temp_filter;
  moving_avg_filter_t hum_filter;
  filter_init(&temp_filter, FILTER_SIZE, SEUIL_TEMPERATURE_DEFAUT); //init du filtre au seuil_temperature
  filter_init(&hum_filter,  FILTER_SIZE, SEUIL_HUMIDITE_DEFAUT);    //init du filtre au seuil humidite

  TickType_t last_wake = xTaskGetTickCount();   // date de référence de la période

  // boucle infinie de la tâche, bloquée à chaque tour en attente de son événement pareil que While(true)
  for (;;)
  {
    rt_trace_activation(RT_TASK_ACQ, ACQ_PERIOD_MS * 1000UL);
    uint32_t t0 = rt_trace_begin(RT_TASK_ACQ);

    reg_event_t evt;
    evt.type = EVT_MESURE;

    /* ================== HUMIDITE sur Io34 (ADC1) ================== */
    uint32_t mv_hum = CAPTEURS_ReadMilliVolts(PIN_HUMIDITE, ADC_OVERSAMPLING);
    evt.humidity = CAPTEURS_ComputeHumidity(mv_hum, AIR_VALUE_MV, WATER_VALUE_MV);

    /* ============== TEMPERATURE sur Io35 (ADC1) (TMP235) ============== */
    uint32_t mv_temp = CAPTEURS_ReadMilliVolts(PIN_TEMPERATURE, ADC_OVERSAMPLING);
    evt.temperature = CAPTEURS_ComputeTemperature(mv_temp, V_OFFSET_MV);

    // ===================== Filtrage numérique Temperature et Humidite =====================
    // Conversion en entier par troncature comme en V1, puis moyenne glissante
    evt.t_filt = filter_update(&temp_filter, (int)evt.temperature);
    evt.h_filt = filter_update(&hum_filter,  (int)evt.humidity);

    // ===================== Publication vers T_REG =====================
    // Délai 0 : T_ACQ n'attend jamais. T_REG (prio 5) vide la file juste après.
    if (xQueueSend(q_reg, &evt, 0) != pdTRUE)
    {
      rt_trace_queue_full();
    }

    rt_trace_end(RT_TASK_ACQ, t0);
    esp_task_wdt_reset();

    // Attente de la prochaine période : réveil calculé depuis le réveil précédent (pas de dérive)
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ACQ_PERIOD_MS));
  }
}


/* ======================================================================== */
/*            T_REG - Régulation (sur événement q_reg, prio 5)               */
/* ======================================================================== */
// Seule tâche qui commande les actionneurs (principe du gardien) :
// mode automatique, mode manuel en page4 et mise en sécurité (SR-GH-031).
static void T_REG(void *pvParameters)
{
  (void)pvParameters;
  esp_task_wdt_add(NULL);

  reg_event_t last_mesure = {};
  bool mesure_valide = false;   // on ne régule pas tant qu'aucune mesure n'est arrivée

    // boucle infinie de la tâche, bloquée à chaque tour en attente de son événement pareil que While(true
  for (;;)
  {
    reg_event_t evt;
    if (xQueueReceive(q_reg, &evt, pdMS_TO_TICKS(REG_WAIT_MS)) == pdTRUE)
    {
      uint32_t t0 = rt_trace_begin(RT_TASK_REG);

      // ===================== Nouvelle mesure : publication pour les autres tâches =====================
      if (evt.type == EVT_MESURE)
      {
        last_mesure = evt;
        mesure_valide = true;
        shared_data_set_mesures(evt.temperature, evt.humidity, evt.t_filt, evt.h_filt);
      }

      // ===================== Préparation des entrées de la régulation =====================
      greenhouse_data_t d;
      if (mesure_valide && shared_data_get(&d))
      {
        regulation_input_t in;
        in.temperature       = last_mesure.temperature;   // valeur non filtrée (comme en V1)
        in.humidity          = last_mesure.humidity;
        in.seuil_temperature = d.seuil_temperature;
        in.seuil_humidite    = d.seuil_humidite;
        in.manual_mode       = (d.page == PAGE_MANUEL);  // régulation auto suspendue sur la page 4
        in.manual_fan        = d.manual_fan;
        in.manual_pump       = d.manual_pump;
        in.pump_fault        = (d.code_erreur == 2 || d.code_erreur == 3);
        in.temp_sensor_fault = d.temp_sensor_fault;

        // ===================== Calcul et application des commandes =====================
        Regulation_Step(&in);

        // Publication de l'état réel des actionneurs (affichage, diagnostic, mode manuel)
        bool fan_on  = (Motor_IsOn() != 0);
        bool pump_on = Pump_IsOn();
        shared_data_set_actionneurs(fan_on, pump_on);

        // Changement d'état d'un actionneur : l'écran (page 4) est mis à jour sans attendre
        if (fan_on != d.fan_on || pump_on != d.pump_on)
        {
          xSemaphoreGive(sem_lcd);
        }
      }

      rt_trace_end(RT_TASK_REG, t0);
    }
    esp_task_wdt_reset();
  }
}


/* ======================================================================== */
/*           T_IHM - Boutons (sur événement q_boutons, prio 4)               */
/* ======================================================================== */
// Chaque appui est traité immédiatement dans le contexte de la page active :
// un appui sans effet sur la page courante est consommé et oublié (SR-GH-025).
static void T_IHM(void *pvParameters)
{
  (void)pvParameters;
  esp_task_wdt_add(NULL);

  for (;;)
  {
    button_event_t btn;
    if (xQueueReceive(q_boutons, &btn, pdMS_TO_TICKS(IHM_WAIT_MS)) == pdTRUE)
    {
      uint32_t t0 = rt_trace_begin(RT_TASK_IHM);

      greenhouse_data_t d;
      if (shared_data_get(&d))
      {
        int  page        = d.page;
        int  seuil_t     = d.seuil_temperature;
        int  seuil_h     = d.seuil_humidite;
        bool manual_fan  = d.manual_fan;
        bool manual_pump = d.manual_pump;
        bool commande    = false;     // true -> réveiller T_REG immédiatement

        switch (btn.id)
        {
        // ===================== PB1 : Changement de page =====================
        case BTN_PB1:
          page++;
          if (page > NB_PAGES)
            page = 1;                 // retour à la page 1

          if (page == PAGE_MANUEL)
          {
            // Entrée en mode manuel : les actionneurs gardent leur état courant (comme en V1)
            manual_fan  = d.fan_on;
            manual_pump = d.pump_on;
          }
          // Entrée ou sortie de la page 4 : la régulation doit changer de mode sans attendre
          commande = (page == PAGE_MANUEL) || (d.page == PAGE_MANUEL);
          break;

        // ===================== PB2 : + consigne ou Ventilateur manuel =====================
        case BTN_PB2:
          if (page == 2 && seuil_t < SEUIL_T_MAX) seuil_t += 1;       // Seuil température +1 (0 -> 51°C)
          else if (page == 3 && seuil_h < SEUIL_H_MAX) seuil_h += 1;  // Seuil humidité +1 (0 -> 100%)
          else if (page == PAGE_MANUEL)
          {
            manual_fan = !d.fan_on;   // si le moteur est allumé on l'éteint, et inversement
            commande = true;
          }
          break;

        // ===================== PB3 : - consigne ou Pompe manuelle =====================
        case BTN_PB3:
          if (page == 2 && seuil_t > 0) seuil_t -= 1;                 // Seuil température -1
          else if (page == 3 && seuil_h > 0) seuil_h -= 1;            // Seuil humidité -1
          else if (page == PAGE_MANUEL)
          {
            manual_pump = !d.pump_on; // si la pompe est allumée on l'éteint, sinon 100% PWM
            commande = true;
          }
          break;

        default:
          break;
        }

        shared_data_set_ihm(page, seuil_t, seuil_h, manual_fan, manual_pump);

        // Réveil immédiat de T_REG (prio 5 > 4 : elle préempte T_IHM dès l'envoi)
        if (commande)
        {
          reg_event_t evt = {};
          evt.type = EVT_COMMANDE;
          if (xQueueSend(q_reg, &evt, 0) != pdTRUE) rt_trace_queue_full();
        }

        rt_trace_press(btn.t_us);   // latence appui -> prise en compte

        // Demande de redessin immédiat de l'écran (sémaphore binaire : synchronisation)
        xSemaphoreGive(sem_lcd);
      }

      rt_trace_end(RT_TASK_IHM, t0);
    }
    esp_task_wdt_reset();
  }
}


/* ======================================================================== */
/*          T_LCD - Affichage (sem_lcd ou 500 ms, prio 3)                    */
/* ======================================================================== */
// Seule tâche qui accède à l'écran (gardien du bus I2C) : aucun mutex nécessaire sur l'I2C.
// Elle copie un instantané des données sous mutex, rend le mutex, PUIS dessine (~35 ms).
static void T_LCD(void *pvParameters)
{
  (void)pvParameters;
  esp_task_wdt_add(NULL);

  // Message de démarrage affiché par setup() : on attend SPLASH_MS avant le 1er redessin.
  // Seule T_LCD attend : l'acquisition et la régulation démarrent immédiatement (pas de delay() bloquant dans setup).
  // vTaskDelay rend le processeur aux autres tâches ; 2 s < 5 s du chien de garde.
  vTaskDelay(pdMS_TO_TICKS(SPLASH_MS));
  xSemaphoreGive(sem_lcd);      // 1er redessin immédiatement à la fin du message (sans attendre 500 ms)

  static lcd_state_t lcd_prev = {};
  static lcd_state_t lcd_curr = {};

  //- Bande morte pour éviter le clear intempestif du LCD --//
  int t_display = 0; // température affichée
  int h_display = 0; // humidité affichée

  for (;;)
  {
    // Réveil sur demande (appui bouton, changement de diagnostic) ou au plus tard toutes les 500 ms
    BaseType_t sur_demande = xSemaphoreTake(sem_lcd, pdMS_TO_TICKS(LCD_REFRESH_MS));

    uint32_t t0 = rt_trace_begin(RT_TASK_LCD);

    greenhouse_data_t d;
    if (shared_data_get(&d))
    {
      /* --- Température --- */
      if (abs(d.t_filt - t_display) >= TEMP_DEADBAND)
      {
          t_display = d.t_filt;
      }

      /* --- Humidité --- */
      if (abs(d.h_filt - h_display) >= HUM_DEADBAND)
      {
          h_display = d.h_filt;
      }

      // ===================== Mise à jour de l'etat courant de la structure lcd_state_t =====================
      lcd_curr.page = d.page;
      lcd_curr.t_filt = t_display;
      lcd_curr.h_filt = h_display;
      lcd_curr.seuil_temperature = d.seuil_temperature;
      lcd_curr.seuil_humidite = d.seuil_humidite;
      lcd_curr.code_erreur = d.code_erreur;
      lcd_curr.pump_on = d.pump_on;
      lcd_curr.fan_on = d.fan_on;

      // ===================== Mise à jour de l'écran LCD uniquement sur évênement =====================
      // On compare l'état actuel (lcd_curr) avec l'état précédent (lcd_prev)
      switch (d.page)
      {
      case 1: lcd_page_1(&lcd_curr, &lcd_prev); break;   // Temperature / Humidite + OK
      case 2: lcd_page_2(&lcd_curr, &lcd_prev); break;   // Temperature / Seuil
      case 3: lcd_page_3(&lcd_curr, &lcd_prev); break;   // Humidite / Seuil
      case 4: lcd_page_4(&lcd_curr, &lcd_prev); break;   // Pompe / Ventilateur ON/OFF
      case 5: lcd_page_5(&lcd_curr, &lcd_prev); break;   // Maintenance
      default: break;
      }

      if (sur_demande == pdTRUE) rt_trace_screen();     // latence appui -> écran à jour
    }

    rt_trace_end(RT_TASK_LCD, t0);
    esp_task_wdt_reset();
  }
}


/* ======================================================================== */
/*            T_DIAG - Diagnostic (périodique 500 ms, prio 2)                */
/* ======================================================================== */
// Seule tâche qui utilise l'error_handler. Elle publie le code erreur dans les données
// partagées : T_REG l'utilise pour la mise en sécurité, T_LCD pour l'affichage.
static void T_DIAG(void *pvParameters)
{
  (void)pvParameters;
  esp_task_wdt_add(NULL);

  int code_precedent = 0;
  TickType_t last_wake = xTaskGetTickCount();

  for (;;)
  {
    uint32_t t0 = rt_trace_begin(RT_TASK_DIAG);

    greenhouse_data_t d;
    if (shared_data_get(&d))
    {
      //Gestion des erreurs
      error_handler_update(
          d.t_filt,
          d.h_filt,
          d.seuil_temperature,
          d.seuil_humidite,
          d.fan_on,
          d.pump_on,
          millis()
      );

      int code = error_handler_has_error();
      shared_data_set_diag(code, error_handler_temp_sensor_fault());

      // Changement d'état du diagnostic : l'écran est mis à jour sans attendre (page 1 / page 5)
      if (code != code_precedent)
      {
        code_precedent = code;
        xSemaphoreGive(sem_lcd);
      }
    }

    rt_trace_end(RT_TASK_DIAG, t0);

#if RT_TRACE
    // Rapport de mesures (hors mesure de T_DIAG) + marge de pile de chaque tâche (octets)
    rt_trace_report();
    static uint32_t last_stack_ms = 0;
    if ((millis() - last_stack_ms) >= RT_TRACE_REPORT_MS)
    {
      last_stack_ms = millis();
      Serial.printf("Pile libre min (o) : ACQ %u | REG %u | IHM %u | LCD %u | DIAG %u\r\n",
                    (unsigned)uxTaskGetStackHighWaterMark(h_acq), (unsigned)uxTaskGetStackHighWaterMark(h_reg),
                    (unsigned)uxTaskGetStackHighWaterMark(h_ihm), (unsigned)uxTaskGetStackHighWaterMark(h_lcd),
                    (unsigned)uxTaskGetStackHighWaterMark(h_diag));
    }
#endif

    esp_task_wdt_reset();

    ////////////////////-----------------Test E6 a commenter en operationel-----------------////////////////////
    /*static int essai_wdt = 0;
    if (++essai_wdt == 20) { vTaskDelay(pdMS_TO_TICKS(8000)); }   // TEST WDT : blocage 8 s après ~10 s*/
    ////////////////////-----------------Test E6 a commenter en operationel-----------------////////////////////

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DIAG_PERIOD_MS));
  }
}


#if RT_STIMULUS
/* ======================================================================== */
/*      T_STIM - Banc de stimulation (mesures WCET uniquement, prio 1)       */
/* ======================================================================== */
// Injecte un appui PB1 toutes les RT_STIMULUS_MS dans q_boutons, comme le ferait la fonction d'interruption.
// Chaque appui change de page : T_IHM s'exécute, T_LCD fait un redessin complet (pire cas)
// et T_REG est réveillée à l'entrée et à la sortie de la page 4.
// Permet d'atteindre 1000 activations de T_IHM et T_LCD sans appuyer 1000 fois.
// Remarque : la latence mesurée part de l'injection (l'ISR n'est pas comprise).
static void T_STIM(void *pvParameters)
{
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(3000));   // laisse le système démarrer
  TickType_t last_wake = xTaskGetTickCount();
  for (;;)
  {
    button_event_t evt = { BTN_PB1, (uint32_t)esp_timer_get_time() };
    xQueueSend(q_boutons, &evt, 0);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RT_STIMULUS_MS));
  }
}
#endif

/* ======================================================================== */
/*                            FONCTIONS LOCALES                             */
/* ======================================================================== */

// Configuration du Task Watchdog Timer d'ESP-IDF (SR-GH-030)
// Délai 5 s, redémarrage (panic) si une tâche surveillée ne s'est pas signalée.
// Les tâches IDLE des 2 cœurs sont aussi surveillées : si nos tâches monopolisaient
// le cœur 1 pendant 5 s (surcharge), le chien de garde le détecterait.
static void Watchdog_Init(void)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  esp_task_wdt_config_t cfg;
  cfg.timeout_ms     = WDT_TIMEOUT_MS;
  cfg.idle_core_mask = (1 << 0) | (1 << 1);
  cfg.trigger_panic  = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK)   // déjà initialisé par le core Arduino
  {
    esp_task_wdt_init(&cfg);                      // sinon initialisation
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
  esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0));
  esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1));
#endif
}

// Erreur d'initialisation : message sur le LCD et arrêt (le chien de garde n'est pas encore actif)
static void Fatal_Error(const char *msg)
{
  Motor_Off();
  Pump_Stop();
  lcd_clear();
  lcd_position_cursor(0, 0);
  lcd_show_txt("ERREUR INIT");
  lcd_position_cursor(0, 1);
  lcd_show_txt(msg);
  for (;;) { delay(1000); }
}
