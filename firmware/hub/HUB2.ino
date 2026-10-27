
#include <SoftwareSerial.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// =====================================
// HUB CENTRAL XBEE - MY = 2
// Mode transparent AT - Broadcast
// =====================================

SoftwareSerial xbee(2, 3); // RX, TX

// =====================================
// TABLE D'ADRESSAGE
// =====================================

struct Route {
  const char* nom;
  byte adresse;
};

Route routes[] = {
  {"POT",      0},
  {"HUB",      2},
  {"LCD",      3},
  {"TP7",      4},
  {"PRESENCE", 5},
  {"SERVO",    6}
};

const byte NB_ROUTES =
  sizeof(routes) / sizeof(routes[0]);

// =====================================
// VARIABLES
// =====================================

char bufferRadio[80];
char bufferPC[80];

byte posRadio = 0;
byte posPC = 0;

int derniereValeurPOT = -1;
int dernierEtatPresence = -1;

unsigned long dernierEnvoiPOT = 0;
const unsigned long PERIODE_POT = 500;

unsigned long nombreRX = 0;
unsigned long nombreTX = 0;
unsigned long nombreRelais = 0;

// =====================================
// TABLE DE ROUTAGE
// =====================================

void afficherTable() {

  Serial.println();
  Serial.println("=== TABLE DE ROUTAGE ===");

  for (byte i = 0; i < NB_ROUTES; i++) {
    Serial.print(routes[i].nom);
    Serial.print(" -> MY = ");
    Serial.println(routes[i].adresse);
  }

  Serial.println("========================");
}

// =====================================
// TRANSMISSION STRUCTUREE
// =====================================

void transmettre(const char* dest,
                 const char* type,
                 const char* valeur) {

  bool trouve = false;

  for (byte i = 0; i < NB_ROUTES; i++) {
    if (strcmp(dest, routes[i].nom) == 0) {
      trouve = true;
      break;
    }
  }

  if (!trouve && strcmp(dest, "ALL") != 0) {
    Serial.println("[ERREUR] Destination inconnue");
    return;
  }

  char message[80];

  int n = snprintf(
    message, sizeof(message),
    "HUB;%s;%s;%s",
    dest, type, valeur
  );

  if (n < 0 || n >= (int)sizeof(message)) {
    Serial.println("[ERREUR] Trame trop longue");
    return;
  }

  xbee.println(message);
  nombreTX++;

  Serial.print("[TX] ");
  Serial.println(message);
}

// =====================================
// TRANSMISSION SIMPLE
// =====================================

void transmettreBrut(const char* message) {

  xbee.println(message);
  nombreTX++;

  Serial.print("[TX BROADCAST] ");
  Serial.println(message);
}

// =====================================
// TRAITEMENT POTENTIOMETRE
// =====================================

void traiterPOT(int valeur) {

  if (valeur < 0 || valeur > 1023) {
    Serial.println("[ERREUR] POT hors plage");
    return;
  }

  nombreRX++;

  Serial.print("[RX POT] POT:");
  Serial.println(valeur);

  unsigned long maintenant = millis();

  bool changement =
    derniereValeurPOT < 0 ||
    abs(valeur - derniereValeurPOT) >= 3;

  if (changement &&
      (derniereValeurPOT < 0 ||
       maintenant - dernierEnvoiPOT >= PERIODE_POT)) {

    char message[20];

    snprintf(
      message,
      sizeof(message),
      "POT:%d",
      valeur
    );

    // Le LCD et TP7 recoivent le Broadcast
    transmettreBrut(message);

    Serial.println("[ROUTAGE] POT -> LCD + TP7");

    derniereValeurPOT = valeur;
    dernierEnvoiPOT = maintenant;
  }
}

// =====================================
// TRAITEMENT PRESENCE
// =====================================

void traiterPresence(int etat) {

  if (etat != 0 && etat != 1) return;

  nombreRX++;

  Serial.print("[RX PRESENCE] PIR:");
  Serial.println(etat);

  // Envoyer seulement si changement
  if (etat != dernierEtatPresence) {

    dernierEtatPresence = etat;

    char message[10];

    snprintf(
      message,
      sizeof(message),
      "PIR:%d",
      etat
    );

    // LCD recoit le message
    transmettreBrut(message);

    Serial.println("[ROUTAGE] PIR -> LCD");
  }
}

// =====================================
// TRAITEMENT DU RELAIS TP7
// =====================================

void traiterRelais(char* ligne) {

  nombreRX++;
  nombreRelais++;

  Serial.print("[RX RELAIS TP7] ");
  Serial.println(ligne);

  // Format :
  // TP7;HUB;TYPE;VALEUR

  char copie[80];

  strncpy(
    copie,
    ligne,
    sizeof(copie) - 1
  );

  copie[sizeof(copie) - 1] = '\0';

  char* source = strtok(copie, ";");
  char* dest = strtok(NULL, ";");
  char* type = strtok(NULL, ";");
  char* valeur = strtok(NULL, ";");

  if (!source || !dest || !type || !valeur) {
    Serial.println("[TP7] Format incorrect");
    return;
  }

  // Presence envoyee par TP7
  if (strcmp(type, "PIR") == 1 ||
      strcmp(type, "PRESENCE") == 0) {

    if (strcmp(valeur, "0") == 0 ||
        strcmp(valeur, "1") == 0) {

      traiterPresence(atoi(valeur));
    }

    return;
  }

  // Retour servomoteur
  if (strcmp(type, "SERVO") == 0) {

    Serial.print("[ETAT SERVO] ");
    Serial.println(valeur);
    return;
  }

  // Retour buzzer
  if (strcmp(type, "BUZZER") == 0) {

    Serial.print("[ETAT BUZZER] ");
    Serial.println(valeur);
    return;
  }

  // Etat general du relais
  if (strcmp(type, "STATUS") == 0) {

    Serial.print("[ETAT TP7] ");
    Serial.println(valeur);
    return;
  }

  Serial.print("[TP7 TYPE INCONNU] ");
  Serial.println(type);
}

// =====================================
// ANALYSE DES MESSAGES XBEE
// =====================================

void traiterMessage(char* ligne) {

  // Ignorer les trames structurees du HUB
  if (strncmp(ligne, "HUB;", 4) == 0)
    return;

  // =====================================
  // MESSAGES DU RELAIS TP7
  // =====================================

  if (strncmp(ligne, "TP7;HUB;", 8) == 0) {

    traiterRelais(ligne);
    return;
  }

  // Retours directs des actionneurs
  if (strncmp(ligne, "SERVO;", 6) == 0 ||
      strncmp(ligne, "LCD;", 4) == 0) {

    Serial.print("[RX RETOUR] ");
    Serial.println(ligne);

    nombreRX++;
    return;
  }

  // =====================================
  // POTENTIOMETRE : POT:390
  // =====================================

  if (strncmp(ligne, "POT:", 4) == 0) {

    char* p = ligne + 4;

    while (*p == ' ') p++;

    char* fin;
    long valeur = strtol(p, &fin, 10);

    while (isspace((unsigned char)*fin))
      fin++;

    if (fin != p &&
        *fin == '\0' &&
        valeur >= 0 &&
        valeur <= 1023) {

      traiterPOT((int)valeur);
    }

    return;
  }

  // =====================================
  // PRESENCE : PRESENCE:1
  // =====================================

  if (strncmp(ligne, "PRESENCE:", 9) == 0) {

    char* p = ligne + 9;
    while (*p == ' ') p++;

    if (strcmp(p, "0") == 0)
      traiterPresence(0);

    else if (strcmp(p, "1") == 0)
      traiterPresence(1);

    return;
  }

  // =====================================
  // PRESENCE : PIR:1 ou IR:1
  // =====================================

  if (strncmp(ligne, "PIR:", 4) == 0 ||
      strncmp(ligne, "IR:", 3) == 0) {

    char* valeur = strchr(ligne, ':') + 1;

    while (*valeur == ' ')
      valeur++;

    if (strcmp(valeur, "0") == 0)
      traiterPresence(0);

    else if (strcmp(valeur, "1") == 0)
      traiterPresence(1);

    else {
      Serial.print("[IR INCOMPLET] ");
      Serial.println(ligne);
    }

    return;
  }

  // =====================================
  // PRESENCE : 0 ou 1
  // =====================================

  if (strcmp(ligne, "0") == 0) {
    traiterPresence(0);
    return;
  }

  if (strcmp(ligne, "1") == 0) {
    traiterPresence(1);
    return;
  }

  // =====================================
  // MESSAGE INCONNU
  // =====================================

  Serial.print("[RX INCONNU] ");
  Serial.println(ligne);
}

// =====================================
// LECTURE SERIE
// =====================================

bool lireLigne(char c,
               char* buffer,
               byte &pos) {

  if (c == '\r' || c == '\n') {

    if (pos == 0)
      return false;

    buffer[pos] = '\0';
    pos = 0;

    return true;
  }

  if (pos < 79) {
    buffer[pos++] = c;
  }
  else {
    pos = 0;
    Serial.println("[ERREUR] Buffer plein");
  }

  return false;
}

// =====================================
// COMMANDES DU MONITEUR SERIE
// =====================================

void commandePC(char* ligne) {

  if (strcmp(ligne, "TABLE") == 0) {
    afficherTable();
    return;
  }

  if (strcmp(ligne, "LOGS") == 0) {

    Serial.println();
    Serial.println("=== STATISTIQUES HUB ===");

    Serial.print("Messages RX = ");
    Serial.println(nombreRX);

    Serial.print("Messages TX = ");
    Serial.println(nombreTX);

    Serial.print("Messages TP7 = ");
    Serial.println(nombreRelais);

    Serial.print("Dernier POT = ");
    Serial.println(derniereValeurPOT);

    Serial.print("Derniere PRESENCE = ");
    Serial.println(dernierEtatPresence);

    return;
  }

  if (strcmp(ligne, "PINGALL") == 0) {

    transmettre("ALL", "PING", "1");
    return;
  }

  // Commandes manuelles :
  // TP7;STATUS;OK
  // LCD;TEXT;Bonjour

  char* dest = strtok(ligne, ";");
  char* type = strtok(NULL, ";");
  char* val = strtok(NULL, ";");

  if (dest && type && val) {
    transmettre(dest, type, val);
  }
  else {
    Serial.println("[ERREUR] Commande incorrecte");
  }
}

// =====================================
// INITIALISATION
// =====================================

void setup() {

  Serial.begin(9600);
  xbee.begin(9600);

  Serial.println();
  Serial.println("================================");
  Serial.println(" HUB CENTRAL XBEE - ROUTAGE");
  Serial.println("================================");

  Serial.println("MY = 2");
  Serial.println("ID = 1234");
  Serial.println("CH = C");
  Serial.println("DL = FFFF");
  Serial.println("AP = 0");

  afficherTable();

  Serial.println();
  Serial.println("ROUTAGE AUTOMATIQUE :");
  Serial.println("POT -> LCD + TP7");
  Serial.println("PRESENCE TP7 -> HUB -> LCD");

  Serial.println();
  Serial.println("COMMANDES PC :");
  Serial.println("TABLE");
  Serial.println("LOGS");
  Serial.println("PINGALL");
  Serial.println("TP7;PING;1");
  Serial.println("LCD;TEXT;Bonjour");

  Serial.println();
  Serial.println("[HUB PRET]");
}

// =====================================
// BOUCLE PRINCIPALE
// =====================================

void loop() {

  // =====================================
  // RECEPTION DES MESSAGES XBEE
  // =====================================

  while (xbee.available()) {

    char c = xbee.read();

    if (lireLigne(
          c,
          bufferRadio,
          posRadio)) {

      traiterMessage(bufferRadio);
    }
  }

  // =====================================
  // COMMANDES DEPUIS LE PC
  // =====================================

  while (Serial.available()) {

    char c = Serial.read();

    if (lireLigne(
          c,
          bufferPC,
          posPC)) {

      commandePC(bufferPC);
    }
  }
}
