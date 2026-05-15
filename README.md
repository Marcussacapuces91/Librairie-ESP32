# Librairie-ESP32

Quelques fichiers pour me faciliter le travail.

## WiFi Manager

[wifi-manager.h](wifi-manager.h) est un contrôleur WiFi moderne et entièrement RAII conçu pour l'ESP32 en mode STA avec provisionnement SmartConfig automatique.

### Caractéristiques principales

- **Pattern RAII** : Initialisation automatique du stack WiFi dans le constructeur, nettoyage complet dans le destructeur. Aucun état global, pas de singleton.
- **Événementiel** : Tous les événements WiFi sont gérés via des méthodes virtuelles protégées surchargeables dans les classes dérivées.
- **SmartConfig intégré** : Bascule automatique vers la provisionnement ESP-Touch après 3 tentatives de connexion échouées.
- **Thread-safe** : Utilise les sémaphores FreeRTOS pour la synchronisation.

### Utilisation basique

```cpp
#include "wifi-manager.h"

WifiManager wifi;

void setup() {
    // Connecter et attendre une adresse IP (timeout de 30 secondes par défaut)
    auto ip_info = wifi.connectAndWaitIP();
    
    // Récupérer les informations de l'AP connecté
    auto ap = wifi.getAPInfo();
}
```

### Extensibilité

La classe est conçue pour être étendue facilement. Surcharger les méthodes virtuelles protégées pour ajouter des comportements personnalisés (LEDs, logging, stockage persistant, etc.) :

```cpp
class MyWifiManager : public WifiManager {
protected:
    void onGotIp(const ip_event_got_ip_t& evt) override {
        WifiManager::onGotIp(evt);  // Appeler le parent
        startMyServices();            // Actions personnalisées
        ledBlink(GREEN);              // Retour visuel
    }
};
```

## Time Manager

[time-manager.h](time-manager.h) est un contrôleur C++ moderne et entièrement RAII destiné à gérer la synchronisation de l’heure via SNTP sur ESP32.  
Il encapsule les API ESP‑IDF liées au temps et fournit un mécanisme fiable pour garantir que l’horloge système est correctement synchronisée avant de lancer des services sensibles (TLS, planification, logs horodatés, etc.).

### Fonctionnalités principales

- **RAII complet** : initialisation du service SNTP et des ressources FreeRTOS dans le constructeur, libération automatique dans le destructeur.
- **Synchronisation bloquante** : la méthode `syncAndWait()` suspend la tâche courante jusqu’à réception d’une mise à jour NTP valide, ou déclenche `abort()` en cas de dépassement de délai.
- **Thread-safe** : utilisation d’atomiques et d’un sémaphore binaire pour gérer l’état de synchronisation et les signaux asynchrones.
- **Callback intégré** : la classe installe automatiquement un callback SNTP (`sntp_time_callback`) pour détecter les mises à jour de temps.
- **Suivi des synchronisations** : compteur interne du nombre de mises à jour NTP réussies, utile pour le monitoring réseau.
- **Gestion du fuseau horaire** : configuration simple via une chaîne POSIX (`setTimezone()`), avec support DST.
- **Formats ISO8601** : récupération de l’heure locale ou UTC sous forme de chaînes normalisées.

### Exemple d’utilisation

```cpp
WifiManager wifi;
wifi.connectAndWaitIP();

TimeManager timeMgr("pool.ntp.org");
timeMgr.syncAndWait(30000);

ESP_LOGI("APP", "System time: %s", timeMgr.getLocal_ISO8601().c_str());
```

## Driver ESP32-4848S040

[esp32-4848S040-driver.h](esp32-4848S040-driver.h) est un pilote C++ moderne et entièrement RAII conçu pour piloter un écran **480×480 ST7701** via l’interface RGB 5/6/5 de l’ESP32‑S3.

Il encapsule toute la configuration matérielle nécessaire (bus de commandes SWSPI, timings RGB, initialisation ST7701, contrôle du rétroéclairage) dans une classe unique, immédiatement opérationnelle dès sa construction. C'est une classe héritée de [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) de [moononournation](https://github.com/moononournation).

Le pilote utilise l’API LEDC du core ESP32 Arduino v3 pour gérer la luminosité du rétroéclairage, et s’appuie sur Arduino_RGB_Display pour offrir une interface graphique complète et compatible avec l’écosystème Arduino GFX.

En pratique, il suffit d’instancier la classe pour que l’écran soit prêt à l’emploi — aucun appel supplémentaire n’est requis.

```cpp
#include "esp32-4848S040-driver.h"

auto gfx = ESP32_4848S040_Driver();

void setup() {

    // --- Start I2C bus (only needed if a touch controller is used) ---
//    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // --- Initialize the display (driver + RGB synchronization) ---
    gfx.begin();

    // --- Draw a simple text screen ---
    gfx.fillScreen(RGB565_WHITE);          // Black background
    gfx.setTextColor(RGB565_BLACK, RGB565_WHITE); // White text on black background
    gfx.setTextSize(2);             // Medium text size
    gfx.setCursor(0, 0);        // Position text near the center
    gfx.print("Hello World");       // Print message
}
```
