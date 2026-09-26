#include <M5Unified.h>
#include <WiFi.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace
{

struct Theme
{
    const char *name;
    uint16_t background;
    uint16_t primary;
    uint16_t accent;
    uint16_t text;
};

constexpr std::array<Theme, 5> themes = {{
    {"CYBERPUNK 2077", 0x0000, 0x96FE, 0xF9FF, 0xFFFF},
    {"HACKER MATRIX", 0x0000, 0x07E0, 0x03E0, 0xFFFF},
    {"TRON REAPER", 0x0000, 0x05FF, 0xF800, 0xFFFF},
    {"FUTURE SCI-FI", 0x0841, 0x07FF, 0xF81F, 0xEFEF},
    {"PINK CANDY", 0x180F, 0xF81F, 0xFD20, 0xFFFF},
}};

struct AccessPoint
{
    char ssid[25];
    int16_t rssi;
    uint8_t channel;
    wifi_auth_mode_t security;
};

constexpr size_t kMaxAccessPoints = 4;
std::array<AccessPoint, kMaxAccessPoints> accessPoints{};
size_t accessPointCount = 0;
int16_t discoveredAccessPointCount = 0;
uint8_t currentTheme = 0;
uint32_t lastFrameMs = 0;
uint32_t animationFrame = 0;
int16_t screenWidth = 240;
int16_t screenHeight = 135;
bool scanFailed = false;

const char *securityName(wifi_auth_mode_t security)
{
    switch (security) {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA MIX";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/3";
    default:
        return "SECURE";
    }
}

void scanNearbyAccessPoints()
{
    accessPointCount = 0;
    discoveredAccessPointCount = 0;
    scanFailed = false;

    const wifi_mode_t previousMode = WiFi.getMode();
    if (previousMode == WIFI_MODE_NULL && !WiFi.mode(WIFI_MODE_STA)) {
        scanFailed = true;
        return;
    }

    discoveredAccessPointCount = WiFi.scanNetworks(false, true, true);
    if (discoveredAccessPointCount < 0) {
        scanFailed = true;
    } else {
        accessPointCount = std::min(static_cast<size_t>(discoveredAccessPointCount), accessPoints.size());
        for (size_t i = 0; i < accessPointCount; ++i) {
            const String ssid = WiFi.SSID(static_cast<int>(i));
            AccessPoint &entry = accessPoints[i];
            if (ssid.isEmpty()) {
                strlcpy(entry.ssid, "(hidden)", sizeof(entry.ssid));
            } else {
                strlcpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid));
            }
            entry.rssi = static_cast<int16_t>(WiFi.RSSI(static_cast<int>(i)));
            entry.channel = static_cast<uint8_t>(WiFi.channel(static_cast<int>(i)));
            entry.security = WiFi.encryptionType(static_cast<int>(i));
        }
    }

    WiFi.scanDelete();
    if (previousMode == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_MODE_NULL);
    }
}

void drawGhostMark(const Theme &theme)
{
    const int16_t size = std::max<int16_t>(18, screenHeight / 5);
    const int16_t x = 8;
    const int16_t y = 7;
    const int16_t bodyHeight = size * 3 / 4;

    M5.Display.fillCircle(x + size / 2, y + size / 3, size / 3, theme.primary);
    M5.Display.fillRect(x + size / 6, y + size / 3, size * 2 / 3, bodyHeight / 2, theme.primary);
    M5.Display.fillTriangle(x + size / 6, y + bodyHeight * 5 / 6, x + size / 2, y + bodyHeight * 5 / 6,
                            x + size / 3, y + bodyHeight, theme.primary);
    M5.Display.fillTriangle(x + size / 2, y + bodyHeight * 5 / 6, x + size * 5 / 6, y + bodyHeight * 5 / 6,
                            x + size * 2 / 3, y + bodyHeight, theme.primary);
    M5.Display.fillCircle(x + size / 3, y + size / 3, std::max<int16_t>(2, size / 12), theme.accent);
    M5.Display.fillCircle(x + size * 2 / 3, y + size / 3, std::max<int16_t>(2, size / 12), theme.accent);
}

void drawDashboard()
{
    const Theme &theme = themes[currentTheme];
    M5.Display.fillScreen(theme.background);
    drawGhostMark(theme);

    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(theme.primary, theme.background);
    M5.Display.drawString("GHOST", screenWidth / 2 - 26, 7);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(theme.accent, theme.background);
    M5.Display.drawString("CYBERPUNK FIELD TOOLS", 8, screenHeight / 4);
    M5.Display.drawFastHLine(8, screenHeight / 4 + 12, screenWidth - 16, theme.accent);

    char line[56];
    M5.Display.setTextColor(theme.text, theme.background);
    if (scanFailed) {
        M5.Display.drawString("Wi-Fi survey unavailable", 8, screenHeight / 3);
    } else {
        snprintf(line, sizeof(line), "PASSIVE WI-FI SURVEY: %d FOUND", discoveredAccessPointCount);
        M5.Display.drawString(line, 8, screenHeight / 3);

        const int16_t rowHeight = std::max<int16_t>(12, std::min<int16_t>(18, screenHeight / 10));
        for (size_t i = 0; i < accessPointCount; ++i) {
            const AccessPoint &entry = accessPoints[i];
            snprintf(line, sizeof(line), "%s  %ddBm CH%u %s", entry.ssid, entry.rssi, entry.channel,
                     securityName(entry.security));
            M5.Display.drawString(line, 8, screenHeight / 3 + 15 + static_cast<int16_t>(i) * rowHeight);
        }
        if (discoveredAccessPointCount == 0) {
            M5.Display.drawString("No access points detected", 8, screenHeight / 3 + 16);
        } else if (discoveredAccessPointCount > static_cast<int16_t>(kMaxAccessPoints)) {
            snprintf(line, sizeof(line), "+ %d more nearby", discoveredAccessPointCount - kMaxAccessPoints);
            M5.Display.drawString(line, 8, screenHeight / 3 + 15 + static_cast<int16_t>(accessPointCount) * rowHeight);
        }
    }

    M5.Display.setTextColor(theme.accent, theme.background);
    M5.Display.drawString(themes[currentTheme].name, 8, screenHeight - 23);
    M5.Display.setTextColor(theme.text, theme.background);
    M5.Display.drawString("BtnA: rescan + change theme", 8, screenHeight - 11);
}

void showBootSplash()
{
    const Theme &theme = themes[0];
    M5.Display.fillScreen(theme.background);
    drawGhostMark(theme);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(theme.primary, theme.background);
    M5.Display.drawString("GHOST", screenWidth / 2, screenHeight * 2 / 3);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(theme.accent, theme.background);
    M5.Display.drawString("BY PINOYUNKNOWN", screenWidth / 2, screenHeight - 8);
    delay(1200);
}

} // namespace

void setup()
{
    auto config = M5.config();
    M5.begin(config);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    showBootSplash();
    scanNearbyAccessPoints();
    drawDashboard();
}

void loop()
{
    M5.update();
    if (M5.BtnA.wasPressed()) {
        currentTheme = static_cast<uint8_t>((currentTheme + 1) % themes.size());
        scanNearbyAccessPoints();
        drawDashboard();
    }

    if (millis() - lastFrameMs >= 50) {
        lastFrameMs = millis();
        ++animationFrame;
        const Theme &theme = themes[currentTheme];
        const int16_t x = static_cast<int16_t>((animationFrame * 3) % screenWidth);
        const int16_t previousX = (x + screenWidth - 3) % screenWidth;
        M5.Display.drawFastVLine(x, 0, 3, theme.accent);
        M5.Display.drawFastVLine(previousX, 0, 3, theme.background);
    }
    delay(1);
}
