
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Initialisation de l'écran LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialisation de l'écran LCD I2C
int potentiometre = 0;
bool presence = false;

// Stockage du message reçu par le XBee
String message = "";

// Fonction pour afficher les données sur le LCD
void afficherLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Pot : ");
  lcd.print(potentiometre);
  lcd.print("       ");

  lcd.setCursor(0, 1);
  lcd.print("Presence : ");
  if (presence) {
    lcd.print("OUI");
  } else {
    lcd.print("NON");
  }
  lcd.print("   ");
}

// Fonction pour traiter les messages reçus
void traiterMessage(String msg) {
  msg.trim();

  // On regarde si le message vient du potentiomètre
  if (msg.startsWith("POT:")) {
    potentiometre = msg.substring(4).toInt();
    afficherLCD();
  }

  // On regarde si le message vient du capteur de présence
  if (msg.startsWith("PIR:")) {
    presence = (msg.substring(4).toInt() == 1);
    afficherLCD();
  }
}

void setup() {
  // Démarrage de la communication série avec le XBee
  Serial.begin(9600);
  Serial.setTimeout(100);

  // Initialisation de l'écran LCD
  lcd.init();
  lcd.backlight();

  // Message affiché au démarrage
  lcd.setCursor(0, 0);
  lcd.print("Projet ZigBee");
  lcd.setCursor(0, 1);
  lcd.print("Initialisation");

  // On efface le message pour afficher les valeurs
  delay(3000);
  lcd.clear();

  afficherLCD();
}

void loop() {
  // On vérifie si le XBee a envoyé des données
  while (Serial.available()) {
    char c = Serial.read();

    // Un retour à la ligne indique la fin du message
    if (c == '\n' || c == '\r') {
      if (message.length() > 0) {
        traiterMessage(message);
        message = "";
      }
       // On ajoute les caractères reçus au message
    } else if (message.length() < 40) {
      message += c;
    } else {
      // On vide le message s'il devient trop long
      message = "";
    }
  }
}
