/**
 * @file driver-esp32-4848S040.h
 * @brief High-level driver for the ESP32-S3 controlling a 480x480 ST7701 RGB panel.
 *
 * Provides a modern C++ wrapper around Arduino_RGB_Display, encapsulating:
 *  - RGB panel configuration and timing
 *  - ST7701 initialization via SWSPI command bus
 *  - Backlight control using LEDC PWM (ESP32 Arduino Core v2 & v3 compatible)
 *
 * @details
 * - **Modern C++ Design**: Strongly typed constants, encapsulated resources,
 *   no global variables, no macros.
 * - **RAII-Friendly**: The constructor builds the internal buses and panel
 *   objects; cleanup occurs in the destructor. begin() is still necessary.
 * - **Version-Aware**: LEDC backlight control automatically adapts to
 *   ESP32 Arduino Core v3 APIs.
 * - **Extendable**: The class can be inherited to add touch support,
 *   UI layers, or custom drawing helpers.
 * - **Safe Backlight Control**: Brightness is clamped to the valid 0–255 range.
 *
 * @author Marc SIBERT
 * @copyright (c) 2026 by M. SIBERT.
 * @version 1.0
 */

#pragma once

#include <Arduino_GFX_Library.h>

/**
 * @class ESP32_4848S040_Driver
 * @brief High‑level driver for the ESP32‑S3 + 480x480 ST7701 RGB panel.
 *
 * This class encapsulates:
 *  - The SWSPI command bus for ST7701 initialization
 *  - The ESP32 RGB panel interface
 *  - Automatic display initialization via Arduino_RGB_Display
 *  - Backlight control using LEDC PWM
 *
 * Usage example:
 * @code
 * ESP32_4848S040_Driver display(200); // brightness 0–255
 * display.fillScreen(BLACK);
 * display.setCursor(100, 200);
 * display.print("Hello!");
 * @endcode
 */
class ESP32_4848S040_Driver : public Arduino_RGB_Display {
public:
/**
 * @brief Construct the display driver and initialize the panel.
 *
 * This constructor:
 *  - Initializes the SWSPI command bus
 *  - Configures the RGB panel timing and pinout
 *  - Calls the parent Arduino_RGB_Display constructor
 *  - Sets up LEDC PWM for the backlight
 *
 * @param brightness Initial backlight brightness (0–255).
 */
    ESP32_4848S040_Driver(const byte brightness = 255);

/**
 * @brief Destructor: releases LEDC resources.
 *
 * Detaches the PWM channel used for the backlight.
 */
    virtual ~ESP32_4848S040_Driver();

/**
 * @brief Set the backlight brightness.
 *
 * @param value Brightness level (0–255).
 */
    inline
    void setBrightness(const byte value);

private:
    // --- BACKLIGHT CONTROL via PWM (LEDC) ---
    static constexpr byte GFX_BL = 38;          ///< Backlight pin
    static constexpr unsigned PWM_FREQ = 1000;  ///< PWM frequency (Hz)
    static constexpr byte PWM_BITS = 8;         ///< PWM resolution (bits)

    Arduino_SWSPI bus;                          ///< Command bus for ST7701
    Arduino_ESP32RGBPanel rgbpanel;             ///< RGB pixel bus
};

// ============================================================================
// Implementation
// ============================================================================

ESP32_4848S040_Driver::ESP32_4848S040_Driver(const byte brightness) :
    bus(
        GFX_NOT_DEFINED,  // DC not used
        39,               // CS: chip select
        48,               // SCK: software SPI clock
        47,               // MOSI: software SPI data
        GFX_NOT_DEFINED   // MISO not used
    ),
    rgbpanel(
        /* DE, VSYNC, HSYNC, PCLK */ 18, 17, 16, 21,
        /* R0..R4 */ 11, 12, 13, 14, 0,
        /* G0..G5 */ 8, 20, 3, 46, 9, 10,
        /* B0..B4 */ 4, 5, 6, 7, 15,
        /* hsync_pol, hfp, hpw, hbp */ 1, 10, 8, 50,   // horizontal timing parameters
        /* vsync_pol, vfp, vpw, vbp */ 1, 10, 8, 20,   // vertical timing parameters
        /* pclk_active_neg */ 0,                       // PCLK polarity
        /* prefer_speed */ 12000000,                   // pixel clock frequency
        /* big_endian */ false,                        // standard byte order
        /* de_idle_high, pclk_idle_high, bounce_buf */ 0, 0, 0
    ),
    Arduino_RGB_Display(
        480, 480,                   // Panel resolution
        &rgbpanel,                  // RGB interface
        0,                          // No rotation
        true,                       // Automatic frame buffer flush
        &bus,                       // Command bus
        GFX_NOT_DEFINED,            // No dedicated reset pin
        st7701_type9_init_operations,
        sizeof(st7701_type9_init_operations)
    )
{
    ledcAttach(GFX_BL, PWM_FREQ, PWM_BITS);
    setBrightness(brightness);
}

ESP32_4848S040_Driver::~ESP32_4848S040_Driver() {
    ledcDetach(GFX_BL);
}

void ESP32_4848S040_Driver::setBrightness(const byte value) {
    ledcWrite(GFX_BL, (value > 255 ? 255 : value));
}
