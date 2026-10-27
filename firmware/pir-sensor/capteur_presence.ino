#include <SoftwareSerial.h>
SoftwareSerial xbee(2, 3);          // XBee en mode DLINE : RX = D2, TX = D3

const int PIR = 4;
const int LED = 13;

int etatPrecedent = -1;
unsigned long dernierEnvoi = 0;
const unsigned long RAPPEL = 5000;  // renvoi toutes les 5 s

void envoyer(int etat) {
  xbee.print("PIR:");
  xbee.println(etat);               // envoi radio : PIR:1 ou PIR:0
  Serial.print("PIR:");
  Serial.println(etat);             // affichage sur le moniteur série
  dernierEnvoi = millis();
}

void setup() {
  pinMode(PIR, INPUT);
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  xbee.begin(9600);
  delay(30000);                     // stabilisation du capteur
  Serial.println("Capteur de presence pret.");
}

void loop() {
  int presence = digitalRead(PIR);
  digitalWrite(LED, presence);

  if (presence != etatPrecedent) {  // envoi au changement d'état
    envoyer(presence);
    etatPrecedent = presence;
  }

  if (millis() - dernierEnvoi > RAPPEL) {  // rappel périodique
    envoyer(presence);
  }

  delay(50);
}