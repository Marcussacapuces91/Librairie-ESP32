# Bibliothèques ESP32

<p align="center">
  <img src="https://img.shields.io/badge/platform-ESP32--S3-blue" />
  <img src="https://img.shields.io/github/license/Marcussacapuces91/Librairie-ESP32" />
  <!-- <img src="https://img.shields.io/github/v/tag/Marcussacapuces91/Librairie-ESP32" /> -->
  <img src="https://img.shields.io/github/last-commit/Marcussacapuces91/Librairie-ESP32" />
</p>

Ensemble de composants C++ modernes et entièrement RAII destinés à simplifier le développement sur ESP32 (WiFi, synchronisation SNTP, affichage RGB).

---

## WiFi Manager

[wifi-manager.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/wifi-manager.h) est un contrôleur WiFi moderne et entièrement RAII conçu pour l’ESP32 en mode **STA**, avec provisionnement **SmartConfig** intégré.

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

## Driver ESP32‑4848S040

[esp32-4848S040-driver.h](https://github.com/Marcussacapuces91/Librairie-ESP32/blob/main/esp32-4848S040-driver.h) est un pilote C++ moderne et entièrement RAII pour piloter un écran **480×480 ST7701** via l’interface **RGB** de l’ESP32‑S3.

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
