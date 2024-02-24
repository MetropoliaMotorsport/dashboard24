/*
 * Adafruit_HX8357.c
 *
 *  Created on: Feb 17, 2024
 *      Author: iamna
 */
#include "Adafruit_HX8357.h"
#include "malloc.h"
#include "string.h"

#define DELAY 0x80

#define MADCTL_MY  0x80 ///< Bottom to top
#define MADCTL_MX  0x40 ///< Right to left
#define MADCTL_MV  0x20 ///< Reverse Mode
#define MADCTL_ML  0x10 ///< LCD refresh Bottom to top
#define MADCTL_RGB 0x00 ///< Red-Green-Blue pixel order
#define MADCTL_BGR 0x08 ///< Blue-Green-Red pixel order
#define MADCTL_MH  0x04 ///< LCD refresh right to left

// THIS REALLY SHOULD GO IN SPITFT (PART OF ADAFRUIT_GFX),
// AND CAN BE FURTHER EXPANDED (e.g. add 12 MHz for M0, 24 for M4),
// BUT TEMPORARILY FOR NOW IT'S HERE:

#if defined (ARDUINO_ARCH_ARC32)
  #define SPI_DEFAULT_FREQ 16000000
#elif defined (__AVR__) || defined(TEENSYDUINO)
  #define SPI_DEFAULT_FREQ 8000000
#elif defined(ESP8266) || defined (ARDUINO_MAXIM)
  #define SPI_DEFAULT_FREQ 16000000
#elif defined(ESP32)
  #define SPI_DEFAULT_FREQ 24000000
#elif defined(RASPI)
  #define SPI_DEFAULT_FREQ 24000000
#else
  #define SPI_DEFAULT_FREQ 24000000
#endif

static const uint8_t
  initb[] = {
    HX8357B_SETPOWER, 3,
      0x44, 0x41, 0x06,
    HX8357B_SETVCOM, 2,
      0x40, 0x10,
    HX8357B_SETPWRNORMAL, 2,
      0x05, 0x12,
    HX8357B_SET_PANEL_DRIVING, 5,
      0x14, 0x3b, 0x00, 0x02, 0x11,
    HX8357B_SETDISPLAYFRAME, 1,
      0x0c,                      // 6.8mhz
    HX8357B_SETPANELRELATED, 1,
      0x01,                      // BGR
    0xEA, 3,                     // seq_undefined1, 3 args
      0x03, 0x00, 0x00,
    0xEB, 4,                     // undef2, 4 args
      0x40, 0x54, 0x26, 0xdb,
    HX8357B_SETGAMMA, 12,
      0x00, 0x15, 0x00, 0x22, 0x00, 0x08, 0x77, 0x26, 0x66, 0x22, 0x04, 0x00,
    HX8357_MADCTL, 1,
      0xC0,
    HX8357_COLMOD, 1,
      0x55,
    HX8357_PASET, 4,
      0x00, 0x00, 0x01, 0xDF,
    HX8357_CASET, 4,
      0x00, 0x00, 0x01, 0x3F,
    HX8357B_SETDISPMODE, 1,
      0x00,                      // CPU (DBI) and internal oscillation ??
    HX8357_SLPOUT, 0x80 + 120/5, // Exit sleep, then delay 120 ms
    HX8357_DISPON, 0x80 +  10/5, // Main screen turn on, delay 10 ms
    0                            // END OF COMMAND LIST
  }, initd[] = {
    HX8357_SWRESET, 0x80 + 10/5, // Soft reset, then delay 10 ms
    HX8357D_SETC, 3,
      0xFF, 0x83, 0x57,
    0xFF, 0x80 + 300/5,          // No command, just delay 300 ms
    HX8357_SETRGB, 4,
      0x80, 0x00, 0x06, 0x06,    // 0x80 enables SDO pin (0x00 disables)
    HX8357D_SETCOM, 1,
      0x25,                      // -1.52V
    HX8357_SETOSC, 1,
      0x68,                      // Normal mode 70Hz, Idle mode 55 Hz
    HX8357_SETPANEL, 1,
      0x05,                      // BGR, Gate direction swapped
    HX8357_SETPWR1, 6,
      0x00,                      // Not deep standby
      0x15,                      // BT
      0x1C,                      // VSPR
      0x1C,                      // VSNR
      0x83,                      // AP
      0xAA,                      // FS
    HX8357D_SETSTBA, 6,
      0x50,                      // OPON normal
      0x50,                      // OPON idle
      0x01,                      // STBA
      0x3C,                      // STBA
      0x1E,                      // STBA
      0x08,                      // GEN
    HX8357D_SETCYC, 7,
      0x02,                      // NW 0x02
      0x40,                      // RTN
      0x00,                      // DIV
      0x2A,                      // DUM
      0x2A,                      // DUM
      0x0D,                      // GDON
      0x78,                      // GDOFF
    HX8357D_SETGAMMA, 34,
      0x02, 0x0A, 0x11, 0x1d, 0x23, 0x35, 0x41, 0x4b, 0x4b,
      0x42, 0x3A, 0x27, 0x1B, 0x08, 0x09, 0x03, 0x02, 0x0A,
      0x11, 0x1d, 0x23, 0x35, 0x41, 0x4b, 0x4b, 0x42, 0x3A,
      0x27, 0x1B, 0x08, 0x09, 0x03, 0x00, 0x01,
    HX8357_COLMOD, 1,
      0x55,                      // 16 bit
    HX8357_MADCTL, 1,
      0xC0,
    HX8357_TEON, 1,
      0x00,                      // TW off
    HX8357_TEARLINE, 2,
      0x00, 0x02,
    HX8357_SLPOUT, 0x80 + 150/5, // Exit Sleep, then delay 150 ms
    HX8357_DISPON, 0x80 +  50/5, // Main screen turn on, delay 50 ms
    0,                           // END OF COMMAND LIST
  };
static void ST7735_Select() {
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);

}
void ST7735_Unselect() {
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_SET);
}
static void ST7735_Reset() {
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_SET);
}
static void ST7735_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&ST7735_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7735_WriteData(uint8_t* buff, size_t buff_size) {
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&ST7735_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
}
static void ST7735_ExecuteCommandList(const uint8_t *addr) {
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while(numCommands--) {
        uint8_t cmd = *addr++;
        ST7735_WriteCommand(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;
        if(numArgs) {
            ST7735_WriteData((uint8_t*)addr, numArgs);
            addr += numArgs;
        }

        if(ms) {
            ms = *addr++;
            if(ms == 255) ms = 500;
            HAL_Delay(ms);
        }
    }
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // column address set
    ST7735_WriteCommand(HX8357_CASET);
    uint8_t data[] = { 0x00, x0 + 2, 0x00, x1 + 2 }; // Check
    ST7735_WriteData(data, sizeof(data));

    // row address set
    ST7735_WriteCommand(HX8357_PASET);
    data[1] = y0 + 3; //  Check
    data[3] = y1 + 3; // Make new code
    ST7735_WriteData(data, sizeof(data));

    // write to RAM
    ST7735_WriteCommand(HX8357_RAMWR);
}
void ST7735_Init() {
    ST7735_Select();
    ST7735_Reset();
    ST7735_ExecuteCommandList(initb);
    ST7735_ExecuteCommandList(initd);
   // ST7735_ExecuteCommandList(init_cmds3);
    ST7735_Unselect();
}
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if((x >= HX8357_TFTWIDTH) || (y >= HX8357_TFTHEIGHT))
        return;

    ST7735_Select();

    ST7735_SetAddressWindow(x, y, x+1, y+1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ST7735_WriteData(data, sizeof(data));

    ST7735_Unselect();
}

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, b, j;

    ST7735_SetAddressWindow(x, y, x+font.width-1, y+font.height-1);

    for(i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for(j = 0; j < font.width; j++) {
            if((b << j) & 0x8000)  {
                uint8_t data[] = { color >> 8, color & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            } else {
                uint8_t data[] = { bgcolor >> 8, bgcolor & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            }
        }
    }
}

void ST7735_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ST7735_Select();

    while(*str) {
        if(x + font.width >= HX8357_TFTWIDTH) {
            x = 0;
            y += font.height;
            if(y + font.height >= HX8357_TFTHEIGHT) {
                break;
            }

            if(*str == ' ') {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        ST7735_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }

    ST7735_Unselect();
}

void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // clipping
    if((x >= HX8357_TFTWIDTH) || (y >= HX8357_TFTHEIGHT)) return;
    if((x + w - 1) >= HX8357_TFTWIDTH) w = HX8357_TFTWIDTH - x;
    if((y + h - 1) >= HX8357_TFTHEIGHT) h = HX8357_TFTHEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    uint8_t data[] = { color >> 8, color & 0xFF };
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    for(y = h; y > 0; y--) {
        for(x = w; x > 0; x--) {
            HAL_SPI_Transmit(&ST7735_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
        }
    }

    ST7735_Unselect();
}
void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // clipping
	if((x >= HX8357_TFTWIDTH) || (y >= HX8357_TFTHEIGHT)) return;
	if((x + w - 1) >= HX8357_TFTWIDTH) w = HX8357_TFTWIDTH - x;
	if((y + h - 1) >= HX8357_TFTHEIGHT) h = HX8357_TFTHEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    // Prepare whole line in a single buffer
    uint8_t pixel[] = { color >> 8, color & 0xFF };
    uint8_t *line = malloc(w * sizeof(pixel));
    for(x = 0; x < w; ++x)
    	memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    for(y = h; y > 0; y--)
        HAL_SPI_Transmit(&ST7735_SPI_PORT, line, w * sizeof(pixel), HAL_MAX_DELAY);

    free(line);
    ST7735_Unselect();
}

void ST7735_FillScreen(uint16_t color) {
    ST7735_FillRectangle(0, 0, HX8357_TFTWIDTH, HX8357_TFTHEIGHT, color);
}

void ST7735_FillScreenFast(uint16_t color) {
    ST7735_FillRectangleFast(0, 0, HX8357_TFTWIDTH, HX8357_TFTHEIGHT, color);
}

void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if((x >= HX8357_TFTWIDTH) || (y >= HX8357_TFTHEIGHT)) return;
    if((x + w - 1) >= HX8357_TFTWIDTH) return;
    if((y + h - 1) >= HX8357_TFTHEIGHT) return;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);
    ST7735_WriteData((uint8_t*)data, sizeof(uint16_t)*w*h);
    ST7735_Unselect();
}

void ST7735_InvertColors(bool invert) {
    ST7735_Select();
    ST7735_WriteCommand(invert ? HX8357_INVON : HX8357_INVOFF);
    ST7735_Unselect();
}
/*
void ST7735_SetGamma(GammaDef gamma)
{
	ST7735_Select();
	ST7735_WriteCommand(ST7735_GAMSET);
	ST7735_WriteData((uint8_t *) &gamma, sizeof(gamma));
	ST7735_Unselect();
}

*/
