#include <M5Unified.h>
#include <BLEDevice.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>

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
constexpr size_t kMaxSurveyResults = 64;
constexpr size_t kVisibleResults = 5;
constexpr size_t kMaxFileEntries = 64;

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

enum class Screen : uint8_t { Home, Feature, SurveyDetail, FilePreview };

struct AccessPoint
{
    char ssid[33];
    int16_t rssi;
    uint8_t channel;
    wifi_auth_mode_t security;
    char bssid[18];
};

struct BluetoothDevice
{
    char name[25];
    char address[18];
    char firstServiceUuid[37];
    uint16_t serviceCount;
    uint16_t manufacturerDataBytes;
    int16_t rssi;
};

struct FileEntry
{
    char name[48];
    char path[96];
    uint32_t size;
    bool isDirectory;
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

std::array<AccessPoint, kMaxSurveyResults> accessPoints{};
std::array<BluetoothDevice, kMaxSurveyResults> bluetoothDevices{};
std::array<FileEntry, kMaxFileEntries> fileEntries{};
size_t accessPointCount = 0;
size_t bluetoothDeviceCount = 0;
size_t fileEntryCount = 0;
int16_t discoveredAccessPointCount = 0;
int16_t discoveredBluetoothDeviceCount = 0;
int16_t screenWidth = 240;
int16_t screenHeight = 135;
uint8_t selectedApp = 0;
uint8_t keyboardModifiers = 0;
size_t selectedResult = 0;
size_t selectedFile = 0;
String currentDirectory = "/";
String manualTimeDigits;
String previewPath;
size_t previewLineOffset = 0;
Screen currentScreen = Screen::Home;
bool keyboardReady = false;
uint8_t keyboardAddress = kKeyboardAddress;
bool scanFailed = false;
bool bluetoothScanFailed = false;
bool bluetoothReady = false;
bool filesystemReady = false;
bool fileListingFailed = false;
bool manualTimeEntry = false;
bool clockSyncFailed = false;
bool clockSyncInProgress = false;
bool clockSynced = false;
bool clockSetFailed = false;
time_t clockDisplaySecond = 0;

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
            const String bssid = WiFi.BSSIDstr(static_cast<int>(i));
            strlcpy(entry.bssid, bssid.c_str(), sizeof(entry.bssid));
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
        const std::string address = device.getAddress().toString();
        strlcpy(entry.address, address.c_str(), sizeof(entry.address));
        entry.serviceCount = static_cast<uint16_t>(device.getServiceUUIDCount());
        entry.firstServiceUuid[0] = '\0';
        if (entry.serviceCount > 0) {
            const std::string service = device.getServiceUUID(0).toString();
            strlcpy(entry.firstServiceUuid, service.c_str(), sizeof(entry.firstServiceUuid));
        }
        entry.manufacturerDataBytes = static_cast<uint16_t>(device.getManufacturerData().size());
        entry.rssi = static_cast<int16_t>(device.getRSSI());
    }
    scanner->clearResults();
}

void loadFileList()
{
    fileEntryCount = 0;
    selectedFile = 0;
    fileListingFailed = false;
    if (!filesystemReady) {
        filesystemReady = SPIFFS.begin(false);
    }
    if (!filesystemReady) {
        fileListingFailed = true;
        return;
    }

    if (currentDirectory != "/" && fileEntryCount < fileEntries.size()) {
        FileEntry &parent = fileEntries[fileEntryCount++];
        strlcpy(parent.name, "..", sizeof(parent.name));
        const int lastSlash = currentDirectory.lastIndexOf('/');
        const String parentPath = currentDirectory.substring(0, lastSlash);
        strlcpy(parent.path, parentPath.c_str(), sizeof(parent.path));
        if (parent.path[0] == '\0') {
            strlcpy(parent.path, "/", sizeof(parent.path));
        }
        parent.size = 0;
        parent.isDirectory = true;
    }

    File directory = SPIFFS.open(currentDirectory.c_str(), FILE_READ);
    if (!directory || !directory.isDirectory()) {
        fileListingFailed = true;
        return;
    }

    File entry = directory.openNextFile();
    while (entry && fileEntryCount < fileEntries.size()) {
        FileEntry &item = fileEntries[fileEntryCount++];
        const char *path = entry.name();
        const char *name = strrchr(path, '/');
        name = name == nullptr ? path : name + 1;
        strlcpy(item.name, name, sizeof(item.name));
        strlcpy(item.path, path, sizeof(item.path));
        item.size = static_cast<uint32_t>(entry.size());
        item.isDirectory = entry.isDirectory();
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
}

void beginManualTimeEntry()
{
    manualTimeDigits = "";
    manualTimeEntry = true;
    clockSetFailed = false;
}

void saveManualTime()
{
    if (manualTimeDigits.length() != 14) {
        clockSetFailed = true;
        return;
    }

    struct tm value = {};
    value.tm_year = manualTimeDigits.substring(0, 4).toInt() - 1900;
    value.tm_mon = manualTimeDigits.substring(4, 6).toInt() - 1;
    value.tm_mday = manualTimeDigits.substring(6, 8).toInt();
    value.tm_hour = manualTimeDigits.substring(8, 10).toInt();
    value.tm_min = manualTimeDigits.substring(10, 12).toInt();
    value.tm_sec = manualTimeDigits.substring(12, 14).toInt();
    if (value.tm_year < 124 || value.tm_mon < 0 || value.tm_mon > 11 || value.tm_mday < 1 ||
        value.tm_mday > 31 || value.tm_hour > 23 || value.tm_min > 59 || value.tm_sec > 59) {
        clockSetFailed = true;
        return;
    }

    setenv("TZ", "UTC0", 1);
    tzset();
    const int expectedYear = value.tm_year;
    const int expectedMonth = value.tm_mon;
    const int expectedDay = value.tm_mday;
    const time_t epoch = mktime(&value);
    struct tm verified = {};
    gmtime_r(&epoch, &verified);
    if (verified.tm_year != expectedYear || verified.tm_mon != expectedMonth ||
        verified.tm_mday != expectedDay) {
        clockSetFailed = true;
        return;
    }

    struct timeval now = {};
    now.tv_sec = epoch;
    now.tv_usec = 0;
    if (settimeofday(&now, nullptr) != 0) {
        clockSetFailed = true;
        return;
    }
    manualTimeEntry = false;
    manualTimeDigits = "";
    clockSynced = false;
    clockSetFailed = false;
}

void syncClockFromNetwork()
{
    clockSyncInProgress = true;
    clockSyncFailed = false;
    const wifi_mode_t previousMode = WiFi.getMode();
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_MODE_STA);
        WiFi.begin();
    }

    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm current = {};
    bool synced = false;
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && getLocalTime(&current, 100) &&
            current.tm_year >= 124) {
            synced = true;
            break;
        }
        delay(250);
    }
    clockSynced = synced;
    clockSyncFailed = !synced;
    clockSyncInProgress = false;
    if (previousMode == WIFI_MODE_NULL && WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_MODE_NULL);
    }
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

void drawFooter(const char *text)
{
    M5.Display.drawFastHLine(0, screenHeight - 13, screenWidth, kAccentColor);
    M5.Display.setTextColor(kAccentColor, kBackgroundColor);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(text, screenWidth / 2, screenHeight - 6);
}

void drawBluetoothSurvey()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    char line[48];
    if (bluetoothScanFailed) {
        M5.Display.drawString("BLE survey unavailable", 5, 5);
        return;
    }
    snprintf(line, sizeof(line), "BLE ADVERTISERS: %d  SHOWING %u", discoveredBluetoothDeviceCount,
             static_cast<unsigned>(bluetoothDeviceCount));
    M5.Display.drawString(line, 5, 4);
    if (bluetoothDeviceCount == 0) {
        M5.Display.drawString("No advertisers found", 5, 27);
        return;
    }
    const size_t first = selectedResult >= kVisibleResults ? selectedResult - kVisibleResults + 1 : 0;
    for (size_t row = 0; row < kVisibleResults && first + row < bluetoothDeviceCount; ++row) {
        const size_t index = first + row;
        const BluetoothDevice &device = bluetoothDevices[index];
        const int16_t y = 21 + static_cast<int16_t>(row) * 19;
        if (index == selectedResult) {
            M5.Display.fillRect(2, y - 1, screenWidth - 4, 18, kAccentColor);
            M5.Display.setTextColor(kBackgroundColor, kAccentColor);
        } else {
            M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        }
        snprintf(line, sizeof(line), "%.17s %ddBm", device.name, device.rssi);
        M5.Display.drawString(line, 5, y);
    }
}

void drawWifiSurvey()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    char line[52];
    if (scanFailed) {
        M5.Display.drawString("Wi-Fi survey unavailable", 5, 5);
        return;
    }
    snprintf(line, sizeof(line), "NETWORKS: %d  SHOWING %u", discoveredAccessPointCount,
             static_cast<unsigned>(accessPointCount));
    M5.Display.drawString(line, 5, 4);
    if (accessPointCount == 0) {
        M5.Display.drawString("No networks found", 5, 27);
        return;
    }
    const size_t first = selectedResult >= kVisibleResults ? selectedResult - kVisibleResults + 1 : 0;
    for (size_t row = 0; row < kVisibleResults && first + row < accessPointCount; ++row) {
        const size_t index = first + row;
        const AccessPoint &network = accessPoints[index];
        const int16_t y = 21 + static_cast<int16_t>(row) * 19;
        if (index == selectedResult) {
            M5.Display.fillRect(2, y - 1, screenWidth - 4, 18, kAccentColor);
            M5.Display.setTextColor(kBackgroundColor, kAccentColor);
        } else {
            M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        }
        snprintf(line, sizeof(line), "%.14s %4ddBm C%02u %s", network.ssid, network.rssi, network.channel,
                 securityName(network.security));
        M5.Display.drawString(line, 5, y);
    }
}

void drawSurveyDetail()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    char line[52];
    if (selectedApp == 4 && selectedResult < accessPointCount) {
        const AccessPoint &network = accessPoints[selectedResult];
        M5.Display.drawString("WI-FI ACCESS POINT", 5, 5);
        snprintf(line, sizeof(line), "SSID: %.30s", network.ssid);
        M5.Display.drawString(line, 5, 26);
        snprintf(line, sizeof(line), "BSSID: %s", network.bssid);
        M5.Display.drawString(line, 5, 44);
        snprintf(line, sizeof(line), "RSSI: %d dBm  CHANNEL: %u", network.rssi, network.channel);
        M5.Display.drawString(line, 5, 62);
        snprintf(line, sizeof(line), "SECURITY: %s", securityName(network.security));
        M5.Display.drawString(line, 5, 80);
        M5.Display.drawString("Passive beacon scan; no connection", 5, 99);
    } else if (selectedApp == 3 && selectedResult < bluetoothDeviceCount) {
        const BluetoothDevice &device = bluetoothDevices[selectedResult];
        M5.Display.drawString("BLE ADVERTISEMENT", 5, 5);
        snprintf(line, sizeof(line), "NAME: %.30s", device.name);
        M5.Display.drawString(line, 5, 26);
        snprintf(line, sizeof(line), "ADDRESS: %s", device.address);
        M5.Display.drawString(line, 5, 44);
        snprintf(line, sizeof(line), "RSSI: %d dBm", device.rssi);
        M5.Display.drawString(line, 5, 62);
        snprintf(line, sizeof(line), "SERVICES: %u  MFG DATA: %uB", device.serviceCount,
                 device.manufacturerDataBytes);
        M5.Display.drawString(line, 5, 80);
        snprintf(line, sizeof(line), "UUID: %.32s", device.firstServiceUuid[0] ? device.firstServiceUuid : "none");
        M5.Display.drawString(line, 5, 98);
    }
    drawFooter("ESC: BACK");
}

void drawClock()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    M5.Display.drawString("UTC CLOCK", 5, 5);
    if (manualTimeEntry) {
        M5.Display.drawString("SET YYYYMMDDhhmmss:", 5, 30);
        M5.Display.drawString(manualTimeDigits.c_str(), 5, 53);
        M5.Display.drawString("14 digits, ENTER saves", 5, 76);
        M5.Display.drawString("BACKSPACE edits", 5, 94);
        if (clockSetFailed) {
            M5.Display.drawString("Invalid date/time", 5, 111);
        }
    } else {
        const time_t now = time(nullptr);
        struct tm value = {};
        char line[48];
        if (now >= 1700000000 && gmtime_r(&now, &value) != nullptr) {
            strftime(line, sizeof(line), "%Y-%m-%d", &value);
            M5.Display.drawString(line, 5, 34);
            strftime(line, sizeof(line), "%H:%M:%S UTC", &value);
            M5.Display.drawString(line, 5, 55);
            snprintf(line, sizeof(line), "SOURCE: %s", clockSynced ? "NTP" : "MANUAL/RTC");
            M5.Display.drawString(line, 5, 78);
        } else {
            M5.Display.drawString("Time not set", 5, 34);
        }
        if (clockSyncInProgress) {
            M5.Display.drawString("Syncing network time...", 5, 98);
        } else if (clockSyncFailed) {
            M5.Display.drawString("NTP failed; check saved Wi-Fi", 5, 98);
        } else {
            M5.Display.drawString("N: NTP SYNC  M: SET TIME", 5, 98);
        }
    }
    drawFooter("ESC: BACK");
}

void drawFileBrowser()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    char heading[52];
    snprintf(heading, sizeof(heading), "FLASH:%s", currentDirectory.c_str());
    M5.Display.drawString(heading, 5, 4);
    if (fileListingFailed) {
        M5.Display.drawString("Flash filesystem unavailable", 5, 28);
        return;
    }
    if (fileEntryCount == 0) {
        M5.Display.drawString("Folder is empty", 5, 28);
        return;
    }
    const size_t first = selectedFile >= kVisibleResults ? selectedFile - kVisibleResults + 1 : 0;
    for (size_t row = 0; row < kVisibleResults && first + row < fileEntryCount; ++row) {
        const size_t index = first + row;
        const FileEntry &entry = fileEntries[index];
        const int16_t y = 21 + static_cast<int16_t>(row) * 19;
        if (index == selectedFile) {
            M5.Display.fillRect(2, y - 1, screenWidth - 4, 18, kAccentColor);
            M5.Display.setTextColor(kBackgroundColor, kAccentColor);
        } else {
            M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        }
        if (entry.isDirectory) {
            snprintf(heading, sizeof(heading), "[DIR] %.38s", entry.name);
        } else {
            snprintf(heading, sizeof(heading), "%.31s %luB", entry.name, static_cast<unsigned long>(entry.size));
        }
        M5.Display.drawString(heading, 5, y);
    }
}

void drawFilePreview()
{
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(kThemeColor, kBackgroundColor);
    M5.Display.drawString(previewPath.substring(previewPath.lastIndexOf('/') + 1).c_str(), 5, 4);
    File file = SPIFFS.open(previewPath.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
        M5.Display.drawString("Unable to read file", 5, 27);
        return;
    }
    for (size_t skipped = 0; skipped < previewLineOffset && file.available(); ++skipped) {
        file.readStringUntil('\n');
    }
    for (uint8_t row = 0; row < kVisibleResults && file.available(); ++row) {
        String text = file.readStringUntil('\n');
        text.trim();
        M5.Display.drawString(text.substring(0, 38).c_str(), 5, 22 + row * 19);
    }
    file.close();
}

void drawFeature()
{
    if (selectedApp == 3) {
        drawBluetoothSurvey();
        drawFooter("W/S: SELECT  ENTER: DETAILS  R: RESCAN");
    } else if (selectedApp == 4) {
        drawWifiSurvey();
        drawFooter("W/S: SELECT  ENTER: DETAILS  R: RESCAN");
    } else if (selectedApp == 2) {
        drawClock();
    } else if (selectedApp == 13) {
        drawFileBrowser();
        drawFooter("W/S: SELECT  ENTER: OPEN  ESC: BACK");
    } else {
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextColor(kThemeColor, kBackgroundColor);
        M5.Display.drawString(themeImages[selectedApp].name, 5, 5);
        M5.Display.drawString(featureDetails[selectedApp], 5, 30);
        if (selectedApp == 5 || selectedApp == 6 || selectedApp == 7 || selectedApp == 10 ||
            selectedApp == 11 || selectedApp == 12) {
            M5.Display.drawString("Required external hardware is not", 5, 52);
            M5.Display.drawString("configured in this Cardputer build.", 5, 68);
        } else if (selectedApp == 8) {
            M5.Display.drawString("Available: passive Wi-Fi / BLE surveys,", 5, 52);
            M5.Display.drawString("UTC clock, and flash file browser.", 5, 68);
        }
        drawFooter("ESC / BACKSPACE: BACK");
    }
}

void redrawScreen()
{
    M5.Display.fillScreen(kBackgroundColor);
    if (currentScreen == Screen::Home) {
        drawHome();
    } else if (currentScreen == Screen::SurveyDetail) {
        drawSurveyDetail();
    } else if (currentScreen == Screen::FilePreview) {
        drawFilePreview();
        drawFooter("W/S: SCROLL  ESC: FILES");
    } else {
        drawFeature();
    }
}

void openSelectedFile()
{
    if (selectedFile >= fileEntryCount) {
        return;
    }
    const FileEntry &entry = fileEntries[selectedFile];
    if (entry.isDirectory) {
        currentDirectory = entry.path;
        loadFileList();
    } else {
        previewPath = entry.path;
        previewLineOffset = 0;
        currentScreen = Screen::FilePreview;
    }
}

void openSelectedApp()
{
    currentScreen = Screen::Feature;
    selectedResult = 0;
    if (selectedApp == 2) {
        clockSyncFailed = false;
        manualTimeEntry = false;
        clockDisplaySecond = 0;
    } else if (selectedApp == 3) {
        scanNearbyBluetoothDevices();
    } else if (selectedApp == 4) {
        scanNearbyAccessPoints();
    } else if (selectedApp == 13) {
        currentDirectory = "/";
        loadFileList();
    }
}

void handleFeatureKey(uint8_t key)
{
    if (manualTimeEntry) {
        if (key == kKeyEscape) {
            manualTimeEntry = false;
            manualTimeDigits = "";
        } else if (key >= '0' && key <= '9' && manualTimeDigits.length() < 14) {
            manualTimeDigits += static_cast<char>(key);
        } else if (key == kKeyBackspace && !manualTimeDigits.isEmpty()) {
            manualTimeDigits.remove(manualTimeDigits.length() - 1);
        } else if (key == kKeySelect || key == '\n') {
            saveManualTime();
        }
        return;
    }

    if (currentScreen == Screen::SurveyDetail) {
        if (key == kKeyEscape || key == kKeyBackspace) {
            currentScreen = Screen::Feature;
        }
        return;
    }

    if (currentScreen == Screen::FilePreview) {
        if (key == kKeyDown || key == 's') {
            File file = SPIFFS.open(previewPath.c_str(), FILE_READ);
            size_t lineCount = 0;
            while (file && file.available()) {
                file.readStringUntil('\n');
                ++lineCount;
            }
            if (lineCount > previewLineOffset + kVisibleResults) {
                ++previewLineOffset;
            }
        } else if ((key == kKeyUp || key == 'w') && previewLineOffset > 0) {
            --previewLineOffset;
        } else if (key == kKeyEscape || key == kKeyBackspace) {
            currentScreen = Screen::Feature;
        }
        return;
    }

    if (selectedApp == 3 || selectedApp == 4) {
        const size_t resultCount = selectedApp == 3 ? bluetoothDeviceCount : accessPointCount;
        if ((key == kKeyUp || key == 'w') && selectedResult > 0) {
            --selectedResult;
        } else if ((key == kKeyDown || key == 's') && selectedResult + 1 < resultCount) {
            ++selectedResult;
        } else if (key == 'r' || key == 'R') {
            selectedResult = 0;
            selectedApp == 3 ? scanNearbyBluetoothDevices() : scanNearbyAccessPoints();
        } else if ((key == kKeySelect || key == '\n') && resultCount > 0) {
            currentScreen = Screen::SurveyDetail;
        }
    } else if (selectedApp == 2) {
        if (key == 'n' || key == 'N') {
            syncClockFromNetwork();
        } else if (key == 'm' || key == 'M') {
            beginManualTimeEntry();
        }
    } else if (selectedApp == 13) {
        if ((key == kKeyUp || key == 'w') && selectedFile > 0) {
            --selectedFile;
        } else if ((key == kKeyDown || key == 's') && selectedFile + 1 < fileEntryCount) {
            ++selectedFile;
        } else if (key == kKeySelect || key == '\n') {
            openSelectedFile();
        }
    }

    if (key == kKeyEscape || key == kKeyBackspace) {
        if (selectedApp == 13 && currentDirectory != "/") {
            const int lastSlash = currentDirectory.lastIndexOf('/');
            currentDirectory = currentDirectory.substring(0, lastSlash);
            if (currentDirectory.isEmpty()) {
                currentDirectory = "/";
            }
            loadFileList();
        } else {
            currentScreen = Screen::Home;
        }
    }
}

void handleKey(uint8_t key)
{
    if (currentScreen == Screen::Home) {
        if (key == kKeyUp || key == 'w' || key == kKeyLeft) {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
        } else if (key == kKeyDown || key == 's' || key == kKeyRight) {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            openSelectedApp();
        }
    } else {
        handleFeatureKey(key);
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
        } else {
            handleFeatureKey(kKeySelect);
        }
        redrawScreen();
    }
    if (currentScreen == Screen::Feature && selectedApp == 2 && !manualTimeEntry) {
        const time_t currentSecond = time(nullptr);
        if (currentSecond != clockDisplaySecond) {
            clockDisplaySecond = currentSecond;
            redrawScreen();
        }
    }
    delay(10);
}
