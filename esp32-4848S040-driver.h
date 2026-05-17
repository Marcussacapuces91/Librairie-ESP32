/**
 * @file esp32-4848S040-driver.h
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

    static constexpr byte TFT_SPI_DC = GFX_NOT_DEFINED;     // DC not used
    static constexpr byte TFT_SPI_CS = 39;                  // CS: chip select   (LCD-CS)
    static constexpr byte TFT_SPI_SCK = 48;                 // SCK: clock        (SCK)
    static constexpr byte TFT_SPI_MOSI = 47;                // MOSI: data -> TFT (SDA)
    static constexpr byte TFT_SPI_MISO = GFX_NOT_DEFINED;   // MISO: TFT -> data (not used)

    static constexpr byte TFT_DE = 18;                      // DE
    static constexpr byte TFT_VSYNC = 17;                   // VSYNC (VS)
    static constexpr byte TFT_HSYNC = 16;                   // HSYNC (HS)
    static constexpr byte TFT_PCLK = 21;                    // PCLK
    static constexpr byte TFT_R[] = {11, 12, 13, 14, 0};    // DB13, DB14, DB15, DB16, DB17
    static constexpr byte TFT_G[] = {8, 20, 3, 46, 9, 10};  // DB6, DB7, DB8, DB9, DB10, DB11
    static constexpr byte TFT_B[] = {4, 5, 6, 7, 15};       // DB1, DB2, DB3, DB4, DB5

    static constexpr byte TP_INT = GFX_NOT_DEFINED;         // connected to GND
    static constexpr byte TP_SDA = 19;
    static constexpr byte TP_SCL = 45;
    static constexpr byte TP_RST = GFX_NOT_DEFINED;         // RC -> 3.3v

    Arduino_SWSPI bus;                          ///< Command bus for ST7701
    Arduino_ESP32RGBPanel rgbpanel;             ///< RGB pixel bus
};

// ============================================================================
// Implementation
// ============================================================================

ESP32_4848S040_Driver::ESP32_4848S040_Driver(const byte brightness) :
    bus(TFT_SPI_DC, TFT_SPI_CS, TFT_SPI_SCK, TFT_SPI_MOSI, TFT_SPI_MISO),
    rgbpanel(
        /* DE, VSYNC, HSYNC, PCLK */ TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
        /* R0..R4 */ TFT_R[0], TFT_R[1], TFT_R[2], TFT_R[3], TFT_R[4],              // Red : 5 bits
        /* G0..G5 */ TFT_G[0], TFT_G[1], TFT_G[2], TFT_G[3], TFT_G[4], TFT_G[5],    // Green : 6 bits
        /* B0..B4 */ TFT_B[0], TFT_B[1], TFT_B[2], TFT_B[3], TFT_B[4],              // Blue : 5 bits
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
