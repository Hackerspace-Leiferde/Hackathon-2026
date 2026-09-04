# ESP8266 Raumklima-Anzeige

Misst Temperatur, Luftfeuchte und Luftdruck und zeigt die Werte auf einem
OLED-Display an. Die Messgrößen werden aus den vorhandenen Sensoren
zusammengesetzt – welche angeschlossen sind, erkennt das Programm beim Start
selbst.

| Sensorbestückung | Temperatur | Feuchte | Druck |
|---|---|---|---|
| BME280 allein | BME280 | BME280 | BME280 |
| **BMP280 + DHT22** (aktuell) | BMP280 | DHT22 | BMP280 |
| BMP280 allein | BMP280 | – | BMP280 |
| DHT22 allein | DHT22 | DHT22 | – |

Bei der aktuellen Bestückung liefert der BMP280 die Temperatur. Der DHT22 wäre
mit ±0,5 K der genauere Fühler – die beiden Messwerte liegen jedoch nur rund
1,4 K auseinander. Wer den DHT22 bevorzugt, tauscht in `loop()` die Reihenfolge
der Quellen.

**Was automatisch erkannt wird und was nicht:** Der Bosch-Sensor meldet seine
Chip-ID über I2C, `boschStarten()` unterscheidet BME280 und BMP280 deshalb zur
Laufzeit – ein Austausch braucht keine Codeänderung, ein Reset genügt (siehe
[BMP280 gegen BME280 tauschen](#bmp280-gegen-bme280-tauschen)). Der DHT hat
kein solches Kennungsfeld und muss daher als `DHT_TYP` fest einkompiliert sein
(siehe [DHT11 oder DHT22?](#dht11-oder-dht22-den-typ-richtig-setzen)).

---

## Hardwarekomponenten

| Komponente | Typ | Anschluss | Bemerkung |
|---|---|---|---|
| Mikrocontroller | LOLIN/Wemos D1 mini (V4, USB-C) | – | ESP8266EX, 4 MB Flash, CH340-USB-Chip |
| Display | SH1106 OLED, 1,3", 128×64 | I2C `0x3C` | **Nicht** SSD1306 – anderer Treiber nötig |
| Sensor (verbaut) | BMP280 | I2C `0x76` | Chip-ID `0x58` – Temperatur + Druck |
| Feuchtesensor | DHT22 / AM2302 | GPIO14 (D5) | Eigenes Protokoll, kein I2C; **weißes** Gehäuse |
| Sensor (Alternative) | BME280 | I2C `0x76` | Chip-ID `0x60` – ersetzt BMP280 **und** DHT22 |

Alle Bausteine werden mit **3,3 V** versorgt. Die 5-V-Pins des D1 mini werden
nicht benötigt – die GPIOs des ESP8266 sind nicht 5-V-fest.

---

## Verkabelung

Display und Bosch-Sensor teilen sich den I2C-Bus. Der DHT22 spricht ein eigenes
Protokoll und bekommt deshalb eine separate Datenleitung.

```
   D1 mini                    BMP280        SH1106 OLED      DHT22
 ┌──────────┐
 │      3V3 ├──────────┬───────── VCC ───────── VCC ───────────  +
 │        G ├──────────┼───────── GND ───────── GND ───────────  -
 │  D2/GPIO4├──────────┼───────── SDA ───────── SDA
 │  D1/GPIO5├──────────┼───────── SCL ───────── SCL
 │ D5/GPIO14├──────────┼──────────────────────────────────────── DATA
 └──────────┘          │
                       └─ gemeinsamer I2C-Bus
```

### I2C-Bus (Display und Bosch-Sensor)

| D1 mini | GPIO | Funktion | Sensor | Display |
|---|---|---|---|---|
| `3V3` | – | Versorgung | `VCC` / `VIN` | `VCC` |
| `G` | – | Masse | `GND` | `GND` |
| `D2` | GPIO4 | I2C-Daten (SDA) | `SDA` / `SDI` | `SDA` |
| `D1` | GPIO5 | I2C-Takt (SCL) | `SCL` / `SCK` | `SCL` |

D1 und D2 sind die Standard-I2C-Pins des ESP8266. Pull-up-Widerstände sind auf
den üblichen Breakout-Boards bereits verbaut.

### DHT22 / AM2302

**3-Pin-Modul** (Sensor auf Platine, Pull-up bereits verbaut):

| DHT-Modul | D1 mini |
|---|---|
| `+` / `VCC` | `3V3` |
| `S` / `DATA` / `OUT` | `D5` |
| `-` / `GND` | `G` |

**Nacktes 4-Pin-Bauteil** (Gittergehäuse, Blick auf das Gitter, von links):

| Pin | Anschluss |
|---|---|
| 1 | `3V3` |
| 2 | `D5` |
| 3 | bleibt frei |
| 4 | `G` |

Hier muss zusätzlich ein **Pull-up von 4,7 kΩ–10 kΩ zwischen Pin 1 und Pin 2**
eingesetzt werden, sonst kommt keine Kommunikation zustande.

> **Warum D5?** D3 (GPIO0), D4 (GPIO2) und D8 (GPIO15) sind Boot-Pins des
> ESP8266 und müssen beim Start definierte Pegel haben. An D8 würde der Pull-up
> des DHT22 den Start sogar verhindern. D5, D6 und D7 sind frei von solchen
> Nebenwirkungen.

---

## Anzeige auf dem Display

| Position | mit Feuchtequelle | ohne Feuchtequelle |
|---|---|---|
| Kopfzeile | `Raumklima` | `Raumklima` |
| Großwert oben | Temperatur (`23.4 °C`) | Temperatur (`23.4 °C`) |
| Großwert unten | **Luftfeuchte** (`45 %rF`) | Luftdruck (`1007 hPa`) |
| Fußzeile | Luftdruck (`1007 hPa`) | Hinweis `keine Feuchte` |

Jede Zeile wird über `zentriert()` horizontal mittig auf dem Display platziert.
Die Funktion vermisst den Text vorher mit `getTextBounds()`, statt die Position
fest zu verdrahten – wechselt ein Wert die Stellenzahl (`9.8 °C` → `10.1 °C`,
`45 %` → `100 %`), bleibt die Zentrierung ohne weiteres Zutun korrekt.

Die Werte werden alle 2 Sekunden aktualisiert und parallel über die serielle
Schnittstelle ausgegeben (115200 Baud). Der DHT22 verträgt nicht mehr als etwa
eine Messung alle zwei Sekunden – das Intervall liegt bewusst darüber.

---

## Software

| Bibliothek | Version | Zweck |
|---|---|---|
| `adafruit/Adafruit SH110X` | 2.1.15 | Treiber für das SH1106-OLED |
| `adafruit/DHT sensor library` | 1.4.7 | DHT22-Protokoll |
| `adafruit/Adafruit BME280 Library` | 2.3.0 | Sensor mit Feuchte |
| `adafruit/Adafruit BMP280 Library` | 3.0.0 | Sensor ohne Feuchte |
| `adafruit/Adafruit GFX Library` | 1.12.6 | Grafikprimitive und Schriften |
| `adafruit/Adafruit Unified Sensor` | 1.1.15 | Abhängigkeit der Sensortreiber |
| `Adafruit BusIO` | 1.17.4 | Wird automatisch mitinstalliert |

### Bauen und Flashen

```bash
pio run                  # nur kompilieren
pio run --target upload  # kompilieren und auf das Board schreiben
pio device monitor       # Messwerte mitlesen (115200 Baud)
```

Der Upload-Port wird normalerweise automatisch gefunden. Falls nicht:

```bash
pio device list                                        # Port ermitteln
pio run --target upload --upload-port /dev/cu.usbserial-210
```

---

## DHT11 oder DHT22? Den Typ richtig setzen

Beide Sensoren sind pinkompatibel und äußerlich fast gleich, kodieren ihre Daten
aber unterschiedlich. Mit falsch gesetztem `DHT_TYP` liefert die Bibliothek
stillschweigend **falsche Werte statt einer Fehlermeldung**:

| | DHT11 | DHT22 / AM2302 |
|---|---|---|
| Gehäusefarbe | blau | **weiß** |
| Feuchte-Genauigkeit | ±5 % rF | ±2–5 % rF |
| Messbereich Feuchte | 20–90 % rF | 0–100 % rF |
| Temperatur-Genauigkeit | ±2 °C | ±0,5 °C |
| Auflösung Feuchte | 1 % | 0,1 % |

Verbaut ist ein **DHT22**; entsprechend steht `DHT_TYP` in `src/main.cpp` auf
`DHT22`.

**So erkennt man einen Typ-Irrtum:** Ein DHT22, der als DHT11 ausgelesen wird,
meldete hier 13,9 % rF und −1,0 °C – bei tatsächlich 62 % rF und 24 °C. Die
unplausible Temperatur ist das verlässlichere Signal, weil sie sich direkt gegen
den BMP280 prüfen lässt. Weicht die DHT-Temperatur um mehr als ein paar Kelvin
von der BMP280-Temperatur ab, stimmt der Typ nicht.

Die Feuchte wird bewusst **ohne Nachkommastelle** angezeigt: Bei ±2–5 %
Genauigkeit wäre eine Nachkommastelle Scheingenauigkeit.

---

## BMP280 gegen BME280 tauschen

Ein **BME280** vereint alle drei Messgrößen in einem Baustein und macht den
DHT22 überflüssig. Der verbaute **BMP280** besitzt kein Feuchte-Messelement –
das ist eine Eigenschaft der Hardware und lässt sich nicht per Software umgehen.

### 1. Beim Kauf auf den richtigen Chip achten

Die beiden Module sehen praktisch identisch aus und werden häufig verwechselt
oder falsch deklariert verkauft:

| | BMP280 | BME280 |
|---|---|---|
| Chip-ID | `0x58` | `0x60` |
| Messgrößen | Temperatur, Druck | Temperatur, Druck, **Feuchte** |
| Gehäusemaße | 2,0 × 2,5 mm | 2,5 × 2,5 mm (quadratisch) |
| Preis | günstiger | teurer |

Nicht selten sitzt auf einer mit "BME280" beschrifteten Platine tatsächlich ein
BMP280. Die Chip-ID im seriellen Log entlarvt das sofort.

### 2. Sensor austauschen

Die Verkabelung bleibt **unverändert** – gleiche vier Leitungen, gleiche Pins,
gleiche I2C-Adresse `0x76`.

> Liegt der Lötjumper `SDO` des neuen Moduls auf VCC, meldet sich der Sensor auf
> `0x77` statt `0x76`. Der Code probiert beide Adressen, ein Eingriff ist also
> auch dann nicht nötig.

### 3. Am Code ist nichts zu ändern

`src/main.cpp` sucht beim Start zuerst nach einem BME280 und fällt erst dann auf
den BMP280 zurück. Alle Bibliotheken sind bereits in `platformio.ini`
eingetragen. Nach dem Tausch genügt ein Reset des Boards.

Der DHT22 kann angeschlossen bleiben – sobald ein BME280 die Feuchte liefert,
wird dessen Wert bevorzugt. Er darf aber ebenso entfernt werden.

### 4. Erfolg prüfen

Im seriellen Monitor muss nach dem Neustart stehen:

```
Scanne I2C-Bus...
  Geraet auf 0x3C
  Geraet auf 0x76
SH1106 bereit.
BME280 erkannt - Temperatur, Feuchte und Druck.
23.4 C   45 %   1007 hPa
```

Erscheint stattdessen weiterhin `BMP280 erkannt`, sitzt trotz Beschriftung der
falsche Chip auf der Platine.

---

## Fehlersuche

| Symptom | Ursache und Abhilfe |
|---|---|
| `Nichts gefunden` beim I2C-Scan | SDA/SCL vertauscht, Masse fehlt oder Versorgung nicht angeschlossen |
| `Kein DHT an D5` | Datenleitung an falschem Pin, oder Pull-up fehlt (nacktes Bauteil) |
| DHT liefert sporadisch keine Werte | Leitung zu lang oder ungeschirmt – unter 20 cm bleiben |
| Feuchte unplausibel, DHT-Temperatur weit neben BMP280 | Falscher `DHT_TYP` – siehe Abschnitt oben |
| Display bleibt dunkel, Scan zeigt `0x3C` | Kontrast/Defekt, oder es ist ein SSD1306 statt SH1106 verbaut |
| Bild um 2 Pixel verschoben | Es ist ein SSD1306 – dann `Adafruit_SSD1306` statt `Adafruit_SH110X` verwenden |
| Display meldet sich auf `0x3D` | Adresse in `ADRESSE_DISPLAY` anpassen |
| `Kein Sensor!` auf dem Display | Weder Bosch-Sensor noch DHT22 antworten – Verkabelung prüfen |
| Board erscheint nicht als `/dev/cu.*` | CH340-Treiber fehlt, oder USB-Kabel ohne Datenleitungen |
