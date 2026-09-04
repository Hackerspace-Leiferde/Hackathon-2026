#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>

// ---------------------------------------------------------------------------
// Raumklima-Anzeige auf SH1106-OLED
//
// Verkabelung
//   I2C-Bus (Display + Bosch-Sensor gemeinsam):
//     SDA -> D2 (GPIO4)      SCL -> D1 (GPIO5)
//     VCC -> 3V3             GND -> G
//   DHT22/AM2302 (eigene Datenleitung, kein I2C):
//     DATA -> D5 (GPIO14)    VCC -> 3V3    GND -> G
//
// Die Messgroessen werden aus den vorhandenen Sensoren zusammengesetzt:
//   BME280  - Temperatur, Feuchte, Druck (alles aus einer Quelle)
//   BMP280  - Temperatur und Druck, keine Feuchte
//   DHT22   - Temperatur und Feuchte, kein Druck
// Liegen BMP280 und DHT22 zusammen an, kommt die Temperatur vom BMP280
// und die Feuchte vom DHT22. Der DHT22 ist mit +-0,5 K zwar der genauere
// Temperaturfuehler, die Werte liegen hier aber nur 1,4 K auseinander -
// wer den DHT22 bevorzugt, tauscht in loop() die Reihenfolge der Quellen.
// ---------------------------------------------------------------------------

#define BILDSCHIRM_BREITE 128
#define BILDSCHIRM_HOEHE   64
#define OLED_RESET         -1   // I2C-Module haben keinen eigenen Reset-Pin

#define DHT_PIN            D5   // GPIO14 - kein Boot-Pin, daher unkritisch
#define DHT_TYP            DHT22   // weisses Gehaeuse = DHT22/AM2302 (blau waere DHT11)

#define GRAD_ZEICHEN       248  // Grad-Symbol im CP437-Zeichensatz

// Das SH1106 ist intern 132 Pixel breit; die Bibliothek gleicht den
// Versatz von 2 Pixeln gegenueber dem SSD1306 selbst aus.
Adafruit_SH1106G display(BILDSCHIRM_BREITE, BILDSCHIRM_HOEHE, &Wire, OLED_RESET);

Adafruit_BME280 bme;
Adafruit_BMP280 bmp(&Wire);
DHT dht(DHT_PIN, DHT_TYP);

enum BoschTyp { BOSCH_KEINER, BOSCH_BME280, BOSCH_BMP280 };
BoschTyp boschTyp = BOSCH_KEINER;

static const uint8_t ADRESSE_DISPLAY  = 0x3C;  // manche Module: 0x3D
static const uint8_t ADRESSE_SENSOR_A = 0x76;  // Standard
static const uint8_t ADRESSE_SENSOR_B = 0x77;  // wenn SDO auf VCC liegt

static const unsigned long MESSINTERVALL_MS = 2000;

bool displayBereit = false;
bool dhtBereit     = false;
unsigned long letzteMessung = 0;

// Schreibt eine Zeile horizontal mittig. Die tatsaechliche Textbreite
// liefert getTextBounds - so bleibt die Zentrierung auch dann korrekt,
// wenn sich die Stellenzahl eines Messwerts aendert (z.B. 9.8 -> 10.1).
void zentriert(const char *text, int16_t y, uint8_t groesse) {
  int16_t x1, y1;
  uint16_t breite, hoehe;

  display.setTextSize(groesse);
  display.getTextBounds(text, 0, y, &x1, &y1, &breite, &hoehe);
  display.setCursor((BILDSCHIRM_BREITE - (int16_t)breite) / 2, y);
  display.print(text);
}

// Scannt den Bus und meldet alle gefundenen Adressen ueber die serielle
// Schnittstelle - die schnellste Diagnose bei Verkabelungsproblemen.
void i2cScannen() {
  Serial.println(F("Scanne I2C-Bus..."));
  uint8_t anzahl = 0;

  for (uint8_t adresse = 1; adresse < 127; adresse++) {
    Wire.beginTransmission(adresse);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Geraet auf 0x%02X\n", adresse);
      anzahl++;
    }
  }

  if (anzahl == 0) {
    Serial.println(F("  Nichts gefunden - SDA/SCL, Masse und 3V3 pruefen."));
  }
}

// Erst BME280 versuchen (kann Feuchte), dann BMP280 als Rueckfallebene.
// Beide sitzen je nach Modul auf 0x76 oder 0x77.
BoschTyp boschStarten() {
  if (bme.begin(ADRESSE_SENSOR_A, &Wire) || bme.begin(ADRESSE_SENSOR_B, &Wire)) {
    Serial.println(F("BME280 erkannt - Temperatur, Feuchte und Druck."));
    return BOSCH_BME280;
  }

  if (bmp.begin(ADRESSE_SENSOR_A) || bmp.begin(ADRESSE_SENSOR_B)) {
    Serial.println(F("BMP280 erkannt - Temperatur und Druck, keine Feuchte."));
    return BOSCH_BMP280;
  }

  Serial.println(F("Kein Bosch-Sensor am I2C-Bus."));
  return BOSCH_KEINER;
}

// Der DHT meldet sich nicht von selbst - ein gelungener Lesevorgang ist
// der einzige Nachweis, dass er angeschlossen ist. Nach dem Anlegen der
// Versorgung braucht er rund eine Sekunde, bis er antwortet.
bool dhtStarten() {
  dht.begin();
  delay(1100);

  for (uint8_t versuch = 0; versuch < 3; versuch++) {
    if (!isnan(dht.readHumidity())) {
      Serial.println(F("DHT erkannt - liefert die Luftfeuchte."));
      return true;
    }
    delay(2000);   // der DHT vertraegt nur etwa eine Messung pro Sekunde
  }

  Serial.println(F("Kein DHT an D5 - Datenleitung und Pull-up pruefen."));
  return false;
}

void meldungAnzeigen(const char *zeile1, const char *zeile2) {
  if (!displayBereit) return;

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  zentriert(zeile1, 24, 1);
  zentriert(zeile2, 36, 1);
  display.display();
}

// Temperatur steht immer oben gross. Darunter kommt die Feuchte, sofern
// eine Quelle sie liefert - sonst rueckt der Druck auf diesen Platz.
void werteAnzeigen(float temperatur, float feuchte, float druck) {
  if (!displayBereit) return;

  const bool hatFeuchte = !isnan(feuchte);
  const bool hatDruck   = !isnan(druck);
  char zeile[24];

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // Kopfzeile mit Trennlinie
  zentriert("Raumklima", 0, 1);
  display.drawFastHLine(0, 10, BILDSCHIRM_BREITE, SH110X_WHITE);

  // Temperatur in doppelter Groesse
  snprintf(zeile, sizeof(zeile), "%.1f%cC", temperatur, (char)GRAD_ZEICHEN);
  zentriert(zeile, 16, 2);

  // Zweiter Grosswert: Feuchte, ersatzweise der Druck
  if (hatFeuchte) {
    snprintf(zeile, sizeof(zeile), "%.0f %%rF", feuchte);
  } else if (hatDruck) {
    snprintf(zeile, sizeof(zeile), "%.0f hPa", druck);
  } else {
    snprintf(zeile, sizeof(zeile), "--");
  }
  zentriert(zeile, 38, 2);

  // Fusszeile: der Druck, sofern er nicht schon oben steht
  if (hatFeuchte && hatDruck) {
    snprintf(zeile, sizeof(zeile), "%.0f hPa", druck);
    zentriert(zeile, 56, 1);
  } else if (!hatFeuchte) {
    zentriert("keine Feuchte", 56, 1);
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(F("=== ESP8266 Raumklima-Anzeige ==="));

  // Ohne Argumente nimmt Wire beim ESP8266 bereits GPIO4/GPIO5,
  // hier trotzdem explizit - so bleibt die Belegung im Code sichtbar.
  Wire.begin(D2, D1);

  i2cScannen();

  displayBereit = display.begin(ADRESSE_DISPLAY, true);
  if (displayBereit) {
    display.cp437(true);          // schaltet das Grad-Zeichen frei
    display.clearDisplay();
    display.display();
    Serial.println(F("SH1106 bereit."));
  } else {
    Serial.println(F("Display antwortet nicht - Adresse 0x3C/0x3D pruefen."));
  }

  boschTyp = boschStarten();
  dhtBereit = dhtStarten();

  if (boschTyp == BOSCH_KEINER && !dhtBereit) {
    meldungAnzeigen("Kein Sensor!", "Verkabelung?");
  }

  // Erste Messung sofort statt erst nach dem Intervall.
  letzteMessung = millis() - MESSINTERVALL_MS;
}

void loop() {
  // Die Subtraktion vorzeichenloser Werte bleibt auch beim
  // millis()-Ueberlauf nach ~49 Tagen korrekt.
  if (millis() - letzteMessung < MESSINTERVALL_MS) return;
  letzteMessung = millis();

  float temperatur = NAN;
  float feuchte    = NAN;
  float druck      = NAN;

  // Bosch-Sensor: Temperatur und Druck, beim BME280 zusaetzlich die Feuchte.
  if (boschTyp == BOSCH_BME280) {
    temperatur = bme.readTemperature();
    feuchte    = bme.readHumidity();
    druck      = bme.readPressure() / 100.0F;   // Pa -> hPa
  } else if (boschTyp == BOSCH_BMP280) {
    temperatur = bmp.readTemperature();
    druck      = bmp.readPressure() / 100.0F;
  }

  // Der DHT ergaenzt die Feuchte und springt bei der Temperatur ein,
  // falls kein Bosch-Sensor vorhanden ist.
  if (dhtBereit) {
    float dhtFeuchte = dht.readHumidity();
    if (!isnan(dhtFeuchte)) {
      if (isnan(feuchte)) feuchte = dhtFeuchte;
      if (isnan(temperatur)) temperatur = dht.readTemperature();
    }
  }

  if (isnan(temperatur)) {
    Serial.println(F("Kein Messwert - Sensoren antworten nicht."));
    meldungAnzeigen("Kein Messwert", "Verbindung weg");

    // Erneut suchen, falls ein Sensor erst spaeter Kontakt bekommt.
    if (boschTyp == BOSCH_KEINER) boschTyp = boschStarten();
    return;
  }

  Serial.printf("%.1f C   ", temperatur);
  if (isnan(feuchte)) Serial.print(F("---- %   ")); else Serial.printf("%.0f %%   ", feuchte);
  if (isnan(druck))   Serial.println(F("---- hPa")); else Serial.printf("%.0f hPa\n", druck);

  werteAnzeigen(temperatur, feuchte, druck);
}
