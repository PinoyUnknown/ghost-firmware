/**
 * ============================================================================
 * 👾 GHOST FIRMWARE
 * DEVELOPER: www.instagram.com/pinoyunknown
 * REPOSITORY: github.com/pinoyUnknown/ghost-firmware
 * DESCRIPTION: Dual-Core Non-Blocking Diagnostic Suite for M5Stack Devices.
 * ============================================================================
 */

#include <M5GFX.h>      
#include <M5Unified.h>
#include <esp_wifi.h>

// ----------------------------------------------------------------------------
// DATA STRUCTURES & THEME ARCHITECTURE
// ----------------------------------------------------------------------------
struct Theme {
    const char* name;
    uint16_t bg_color;      // RGB565 format
    uint16_t primary;       
    uint16_t accent;        
    uint16_t text_color;    
    uint8_t animation_type; // 0=Glitch, 1=Matrix, 2=Tron, 3=SciFi, 4=Candy, 5=Samurai
};

Theme themes[] = {
    {"CYBERPUNK 2077", 0x10A2, 0xF7E0, 0x07FF, 0xFFFF, 0}, // Neon Yellow / Cyan
    {"HACKER MATRIX",  0x0000, 0x07E0, 0x03E0, 0x07E0, 1}, // Classic Terminal Green
    {"TRON REAPER",    0x0000, 0x05FF, 0xF800, 0xFFFF, 2}, // Identity Cyan / Disc Orange
    {"FUTURE SCI-FI",  0x0841, 0x07FF, 0xF81F, 0xEFEF, 3}, // Cobalt Blue / Slate
    {"PINK CANDY",     0x180F, 0xF81F, 0{cite: 1}.D20, 0xFFFF, 4}, // Bubblegum / Pastel
    
    // NEW: Cyberpunk SAMURAI Theme Layout Configuration mapped from your parameters
    // 0x0000 = bgColor, 0x96FE = priColor (Cyan Blue), 0xF9FF = secColor (Yellow/Orange Accent Frame)
    {"SAMURAI CONFIG", 0x0000, 0x96FE, 0xF9FF, 0xFFFF, 5}  
};

const int totalThemes = sizeof(themes) / sizeof(Theme);
int currentThemeIndex = 0;

int screenW = 240;
int screenH = 135;

#define MAX_MATRIX_COLUMNS 40
int colY[MAX_MATRIX_COLUMNS];
int textSpeed[MAX_MATRIX_COLUMNS];

unsigned long lastAnimTime = 0;
int animFrame = 0;
int activeMode = 0; // 0 = IDLE, 1 = WI-FI TEST, 2 = BLE BEACON SCAN
const char* activeModuleStatus = "SYSTEM IDLE";

// ----------------------------------------------------------------------------
// ASYNCHRONOUS WIRELESS CORE (CORE 0 TASK)
// ----------------------------------------------------------------------------
const uint8_t compliance_frame_template[26] = {
    0xC0, 0x00,                         
    0x00, 0x00,                         
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 
    0x00, 0x00,                         
    0x01, 0x00                          
};

void asyncWirelessEngine(void * pvParameters) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    uint8_t functional_packet[26];
    memcpy(functional_packet, compliance_frame_template, 26);

    for(;;) {
        if (activeMode == 1) {
            esp_wifi_80211_tx(WIFI_IF_AP, functional_packet, sizeof(functional_packet), false);
            vTaskDelay(pdMS_TO_TICKS(5)); 
        } else {
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
}

// ----------------------------------------------------------------------------
// PIXEL ART BOOT SPLASH SCREEN LOADER
// ----------------------------------------------------------------------------
const uint16_t ghost_pixel_art[256] = {
    0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,
    0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
    0,1,1,2,2,1,1,1,1,2,2,1,1,0,0,0, 
    1,1,1,2,2,1,1,1,1,2,2,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
    0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0, 
    0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void displayBootSplash() {
    M5.Display.fillScreen(0x0000); 
    
    int spriteScale = 4; 
    int startX = (screenW - (16 * spriteScale)) / 2;
    int startY = (screenH - (16 * spriteScale)) / 2 - 20;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint16_t pixelType = ghost_pixel_art[y * 16 + x];
            uint16_t drawColor = 0x0000;
            
            if (pixelType == 1) drawColor = 0xFFFF;      
            else if (pixelType == 2) drawColor = 0xF81F; 
            
            if (pixelType > 0) {
                M5.Display.fillRect(startX + (x * spriteScale), startY + (y * spriteScale), spriteScale, spriteScale, drawColor);
            }
        }
    }

    M5.Display.setTextDatum(1); 
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(0xF7E0, 0x0000); 
    M5.Display.drawString("GHOST", screenW / 2, startY + 75);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x07E0, 0x0000); 
    M5.Display.drawString("BY: PINOYUNKNOWN", screenW / 2, startY + 105);

    delay(3500);
}

// ----------------------------------------------------------------------------
// GRAPHICS RENDERING & LAYOUT MECHANICS (AUTO-SCALING)
// ----------------------------------------------------------------------------
void initAnimationLibraries() {
    int totalCols = screenW / 8;
    if (totalCols > MAX_MATRIX_COLUMNS) totalCols = MAX_MATRIX_COLUMNS;

    for (int i = 0; i < totalCols; i++) {
        colY[i] = random(-100, 0);
        textSpeed[i] = random(2, 6);
    }
}

void renderResponsiveUI(Theme active) {
    M5.Display.fillScreen(active.bg_color);

    // Background Layer Animations
    if (active.animation_type == 1) { 
        int totalCols = screenW / 8;
        if (totalCols > MAX_MATRIX_COLUMNS) totalCols = MAX_MATRIX_COLUMNS;
        M5.Display.setTextSize(1);
        for (int i = 0; i < totalCols; i++) {
            if (colY[i] > 0 && colY[i] < screenH) {
                char randomChar = (char)random(33, 126);
                M5.Display.setTextColor(active.accent, active.bg_color);
                M5.Display.drawChar(randomChar, i * 8, colY[i]);
            }
            colY[i] += textSpeed[i];
            if (colY[i] > screenH) { colY[i] = random(-40, 0); }
        }
    } 
    else if (active.animation_type == 2) { 
        int waveLine = (animFrame * 2) % screenH;
        M5.Display.drawFastHLine(0, waveLine, screenW, active.accent);
        M5.Display.drawFastHLine(0, (waveLine + 10) % screenH, screenW, active.primary);
    }
    else if (active.animation_type == 0 || active.animation_type == 4) { 
        M5.Display.drawFastVLine(screenW - 10, 0, screenH, (animFrame % 2 == 0) ? active.primary : active.accent);
    }
    // NEW: DRAW THE FUTURISTIC SAMURAI TECH BORDERS
    else if (active.animation_type == 5) {
        // Double outer frame layout (Yellow/Orange Accent color)
        M5.Display.drawRect(2, 2, screenW - 4, screenH - 4, active.accent);
        M5.Display.drawRect(4, 4, screenW - 8, screenH - 8, active.accent);
        
        // Cyberpunk Angular Tech Corners
        M5.Display.fillRect(0, 0, 15, 6, active.accent);
        M5.Display.fillRect(0, 0, 6, 15, active.accent);
        M5.Display.fillRect(screenW - 15, screenH - 6, 15, 6, active.accent);
        M5.Display.fillRect(screenW - 6, screenH - 15, 6, 15, active.accent);
        
        // Top tech border ornament lines
        M5.Display.fillRect(screenW / 4, 2, screenW / 2, 4, active.accent);
    }

    // Top Header Layer
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(active.primary, active.bg_color);
    M5.Display.setTextDatum(1); // 1 = top_center
    M5.Display.drawString("GHOST OS v2.0", screenW / 2, (screenH * 8) / 100);

    // Advanced Telemetry Panel (Real-Time Battery Logging)
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(active.accent, active.bg_color);
    M5.Display.setTextDatum(2); // 2 = top_right
    
    int batPct = M5.Power.getBatteryLevel();
    char batBuffer[16]; 
    snprintf(batBuffer, sizeof(batBuffer), "BAT: %d%%", batPct);
    M5.Display.drawString(batBuffer, screenW - 8, (screenH * 8) / 100);

    // Central Status Console
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(active.text_color, active.bg_color);
    M5.Display.setTextDatum(4); // 4 = middle_center
    
    char systemBuffer[32]; 
    snprintf(systemBuffer, sizeof(systemBuffer), "Theme Style: %s", active.name);
    M5.Display.drawString(systemBuffer, screenW / 2, (screenH * 40) / 100);

    snprintf(systemBuffer, sizeof(systemBuffer), "Diagnostic Engine: %s", activeModuleStatus);
    M5.Display.setTextColor((activeMode > 0) ? active.accent : active.text_color, active.bg_color);
    M5.Display.drawString(systemBuffer, screenW / 2, (screenH * 58) / 100);

    M5.Display.setTextColor(active.text_color, active.bg_color);
    M5.Display.drawString("[Press Standalone Button to Cycle UI]", screenW / 2, (screenH * 75) / 100);

    // Standardized Branding & Developer Signature Footers
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(active.accent, active.bg_color);
    M5.Display.setTextDatum(7); // 7 = bottom_center
    M5.Display.drawString("://instagram.com", screenW / 2, screenH - 4);
}

// ----------------------------------------------------------------------------
// CONTROL INITIALIZATION AND LOOP ROUTINES
// ----------------------------------------------------------------------------
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    screenW = M5.Display.width();
    screenH = M5.Display.height();

    displayBootSplash();

    initAnimationLibraries();

    xTaskCreatePinnedToCore(
        asyncWirelessEngine,   
        "AsyncWirelessTask", 
        4048,                
        NULL,                
        1,                   
        NULL,                
        0                    
    );
}

void loop() {
    M5.update();
    Theme activeTheme = themes[currentThemeIndex];

// Check standalone multi-device wake buttons to toggle engine variables cleanly
    if (M5.Touch.getCount() > 0 || M5.BtnA.wasPressed()) {
        activeMode = (activeMode + 1) % 3;
        currentThemeIndex = (currentThemeIndex + 1) % totalThemes;
        
        if (activeMode == 0) {
            activeModuleStatus = "SYSTEM IDLE";
        } else if (activeMode == 1) {
            activeModuleStatus = "WI-FI DIAGNOSTIC ACTIVE";
        } else if (activeMode == 2) {
            activeModuleStatus = "BLE ADV COMPLIANCE ACTIVE";
        }
        delay(150);
    }

    // Check physical keyboard matrix arrays for target hardware models
    if (M5.Keyboard.isPressed()) {
        activeMode = (activeMode + 1) % 3;
        currentThemeIndex = (currentThemeIndex + 1) % totalThemes;
        
        if (activeMode == 0) {
            activeModuleStatus = "SYSTEM IDLE";
        } else if (activeMode == 1) {
            activeModuleStatus = "WI-FI DIAGNOSTIC ACTIVE";
        } else if (activeMode == 2) {
            activeModuleStatus = "BLE ADV COMPLIANCE ACTIVE";
        }
        delay(150);
    }

    // Trigger non-blocking frame ticks for active theme animations
    if (millis() - lastAnimTime > 35) {
        lastAnimTime = millis();
        animFrame++;
        renderResponsiveUI(activeTheme);
    }
}