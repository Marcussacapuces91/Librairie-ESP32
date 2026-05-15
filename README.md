# Librairie-ESP32

Quelques fichiers pour me faciliter le travail.

## Wifi Manager

## Time Manager

## Driver ESP32-4848S040

[esp32-4848S040-driver.h](esp32-4848S040-driver.h) est un pilote C++ moderne et entièrement RAII conçu pour piloter un écran **480×480 ST7701** via l’interface RGB 5/6/5 de l’ESP32‑S3.

Il encapsule toute la configuration matérielle nécessaire (bus de commandes SWSPI, timings RGB, initialisation ST7701, contrôle du rétroéclairage) dans une classe unique, immédiatement opérationnelle dès sa construction. C'est une classe héritée de [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) de [moononournation](https://github.com/moononournation).

Le pilote utilise l’API LEDC du core ESP32 Arduino v3 pour gérer la luminosité du rétroéclairage, et s’appuie sur Arduino_RGB_Display pour offrir une interface graphique complète et compatible avec l’écosystème Arduino GFX.

En pratique, il suffit d’instancier la classe pour que l’écran soit prêt à l’emploi — aucun appel supplémentaire n’est requis.

``` C++
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
