# Bibliothèques ESP32

<p align="center">
  <img src="https://img.shields.io/badge/platform-ESP32--S3-blue" />
  <img src="https://img.shields.io/github/license/Marcussacapuces91/Librairie-ESP32" />
  <!-- <img src="https://img.shields.io/github/v/tag/Marcussacapuces91/Librairie-ESP32" /> -->
  <img src="https://img.shields.io/github/last-commit/Marcussacapuces91/Librairie-ESP32" />
</p>

Ensemble de composants C++ modernes et entièrement RAII destinés à simplifier le développement sur ESP32 (WiFi, synchronisation SNTP, affichage RGB).

### Bibliothèques disponibles

- [wifi-manager.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/wifi-manager.h) — gestion WiFi et connexion automatique
- [time-manager.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/time-manager.h) — synchronisation SNTP et fuseau horaire
- [adc-reader.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/adc-reader.h) — lecture ADC calibrée sur GPIO ESP32
- [http-client.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/http-client.h) — client HTTP GET simple et robuste
- [temp-sensor.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/temp-sensor.h) — capteur de température interne ESP32
- [esp32-4848S040-driver.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/esp32-4848S040-driver.h) — pilote d’écran RGB 480×480

---

## WiFi Manager

[wifi-manager.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/wifi-manager.h) est un contrôleur WiFi moderne et entièrement RAII conçu pour l’ESP32 en mode **STA**, avec prise en charge automatique de la connexion, de la récupération d’IP et de la gestion des événements. Il encapsule la logique du WiFi et réduit les risques de fuite de ressources ou de code de connexion répétitif.

### Caractéristiques principales

- **RAII complet** : initialisation automatique du stack WiFi dans le constructeur, nettoyage dans le destructeur.  
- **Événementiel** : tous les événements WiFi sont gérés via des méthodes virtuelles protégées, surchargeables dans les classes dérivées.  
- **SmartConfig automatique** : bascule vers ESP‑Touch après 3 échecs de connexion.  
- **Thread-safe** : synchronisation via sémaphores FreeRTOS.

### Exemple d’utilisation

```cpp
#include "wifi-manager.h"

WifiManager wifi;

void setup() {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi.begin();

    // Connexion et attente d’une adresse IP (timeout 30 s par défaut)
    auto ip_info = wifi.connectAndWaitIP();

    // Informations sur l’AP connecté
    auto ap = wifi.getAPInfo();
}
```

### Extensibilité

```cpp
class MyWifiManager : public WifiManager {
protected:
    void onGotIp(const ip_event_got_ip_t& evt) override {
        WifiManager::onGotIp(evt);  // Appel du parent
        startMyServices();          // Actions personnalisées
        ledBlink(GREEN);            // Retour visuel
    }
};
```

---

## Time Manager

[time-manager.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/time-manager.h) est un contrôleur C++ RAII dédié à la synchronisation de l’heure via **SNTP** sur ESP32.  
Il encapsule les API ESP‑IDF et garantit que l’horloge système est correcte avant de lancer des services sensibles (TLS, planification, logs horodatés…).

### Fonctionnalités principales

- **RAII complet** : initialisation SNTP et ressources FreeRTOS dans le constructeur, libération dans le destructeur.  
- **Synchronisation bloquante** : `syncAndWait()` suspend la tâche jusqu’à réception d’une mise à jour NTP valide (ou `abort()` en cas de timeout).  
- **Thread-safe** : atomiques + sémaphore binaire pour gérer l’état et les signaux asynchrones.  
- **Callback intégré** : installation automatique du callback SNTP (`sntp_time_callback`).  
- **Compteur de synchronisations** : suivi du nombre de mises à jour NTP réussies.  
- **Gestion du fuseau horaire** : via une chaîne POSIX (`setTimezone()`), avec support DST.  
- **Formats ISO8601** : récupération de l’heure locale ou UTC sous forme normalisée.

### Exemple d’utilisation

```cpp
WifiManager wifi;
wifi.connectAndWaitIP();

TimeManager timeMgr("pool.ntp.org");
timeMgr.syncAndWait(30000);

ESP_LOGI("APP", "System time: %s", timeMgr.getLocal_ISO8601().c_str());
```

---

## ADC Reader

[adc-reader.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/adc-reader.h) fournit un wrapper RAII pour lire des entrées analogiques sur les GPIO ESP32. La classe détermine automatiquement l’unité ADC et le canal associé au GPIO, configure le atténuateur et la largeur des données, puis applique un calibrage de type "curve fitting" pour obtenir une mesure exploitable et stable.

### Caractéristiques principales

- **Détection automatique** : conversion GPIO → unité ADC + canal via `adc_oneshot_io_to_channel()`.  
- **Calibrage intégré** : support des courbes de calibration ESP-IDF pour une lecture plus fiable.  
- **RAII complet** : allocation et libération des ressources ADC dans le constructeur/destructeur.  
- **Lecture simple** : `readRaw()` pour la valeur brute et `readVoltage()` pour la tension calculée.

### Exemple d’utilisation

```cpp
#include "adc-reader.h"

AdcReader<34> adc;  // GPIO 34

void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.printf("ADC raw: %d\n", adc.readRaw());
    Serial.printf("Voltage: %d mV\n", adc.readVoltage());
    delay(1000);
}
```

---

## HTTP Client

[http-client.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/http-client.h) est un petit client HTTP RAII dédié aux requêtes GET sur ESP32. Il encapsule `esp_http_client` et fournit une API minimale pour exécuter une requête, lire la réponse et fermer proprement la session sans fuite de ressource.

### Caractéristiques principales

- **API minimale** : création du client avec une URL fixe et appel à `get()`.  
- **RAII complet** : nettoyage du handle HTTP dans le destructeur.  
- **Header d’autorisation** : ajout facile du champ `Authorization` via `setAuthorization()`.  
- **Lecture simple de réponse** : retourne le contenu sous forme de `std::string`.

### Exemple d’utilisation

```cpp
#include "http-client.h"

HttpClient client("https://example.com/api");
client.setAuthorization("Bearer token");

void setup() {
    Serial.begin(115200);
    std::string resp = client.get();
    Serial.println(resp.c_str());
}
```

---

## Temp Sensor

[temp-sensor.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/temp-sensor.h) encapsule le capteur de température interne de l’ESP32. Il s’agit d’un wrapper C++ moderne qui initialise le driver, active les mesures, puis libère proprement les ressources lorsque l’objet est détruit.

### Caractéristiques principales

- **RAII complet** : installation et nettoyage automatique du capteur.  
- **Validation de configuration** : vérification des bornes de température via `static_assert`.  
- **Lecture simple** : méthode `read()` retournant la température en °C.  
- **Non-copiable** : l’instance possède un handle unique et exclusif.

### Exemple d’utilisation

```cpp
#include "temp-sensor.h"

TempSensor<> sensor;

void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.printf("Temp: %.2f C\n", sensor.read());
    delay(2000);
}
```

---

## Driver ESP32‑4848S040

[esp32-4848S040-driver.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/esp32-4848S040-driver.h) est un pilote C++ moderne et entièrement RAII pour piloter un écran **480×480 S[...](...)**

Il encapsule :

- le bus de commandes SWSPI  
- la configuration complète du panneau RGB  
- l’initialisation ST7701  
- le contrôle du rétroéclairage via LEDC (ESP32 Arduino Core v3)  

Le pilote hérite de [`Arduino_RGB_Display`](https://github.com/moononournation/Arduino_GFX) et s’intègre naturellement dans l’écosystème **Arduino GFX**.

L’écran est opérationnel immédiatement après construction — aucun appel supplémentaire n’est requis.

### Exemple d’utilisation

```cpp
#include "esp32-4848S040-driver.h"

auto gfx = ESP32_4848S040_Driver();

void setup() {

    // --- Start I2C bus (only needed if a touch controller is used) ---
    // Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // --- Draw a simple text screen ---
    gfx.fillScreen(RGB565_WHITE);
    gfx.setTextColor(RGB565_BLACK, RGB565_WHITE);
    gfx.setTextSize(2);
    gfx.setCursor(0, 0);
    gfx.print("Hello World");
}
```
