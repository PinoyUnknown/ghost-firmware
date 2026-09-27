#include <M5Unified.h>
#include <BLEDevice.h>
#include <WiFi.h>
#include <Wire.h>

#include "theme_assets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{

constexpr uint8_t kKeyboardAddress = 0x34;
constexpr uint8_t kKeyboardAddressAlternate = 0x5F;
constexpr uint8_t kKeyEventCountRegister = 0x03;
constexpr uint8_t kKeyEventRegister = 0x04;
constexpr uint8_t kMaxKeysPerRead = 10;
constexpr int16_t kThemeColor = cyberpunkPrimaryColor;
constexpr int16_t kAccentColor = cyberpunkAccentColor;
constexpr int16_t kBackgroundColor = cyberpunkBackgroundColor;

constexpr uint8_t kKeySelect = 0x0D;
constexpr uint8_t kKeyBackspace = 0x08;
constexpr uint8_t kKeyEscape = 0x1B;
constexpr uint8_t kKeyLeft = 0xB4;
constexpr uint8_t kKeyUp = 0xB5;
constexpr uint8_t kKeyDown = 0xB6;
constexpr uint8_t kKeyRight = 0xB7;
constexpr uint8_t kKeyShift = 0x06;
constexpr uint8_t kKeyFunction = 0x02;
constexpr uint8_t kKeyboardRows = 7;
constexpr uint8_t kKeyboardColumns = 8;
constexpr uint8_t kThemeCount = 14;

constexpr uint8_t kKeyMap[56][3] = {
    {'`', '~', kKeyEscape}, {'\t', 0, 0},                         {0, 0, 0},             {0, 0, 0},
    {'1', '!', 0},           {'q', 'Q', 0x90},                    {0, 0, 0},             {0, 0, 0},
    {'2', '@', 0},           {'w', 'W', 0},                       {'a', 'A', 0},          {0, 0, 0},
    {'3', '#', 0},           {'e', 'E', 0},                       {'s', 'S', 0},          {'z', 'Z', 0},
    {'4', '$', 0},           {'r', 'R', 0},                       {'d', 'D', 0},          {'x', 'X', 0},
    {'5', '%', 0},           {'t', 'T', 0},                       {'f', 'F', 0},          {'c', 'C', 0},
    {'6', '^', 0},           {'y', 'Y', 0},                       {'g', 'G', 0x9E},       {'v', 'V', 0},
    {'7', '&', 0},           {'u', 'U', 0},                       {'h', 'H', 0},          {'b', 'B', 0xAA},
    {'8', '*', 0},           {'i', 'I', 0},                       {'j', 'J', 0},          {'n', 'N', 0},
    {'9', '(', 0},           {'o', 'O', 0},                       {'k', 'K', 0},          {'m', 'M', 0xAC},
    {'0', ')', 0},           {'p', 'P', 0xAF},                    {'l', 'L', 0},          {',', '<', kKeyLeft},
    {'_', '-', 0},           {'[', '{', 0},                       {';', ':', kKeyUp},     {'.', '>', kKeyDown},
    {'=', '+', 0},           {']', '}', 0},                       {'\'', '"', 0},        {'/', '?', kKeyRight},
    {kKeyBackspace, 0, 0},   {'\\', '|', 0},                      {kKeySelect, 0, 0},     {' ', ' ', ' '}};

enum class Screen : uint8_t { Home, Feature };

struct AccessPoint
{
    char ssid[25];
    int16_t rssi;
    uint8_t channel;
    wifi_auth_mode_t security;
};

struct BluetoothDevice
{
    char name[25];
    int16_t rssi;
};

struct ThemeImage
{
    const uint8_t *data;
    size_t size;
    const char *name;
};

constexpr std::array<ThemeImage, kThemeCount> themeImages = {{
    {cyberpunkConnectImage, sizeof(cyberpunkConnectImage), "CONNECT"},
    {cyberpunkConfigImage, sizeof(cyberpunkConfigImage), "CONFIG"},
    {cyberpunkClockImage, sizeof(cyberpunkClockImage), "CLOCK"},
    {cyberpunkBleImage, sizeof(cyberpunkBleImage), "BLE"},
    {cyberpunkWifiImage, sizeof(cyberpunkWifiImage), "WIFI"},
    {cyberpunkRfImage, sizeof(cyberpunkRfImage), "RF"},
    {cyberpunkNrfImage, sizeof(cyberpunkNrfImage), "NRF"},
    {cyberpunkNfcImage, sizeof(cyberpunkNfcImage), "NFC"},
    {cyberpunkMiscImage, sizeof(cyberpunkMiscImage), "MISC"},
    {cyberpunkJsImage, sizeof(cyberpunkJsImage), "JS"},
    {cyberpunkIrImage, sizeof(cyberpunkIrImage), "IR"},
    {cyberpunkGpsImage, sizeof(cyberpunkGpsImage), "GPS"},
    {cyberpunkFmImage, sizeof(cyberpunkFmImage), "FM"},
    {cyberpunkFilesImage, sizeof(cyberpunkFilesImage), "FILE"},
}};

constexpr const char *featureDetails[kThemeCount] = {
    "Connection tools not implemented",
    "Settings screen not implemented",
    "Cardputer uptime clock",
    "Passive nearby BLE survey",
    "Passive nearby network survey",
    "LoRa radio module not configured",
    "External nRF24 module required",
    "External NFC module required",
    "Device info screen not implemented",
    "No JavaScript runtime installed",
    "Infrared hardware support unavailable",
    "GPS support not configured",
    "External FM receiver required",
    "File browser support unavailable",
};

class MemoryImageStream : public Stream
{
  public:
    MemoryImageStream(const uint8_t *data, size_t length) : data(data), length(length) {}

    int available() override
    {
        return static_cast<int>(length - position);
    }

    int read() override
    {
        return position < length ? data[position++] : -1;
    }

    int peek() override
    {
        return position < length ? data[position] : -1;
    }

    void flush() override {}

    size_t write(uint8_t) override
    {
        return 0;
    }

  private:
    const uint8_t *data;
    size_t length;
    size_t position = 0;
};

std::array<AccessPoint, 5> accessPoints{};
std::array<BluetoothDevice, 5> bluetoothDevices{};
size_t accessPointCount = 0;
size_t bluetoothDeviceCount = 0;
int16_t discoveredAccessPointCount = 0;
int16_t discoveredBluetoothDeviceCount = 0;
int16_t screenWidth = 240;
int16_t screenHeight = 135;
uint8_t selectedApp = 0;
uint8_t keyboardModifiers = 0;
Screen currentScreen = Screen::Home;
bool keyboardReady = false;
uint8_t keyboardAddress = kKeyboardAddress;
bool scanFailed = false;
bool bluetoothScanFailed = false;
bool bluetoothReady = false;

void drawThemeImage(uint8_t index, int32_t x, int32_t y, int32_t width, int32_t height)
{
    const ThemeImage &asset = themeImages[index % themeImages.size()];
    constexpr float assetWidth = 320.0F;
    constexpr float assetHeight = 144.0F;
    const float scale = std::min(width / assetWidth, height / assetHeight);
    const int32_t drawWidth = static_cast<int32_t>(std::lround(assetWidth * scale));
    const int32_t drawHeight = static_cast<int32_t>(std::lround(assetHeight * scale));
    const int32_t drawX = x + (width - drawWidth) / 2;
    const int32_t drawY = y + (height - drawHeight) / 2;
    MemoryImageStream image(asset.data, asset.size);
    if (!M5.Display.drawPng(&image, drawX, drawY, 0, 0, 0, 0, scale, scale)) {
        M5.Display.drawRect(x, y, width, height, kAccentColor);
        M5.Display.setTextColor(kAccentColor, kBackgroundColor);
        M5.Display.drawCenterString("THEME IMAGE ERROR", x + width / 2, y + height / 2);
    }
}

bool writeKeyboardRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool initializeKeyboard()
{
    Wire.begin(8, 9, 400000);
    const uint8_t candidates[] = {kKeyboardAddress, kKeyboardAddressAlternate};
    bool found = false;
    for (uint8_t candidate : candidates) {
        Wire.beginTransmission(candidate);
        if (Wire.endTransmission() == 0) {
            keyboardAddress = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    constexpr uint8_t resetRegisters[][2] = {
        {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x20, 0xFF}, {0x21, 0xFF}, {0x22, 0xFF},
        {0x26, 0x00}, {0x27, 0x00}, {0x28, 0x00}, {0x1A, 0xFF}, {0x1B, 0xFF}, {0x1C, 0xFF},
        {0x1D, 0x7F}, {0x1E, 0xFF}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00}, {0x02, 0x03}};
    for (const auto &entry : resetRegisters) {
        if (!writeKeyboardRegister(entry[0], entry[1])) {
            return false;
        }
    }
    return true;
}

int readKeyboardRegister(uint8_t reg)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(keyboardAddress, static_cast<uint8_t>(1)) != 1) {
        return -1;
    }
    return Wire.read();
}

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

void scanNearbyBluetoothDevices()
{
    bluetoothDeviceCount = 0;
    discoveredBluetoothDeviceCount = 0;
    bluetoothScanFailed = false;

    if (!bluetoothReady) {
        BLEDevice::init("");
        bluetoothReady = BLEDevice::getInitialized();
    }
    BLEScan *scanner = bluetoothReady ? BLEDevice::getScan() : nullptr;
    if (scanner == nullptr) {
        bluetoothScanFailed = true;
        return;
    }

    scanner->setActiveScan(false);
    scanner->setInterval(100);
    scanner->setWindow(50);
    BLEScanResults results = scanner->start(3, false);
    discoveredBluetoothDeviceCount = static_cast<int16_t>(results.getCount());
    bluetoothDeviceCount =
        std::min(static_cast<size_t>(discoveredBluetoothDeviceCount), bluetoothDevices.size());
    for (size_t i = 0; i < bluetoothDeviceCount; ++i) {
        BLEAdvertisedDevice device = results.getDevice(static_cast<uint32_t>(i));
        const std::string name = device.getName();
        BluetoothDevice &entry = bluetoothDevices[i];
        strlcpy(entry.name, name.empty() ? "(unnamed)" : name.c_str(), sizeof(entry.name));
        entry.rssi = static_cast<int16_t>(device.getRSSI());
    }
    scanner->clearResults();
}

void drawHome()
{
    M5.Display.fillScreen(kBackgroundColor);
    drawThemeImage(selectedApp, 0, 0, screenWidth, screenHeight - 25);
    M5.Display.fillRect(0, screenHeight - 25, screenWidth, 25, kBackgroundColor);
    M5.Display.drawFastHLine(0, screenHeight - 25, screenWidth, kAccentColor);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    M5.Display.drawString(keyboardReady ? "W/S OR FN+ARROWS: MOVE" : "KEYBOARD NOT FOUND",
                          screenWidth / 2, screenHeight - 19);
    M5.Display.setTextColor(kAccentColor, kBackgroundColor);
    M5.Display.drawString("ENTER / BtnA: OPEN", screenWidth / 2, screenHeight - 7);
}

void drawFeature()
{
    if (selectedApp == 3) {
        drawThemeImage(selectedApp, 0, 0, screenWidth, 58);
        M5.Display.fillRect(0, 58, screenWidth, screenHeight - 58, kBackgroundColor);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        char line[48];
        if (bluetoothScanFailed) {
            M5.Display.drawString("BLE survey unavailable", 5, 63);
        } else {
            snprintf(line, sizeof(line), "NEARBY BLE: %d", discoveredBluetoothDeviceCount);
            M5.Display.drawString(line, 5, 60);
            for (size_t i = 0; i < bluetoothDeviceCount && i < 3; ++i) {
                const BluetoothDevice &device = bluetoothDevices[i];
                snprintf(line, sizeof(line), "%.22s %ddBm", device.name, device.rssi);
                M5.Display.drawString(line, 5, 73 + static_cast<int16_t>(i) * 13);
            }
            if (discoveredBluetoothDeviceCount == 0) {
                M5.Display.drawString("No advertisers found", 5, 73);
            }
        }
        M5.Display.drawFastHLine(0, screenHeight - 13, screenWidth, kAccentColor);
        M5.Display.setTextColor(kAccentColor, kBackgroundColor);
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.drawString("R/ENTER/BtnA: SCAN  ESC: BACK", screenWidth / 2, screenHeight - 6);
        return;
    }

    if (selectedApp == 4) {
        drawThemeImage(selectedApp, 0, 0, screenWidth, 58);
        M5.Display.fillRect(0, 58, screenWidth, screenHeight - 58, kBackgroundColor);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        char line[52];
        if (scanFailed) {
            M5.Display.drawString("Wi-Fi survey unavailable", 5, 63);
        } else {
            snprintf(line, sizeof(line), "NEARBY NETWORKS: %d", discoveredAccessPointCount);
            M5.Display.drawString(line, 5, 60);
            for (size_t i = 0; i < accessPointCount && i < 3; ++i) {
                const AccessPoint &entry = accessPoints[i];
                snprintf(line, sizeof(line), "%.17s %ddBm CH%u %s", entry.ssid, entry.rssi, entry.channel,
                         securityName(entry.security));
                M5.Display.drawString(line, 5, 73 + static_cast<int16_t>(i) * 13);
            }
            if (discoveredAccessPointCount == 0) {
                M5.Display.drawString("No networks found", 5, 73);
            }
        }
        M5.Display.drawFastHLine(0, screenHeight - 13, screenWidth, kAccentColor);
        M5.Display.setTextColor(kAccentColor, kBackgroundColor);
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.drawString("R/ENTER/BtnA: RESCAN  ESC: BACK", screenWidth / 2, screenHeight - 6);
        return;
    }

    drawThemeImage(selectedApp, 0, 0, screenWidth, screenHeight - 29);
    M5.Display.fillRect(0, screenHeight - 29, screenWidth, 29, kBackgroundColor);
    M5.Display.drawFastHLine(0, screenHeight - 29, screenWidth, kAccentColor);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    M5.Display.drawString(themeImages[selectedApp].name, screenWidth / 2, screenHeight - 22);
    M5.Display.setTextColor(kAccentColor, kBackgroundColor);
    if (selectedApp == 2) {
        const uint32_t seconds = millis() / 1000;
        char line[52];
        snprintf(line, sizeof(line), "UPTIME %02lu:%02lu:%02lu  |  Esc: back", seconds / 3600,
                 seconds / 60 % 60, seconds % 60);
        M5.Display.drawString(line, screenWidth / 2, screenHeight - 7);
    } else {
        M5.Display.drawString(featureDetails[selectedApp], screenWidth / 2, screenHeight - 7);
    }
}

void redrawScreen()
{
    if (currentScreen == Screen::Home) {
        drawHome();
    } else {
        drawFeature();
    }
}

void openSelectedApp()
{
    currentScreen = Screen::Feature;
    if (selectedApp == 3) {
        scanNearbyBluetoothDevices();
    } else if (selectedApp == 4) {
        scanNearbyAccessPoints();
    }
}

void handleKey(uint8_t key)
{
    if (currentScreen == Screen::Home) {
        if (key == kKeyUp || key == 'w') {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
        } else if (key == kKeyDown || key == 's') {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
        } else if (key == kKeyLeft) {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
        } else if (key == kKeyRight) {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            openSelectedApp();
        }
    } else if ((selectedApp == 3 || selectedApp == 4) &&
               (key == 'r' || key == 'R' || key == kKeySelect)) {
        if (selectedApp == 3) {
            scanNearbyBluetoothDevices();
        } else {
            scanNearbyAccessPoints();
        }
    }

    if (key == kKeyEscape || key == kKeyBackspace) {
        currentScreen = Screen::Home;
    }
    redrawScreen();
}

void pollKeyboard()
{
    if (!keyboardReady) {
        return;
    }
    const int count = readKeyboardRegister(kKeyEventCountRegister);
    if (count < 0) {
        keyboardReady = false;
        redrawScreen();
        return;
    }

    for (int i = 0; i < std::min(count & 0x0F, static_cast<int>(kMaxKeysPerRead)); ++i) {
        const int event = readKeyboardRegister(kKeyEventRegister + i);
        if (event < 0) {
            keyboardReady = false;
            redrawScreen();
            return;
        }
        const uint8_t keyNumber = static_cast<uint8_t>(event & 0x7F);
        if (keyNumber == 0) {
            continue;
        }
        const uint8_t row = static_cast<uint8_t>((keyNumber - 1) / 10);
        const uint8_t column = static_cast<uint8_t>((keyNumber - 1) % 10);
        if (row >= kKeyboardRows || column >= kKeyboardColumns) {
            continue;
        }

        const uint8_t keyIndex = static_cast<uint8_t>(row * kKeyboardColumns + column);
        if ((event & 0x80) != 0) {
            if (keyIndex == kKeyFunction) {
                keyboardModifiers ^= 0x02;
            } else if (keyIndex == kKeyShift) {
                keyboardModifiers ^= 0x01;
            }
            continue;
        }
        if (keyIndex == kKeyFunction || keyIndex == kKeyShift) {
            continue;
        }
        const uint8_t key = kKeyMap[keyIndex][keyboardModifiers & 0x03];
        if (key != 0) {
            handleKey(key);
        }
        keyboardModifiers = 0;
    }
}

void showBootSplash()
{
    constexpr uint8_t frames = 24;
    for (uint8_t frame = 0; frame < frames; ++frame) {
        const float progress = static_cast<float>(frame) / (frames - 1);
        const float pulse = 1.0F + 0.12F * std::sin(progress * 2.0F * 3.14159F);
        const int16_t size = static_cast<int16_t>(48.0F * pulse);
        const int16_t centerX = screenWidth / 2;
        const int16_t centerY = static_cast<int16_t>(36 + (1.0F - progress) * 10.0F);
        const int16_t bodyHeight = size * 3 / 4;
        const int16_t left = centerX - size / 2;
        const int16_t top = centerY - size / 2;

        M5.Display.fillScreen(kBackgroundColor);
        M5.Display.drawFastHLine(0, 8, static_cast<int32_t>(screenWidth * progress), kAccentColor);
        M5.Display.drawRect(8, 8, screenWidth - 16, screenHeight - 16, kThemeColor);
        M5.Display.fillCircle(centerX, top + size / 3, size / 3, kThemeColor);
        M5.Display.fillRect(left + size / 6, top + size / 3, size * 2 / 3, bodyHeight / 2, kThemeColor);
        M5.Display.fillTriangle(left + size / 6, top + bodyHeight * 5 / 6, centerX, top + bodyHeight * 5 / 6,
                                left + size / 3, top + bodyHeight, kThemeColor);
        M5.Display.fillTriangle(centerX, top + bodyHeight * 5 / 6, left + size * 5 / 6, top + bodyHeight * 5 / 6,
                                left + size * 2 / 3, top + bodyHeight, kThemeColor);
        M5.Display.fillCircle(centerX - size / 6, top + size / 3, std::max<int16_t>(2, size / 12), kAccentColor);
        M5.Display.fillCircle(centerX + size / 6, top + size / 3, std::max<int16_t>(2, size / 12), kAccentColor);

        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(kAccentColor, kBackgroundColor);
        M5.Display.drawString("GHOST", centerX, 85);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        M5.Display.drawString("PinoyUnknown", centerX, 104);
        M5.Display.drawFastHLine(0, screenHeight - 9,
                                 static_cast<int32_t>(screenWidth * (1.0F - progress)), kAccentColor);
        delay(24);
    }
}

} // namespace

void setup()
{
    auto config = M5.config();
    M5.begin(config);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    keyboardReady = initializeKeyboard();
    showBootSplash();
    redrawScreen();
}

void loop()
{
    M5.update();
    pollKeyboard();
    if (M5.BtnA.wasPressed()) {
        if (currentScreen == Screen::Home) {
            openSelectedApp();
        } else if (selectedApp == 3) {
            scanNearbyBluetoothDevices();
        } else if (selectedApp == 4) {
            scanNearbyAccessPoints();
        } else {
            currentScreen = Screen::Home;
        }
        redrawScreen();
    }
    delay(10);
}
