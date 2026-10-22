#include <Wire.h>
#include <LCD_I2C.h>

// Initialisation de l'écran LCD
LCD_I2C lcd(0x27, 16, 2); 

// --- DÉFINITION DES BROCHES ---
const int pinTemp = 35;     // TMP235 (Entrée ADC)
const int pinHumidity = 34; // Capteur d'humidité (Entrée ADC)

// Moteurs (Motodriver2)
const int pinFanIn1 = 26;   
const int pinFanIn2 = 25;   
const int pinPumpIn3 = 17;  
const int pinPumpIn4 = 16;  

// Boutons Poussoirs
const int btnPagePin = 18;  // Changement de page
const int btnPlusPin = 19;  // Incrémentation / Action +
const int btnMinusPin = 23; // Décrémentation / Action -

// --- VARIABLES GLOBALES & ÉTATS ---
int currentPage = 0;            // De 0 à 3
float tempThreshold = 25.0;     // Seuil de température par défaut (°C)
int humThreshold = 50;          // Seuil d'humidité par défaut (%)

bool manualOverride = false;    // Mode manuel activé ou non
bool manualFanState = false;    // État manuel du ventilateur
bool manualPumpState = false;   // État manuel de la pompe

// Variables pour anti-rebond basique des boutons
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200; // Délai en ms

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Demarrage Serre Automatee (avec analogReadMilliVolts)...");

  // Configuration des broches moteurs
  pinMode(pinFanIn1, OUTPUT);
  pinMode(pinFanIn2, OUTPUT);
  pinMode(pinPumpIn3, OUTPUT);
  pinMode(pinPumpIn4, OUTPUT);

  // Configuration des boutons avec résistance Pull-Up interne
  pinMode(btnPagePin, INPUT_PULLUP);
  pinMode(btnPlusPin, INPUT_PULLUP);
  pinMode(btnMinusPin, INPUT_PULLUP);

  // Initialisation I2C et LCD
  Wire.begin();
  lcd.begin();          
  lcd.backlight();
}

void loop() {
  // --- 1. LECTURE DES BOUTONS AVEC ANTI-REBOND ---
  if (millis() - lastDebounceTime > debounceDelay) {
    if (digitalRead(btnPagePin) == LOW) {
      currentPage = (currentPage + 1) % 4; // Passe à la page suivante (0 -> 1 -> 2 -> 3 -> 0)
      lastDebounceTime = millis();
    }
    else if (digitalRead(btnPlusPin) == LOW) {
      handleButtonAction(1); // Action "+"
      lastDebounceTime = millis();
    }
    else if (digitalRead(btnMinusPin) == LOW) {
      handleButtonAction(-1); // Action "-"
      lastDebounceTime = millis();
    }
  }

  // --- 2. LECTURE CAPTEURS ---
  // Utilisation d'analogReadMilliVolts pour une conversion directe en mV calibrés
  int voltage_mV = analogReadMilliVolts(pinTemp);
  // Formule TMP235 : Température = (Tension_mV - 500) / 10
  float temperature = (voltage_mV - 500.0) / 10.0;

  int rawHum = analogRead(pinHumidity);
  int humidityPercent = map(rawHum, 3500, 1200, 0, 100);
  humidityPercent = constrain(humidityPercent, 0, 100);

  // --- 3. GESTION DES ACTIONNEURS (Automatique ou Manuel) ---
  bool fanActive = false;
  bool pumpActive = false;

  if (manualOverride) {
    fanActive = manualFanState;
    pumpActive = manualPumpState;
  } else {
    // Mode automatique basé sur les seuils
    fanActive = (temperature > tempThreshold);
    pumpActive = (humidityPercent < humThreshold);
  }

  // Application physique sur la Motodriver2
  digitalWrite(pinFanIn1, fanActive ? HIGH : LOW);
  digitalWrite(pinFanIn2, LOW);
  
  digitalWrite(pinPumpIn3, pumpActive ? HIGH : LOW);
  digitalWrite(pinPumpIn4, LOW);

  // --- 4. AFFICHAGE LCD SELON LA PAGE ACTIVE ---
  updateLCD(temperature, humidityPercent, fanActive, pumpActive);

  delay(100); // Petite pause pour stabiliser la boucle
}

// --- FONCTION DE GESTION DES ACTIONS DES BOUTONS + / - ---
void handleButtonAction(int direction) {
  switch (currentPage) {
    case 1: // Page Réglage Température
      tempThreshold += direction * 1.0; // +1 ou -1 °C
      break;
    case 2: // Page Réglage Humidité
      humThreshold += direction * 1;    // +1 ou -1 %
      humThreshold = constrain(humThreshold, 0, 100);
      break;
    case 3: // Page Mode Manuel
      manualOverride = true;
      if (direction > 0) {
        // Bouton + : Tout allumer
        manualFanState = true;
        manualPumpState = true;
      } else {
        // Bouton - : Tout éteindre et repasser en auto
        manualFanState = false;
        manualPumpState = false;
        manualOverride = false; 
      }
      break;
    default:
      break;
  }
}

// --- FONCTION DE MISE À JOUR DE L'ÉCRAN LCD ---
void updateLCD(float temp, int hum, bool fan, bool pump) {
  lcd.clear(); // Nettoyage de l'écran pour éviter les résidus entre les pages
  
  switch (currentPage) {
    case 0: // Page Principale
      lcd.setCursor(0, 0);
      lcd.print("T:"); lcd.print(temp, 1); lcd.print("C  F:"); lcd.print(fan ? "ON" : "OFF");
      lcd.setCursor(0, 1);
      lcd.print("H:"); lcd.print(hum); lcd.print("%   P:"); lcd.print(pump ? "ON" : "OFF");
      break;

    case 1: // Page Réglage Température
      lcd.setCursor(0, 0);
      lcd.print("Reglage Temp");
      lcd.setCursor(0, 1);
      lcd.print("Seuil: "); lcd.print(tempThreshold, 1); lcd.print("C");
      break;

    case 2: // Page Réglage Humidité
      lcd.setCursor(0, 0);
      lcd.print("Reglage Humidite");
      lcd.setCursor(0, 1);
      lcd.print("Seuil: "); lcd.print(humThreshold); lcd.print("%");
      break;

    case 3: // Page Mode Manuel
      lcd.setCursor(0, 0);
      lcd.print(manualOverride ? "Mode: MANUEL" : "Mode: AUTO");
      lcd.setCursor(0, 1);
      lcd.print("+:ON  -:OFF/Auto");
      break;
  }
}