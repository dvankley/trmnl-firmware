#include <Arduino.h>
#include <ArduinoLog.h>
#include <display.h>
#include <PNGdec.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <preferences_persistence.h>
#include "DEV_Config.h"
#define BB_EPAPER
#ifdef BB_EPAPER
#include "bb_epaper.h"
//#define ONE_BIT_PANEL EP426_800x480
//#define TWO_BIT_PANEL EP426_800x480_4GRAY
// #define WAVESHARE_3_COLOR EP75R_800x480
#define ONE_BIT_PANEL EP75_800x480
#define TWO_BIT_PANEL EP75_800x480_4GRAY_OLD
BBEPAPER bbep(EP73_SPECTRA_800x480);
// Counts the number of partial updates to know when to do a full update
RTC_DATA_ATTR int iUpdateCount = 0;
#else
#include "FastEPD.h"
FASTEPD bbep;
#endif
#include "Group5.h"
#include <config.h>
#include "wifi_connect_qr.h"
#include "wifi_failed_qr.h"
#include <ctype.h> //iscntrl()
#include <api-client/display.h>
#include <trmnl_log.h>
#include "png_flip.h"
#include "../lib/bb_epaper/Fonts/Roboto_20.h"
#include "../lib/bb_epaper/Fonts/nicoclean_8.h"
extern char filename[];
extern Preferences preferences;
extern ApiDisplayResult apiDisplayResult;

int paletteMapSize;
/**
 * Mapping from indexed PNG palette to display palette.
 * Each pixel in an indexed PNG is an index value in the PNG's palette that corresponds
 *  to the pixel's color code.
 * This array allows direct translation from the PNG's pixel values/palette indexes into
 *  the color codes expected by the display.
 * This array's value at a given index should be the display's color code that corresponds
 *  to the PNG's palette entry at that index.
 *
 * Example:
 * - PNG palette: [0xff0000, 0x0000ff] (red and blue)
 * - Display palette: first 6 of u8Colors_spectra6
 * This array would then be: [0x03, 0x05] (spectra 6 color codes for red and blue)
 */
uint8_t paletteMap[MAX_COLOR_COUNT];

/**
 * @brief Function to init the display
 * @param none
 * @return none
 */
void display_init(void)
{
    Log_info("dev module start");
#ifdef BB_EPAPER
    bbep.initIO(EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_CS_PIN, EPD_MOSI_PIN, EPD_SCK_PIN, 8000000);
#else
    bbep.initPanel(BB_PANEL_EPDIY_V7);
    bbep.setPanelSize(1448, 1072);
#endif
    Log_info("dev module end");
}

void display_show_battery(float vBatt)
{
char szTemp[32];

    bbep.allocBuffer(false);
    bbep.fillScreen(BBEP_WHITE); // draw the image centered on a white background
    bbep.setFont(nicoclean_8); //Roboto_20);
    bbep.setTextColor(BBEP_BLACK, BBEP_WHITE);
    bbep.setCursor(0, 100);
    sprintf(szTemp, "VBatt = %f", vBatt);
    bbep.print(szTemp);
    bbep.writePlane();
    bbep.refresh(REFRESH_FULL, true);
    bbep.sleep(DEEP_SLEEP);
    while (1) {
        vTaskDelay(1);
    }
} /* display_show_battery() */

/**
 * @brief Function to sleep the ESP32 while saving power
 * @param u32Millis represents the sleep time in milliseconds
 * @return none
 */
void display_sleep(uint32_t u32Millis)
{
  esp_sleep_enable_timer_wakeup(u32Millis * 1000L);
  esp_light_sleep_start();
}

/**
 * @brief Function to reset the display
 * @param none
 * @return none
 */
void display_reset(void)
{
    Log_info("e-Paper Clear start");
    bbep.fillScreen(BBEP_WHITE);
#ifdef BB_EPAPER
    if (!apiDisplayResult.response.maximum_compatibility) {
        bbep.refresh(REFRESH_FAST, true);
    } else {
        bbep.refresh(REFRESH_FULL, true); // incompatible panel
    }
#else
    bbep.fullUpdate();
#endif
    Log_info("e-Paper Clear end");
    // DEV_Delay_ms(500);
}

/**
 * @brief Function to read the display height
 * @return uint16_t - height of display in pixels
 */
uint16_t display_height()
{
    return bbep.height();
}

/**
 * @brief Function to read the display width
 * @return uint16_t - width of display in pixels
 */
uint16_t display_width()
{
    return bbep.width();
}

/**
 * @brief Function to draw multi-line text onto the display
 * @param x_start X coordinate to start drawing
 * @param y_start Y coordinate to start drawing
 * @param message Text message to draw
 * @param max_width Maximum width in pixels for each line
 * @param font_width Width of a single character in pixels
 * @param color_fg Foreground color
 * @param color_bg Background color
 * @param font Font to use
 * @param is_center_aligned If true, center the text; if false, left-align
 * @return none
 */
void Paint_DrawMultilineText(UWORD x_start, UWORD y_start, const char *message,
                             uint16_t max_width, uint16_t font_width,
                             UWORD color_fg, UWORD color_bg, const void *font,
                             bool is_center_aligned)
{
    BB_FONT_SMALL *pFont = (BB_FONT_SMALL *)font;
    uint16_t display_width_pixels = max_width;
    int max_chars_per_line = display_width_pixels / font_width;
    const int font_height = pFont->height;
    uint8_t MAX_LINES = 4;

    char lines[MAX_LINES][max_chars_per_line + 1] = {0};
    uint16_t line_count = 0;

    int text_len = strlen(message);
    int current_width = 0;
    int line_index = 0;
    int line_pos = 0;
    int word_start = 0;
    int i = 0;
    char word_buffer[max_chars_per_line + 1] = {0};
    int word_length = 0;

    bbep.setFont(font);
    bbep.setTextColor(color_fg, color_bg);

    bbep.setFont(font);
    bbep.setTextColor(color_fg, color_bg);

    while (i <= text_len && line_index < MAX_LINES)
    {
        word_length = 0;
        word_start = i;

        // Skip leading spaces
        while (i < text_len && message[i] == ' ')
        {
            i++;
        }
        word_start = i;

        // Find end of word or end of text
        while (i < text_len && message[i] != ' ')
        {
            i++;
        }

        word_length = i - word_start;
        if (word_length > max_chars_per_line)
        {
            word_length = max_chars_per_line; // Truncate if word is too long
        }

        if (word_length > 0)
        {
            strncpy(word_buffer, message + word_start, word_length);
            word_buffer[word_length] = '\0';
        }
        else
        {
            i++;
            continue;
        }

        int word_width = word_length * font_width;

        // Check if adding the word exceeds max_width
        if (current_width + word_width + (current_width > 0 ? font_width : 0) <= display_width_pixels)
        {
            // Add space before word if not the first word in the line
            if (current_width > 0 && line_pos < max_chars_per_line - 1)
            {
                lines[line_index][line_pos++] = ' ';
                current_width += font_width;
            }

            // Add word to current line
            if (line_pos + word_length <= max_chars_per_line)
            {
                strcpy(&lines[line_index][line_pos], word_buffer);
                line_pos += word_length;
                current_width += word_width;
            }
        }
        else
        {
            // Current line is full, draw it
            if (line_pos > 0)
            {
                lines[line_index][line_pos] = '\0'; // Null-terminate the current line
                line_index++;
                line_count++;

                if (line_index >= MAX_LINES)
                {
                    break;
                }

                // Start new line with this word
                strncpy(lines[line_index], word_buffer, word_length);
                line_pos = word_length;
                current_width = word_width;
            }
            else
            {
                // Single long word case
                strncpy(lines[line_index], word_buffer, max_chars_per_line);
                lines[line_index][max_chars_per_line] = '\0';
                line_index++;
                line_count++;
                line_pos = 0;
                current_width = 0;
            }
        }

        // Move to next word
        if (message[i] == ' ')
        {
            i++;
        }
    }

    // Store the last line if any
    if (line_pos > 0 && line_index < MAX_LINES)
    {
        lines[line_index][line_pos] = '\0';
        line_count++;
    }

    // Draw the lines
    for (int j = 0; j < line_count; j++)
    {
        uint16_t line_width = strlen(lines[j]) * font_width;
        uint16_t draw_x = x_start;

        if (is_center_aligned)
        {
            if (line_width < max_width)
            {
                draw_x = x_start + (max_width - line_width) / 2;
            }
        }
        bbep.setCursor(draw_x, y_start + j * (font_height + 5));
        bbep.print(lines[j]);
    }
}
/** 
 * @brief Reduce the bit depth of line of pixels using thresholding (aka simple color mapping)
 * @param Destination bit count (1 or 2)
 * @param Pointer to a PNG palette (3 bytes per entry)
 * @param Pointer to the source pixels
 * @param Pointer to the destination pixels
 * @param Pixel count
 * @param Original bit depth
 * @return none
 */
void ReduceBpp(int iDestBpp, int iPixelType, uint8_t *pPalette, uint8_t *pSrc, uint8_t *pDest, int w, int iSrcBpp)
{
    int g = 0, x, iDelta;
    uint8_t *s, *d, *pPal, u8, count;
    const uint8_t u8G2ToG8[4] = {0x00, 0x55, 0xaa, 0xff}; // 2-bit to 8-bit gray

    if (iPixelType == PNG_PIXEL_TRUECOLOR) iSrcBpp = 24;
    else if (iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) iSrcBpp = 32;
    iDelta = iSrcBpp/8; // bytes per pixel
    count = 8; // bits in a byte
    u8 = 0; // start with all black
    d = pDest;
    s = pSrc;
    for (x=0; x<w; x++) {
        u8 <<= iDestBpp;
        switch (iSrcBpp) {
            case 24:
            case 32:
                g = (s[0] + s[1]*2 + s[2])/4; // convert color to gray value
                s += iDelta;
                break;
            case 8:
                if (iPixelType == PNG_PIXEL_INDEXED) {
                    pPal = &pPalette[s[0] * 3];
                    g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                } else { // must be grayscale
                    g = s[0];
                }
                s++;
                break;
            case 4:
                if (x & 1) {
                    if (iPixelType == PNG_PIXEL_INDEXED) {
                        pPal = &pPalette[(s[0] & 0xf) * 3];
                        g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                    } else {
                        g = (s[0] & 0xf) | (s[0] << 4);
                    }
                    s++;
                } else {
                    if (iPixelType == PNG_PIXEL_INDEXED) {
                        pPal = &pPalette[(s[0]>>4) * 3];
                        g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                    } else {
                        g = (s[0] & 0xf0) | (s[0] >> 4);
                    }
                }
                break;
            case 2: // We need to handle this case for 2-bit images with (random) palettes
                g = s[0] >> (6-((x & 3) * 2));
                if (iPixelType == PNG_PIXEL_INDEXED) {
                    pPal = &pPalette[(g & 3)*3];
                    g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                } else {
                    g = u8G2ToG8[g & 3];
                }
                if ((x & 3) == 3) {
                    s++;
                }
                break;
        } // switch on bpp
        if (iDestBpp == 1) {
            u8 |= (g >> 7); // B/W
        } else { // generate 4 gray levels (2 bits)
            u8 |= (3 ^ (g >> 6)); // 4 gray levels (inverted relative to 1-bit)
        }
        count -= iDestBpp;        
        if (count == 0) { // byte is full, move on
            *d++ = u8;
            u8 = 0;
            count = 8;
        }
    } // for x
    if (count != 8) { // partial byte remaining
        u8 <<= count;
        *d++ = u8;
    }
} /* ReduceBpp() */

/**
 * @brief Callback function for each line of PNG decoded
 * @param PNGDRAW structure containing the current line and relevant info
 * @return none
 */
int png_draw(PNGDRAW *pDraw)
{
    static bool hasLogged = false;
    int x;
    uint8_t ucBppChanged = 0, ucInvert = 0;
    uint8_t uc, ucMask, src, *s, *d, *pTemp = bbep.getCache(); // get some scratch memory (not from the stack)

    if (!hasLogged) {
        Log_info("png_draw pixel type: %d; bpp: %d; ", pDraw->iPixelType, pDraw->iBpp);
    }
    if (pDraw->iPixelType == PNG_PIXEL_INDEXED && pDraw->iBpp == 1) {
        // 1-bit output, just see which color is brighter
        if (!hasLogged) {
            Log_info("1bpp indexed");
        }
        uint32_t u32Gray0, u32Gray1;
        u32Gray0 = pDraw->pPalette[0] + (pDraw->pPalette[1]<<2) + pDraw->pPalette[2];
        u32Gray1 = pDraw->pPalette[3] + (pDraw->pPalette[4]<<2) + pDraw->pPalette[5];
        if (u32Gray0 < u32Gray1) {
            ucInvert = 0xff;
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_INDEXED && (bbep.capabilities() | BBEP_FULL_COLOR)) {
        // "full" "color" display with indexed PNG (happy path for color displays)
        if (!hasLogged) {
            Log_info("Indexed PNG with full color display");
        }

        if (paletteMapSize == 0) {
            if (!hasLogged) {
                Log_error("png_draw attempting to decode indexed PNG without palette map defined.");
            }
            return 0;
        }
    } else if (pDraw->iBpp == 2) {
        if (!hasLogged) {
            Log_info("2bpp non-indexed; inverting for 4 gray");
        }
        ucInvert = 0xff; // 2-bit non-palette images need to be inverted colors for 4-gray mode
    } else if (pDraw->iBpp > 2) {
        if (!hasLogged) {
            Log_info("bpp > 2; reducing to 1 or 2 bpp");
        }
        // Reduce the source image to 1-bpp or 2-bpp
        ReduceBpp((pDraw->pUser) ? 2:1, pDraw->iPixelType, pDraw->pPalette, pDraw->pPixels, pTemp, pDraw->iWidth, pDraw->iBpp);
        ucBppChanged = 1;
    }
    hasLogged = true;
    s = (ucBppChanged) ? pTemp : (uint8_t *)pDraw->pPixels;
    d = pTemp;
    // Handle indexed-color PNG rows for full-color-capable EPDs by translating
    // PNG palette indices -> EPD 4-bpp color codes using paletteMap, then pack
    // two pixels per byte (even x in high nibble, odd x in low nibble).

    // Junie check this if block
    if (pDraw->iPixelType == PNG_PIXEL_INDEXED && (bbep.capabilities() & BBEP_FULL_COLOR)) {
        // int i;
        // int iPitch, iSize;
        // uint8_t u8;
        // BBEPDISP *pBBEP = (BBEPDISP *)pb;
        //
        // // only available for local buffer operations
        // if (!pBBEP || !pBBEP->ucScreen) return BBEP_ERROR_BAD_PARAMETER;
        // ucColor = pBBEP->pColorLookup[ucColor & 0xf]; // translate the color for this display type
        //
        // iPitch = pBBEP->width >> 1;
        // iSize = (pBBEP->native_width >> 1) * pBBEP->native_height;
        //
        // i = (x >> 1) + (y * iPitch);
        // if (x < 0 || x >= pBBEP->width || i < 0 || i > iSize-1) { // off the screen
        //     pBBEP->last_error = BBEP_ERROR_BAD_PARAMETER;
        //     return BBEP_ERROR_BAD_PARAMETER;
        // }
        // u8 = pBBEP->ucScreen[i];
        // if (x & 1) {
        //     u8 &= 0xf0;
        //     u8 |= ucColor;
        // } else {
        //     u8 &= 0x0f;
        //     u8 |= (ucColor << 4);
        // }
        // pBBEP->ucScreen[i] = u8;

        bool moreLogging = pDraw->y == 200 || pDraw->y == 400;

        int width = pDraw->iWidth;
        int outLen = (width + 1) >> 1; // 2 pixels per byte

        const uint8_t *srcRow = s;   // 8-bit palette index per pixel from PNGdec
        uint8_t *dstRow = pTemp;     // output buffer to transmit to EPD

        if (moreLogging) {
            Log_info_serial("Rendering PNG line %d. Width: %d; out length: %d", pDraw->y, width, outLen);
            esp_log_buffer_hex("palette map", &paletteMap, 6);
        }

        x = 0;
        for (int i = 0; i < outLen; ++i) {
            // This assumes 8bpp; update it to do it right
            moreLogging = moreLogging && i < 5;
            // First (even) pixel -> high nibble
            uint8_t idx0 = (x < width) ? srcRow[x++] : 0;
            uint8_t c0 = (idx0 < paletteMapSize) ? paletteMap[idx0] : 0x0; // default to 0 (usually white)
            if (moreLogging) {
                Log_info_serial("Rendering PNG column %d; output byte: %d; png palette index: %X; color code: %X", x - 1, i, idx0, c0);
            }

            // Second (odd) pixel -> low nibble
            uint8_t idx1 = (x < width) ? srcRow[x++] : 0;
            uint8_t c1 = (idx1 < paletteMapSize) ? paletteMap[idx1] : 0x0;
            if (moreLogging) {
                Log_info_serial("PNG column: %d; png palette index: %X; color code: %X", x - 1, idx1, c1);
            }

            dstRow[i] = (uint8_t)((c0 << 4) | c1);
            if (moreLogging) {
                Log_info_serial("Output rendered byte: %X into byte %d", (uint8_t)((c0 << 4) | c1), i);
            }
        }

        esp_log_buffer_hex("png_line start", pTemp, 5);
        esp_log_buffer_hex("png_line end", pTemp + outLen - 5, 5);
        // Send the packed 4-bpp row to the panel and return; skip 1/2-bpp paths below
        bbep.writeData(pTemp, outLen);
        return 1;
    }
    if (!pDraw->pUser) {
        // 1-bit output, decode the single plane and write it
        for (x=0; x<pDraw->iWidth; x+= 8) {
          d[0] = s[0] ^ ucInvert;
          d++; s++;
        }
    } else { // we need to split the 2-bit data into plane 0 and 1
        src = *s++;
        src ^= ucInvert;
        uc = 0; // suppress warning/error
        if (*(int *)pDraw->pUser > 1) { // draw 2bpp data as 1-bit to use for partial update
            ucInvert = ~ucInvert; // the invert rule is backwards for grayscale data
            src = ~src;
            for (x=0; x<pDraw->iWidth; x++) {
                uc <<= 1;
                if (src & 0xc0) { // non-white -> black
                    uc |= 1; // high bit of source pair
                }
                src <<= 2;
                if ((x & 3) == 3) { // new input byte
                    src = *s++;
                    src ^= ucInvert;
                }
                if ((x & 7) == 7) { // new output byte
                    *d++ = uc;
                }
            } // for x
        } else { // normal 0/1 split plane
            ucMask = (*(int *)pDraw->pUser == 0) ? 0x40 : 0x80; // lower or upper source bit
            for (x=0; x<pDraw->iWidth; x++) {
                uc <<= 1;
                if (src & ucMask) {
                    uc |= 1; // high bit of source pair
                }
                src <<= 2;
                if ((x & 3) == 3) { // new input byte
                    src = *s++;
                    src ^= ucInvert;
                }
                if ((x & 7) == 7) { // new output byte
                    *d++ = uc;
                }
            } // for x
        }
    }
    bbep.writeData(pTemp, (pDraw->iWidth+7)/8);
    return 1;
} /* png_draw() */

/**
 * Simple linear array search.
 *
 * @param array Search array (haystack)
 * @param size Size of the search array
 * @param targetValue Value to search for (needle)
 * @return Index of target value in array, or -1 if not found.
 */
static int uint32_array_search(const uint32_t *array, int size, uint32_t targetValue) {
    for (int i = 0; i < size; ++i) {
        if (array[i] == targetValue) {
            return i;
        }
    }
    return -1;
}

static uint32_t png_palette_triplet_to_rgb888(const uint8_t *pTripletStart) {
    return
    // Red
    (pTripletStart[0] << 16) |
    // Green
    (pTripletStart[1] << 8) |
    // Blue
    (pTripletStart[2]);
}

/**
 * Gets the number of actually used entries in the palette (PNG PLTE) table by the crude
 *  means of just iterating through the array until we find a null entry.
 * Future improvement: store the size of the palette in PNGDRAW on image decode so we
 *  don't have to do this crap.
 *
 * TODO: write a unit test for this once I can sort out the tooling
 * @param pPalette PNGDRAW::pPalette pointer. 768 bytes where each set of 3 bytes is
 *  blue, green, red values to define a single color in the palette.
 * Bytes 769-1024 are alpha values, but we don't care about those for this application.
 * @return number of actually used entries (colors) in the palette.
 */
static int get_palette_size(uint8_t *pPalette) {
    if (!pPalette) {
        return -1;
    }
    // Stop only on a 0,0,0 triplet that is immediately
    // followed by another 0,0,0 (or we're at the last entry). This treats
    // a single 0,0,0 (true black) followed by non-zero as a valid color.
    int count = 0;
    for (int i = 0; i < 256; ++i) {
        const int idx = i * 3;
        if (png_palette_triplet_to_rgb888(&pPalette[idx]) == 0) {
            // Peek at the next triplet if available
            if (i >= 255) { // no next triplet; treat as terminator
                break;
            }
            int nextIdx = idx + 3;
            if (png_palette_triplet_to_rgb888(&pPalette[nextIdx]) == 0) {
                // Two consecutive null triplets -> end of palette
                break;
            }
            // Single 0,0,0 followed by non-zero -> count it (black) and continue
            ++count;
            continue;
        }
        ++count;
    }
    return count;
}

/**
 * Builds the paletteMap global variable and sets the corresponding paletteMapSize global variable.
 *
 * For "full" "color" displays, this function must be called before attempting to decode
 *  and write to the EPD display.
 *
 * The assumption here is that for "full color" displays, the server should only ever
 *  provide PNG images with color mode = indexed, where the palette exactly corresponds
 *  to the model's display's palette. This is because the server is expected to do all
 *  necessary processing to prepare the image for the EPD, specifically quantization/dithering.
 *
 *
 * @param displayPngPalette Array of PNG color codes supported by the current model's display.
 * @param displayColorCodes Array of display color codes supported by the current model's display.
 *  Indexes match displayPngPalette.
 * @param displayPaletteSize Size of displayPngPalette and displayColorCodes.
 * @param pngPalette Array of palette values defined in the indexed PNG file being decoded
 */
static void build_palette_map(
    // const uint32_t displayPngPalette[],
    // const uint8_t displayColorCodes[],
    // int displayPaletteSize,
    uint8_t pngPalette[]
) {
    int pngPaletteSize = get_palette_size(pngPalette);
    if (pngPaletteSize <= 0) {
        Log_error("Unable to determine palette size for indexed PNG");
        // TODO: restart the MCU or something more fatal
        return;
    }

    // Validate that the PNG's palette is the same size as the display's palette
    if (pngPaletteSize != bbep.getColorCount()) {
        // If it's not, we'll still give it a go, but this is not expected behavior.
        Log_error("WARNING: PNG palette size %d does not equal the palette size expected by this model's display %d",
            pngPaletteSize, bbep.getColorCount());
    }

    int i = 0;

    esp_log_buffer_hex("display rgb lookup table", bbep.getRgbColorLookup(), bbep.getColorCount() * 3);
    // Iterate over all PNG palette entries
    for (i = 0; i < pngPaletteSize; ++i) {
        uint32_t pngPaletteEntry = png_palette_triplet_to_rgb888(&pngPalette[i*3]);
        Log_info_serial("PNG palette entry index %d is %X", i, pngPaletteEntry);
        // Find the PNG palette entry in the display's list of supported PNG color codes
        int displayPngPaletteIndex = uint32_array_search(bbep.getRgbColorLookup(), bbep.getColorCount(), pngPaletteEntry);
        Log_info_serial("PNG palette entry index %d index into display palette is %d", i, displayPngPaletteIndex);

        if (displayPngPaletteIndex < 0) {
            // PNG palette entry is not supported by the display
            Log_error("WARNING: PNG palette entry %X with index %d is not in the device's supported color list."
                " PNG pixels with this palette's entry index will be drawn to the display as the first color in the palette (probably black).",
                pngPaletteEntry,
                i
            );
            paletteMap[i] = 0x00;
            continue;
        }

        // Write to the palette map
        paletteMap[i] = bbep.getColorLookup()[displayPngPaletteIndex];
    }

    paletteMapSize = i;
}

// static void build_palette_map(PNG *png) {
//     // TODO: move this mapping into #defines
//     if (bbep.capabilities() | BBEP_7COLOR) {
//         build_palette_map_for_device(
//             acep_7clr_rgb_palette,
//             u8Colors_7clr,
//             7,
//             png->getPalette()
//         );
//     } else if (bbep.capabilities() | BBEP_SPECTRA_6COLOR) {
//         build_palette_map_for_device(
//             spectra_6clr_rgb_palette,
//             u8Colors_spectra6,
//             6,
//             png->getPalette()
//         );
//     }
//     // TODO: } else if (bbep.capabilities() | BBEP_SPECTRA_9COLOR) {
// }

//
// A table to accelerate the testing of 2-bit images for the number
// of unique colors. Each entry sets bits 0-3 depending on the presence
// of colors 0-3 in each 2-bit pixel
//
const uint8_t ucTwoBitFlags[256] = {
0x01,0x03,0x05,0x09,0x03,0x03,0x07,0x0b,0x05,0x07,0x05,0x0d,0x09,0x0b,0x0d,0x09,
0x03,0x03,0x07,0x0b,0x03,0x03,0x07,0x0b,0x07,0x07,0x07,0x0f,0x0b,0x0b,0x0f,0x0b,
0x05,0x07,0x05,0x0d,0x07,0x07,0x07,0x0f,0x05,0x07,0x05,0x0d,0x0d,0x0f,0x0d,0x0d,
0x09,0x0b,0x0d,0x09,0x0b,0x0b,0x0f,0x0b,0x0d,0x0f,0x0d,0x0d,0x09,0x0b,0x0d,0x09,
0x03,0x03,0x07,0x0b,0x03,0x03,0x07,0x0b,0x07,0x07,0x07,0x0f,0x0b,0x0b,0x0f,0x0b,
0x03,0x03,0x07,0x0b,0x03,0x02,0x06,0x0a,0x07,0x06,0x06,0x0e,0x0b,0x0a,0x0e,0x0a,
0x07,0x07,0x07,0x0f,0x07,0x06,0x06,0x0e,0x07,0x06,0x06,0x0e,0x0f,0x0e,0x0e,0x0e,
0x0b,0x0b,0x0f,0x0b,0x0b,0x0a,0x0e,0x0a,0x0f,0x0e,0x0e,0x0e,0x0b,0x0a,0x0e,0x0a,
0x05,0x07,0x05,0x0d,0x07,0x07,0x07,0x0f,0x05,0x07,0x05,0x0d,0x0d,0x0f,0x0d,0x0d,
0x07,0x07,0x07,0x0f,0x07,0x06,0x06,0x0e,0x07,0x06,0x06,0x0e,0x0f,0x0e,0x0e,0x0e,
0x05,0x07,0x05,0x0d,0x07,0x06,0x06,0x0e,0x05,0x06,0x04,0x0c,0x0d,0x0e,0x0c,0x0c,
0x0d,0x0f,0x0d,0x0d,0x0f,0x0e,0x0e,0x0e,0x0d,0x0e,0x0c,0x0c,0x0d,0x0e,0x0c,0x0c,
0x09,0x0b,0x0d,0x09,0x0b,0x0b,0x0f,0x0b,0x0d,0x0f,0x0d,0x0d,0x09,0x0b,0x0d,0x09,
0x0b,0x0b,0x0f,0x0b,0x0b,0x0a,0x0e,0x0a,0x0f,0x0e,0x0e,0x0e,0x0b,0x0a,0x0e,0x0a,
0x0d,0x0f,0x0d,0x0d,0x0f,0x0e,0x0e,0x0e,0x0d,0x0e,0x0c,0x0c,0x0d,0x0e,0x0c,0x0c,
0x09,0x0b,0x0d,0x09,0x0b,0x0a,0x0e,0x0a,0x0d,0x0e,0x0c,0x0c,0x09,0x0a,0x0c,0x08
};

int png_draw_count(PNGDRAW *pDraw)
{
    int x, *pFlags = (int *)pDraw->pUser;
    uint8_t *s, set_bits;

    if (pDraw->y > 430) return 0; // Workaround to ignore the icon in the lower left corner

    set_bits = pFlags[0]; // use a local var
    s = (uint8_t *)pDraw->pPixels;
    for (x=0; x<pDraw->iWidth; x+=4) {
        set_bits |= ucTwoBitFlags[*s++]; // do 4 pixels at a time
    } // for x
    pFlags[0] = set_bits; // put it back in the flags array
    return 1;
} /* png_draw_count() */
/** 
 * @brief Function to decode a PNG and count the number of unique colors
 *        This is needed because 2-bit (4gray) images can sometimes contain
 *        only 2 unique colors. This will allow us to use partial (non-flickering)
 *        updates on these images.
 * @param pointer to the PNG class instance
 * @param pointer to the buffer holding the PNG file
 * @param size of the PNG file
 * @return the number of unique colors in the image (2 to 4)
 */
int png_count_colors(PNG *png, const uint8_t *pData, int iDataSize)
{
int i, iColors;
    png->openRAM((uint8_t *)pData, iDataSize, png_draw_count);
    i = 0;
    png->decode(&i, 0);
    png->close();
    iColors = 0;
    if (i & 1) iColors++;
    if (i & 2) iColors++;
    if (i & 4) iColors++;
    if (i & 8) iColors++;
    Log_info("%s [%d]: png_count_colors: %d\r\n", __FILE__, __LINE__, iColors);
    return iColors;
} /* png_count_colors() */
/** 
 * @brief Function to decode and display a PNG image from memory
 *        The decoded lines are written directly into the EPD framebuffer
 *        due to insufficient RAM to hold the fully decoded image
 * @param pointer to the buffer holding the PNG file
 * @param size of the PNG file
 * @return refresh mode based on image type and presence of old image
 */

int png_to_epd(const uint8_t *pPNG, int iDataSize)
{
int iPlane, rc = -1;
PNG *png = new PNG();

    if (!png) return PNG_MEM_ERROR; // not enough memory for the decoder instance
    rc = png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
    png->close();
    if (rc == PNG_SUCCESS) {
        if (png->getWidth() != bbep.width() || png->getHeight() != bbep.height()) {
            Log_error("PNG image size %d x %d doesn't match display size %d x %d", png->getWidth(), png->getHeight(), bbep.width(), bbep.height());
            rc = -1;
        } else { // okay to decode
            bool bDisplayIsFullColor = bbep.capabilities() & BBEP_FULL_COLOR;
            auto iPngColorCount = png_count_colors(png, pPNG, iDataSize);
            Log_info("%s [%d]: Decoding %d-bpp png with %d colors; display supports >= 6 colors: %d", __FILE__, __LINE__, png->getBpp(), iPngColorCount, bDisplayIsFullColor);
            // Prepare target memory window (entire display)
            bbep.setAddrWindow(0, 0, bbep.width(), bbep.height());
            if (png->getBpp() == 1 || (png->getBpp() == 2 && iPngColorCount == 2)) {
                Log_info("Drawing monochrome image");
                // If the image is 1 bit per pixel, or 2 bits per pixel but only contains 2 colors and is thus still monochrome
                // It's a 1-bit image (single plane)
                bbep.setPanelType(ONE_BIT_PANEL);
                rc = REFRESH_PARTIAL; // the new image is 1bpp - try a partial update
                bbep.startWrite(PLANE_0); // start writing image data to plane 0
                png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                if (png->getBpp() == 1 || png->getBpp() > 2) {
                    png->decode(NULL, 0);
                } else { // convert the 2-bit image to 1-bit output
                    Log_info("%s [%d]: Current png only has 2 unique colors!\n", __FILE__, __LINE__);
                    iPlane = 2;
                    if (png->decode(&iPlane, 0) != PNG_SUCCESS) {
                        Log_info("%s [%d]: Error decoding image = %d\n", __FILE__, __LINE__, png->getLastError());
                    }
                }
                png->close();
            } else if (png->getBpp() == 2 && iPngColorCount == 3 && !bDisplayIsFullColor) {
                Log_info("Drawing 3 color image");
                // If the image is 2 bits per pixel and 3 colors, it's a 3 color display B/W/R

                bbep.startWrite(PLANE_0); // start writing image data to plane 0
                iPlane = 0;
                Log_info("Writing plane 0 B\\W");
                png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                if (png->decode(&iPlane, 0) != PNG_SUCCESS) {
                    Log_error("Plane 0 decode failed: %d", png->getLastError());
                    png->close();
                    return -1;
                }
                png->close(); // start over for plane 1
                iPlane = 1;
                Log_info("Writing plane 1 R");
                png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                bbep.startWrite(PLANE_1); // start writing image data to plane 1
                if (png->decode(&iPlane, 0) != PNG_SUCCESS) {
                    Log_error("Plane 1 decode failed: %d", png->getLastError());
                    png->close();
                    return -1;
                }

                bbep.writePlane();

                // bbep.loadG5Image(smiley_bwr, 0, 160, 0,0,1.0f);
                // bbep.writePlane();
                // bbep.refresh(REFRESH_FULL);
                // bbep.wait();
            } else if (png->getBpp() == 2 && iPngColorCount == 4 && !bDisplayIsFullColor) {
                // TODO: could be B/W/R/Y or 4-gray. To be implemented
                Log_info("4 color image not yet implemented");
                rc = -1;
                // bbep.setPanelType(WAVESHARE_3_COLOR);
                // rc = REFRESH_FULL; // 4gray mode must be full refresh
                // iUpdateCount = 0; // grayscale mode resets the partial update counter
                // bbep.startWrite(PLANE_0); // start writing image data to plane 0
                // iPlane = 0;
                // Log_info("%s [%d]: decoding 4-gray plane 0", __FILE__, __LINE__);
                // png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                // png->decode(&iPlane, 0); // tell PNGDraw to use bits for plane 0
                // png->close(); // start over for plane 1
                // iPlane = 1;
                // Log_info("%s [%d]: decoding 4-gray plane 1", __FILE__, __LINE__);
                // png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                // bbep.startWrite(PLANE_1); // start writing image data to plane 1
                // png->decode(&iPlane, 0); // decode it again to get plane 1 data
            } else if ((png->getBpp() >= 1 && png->getBpp() <= 8) && bDisplayIsFullColor) {
                Log_info("Drawing %d color image to 'full color' display", iPngColorCount);

                // Build palette map in global memory
                // TODO: change PNGdec to accept palette map as a callback parameter so it's not always
                //  sitting around in global memory even when not needed.
                build_palette_map(png->getPalette());

                bbep.startWrite(PLANE_0); // start writing image data to plane 0
                png->openRAM((uint8_t *)pPNG, iDataSize, png_draw);
                if (png->decode(&iPlane, 0) != PNG_SUCCESS) {
                    Log_error("Plane 0 decode failed: %d", png->getLastError());
                    png->close();
                    return -1;
                }
                png->close(); // start over for plane 1
                // bbep.writePlane();
            } else {
                Log_info("Unhandled display configuration");
                rc = -1;
            }
        }
    }
    free(png); // free the decoder instance
    return rc;
} /* png_to_epd() */
/** 
 * @brief Function to show the image on the display
 * @param image_buffer pointer to the uint8_t image buffer
 * @param reverse shows if the color scheme is reverse
 * @return none
 */
void display_show_image(uint8_t *image_buffer, int data_size, bool bWait)

{
    Log_info("display_show_image start. Data size: %d; wait: %d", data_size, bWait);
    Log_info("image buffer start: %X%X%X%X", image_buffer[0], image_buffer[1], image_buffer[2], image_buffer[3]);
    bool isPNG = data_size >= 4 && MOTOLONG(image_buffer) == (int32_t)0x89504e47;
    auto width = display_width();
    auto height = display_height();
//    uint32_t *d32;
    bool bAlloc = false;
    int iRefreshMode = REFRESH_FULL; // assume full (slow) refresh

   // Log_info("Paint_NewImage %d", reverse);
    Log_info("maximum_compatibility = %d\n", apiDisplayResult.response.maximum_compatibility);
#ifdef FUTURE
    if (reverse)
    {
        d32 = (uint32_t *)image_buffer; // get framebuffer as a 32-bit pointer
        d32 = (uint32_t *)image_buffer; // get framebuffer as a 32-bit pointer
        Log_info("inverse the image");
        for (size_t i = 0; i < buf_size; i+=sizeof(uint32_t))
        for (size_t i = 0; i < buf_size; i+=sizeof(uint32_t))
        {
            d32[0] = ~d32[0];
            d32++;
            d32[0] = ~d32[0];
            d32++;
        }
    }
#endif
    if (isPNG == true && data_size < MAX_IMAGE_SIZE)
    {
        Log_info("White fill");
        bbep.fillScreen(BBEP_WHITE);
        Log_info("Drawing PNG");
        iRefreshMode = png_to_epd(image_buffer, data_size);
        if (iRefreshMode == -1) {
            Log_fatal("PNG decoding failed");
            delay(10000);
            ESP.restart();
        }
    }
    else // uncompressed BMP or Group5 compressed image
    {
        if (*(uint16_t *)image_buffer == BB_BITMAP_MARKER)
        {
            Log_info("G5 compressed image");
            BB_BITMAP *pBBB = (BB_BITMAP *)image_buffer;
#ifdef BB_EPAPER
            bbep.allocBuffer(false);
            bAlloc = true;
#endif
            const int panel_w = width;
            const int panel_h = height;
            if (pBBB->width <= 0 || pBBB->height <= 0 ||
                pBBB->width > 8192 || pBBB->height > 8192) {
                Log_fatal("G5 header invalid: %dx%d", pBBB->width, pBBB->height);
                ESP.restart();
            }
            int x = (panel_w - pBBB->width)/2;
            int y = (panel_h - pBBB->height)/2;
            if (pBBB->width > panel_w || pBBB->height > panel_h) {
                Log_fatal("G5 image too large for panel: %dx%d > %dx%d", pBBB->width, pBBB->height, panel_w, panel_h);
                // Option A: scale down using bbep.loadG5Image(..., scale)
                // Option B: implement a clipped blit (preferred for safety)
                // Until clipping is implemented, refuse to draw to avoid overflow:
                ESP.restart();
            }

            const int frame_bytes = ((panel_w + 7) >> 3) * panel_h;
            const bool width_byte_aligned = pBBB->width % 8 == 0;
            const bool x_byte_aligned = x % 8 == 0;

            if (pBBB->width != panel_w || pBBB->height != panel_h) {
                Log_fatal("G5 not full-frame: %dx%d, panel=%dx%d", pBBB->width, pBBB->height, panel_w, panel_h);
                ESP.restart();
            }

            if (!width_byte_aligned || !x_byte_aligned) {
                Log_info("G5 placement not byte-aligned (w=%d, x=%d). Drawing at (0,0) to avoid overrun.", pBBB->width, x);
                // Optional: clear to white if image smaller than panel
                bbep.fillScreen(BBEP_WHITE);
                bbep.loadG5Image(image_buffer, 0, 0, BBEP_WHITE, BBEP_BLACK);
            } else {
                if (x > 0 || y > 0) bbep.fillScreen(BBEP_WHITE);
                bbep.loadG5Image(image_buffer, x, y, BBEP_WHITE, BBEP_BLACK);
            }
        }
        else 
        {
        Log_info("BMP image");
        Log_info("Hacky flip and set buffer");
         // This work-around is due to a lack of RAM; the correct method would be to use loadBMP()
            flip_image(image_buffer+62, bbep.width(), bbep.height(), false); // fix bottom-up bitmap images
#ifdef BB_EPAPER
            bbep.setBuffer(image_buffer+62); // uncompressed 1-bpp bitmap
#endif
        }
        bbep.writePlane(PLANE_0); // send image data to the EPD
        iRefreshMode = REFRESH_PARTIAL;
        iUpdateCount = 1; // use partial update
    }
    Log_info("Display refresh start");
#ifdef BB_EPAPER
    if ((iUpdateCount & 7) == 0 || apiDisplayResult.response.maximum_compatibility == true) {
        Log_info("%s [%d]: Forcing full refresh; desired refresh mode was: %d", __FILE__, __LINE__, iRefreshMode);
        iRefreshMode = REFRESH_FULL; // force full refresh every 8 partials
    }
    int refresh_seconds = preferences.getUInt(PREFERENCES_SLEEP_TIME_KEY, SLEEP_TIME_TO_SLEEP);
    if (refresh_seconds >= 30*60 && iRefreshMode == REFRESH_PARTIAL) {
        // For users who set updates 30 minutes or longer, use the "fast" update to prevent ghosting
        Log_info("%s [%d]: Forcing fast refresh (not partial) since the TRMNL refresh_rate is set to > 30 min\n", __FILE__, __LINE__);
        iRefreshMode = REFRESH_FAST;
    }
    if (!bWait) iRefreshMode = REFRESH_PARTIAL; // fast update when showing loading screen
    Log_info("%s [%d]: EPD refresh mode: %d", __FILE__, __LINE__, iRefreshMode);
    bbep.refresh(iRefreshMode, bWait);
    Log_info("display_show_image near end; freeing buffer: %d", bAlloc);
    if (bAlloc) {
        bbep.freeBuffer();
    }
    iUpdateCount++;
#else
    bbep.fullUpdate();
#endif
    Log_info("display_show_image end");
}
/**
 * @brief Function to read an image from the file system
 * @param filename
 * @param pointer to file size returned
 * @return pointer to allocated buffer
 */
uint8_t * display_read_file(const char *filename, int *file_size)
{
File f = SPIFFS.open(filename, "r");
uint8_t *buffer;

  if (!f) {
    Serial.println("Failed to open file!");
    *file_size = 0;
    return nullptr;
  }
  *file_size = f.size();
  buffer = (uint8_t *)malloc(*file_size);
  if (!buffer) {
    Serial.println("Memory allocation filed!");
    *file_size = 0;
    return nullptr;
  }
  f.read(buffer, *file_size);
  f.close();
  return buffer;
} /* display_read_file() */

/**
 * @brief Function to show the image with message on the display
 * @param image_buffer pointer to the uint8_t image buffer
 * @param message_type type of message that will show on the screen
 * @return none
 */
void display_show_msg(uint8_t *image_buffer, MSG message_type)
{
    auto width = display_width();
    auto height = display_height();
    UWORD Imagesize = ((width % 8 == 0) ? (width / 8) : (width / 8 + 1)) * height;
    BB_RECT rect;

    Log_info("display_show_msg start");
    Log_info("maximum_compatibility = %d\n", apiDisplayResult.response.maximum_compatibility);
#ifdef BB_EPAPER
    bbep.allocBuffer(false);
#endif
    if (*(uint16_t *)image_buffer == BB_BITMAP_MARKER)
    {
        // G5 compressed image
        BB_BITMAP *pBBB = (BB_BITMAP *)image_buffer;
        int x = (width - pBBB->width)/2;
        int y = (height - pBBB->height)/2; // center it
        if (x > 0 || y > 0) // only clear if the image is smaller than the display
        {
            bbep.fillScreen(BBEP_WHITE); 
        }
        bbep.loadG5Image(image_buffer, x, y, BBEP_WHITE, BBEP_BLACK);
    }
    else
    {
#ifdef BB_EPAPER
        memcpy(bbep.getBuffer(), image_buffer+62, Imagesize); // uncompressed 1-bpp bitmap
#endif
    }

    bbep.setFont(nicoclean_8); //Roboto_20);
    bbep.setTextColor(BBEP_BLACK, BBEP_WHITE);

    switch (message_type)
    {
    case WIFI_CONNECT:
    {
        const char string1[] = "Connect to TRMNL WiFi";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w)/2, 430);
        bbep.println(string1);
        const char string2[] = "on your phone or computer";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w)/2, -1);
        bbep.print(string2);
    }
    break;
    case WIFI_FAILED:
    {
        const char string1[] = "Can't establish WiFi connection.";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w)/2, 386);
        bbep.println(string1);
        const char string2[] = "Hold button on the back to reset WiFi, or scan QR Code for help.";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.println(string2);

        bbep.loadG5Image(wifi_failed_qr, bbep.width() - 66 - 40, 40, BBEP_WHITE, BBEP_BLACK);
    }
    break;
    case WIFI_INTERNAL_ERROR:
    {
        const char string1[] = "WiFi connected, but";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - 132 - rect.w) / 2, 340);
        bbep.println(string1);
        const char string2[] = "API connection cannot be";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - 132 - rect.w) / 2, -1);
        bbep.println(string2);
        const char string3[] = "established. Try to refresh,";
        bbep.getStringBox(string3, &rect);
        bbep.setCursor((bbep.width() - 132 - rect.w) / 2, -1);
        bbep.println(string3);
        const char string4[] = "or scan QR Code for help.";
        bbep.getStringBox(string4, &rect);
        bbep.setCursor((bbep.width() - 132 - rect.w) / 2, -1);
        bbep.print(string4);

        bbep.loadG5Image(wifi_failed_qr, 639, 336, BBEP_WHITE, BBEP_BLACK);
    }
    break;
    case WIFI_WEAK:
    {
        const char string1[] = "WiFi connected but signal is weak";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.print(string1);
    }
    break;
    case API_ERROR:
    {
        const char string1[] = "WiFi connected, TRMNL not responding.";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 340);
        bbep.println(string1);
        const char string2[] = "Short click the button on back,";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.println(string2);
        const char string3[] = "otherwise check your internet.";
        bbep.getStringBox(string3, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.print(string3);
    }
    break;
    case API_SIZE_ERROR:
    {
        const char string1[] = "WiFi connected, TRMNL content malformed.";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.println(string1);
        const char string2[] = "Wait or reset by holding button on back.";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.print(string2);
    }
    break;
    case FW_UPDATE:
    {
        const char string1[] = "Firmware update available! Starting now...";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.print(string1);
    }
    break;
    case FW_UPDATE_FAILED:
    {
        const char string1[] = "Firmware update failed. Device will restart...";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.print(string1);
    }
    break;
    case FW_UPDATE_SUCCESS:
    {
        const char string1[] = "Firmware update success. Device will restart...";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.print(string1);
    }
    break;
    case MSG_TOO_BIG:
    {
        const char string1[] = "The image file from this URL is too large.";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 360);
        bbep.println(string1);
        if (strlen(filename) > 40) {
            filename[40] = 0; // truncate and add elipses
            strcat(filename, "...");
        }
        bbep.getStringBox(filename, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.println(filename);

        const char string2[] = "PNG images can be a maximum of";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.println(string2);
        String string3 = String(MAX_IMAGE_SIZE) + String(" bytes each and 1 or 2-bpp");
        bbep.getStringBox(string3.c_str(), &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.print(string3);
    }
    break;
    case MSG_FORMAT_ERROR:
    {
        const char string1[] = "The image format is incorrect";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 400);
        bbep.print(string1);
    }
    break;
    case TEST:
    {
        bbep.setCursor(0, 40);
        bbep.println("ABCDEFGHIYABCDEFGHIYABCDEFGHIYABCDEFGHIYABCDEFGHIY");
        bbep.println("abcdefghiyabcdefghiyabcdefghiyabcdefghiyabcdefghiy");
        bbep.println("A B C D E F G H I Y A B C D E F G H I Y A B C D E");
        bbep.println("a b c d e f g h i y a b c d e f g h i y a b c d e");
    }
    break;
    default:
        break;
    }
#ifdef BB_EPAPER
    bbep.writePlane(PLANE_0);
    bbep.refresh(REFRESH_FULL, true);
    bbep.freeBuffer();
#else
    bbep.fullUpdate();
#endif
    Log_info("display_show_msg end");
}

/**
 * @brief Function to show the image with message on the display
 * @param image_buffer pointer to the uint8_t image buffer
 * @param message_type type of message that will show on the screen
 * @param friendly_id device friendly ID
 * @param id shows if ID exists
 * @param fw_version version of the firmware
 * @param message additional message
 * @return none
 */
void display_show_msg(uint8_t *image_buffer, MSG message_type, String friendly_id, bool id, const char *fw_version, String message)
{
    Log_info("Free heap in display_show_msg - %d", ESP.getFreeHeap());
    Log_info("maximum_compatibility = %d\n", apiDisplayResult.response.maximum_compatibility);
#ifdef BB_EPAPER
    bbep.allocBuffer(false);
    Log_info("Free heap after bbep.allocBuffer(): %d", ESP.getFreeHeap());
#endif

    if (message_type == WIFI_CONNECT)
    {
        Log_info("Display set to black");
        bbep.fillScreen(BBEP_BLACK);
#ifdef BB_EPAPER
        bbep.writePlane(PLANE_0);
        if (!apiDisplayResult.response.maximum_compatibility) {
            bbep.refresh(REFRESH_FAST, true); // newer panel can handle the fast refresh
        } else {
            bbep.refresh(REFRESH_FULL, true); // incompatible panel (for now)
        }
#else
        bbep.fullUpdate();
#endif
        display_sleep(1000);
    }

    auto width = display_width();
    auto height = display_height();
    UWORD Imagesize = ((width % 8 == 0) ? (width / 8) : (width / 8 + 1)) * height;
    BB_RECT rect;

    Log_info("display_show_msg2 start");

    // Load the image into the bb_epaper framebuffer
    if (*(uint16_t *)image_buffer == BB_BITMAP_MARKER)
    {
        // G5 compressed image
        BB_BITMAP *pBBB = (BB_BITMAP *)image_buffer;
        int x = (width - pBBB->width)/2;
        int y = (height - pBBB->height)/2; // center it
        if (x > 0 || y > 0) // only clear if the image is smaller than the display
        { 
            bbep.fillScreen(BBEP_WHITE);
        }
        bbep.loadG5Image(image_buffer, x, y, BBEP_WHITE, BBEP_BLACK);
    }
    else
    {
#ifdef BB_EPAPER
        memcpy(bbep.getBuffer(), image_buffer+62, Imagesize); // uncompressed 1-bpp bitmap
#endif
    }

    bbep.setFont(nicoclean_8); //Roboto_20);
    bbep.setTextColor(BBEP_BLACK, BBEP_WHITE);
    switch (message_type)
    {
    case FRIENDLY_ID:
    {
        Log_info("friendly id case");
        const char string1[] = "Please sign up at usetrmnl.com/signup";
        bbep.getStringBox(string1, &rect);
        bbep.setCursor((bbep.width() - rect.w)/2, 400);
        bbep.println(string1);

        String string2 = "with Friendly ID ";
        if (id)
        {
            string2 += friendly_id;
        }
        string2 += " to finish setup";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w)/2, -1);
        bbep.print(string2);
    }
    break;
    case WIFI_CONNECT:
    {
        Log_info("wifi connect case");

        String string1 = "TRMNL firmware ";
        string1 += fw_version;
        bbep.setCursor(40, 48); // place in upper left corner
        bbep.println(string1);
        const char string2[] = "Connect your phone or computer to TRMNL WiFi network";
        bbep.getStringBox(string2, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, 386);
        bbep.println(string2);
        const char string3[] = "or scan the QR code for help";
        bbep.getStringBox(string3, &rect);
        bbep.setCursor((bbep.width() - rect.w) / 2, -1);
        bbep.print(string3);
        bbep.loadG5Image(wifi_connect_qr, bbep.width() - 40 - 66, 40, BBEP_WHITE, BBEP_BLACK); // 66x66 QR code
    }
    break;
    case MAC_NOT_REGISTERED:
    {
        UWORD y_start = 340;
        UWORD font_width = 18; // DEBUG
        Paint_DrawMultilineText(0, y_start, message.c_str(), width, font_width, BBEP_BLACK, BBEP_WHITE, nicoclean_8/*Roboto_20*/, true);
    }
    break;
    default:
        break;
    }
    Log_info("Start drawing...");
#ifdef BB_EPAPER
    bbep.writePlane(PLANE_0);
    bbep.refresh(REFRESH_FULL, true);
    bbep.freeBuffer();
#else
    bbep.fullUpdate();
#endif
    Log_info("display_show_msg2 end");
}

/**
 * @brief Function to got the display to the sleep
 * @param none
 * @return none
 */
void display_sleep(void)
{
    Log_info("Goto Sleep...");
#ifdef BB_EPAPER
    bbep.sleep(DEEP_SLEEP);
#else
    bbep.einkPower(0);
    bbep.deInit();
#endif
}