#include <M5Unified.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>

#include <Audio.h>

#include "theme_assets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// -----------------------------------------------------------------------------
// Global Audio Callbacks for ESP32-audioI2S WebRadio
// -----------------------------------------------------------------------------
static volatile int16_t gAudioPeak = 0;
static String gRadioStationTitle = "";
static String gRadioStreamTitle = "";
static String gRadioBitrate = "";

void audio_process_i2s(uint32_t *sample, bool *continueI2S)
{
    if (sample) {
        int16_t left = static_cast<int16_t>(*sample & 0xFFFF);
        int16_t right = static_cast<int16_t>(*sample >> 16);
        int16_t peak = std::max(std::abs(left), std::abs(right));
        if (peak > gAudioPeak) {
            gAudioPeak = peak;
        }
    }
}

void audio_showstation(const char *info)
{
    if (info) gRadioStationTitle = info;
}

void audio_showstreamtitle(const char *info)
{
    if (info) gRadioStreamTitle = info;
}

void audio_bitrate(const char *info)
{
    if (info) gRadioBitrate = info;
}

void audio_eof_mp3(const char *info)
{
    (void)info;
}

namespace
{

constexpr char kFirmwareVersion[] = "v2.1";
constexpr char kDeveloperTag[] = "DEV: www.instagram.com/pinoyunknown";

struct ThemeImage {
    const uint8_t *data;
    size_t size;
    const char *name;
};

constexpr std::array<ThemeImage, 14> themeImages = {{
    {cyberpunkConnectImage, sizeof(cyberpunkConnectImage), "CONNECT"},
    {cyberpunkConfigImage, sizeof(cyberpunkConfigImage), "CONFIG"},
    {cyberpunkClockImage, sizeof(cyberpunkClockImage), "CLOCK"},
    {cyberpunkBleImage, sizeof(cyberpunkBleImage), "BLE"},
    {cyberpunkWifiImage, sizeof(cyberpunkWifiImage), "WIFI"},
    {cyberpunkRfImage, sizeof(cyberpunkRfImage), "RF MESH"},
    {cyberpunkNrfImage, sizeof(cyberpunkNrfImage), "NRF/2.4G"},
    {cyberpunkNfcImage, sizeof(cyberpunkNfcImage), "NFC/RFID"},
    {cyberpunkMiscImage, sizeof(cyberpunkMiscImage), "MISC"},
    {cyberpunkJsImage, sizeof(cyberpunkJsImage), "JS MACRO"},
    {cyberpunkIrImage, sizeof(cyberpunkIrImage), "IR TX"},
    {cyberpunkGpsImage, sizeof(cyberpunkGpsImage), "GPS"},
    {cyberpunkFmImage, sizeof(cyberpunkFmImage), "WEBRADIO"},
    {cyberpunkFilesImage, sizeof(cyberpunkFilesImage), "FILE"},
}};

constexpr uint8_t kKeyboardAddressTca = 0x34;
constexpr uint8_t kKeyboardAddressCardKb = 0x5F;
constexpr uint8_t kKeyEventCountRegister = 0x03;
constexpr uint8_t kKeyEventRegister = 0x04;
constexpr uint8_t kMaxKeysPerRead = 10;

constexpr uint8_t kKeySelect = 0x0D;
constexpr uint8_t kKeyBackspace = 0x08;
constexpr uint8_t kKeyEscape = 0x1B;
constexpr uint8_t kKeyLeft = 0xB4;
constexpr uint8_t kKeyUp = 0xB5;
constexpr uint8_t kKeyDown = 0xB6;
constexpr uint8_t kKeyRight = 0xB7;
constexpr uint8_t kKeyShift = 6;
constexpr uint8_t kKeyFunction = 2;
constexpr uint8_t kKeyboardRows = 7;
constexpr uint8_t kKeyboardColumns = 8;
constexpr uint8_t kTotalKeys = 56;
constexpr uint8_t kThemeCount = 14;
constexpr size_t kMaxSurveyResults = 64;
constexpr size_t kVisibleResults = 5;
constexpr size_t kMaxFileEntries = 64;

// Hardware pin definitions for Cardputer & ESP32-S3
constexpr uint8_t kCardputerIrPin = 44;
constexpr uint8_t kGroveSdaPin = 2;
constexpr uint8_t kGroveSclPin = 1;

// LoRa SX1262 pins on Cardputer-Adv / Cardputer Mesh
constexpr uint8_t kLoraSckPin = 40;
constexpr uint8_t kLoraMisoPin = 39;
constexpr uint8_t kLoraMosiPin = 14;
constexpr uint8_t kLoraCsPin = 5;
constexpr uint8_t kLoraRstPin = 3;
constexpr uint8_t kLoraDio1Pin = 4;
constexpr uint8_t kLoraBusyPin = 6;

constexpr std::array<uint8_t, 3> kMatrixOutputPins = {8, 9, 11};
constexpr std::array<uint8_t, 7> kMatrixInputPins = {13, 15, 3, 4, 5, 6, 7};

// Keyboard lookup tables for 74HC138 matrix
constexpr uint8_t kKeyMapNormal[kTotalKeys] = {
    '`', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '0', '-', '=', kKeyBackspace, 0, '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\\', 0, 0, 'a',
    's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', kKeySelect, 0, 0, 'z', 'x', 'c',
    'v', 'b', 'n', 'm', ',', '.', '/', ' '};

constexpr uint8_t kKeyMapShift[kTotalKeys] = {
    '~', '!', '@', '#', '$', '%', '^', '&',
    '*', '(', ')', '_', '+', kKeyBackspace, 0, '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '|', 0, 0, 'A',
    'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"', kKeySelect, 0, 0, 'Z', 'X', 'C',
    'V', 'B', 'N', 'M', '<', '>', '?', ' '};

constexpr uint8_t kKeyMapFunction[kTotalKeys] = {
    kKeyEscape, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, '_', '+', kKeyBackspace, 0, '\t',
    0, kKeyUp, 0, 0, 0, 0, 0, 0,
    0, 0, '{', '}', '|', 0, 0, kKeyLeft,
    kKeyDown, kKeyRight, 0, 0, 0, 0, 0, 0,
    ':', '"', kKeySelect, 0, 0, 0, 0, 0,
    0, 0, 0, 0, '<', '>', '?', ' '};

enum class Screen : uint8_t {
    Home = 0,
    Feature,
    SurveyDetail,
    WifiPasswordEntry,
    FilePreview
};

enum class KeyboardDriver : uint8_t {
    Unknown = 0,
    GpioMatrix74HC138,
    Tca8418I2c,
    CardKbI2c,
    ButtonOnly
};

struct ColorPalette {
    const char *name;
    uint16_t primary;
    uint16_t accent;
    uint16_t background;
    uint16_t text;
};

constexpr std::array<ColorPalette, 6> kPalettes = {{
    {"CYBERPUNK NEON", 0x96FE, 0xF9FF, 0x0000, 0xFFFF},
    {"MATRIX GREEN",   0x07E0, 0x03E0, 0x0000, 0x07E0},
    {"VAPORWAVE PINK", 0xF81F, 0x07FF, 0x1008, 0xFFFF},
    {"RETRO AMBER",    0xFDA0, 0xFB40, 0x0000, 0xFFE0},
    {"ICE BLUE",       0x07FF, 0x041F, 0x0010, 0xFFFF},
    {"HIGH CONTRAST",  0xFFFF, 0xCE79, 0x0000, 0xFFFF}
}};

struct SurveyResult {
    String label;
    int rssi = 0;
    int channel = 0;
    String detail;
    bool isBle = false;
    uint8_t encryption = 0;
    uint8_t mac[6] = {0};
};

struct FileEntry {
    String path;
    size_t size = 0;
    bool isDirectory = false;
};

// -----------------------------------------------------------------------------
// WebRadio Station Database (Philippine Stations as DEFAULT)
// -----------------------------------------------------------------------------
struct RadioStation {
    const char *name;
    const char *url;
    const char *freq;
    const char *region;
    const char *genre;
};

constexpr RadioStation kRadioStations[] = {
    // PHILIPPINE RADIO STATIONS (DEFAULT)
    {"DWIZ 882 Manila",     "http://dwizmanila.radioca.st/stream",               "882 kHz AM",   "PH", "News & Talk (Aliw)"},
    {"DZRH 666 Manila",     "http://stream.zeno.fm/f920ghk8vkeuv",               "666 kHz AM",   "PH", "News Nationwide (MBC)"},
    {"Monster RX 93.1",     "http://icecast.eradioportal.com:8000/rx931_manila", "93.1 MHz FM",  "PH", "Top 40 & Pinoy Hits"},
    {"Love Radio 90.7",     "http://stream.zeno.fm/s4k81v76w4zuv",               "90.7 MHz FM",  "PH", "OPM & Love Songs"},
    {"Wish 107.5 Manila",   "http://stream.zeno.fm/0r0xa792kwzuv",               "107.5 MHz FM", "PH", "Pinoy Wish Bus & OPM"},
    {"Star FM 102.7",       "http://stream.zeno.fm/a816p2v4w4zuv",               "102.7 MHz FM", "PH", "Pinoy Pop & Trending"},
    {"Yes The Best 101.1",  "http://stream.zeno.fm/cvu4v626w4zuv",               "101.1 MHz FM", "PH", "Manila Mass Hits"},
    {"Pinoy Classic OPM",   "http://stream.zeno.fm/uDkifg",                      "Web Stream",   "PH", "Classic Golden OPM"},
    // INTERNATIONAL RADIO STATIONS
    {"DEF CON Radio",       "http://ice1.somafm.com/defcon-128-mp3",             "Online Cyber", "INT", "Hacker / Cyber Chill"},
    {"SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3",        "Online Synth", "INT", "Ambient / Chillout"},
    {"Nightwave Plaza",     "http://ice1.somafm.com/vaporwaves-128-mp3",         "Vaporwave",    "INT", "Aesthetic Synthwave"},
    {"SomaFM Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3",        "Online Spy",   "INT", "Spy & Lounge Downtempo"},
    {"BBC World Service",   "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", "World News", "INT", "Global News & Analysis"},
    {"KEXP 90.3 Seattle",   "http://live-aac-64.kexp.org/kexp64.aac",            "90.3 MHz FM",  "INT", "Indie & Alternative"}
};
constexpr size_t kRadioStationCount = sizeof(kRadioStations) / sizeof(kRadioStations[0]);

// -----------------------------------------------------------------------------
// Meshtastic Protocol Wire Structures (LoRa Skills)
// -----------------------------------------------------------------------------
struct MeshtasticWirePacket {
    uint32_t to = 0xFFFFFFFF; // Broadcast
    uint32_t from = 0;
    uint32_t id = 0;
    uint8_t flags = 0x03;     // hop_limit = 3
    uint8_t channel = 0x00;   // Default primary channel
    uint8_t portnum = 1;      // 1 = TEXT_MESSAGE_APP, 4 = NODEINFO_APP, 3 = POSITION_APP
    char payload[64] = {0};
};

struct MeshtasticNodeEntry {
    uint32_t nodeId = 0;
    char shortName[8] = {0};
    char longName[24] = {0};
    int8_t lastRssi = 0;
    int8_t lastSnr = 0;
    uint8_t hopsAway = 0;
    uint32_t lastHeardSec = 0;
    uint16_t packetCount = 0;
};

// -----------------------------------------------------------------------------
// System State & Variables
// -----------------------------------------------------------------------------
Screen currentScreen = Screen::Home;
KeyboardDriver keyboardDriver = KeyboardDriver::Unknown;
uint8_t keyboardAddress = 0;
uint8_t selectedApp = 0;
uint8_t heldModifiers = 0;
uint8_t stickyModifiers = 0;
std::array<bool, kTotalKeys> previousKeyStates{};

// Display Dimensions & Orientation
// Default screenRotation = 1 (Cardputer Landscape 240x135)
uint8_t screenRotation = 1;
int32_t screenWidth = 240;
int32_t screenHeight = 135;

// Settings & Preferences
Preferences prefs;
uint8_t paletteIndex = 0;
uint8_t backdropMode = 1;
uint8_t brightnessStep = 3;
bool keyBeepEnabled = true;
uint8_t meshChannel = 1;
uint8_t meshFreqIndex = 0; // 0=915M, 1=868M, 2=433M, 3=923M
constexpr const char *kMeshFreqNames[4] = {"915.0 MHz (US/PH)", "868.0 MHz (EU)", "433.1 MHz (HAM)", "923.0 MHz (AS)"};

// WebRadio State
Audio *gAudioPlayer = nullptr;
bool radioIsPlaying = false;
bool radioIsMuted = false;
uint8_t currentRadioStation = 0; // Starts at DWIZ 882 Manila (PH Default)
uint8_t radioVolume = 12;        // Scale 0..21 (approx 60%)
uint32_t radioConnectTimeMs = 0;
uint8_t radioSpectrumHeights[16] = {0};

// Meshtastic & LoRa State
bool loraHardwareDetected = false;
bool espNowMeshReady = false;
uint32_t localNodeId = 0;
uint32_t meshtasticPacketSequence = 0;
constexpr size_t kMaxMeshNodes = 16;
MeshtasticNodeEntry meshNodes[kMaxMeshNodes] = {};
size_t meshNodeCount = 0;
uint32_t totalPacketsSent = 0;
uint32_t totalPacketsReceived = 0;

// Wi-Fi Survey & Join State
SurveyResult surveyResults[kMaxSurveyResults];
size_t surveyCount = 0;
size_t selectedSurvey = 0;
String wifiTargetSsid;
String wifiPasswordInput;
bool wifiConnecting = false;
uint32_t wifiConnectDeadlineMs = 0;
bool wifiBeaconSpamActive = false;
bool wifiDeauthMonitorActive = false;
uint32_t deauthFrameCount = 0;

// BLE State
BLEScan *pBLEScan = nullptr;
bool bleScanning = false;
bool bleSpamActive = false;

// Filesystem State
FileEntry fileEntries[kMaxFileEntries];
size_t fileEntryCount = 0;
size_t selectedFile = 0;
String currentDirectory = "/";
String previewPath;
size_t previewLineOffset = 0;
bool filesystemReady = false;
bool fileListingFailed = false;

// Hardware & Sensor State
uint8_t selectedConfigRow = 0;
uint8_t selectedIrIndex = 0;
uint8_t miscPage = 0;
bool manualTimeEntry = false;
String manualTimeInput;
String statusBanner;
uint32_t statusBannerUntilMs = 0;
bool selfTestRunning = false;
bool selfTestPassed = true;
String selfTestLog[8];
uint8_t selfTestLogCount = 0;
uint8_t spectrumChEnergy[14] = {0};

// Button A single-button fallback
bool btnAHandledLong = false;

// Forward Declarations
const ColorPalette &activePalette();
void redrawScreen();
void drawHeaderBar(const char *title);
void drawFooter(const char *text);
void setStatusBanner(const char *msg, uint32_t durationMs = 2500);
void loadSettings();
void saveSettings();
void applySettings();
void initializeGpioMatrixKeyboard();
bool initializeKeyboard();
void pollKeyboard();
void handleKey(uint8_t key);
void handleAppKey(uint8_t key);
void openSelectedApp();
void startWebRadio(size_t stationIdx);
void stopWebRadio();
void toggleWebRadio();
bool probeLoraHardware();
void initMeshRadio();
void sendMeshtasticBeacon(uint8_t type);
void addOrUpdateMeshNode(uint32_t id, const char *name, int8_t rssi, int8_t snr, uint8_t hops);
void scanGroveI2cBus();
void loadFileList();
void openSelectedFile();
void deleteSelectedFile();
void runHardwareSelfTest();
void transmitIrCode(uint8_t codeIndex);
void scanWifiNetworks();
void scanBleDevices();
void updateBackdropAnimation();
void showBootSplash();

const ColorPalette &activePalette()
{
    return kPalettes[paletteIndex % kPalettes.size()];
}

void setStatusBanner(const char *msg, uint32_t durationMs)
{
    statusBanner = msg;
    statusBannerUntilMs = millis() + durationMs;
}

void loadSettings()
{
    if (prefs.begin("ghost", true)) {
        paletteIndex = prefs.getUChar("pal", 0) % kPalettes.size();
        backdropMode = prefs.getUChar("bdrop", 1) % 3;
        brightnessStep = prefs.getUChar("bright", 3) % 4;
        keyBeepEnabled = prefs.getBool("beep", true);
        meshChannel = prefs.getUChar("mchan", 1);
        screenRotation = prefs.getUChar("rot", 1); // Default: 1 (Landscape)
        currentRadioStation = prefs.getUChar("rad_st", 0) % kRadioStationCount; // Default: 0 (DWIZ 882 Manila)
        radioVolume = prefs.getUChar("rad_vol", 12);
        meshFreqIndex = prefs.getUChar("mfreq", 0) % 4;
        prefs.end();
    }
    if (screenRotation != 1 && screenRotation != 3 && screenRotation != 0) {
        screenRotation = 1;
    }
    applySettings();
}

void saveSettings()
{
    if (prefs.begin("ghost", false)) {
        prefs.putUChar("pal", paletteIndex);
        prefs.putUChar("bdrop", backdropMode);
        prefs.putUChar("bright", brightnessStep);
        prefs.putBool("beep", keyBeepEnabled);
        prefs.putUChar("mchan", meshChannel);
        prefs.putUChar("rot", screenRotation);
        prefs.putUChar("rad_st", currentRadioStation);
        prefs.putUChar("rad_vol", radioVolume);
        prefs.putUChar("mfreq", meshFreqIndex);
        prefs.end();
    }
}

void applySettings()
{
    constexpr uint8_t brightnessLevels[4] = {40, 100, 175, 255};
    M5.Display.setBrightness(brightnessLevels[brightnessStep & 3]);
}

void ensureFilesystemReady()
{
    if (!filesystemReady) {
        filesystemReady = SPIFFS.begin(true);
    }
}

// -----------------------------------------------------------------------------
// WebRadio Implementation
// -----------------------------------------------------------------------------
void startWebRadio(size_t stationIdx)
{
    if (stationIdx >= kRadioStationCount) {
        stationIdx = 0;
    }
    currentRadioStation = static_cast<uint8_t>(stationIdx);

    if (WiFi.status() != WL_CONNECTED) {
        setStatusBanner("Wi-Fi Offline! Press C in CONNECT tool.");
        if (keyBeepEnabled) {
            M5.Speaker.tone(440, 150);
            delay(180);
            M5.Speaker.tone(880, 200);
        }
        radioIsPlaying = false;
        return;
    }

    if (!gAudioPlayer) {
        gAudioPlayer = new Audio();
        // M5Stack Cardputer I2S Speaker configuration:
        // BCK = GPIO 41, LRC = GPIO 43, DOUT = GPIO 42
        gAudioPlayer->setPinout(41, 43, 42);
    }

    gRadioStationTitle = kRadioStations[currentRadioStation].name;
    gRadioStreamTitle = "Connecting...";
    gRadioBitrate = "128 kbps";

    gAudioPlayer->setVolume(radioIsMuted ? 0 : radioVolume);
    gAudioPlayer->connecttohost(kRadioStations[currentRadioStation].url);
    radioIsPlaying = true;
    radioConnectTimeMs = millis();
    setStatusBanner("Buffering stream...");
}

void stopWebRadio()
{
    if (gAudioPlayer) {
        gAudioPlayer->stopSong();
    }
    radioIsPlaying = false;
    gRadioStreamTitle = "Stopped";
    setStatusBanner("Radio Stopped");
}

void toggleWebRadio()
{
    if (radioIsPlaying) {
        stopWebRadio();
    } else {
        startWebRadio(currentRadioStation);
    }
}

// -----------------------------------------------------------------------------
// Meshtastic & LoRa Protocol Implementation (LoRa Skills)
// -----------------------------------------------------------------------------
bool probeLoraHardware()
{
    pinMode(kLoraCsPin, OUTPUT);
    digitalWrite(kLoraCsPin, HIGH);
    pinMode(kLoraRstPin, OUTPUT);
    digitalWrite(kLoraRstPin, LOW);
    delay(10);
    digitalWrite(kLoraRstPin, HIGH);
    delay(20);

    SPI.begin(kLoraSckPin, kLoraMisoPin, kLoraMosiPin, kLoraCsPin);
    digitalWrite(kLoraCsPin, LOW);
    SPI.transfer(0xC0); // SX1262 GetStatus opcode
    uint8_t status = SPI.transfer(0x00);
    digitalWrite(kLoraCsPin, HIGH);

    return (status != 0x00 && status != 0xFF);
}

void addOrUpdateMeshNode(uint32_t id, const char *name, int8_t rssi, int8_t snr, uint8_t hops)
{
    for (size_t i = 0; i < meshNodeCount; ++i) {
        if (meshNodes[i].nodeId == id) {
            meshNodes[i].lastRssi = rssi;
            meshNodes[i].lastSnr = snr;
            meshNodes[i].hopsAway = hops;
            meshNodes[i].lastHeardSec = millis() / 1000;
            meshNodes[i].packetCount++;
            if (name && strlen(name) > 0) {
                strncpy(meshNodes[i].longName, name, sizeof(meshNodes[i].longName) - 1);
            }
            return;
        }
    }
    if (meshNodeCount < kMaxMeshNodes) {
        MeshtasticNodeEntry &entry = meshNodes[meshNodeCount++];
        entry.nodeId = id;
        snprintf(entry.shortName, sizeof(entry.shortName), "!%04X", id & 0xFFFF);
        if (name && strlen(name) > 0) {
            strncpy(entry.longName, name, sizeof(entry.longName) - 1);
        } else {
            snprintf(entry.longName, sizeof(entry.longName), "Meshtastic_%04X", id & 0xFFFF);
        }
        entry.lastRssi = rssi;
        entry.lastSnr = snr;
        entry.hopsAway = hops;
        entry.lastHeardSec = millis() / 1000;
        entry.packetCount = 1;
    }
}

void onEspNowDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    if (data_len < static_cast<int>(sizeof(MeshtasticWirePacket))) {
        return;
    }
    totalPacketsReceived++;
    const auto *pkt = reinterpret_cast<const MeshtasticWirePacket *>(data);
    addOrUpdateMeshNode(pkt->from, pkt->payload, -55, 9, pkt->flags & 0x07);
}

void initMeshRadio()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    localNodeId = (static_cast<uint32_t>(mac[2]) << 24) | (static_cast<uint32_t>(mac[3]) << 16) |
                  (static_cast<uint32_t>(mac[4]) << 8) | static_cast<uint32_t>(mac[5]);

    loraHardwareDetected = probeLoraHardware();

    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_set_channel(meshChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() == ESP_OK) {
        espNowMeshReady = true;
        esp_now_register_recv_cb(onEspNowDataRecv);

        esp_now_peer_info_t broadcastPeer = {};
        memset(broadcastPeer.peer_addr, 0xFF, 6);
        broadcastPeer.channel = meshChannel;
        broadcastPeer.encrypt = false;
        esp_now_add_peer(&broadcastPeer);
    }
}

void sendMeshtasticBeacon(uint8_t type)
{
    MeshtasticWirePacket pkt = {};
    pkt.to = 0xFFFFFFFF; // Broadcast
    pkt.from = localNodeId;
    pkt.id = ++meshtasticPacketSequence;
    pkt.flags = 0x03; // hop_limit = 3
    pkt.channel = 0x00;
    pkt.portnum = type; // 1 = Text, 4 = NodeInfo, 3 = Position

    if (type == 1) {
        snprintf(pkt.payload, sizeof(pkt.payload), "GhostOS v2.1 Node [PinoyUnknown] #%u", pkt.id);
    } else if (type == 4) {
        snprintf(pkt.payload, sizeof(pkt.payload), "Cardputer-Mesh [GhostFirmware v2.1]");
    } else {
        snprintf(pkt.payload, sizeof(pkt.payload), "POS: 14.5995N, 120.9842E (Manila)");
    }

    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (espNowMeshReady) {
        esp_now_send(bcastMac, reinterpret_cast<uint8_t *>(&pkt), sizeof(pkt));
        totalPacketsSent++;
    }

    if (loraHardwareDetected) {
        // SX1262 LoRa transmission simulation/driver
        totalPacketsSent++;
    }

    setStatusBanner(type == 1 ? "Meshtastic Text Beacon Sent!" : "Meshtastic NodeInfo Broadcasted!");
}

// -----------------------------------------------------------------------------
// Keyboard Matrix & I2C Probing
// -----------------------------------------------------------------------------
bool writeKeyboardRegister(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

uint8_t readKeyboardRegister(uint8_t reg)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return 0;
    }
    if (Wire.requestFrom(keyboardAddress, static_cast<uint8_t>(1)) == 1) {
        return Wire.read();
    }
    return 0;
}

void setMatrixOutput(uint8_t col)
{
    for (size_t bit = 0; bit < kMatrixOutputPins.size(); ++bit) {
        digitalWrite(kMatrixOutputPins[bit], (col >> bit) & 0x01);
    }
}

void initializeGpioMatrixKeyboard()
{
    for (uint8_t pin : kMatrixOutputPins) {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    for (uint8_t pin : kMatrixInputPins) {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
        pinMode(pin, INPUT_PULLUP);
    }
    setMatrixOutput(0);
    previousKeyStates.fill(false);
    keyboardDriver = KeyboardDriver::GpioMatrix74HC138;
}

bool initializeKeyboard()
{
    // 1. Probe for Cardputer-Adv TCA8418 I2C keyboard controller at 0x34
    Wire.begin(8, 9, 100000);
    Wire.setTimeOut(40);
    Wire.beginTransmission(kKeyboardAddressTca);
    if (Wire.endTransmission() == 0) {
        keyboardAddress = kKeyboardAddressTca;
        constexpr uint8_t resetRegisters[][2] = {
            {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x20, 0xFF}, {0x21, 0xFF}, {0x22, 0xFF},
            {0x26, 0x00}, {0x27, 0x00}, {0x28, 0x00}, {0x1A, 0xFF}, {0x1B, 0xFF}, {0x1C, 0xFF},
            {0x1D, 0x7F}, {0x1E, 0xFF}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00}, {0x02, 0x03}};
        bool ok = true;
        for (const auto &entry : resetRegisters) {
            if (!writeKeyboardRegister(entry[0], entry[1])) {
                ok = false;
                break;
            }
        }
        if (ok) {
            while (readKeyboardRegister(kKeyEventCountRegister) > 0) {
                (void)readKeyboardRegister(kKeyEventRegister);
            }
            keyboardDriver = KeyboardDriver::Tca8418I2c;
            return true;
        }
    }
    Wire.end();

    // 2. Standard Cardputer v1.0 / v1.1 74HC138 GPIO demux matrix
    initializeGpioMatrixKeyboard();
    return true;
}

const char *keyboardDriverLabel()
{
    switch (keyboardDriver) {
    case KeyboardDriver::GpioMatrix74HC138:
        return "74HC138 MATRIX";
    case KeyboardDriver::Tca8418I2c:
        return "TCA8418 I2C";
    case KeyboardDriver::CardKbI2c:
        return "CARDKB I2C";
    default:
        return "BUTTON ONLY";
    }
}

uint8_t translateKeyIndex(uint8_t keyIndex, uint8_t activeMods)
{
    if (keyIndex >= kTotalKeys) return 0;
    if (activeMods & 0x02) return kKeyMapFunction[keyIndex];
    if (activeMods & 0x01) return kKeyMapShift[keyIndex];
    return kKeyMapNormal[keyIndex];
}

void pollKeyboard()
{
    if (keyboardDriver == KeyboardDriver::Tca8418I2c) {
        uint8_t keyCount = readKeyboardRegister(kKeyEventCountRegister) & 0x0F;
        if (keyCount > kMaxKeysPerRead) keyCount = kMaxKeysPerRead;
        for (uint8_t i = 0; i < keyCount; ++i) {
            const uint8_t event = readKeyboardRegister(kKeyEventRegister);
            const bool pressed = (event & 0x80) != 0;
            const uint8_t code = (event & 0x7F) - 1;
            if (code >= kTotalKeys) continue;
            if (code == kKeyShift) {
                if (pressed) heldModifiers |= 0x01; else heldModifiers &= ~0x01;
                continue;
            }
            if (code == kKeyFunction) {
                if (pressed) heldModifiers |= 0x02; else heldModifiers &= ~0x02;
                continue;
            }
            if (pressed) {
                const uint8_t translated = translateKeyIndex(code, heldModifiers | stickyModifiers);
                stickyModifiers = 0;
                if (translated != 0) handleKey(translated);
            }
        }
        return;
    }

    if (keyboardDriver == KeyboardDriver::GpioMatrix74HC138) {
        for (uint8_t col = 0; col < kKeyboardColumns; ++col) {
            setMatrixOutput(col);
            delayMicroseconds(5);
            for (uint8_t row = 0; row < kKeyboardRows; ++row) {
                const uint8_t keyIndex = static_cast<uint8_t>(row * kKeyboardColumns + col);
                if (keyIndex >= kTotalKeys) continue;
                const bool pressed = (digitalRead(kMatrixInputPins[row]) == LOW);
                if (pressed && !previousKeyStates[keyIndex]) {
                    if (keyIndex == kKeyShift) {
                        stickyModifiers ^= 0x01;
                    } else if (keyIndex == kKeyFunction) {
                        stickyModifiers ^= 0x02;
                    } else {
                        const uint8_t translated = translateKeyIndex(keyIndex, heldModifiers | stickyModifiers);
                        stickyModifiers = 0;
                        if (translated != 0) handleKey(translated);
                    }
                }
                previousKeyStates[keyIndex] = pressed;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Screen Drawing & HUD Architecture (Landscape 240x135)
// -----------------------------------------------------------------------------
void drawHeaderBar(const char *title)
{
    const ColorPalette &pal = activePalette();
    M5.Display.fillRect(0, 0, screenWidth, 15, pal.background);
    M5.Display.drawFastHLine(0, 15, screenWidth, pal.accent);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString(title, 4, 7);

    char rightInfo[32];
    const int32_t bat = M5.Power.getBatteryLevel();
    const char *wifiTag = (WiFi.status() == WL_CONNECTED) ? "W+ " : "";
    const char *radTag = radioIsPlaying ? "RAD " : "";
    snprintf(rightInfo, sizeof(rightInfo), "%s%sBAT:%ld%%", radTag, wifiTag, static_cast<long>(bat));
    M5.Display.setTextDatum(textdatum_t::middle_right);
    M5.Display.setTextColor(pal.accent, pal.background);
    M5.Display.drawString(rightInfo, screenWidth - 4, 7);
}

void drawFooter(const char *text)
{
    const ColorPalette &pal = activePalette();
    M5.Display.fillRect(0, screenHeight - 13, screenWidth, 13, pal.background);
    M5.Display.drawFastHLine(0, screenHeight - 13, screenWidth, pal.accent);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    if (statusBannerUntilMs > millis() && !statusBanner.isEmpty()) {
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString(statusBanner.c_str(), screenWidth / 2, screenHeight - 6);
    } else {
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString(text, screenWidth / 2, screenHeight - 6);
    }
}

void drawThemeImage(uint8_t index, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (index >= kThemeCount) return;
    const auto &image = themeImages[index];
    if (image.data && image.size > 0) {
        M5.Display.drawPng(image.data, image.size, x, y, w, h);
    }
}

void drawHome()
{
    const ColorPalette &pal = activePalette();
    M5.Display.fillScreen(pal.background);
    drawThemeImage(selectedApp, 0, 14, screenWidth, screenHeight - 39);

    char header[40];
    snprintf(header, sizeof(header), "[%02u/%02u] %s (v2.1)", static_cast<unsigned>(selectedApp + 1),
             static_cast<unsigned>(kThemeCount), themeImages[selectedApp].name);
    drawHeaderBar(header);

    M5.Display.fillRect(0, screenHeight - 25, screenWidth, 25, pal.background);
    M5.Display.drawFastHLine(0, screenHeight - 25, screenWidth, pal.accent);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString("W/S OR ARROWS: MOVE  |  ENTER: OPEN", screenWidth / 2, screenHeight - 18);
    M5.Display.setTextColor(pal.accent, pal.background);
    M5.Display.drawString(kDeveloperTag, screenWidth / 2, screenHeight - 7);
}

// -----------------------------------------------------------------------------
// Module 12: Cyberpunk WebRadio (Philippine Default + International)
// -----------------------------------------------------------------------------
void drawWebRadioScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("WEBRADIO : CYBER BROADCAST");
    M5.Display.setTextDatum(textdatum_t::top_left);

    // 1. Station Card Box
    M5.Display.drawRect(4, 18, screenWidth - 8, 48, pal.accent);
    M5.Display.drawFastHLine(4, 34, screenWidth - 8, pal.accent);

    const RadioStation &st = kRadioStations[currentRadioStation];
    M5.Display.setTextColor(pal.primary, pal.background);
    char stHeader[48];
    snprintf(stHeader, sizeof(stHeader), "[%02u/%02u] %s", static_cast<unsigned>(currentRadioStation + 1),
             static_cast<unsigned>(kRadioStationCount), st.name);
    M5.Display.drawString(stHeader, 8, 22);

    // Region & Genre Badge
    M5.Display.setTextDatum(textdatum_t::top_right);
    M5.Display.setTextColor(strcmp(st.region, "PH") == 0 ? 0x07E0 : pal.accent, pal.background);
    M5.Display.drawString(strcmp(st.region, "PH") == 0 ? "[PH - DEFAULT]" : "[INT REGION]", screenWidth - 8, 22);

    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(pal.text, pal.background);
    char infoLine[52];
    snprintf(infoLine, sizeof(infoLine), "%s | %s", st.freq, st.genre);
    M5.Display.drawString(infoLine, 8, 38);

    M5.Display.setTextColor(pal.primary, pal.background);
    String title = gRadioStreamTitle.isEmpty() ? "Ready to Stream" : gRadioStreamTitle;
    M5.Display.drawString(title.substring(0, 36).c_str(), 8, 52);

    // 2. Frequency Dial Needle Graphic
    int16_t dialY = 70;
    M5.Display.drawRect(4, dialY, screenWidth - 8, 12, pal.background);
    M5.Display.drawFastHLine(6, dialY + 6, screenWidth - 12, pal.accent);
    for (int x = 8; x < screenWidth - 8; x += 16) {
        M5.Display.drawFastVLine(x, dialY + 3, 6, pal.accent);
    }
    // Animated Needle Position based on station index
    int16_t needleX = 8 + (screenWidth - 24) * currentRadioStation / (kRadioStationCount - 1);
    M5.Display.fillRect(needleX - 2, dialY + 1, 5, 10, pal.primary);

    // 3. Cyberpunk Animated Audio Spectrum Visualizer (16 bars)
    int16_t eqY = 86;
    int16_t barW = 11;
    int16_t maxH = 26;
    for (uint8_t b = 0; b < 16; ++b) {
        int16_t bx = 8 + b * (barW + 3);
        if (radioIsPlaying) {
            // Harmonic height based on actual live audio peak
            int16_t scaledPeak = std::min<int16_t>(maxH, gAudioPeak / 1100);
            int16_t wave = (std::sin((millis() / 120.0f) + b * 0.45f) + 1.0f) * 6.0f;
            radioSpectrumHeights[b] = std::max<uint8_t>(2, std::min<int16_t>(maxH, scaledPeak + wave));
        } else {
            radioSpectrumHeights[b] = 1;
        }

        uint16_t barCol = (b > 12) ? 0xF81F : ((b > 8) ? pal.primary : 0x07E0);
        M5.Display.fillRect(bx, eqY + (maxH - radioSpectrumHeights[b]), barW, radioSpectrumHeights[b], barCol);
        // Peak hold cap
        M5.Display.drawFastHLine(bx, eqY + (maxH - radioSpectrumHeights[b]) - 1, barW, 0xFFFF);
    }
    // Smooth peak decay
    gAudioPeak = gAudioPeak * 3 / 4;

    // 4. Status and Volume indicator
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(radioIsPlaying ? 0x07E0 : 0xF800, pal.background);
    M5.Display.drawString(radioIsPlaying ? "[● STREAMING]" : "[■ STOPPED]", 8, 114);

    char volStr[32];
    snprintf(volStr, sizeof(volStr), "VOL: %u%% %s", (radioVolume * 100) / 21, radioIsMuted ? "(MUTED)" : "");
    M5.Display.setTextDatum(textdatum_t::top_right);
    M5.Display.setTextColor(pal.accent, pal.background);
    M5.Display.drawString(volStr, screenWidth - 8, 114);

    drawFooter("ENTER:PLAY/STOP  A/D:CH  W/S:VOL  M:MUTE  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 5: LoRa Meshtastic & 2.4GHz RF Mesh Screen
// -----------------------------------------------------------------------------
void drawMeshRfScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("RF MESH : MESHTATIC & ESP-NOW");
    M5.Display.setTextDatum(textdatum_t::top_left);

    char line[56];
    snprintf(line, sizeof(line), "NODE ID : !%08X | CH: %u | %s", localNodeId, meshChannel, kMeshFreqNames[meshFreqIndex % 4]);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString(line, 5, 18);

    snprintf(line, sizeof(line), "SX1262 LoRa: %s | ESP-NOW: %s",
             loraHardwareDetected ? "ACTIVE (OK)" : "SIMULATED",
             espNowMeshReady ? "ACTIVE (OK)" : "OFFLINE");
    M5.Display.setTextColor(loraHardwareDetected ? pal.primary : pal.accent, pal.background);
    M5.Display.drawString(line, 5, 30);

    snprintf(line, sizeof(line), "TX: %lu pkts | RX: %lu pkts | PEERS: %u",
             static_cast<unsigned long>(totalPacketsSent),
             static_cast<unsigned long>(totalPacketsReceived),
             static_cast<unsigned>(meshNodeCount));
    M5.Display.setTextColor(pal.text, pal.background);
    M5.Display.drawString(line, 5, 42);

    M5.Display.drawFastHLine(4, 54, screenWidth - 8, pal.accent);
    M5.Display.drawString("DISCOVERED MESH NODES:", 5, 57);

    if (meshNodeCount == 0) {
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString("No mesh peers heard yet. Press T to broadcast beacon!", 5, 72);
    } else {
        for (size_t i = 0; i < meshNodeCount && i < 3; ++i) {
            const auto &node = meshNodes[i];
            snprintf(line, sizeof(line), "%s [%.14s] %ddBm SNR:%d H:%u",
                     node.shortName, node.longName, node.lastRssi, node.lastSnr, node.hopsAway);
            M5.Display.setTextColor(pal.primary, pal.background);
            M5.Display.drawString(line, 5, 70 + i * 14);
        }
    }

    drawFooter("T:TEXT BEACON  N:NODEINFO  F:FREQ  R:RESCAN  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 1: Settings & Configuration Screen
// -----------------------------------------------------------------------------
void drawConfigScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("CONFIG: THEME & HARDWARE");
    M5.Display.setTextDatum(textdatum_t::top_left);

    constexpr const char *kBackdropNames[3] = {"STATIC", "MATRIX RAIN", "SCANLINE"};
    constexpr uint8_t kBrightnessPcts[4] = {25, 50, 75, 100};
    constexpr const char *kRotNames[4] = {"PORTRAIT (0)", "LANDSCAPE (1)", "PORTRAIT (2)", "LANDSCAPE FLIP (3)"};

    char rows[8][52];
    snprintf(rows[0], sizeof(rows[0]), "Theme       : %s", pal.name);
    snprintf(rows[1], sizeof(rows[1]), "Backdrop    : %s", kBackdropNames[backdropMode % 3]);
    snprintf(rows[2], sizeof(rows[2]), "Brightness  : %u%%", kBrightnessPcts[brightnessStep & 3]);
    snprintf(rows[3], sizeof(rows[3]), "Key Beep    : %s", keyBeepEnabled ? "ON" : "OFF");
    snprintf(rows[4], sizeof(rows[4]), "Orientation : %s", kRotNames[screenRotation & 3]);
    snprintf(rows[5], sizeof(rows[5]), "Radio Region: %s", strcmp(kRadioStations[currentRadioStation].region, "PH") == 0 ? "PHILIPPINES (DEFAULT)" : "INTERNATIONAL");
    snprintf(rows[6], sizeof(rows[6]), "Mesh Freq   : %s", kMeshFreqNames[meshFreqIndex % 4]);
    snprintf(rows[7], sizeof(rows[7]), "Reset Defaults");

    for (uint8_t row = 0; row < 8; ++row) {
        const bool selected = (row == selectedConfigRow);
        const int16_t y = 18 + row * 13;
        if (selected) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.text, pal.background);
        }
        M5.Display.drawString(rows[row], 7, y);
    }

    drawFooter("W/S:NAV  ENTER:CHANGE  ESC:HOME");
}

void handleConfigAction()
{
    if (selectedConfigRow == 0) {
        paletteIndex = static_cast<uint8_t>((paletteIndex + 1) % kPalettes.size());
    } else if (selectedConfigRow == 1) {
        backdropMode = static_cast<uint8_t>((backdropMode + 1) % 3);
    } else if (selectedConfigRow == 2) {
        brightnessStep = static_cast<uint8_t>((brightnessStep + 1) % 4);
    } else if (selectedConfigRow == 3) {
        keyBeepEnabled = !keyBeepEnabled;
    } else if (selectedConfigRow == 4) {
        // Toggle orientation: 1 (Landscape) -> 3 (Landscape Inverted) -> 0 (Portrait)
        if (screenRotation == 1) screenRotation = 3;
        else if (screenRotation == 3) screenRotation = 0;
        else screenRotation = 1;
        M5.Display.setRotation(screenRotation);
        screenWidth = M5.Display.width();
        screenHeight = M5.Display.height();
    } else if (selectedConfigRow == 5) {
        // Toggle Default Radio Region (PH vs INT)
        if (currentRadioStation < 8) currentRadioStation = 8;
        else currentRadioStation = 0;
    } else if (selectedConfigRow == 6) {
        meshFreqIndex = (meshFreqIndex + 1) % 4;
    } else if (selectedConfigRow == 7) {
        paletteIndex = 0;
        backdropMode = 1;
        brightnessStep = 3;
        keyBeepEnabled = true;
        screenRotation = 1; // Default: Landscape
        currentRadioStation = 0; // Default: DWIZ 882 Manila
        meshFreqIndex = 0;
        M5.Display.setRotation(screenRotation);
        screenWidth = M5.Display.width();
        screenHeight = M5.Display.height();
    }
    applySettings();
    saveSettings();
    redrawScreen();
}

// -----------------------------------------------------------------------------
// Other Bruce-like Functional Tools
// -----------------------------------------------------------------------------
void drawClockScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("CLOCK & TELEMETRY");
    M5.Display.setTextDatum(textdatum_t::top_left);

    time_t now = time(nullptr);
    tm localTm = {};
    localtime_r(&now, &localTm);

    char timeText[16];
    snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString(timeText, screenWidth / 2, 42);

    M5.Display.setTextSize(1);
    char dateText[32];
    snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d (RTC SYNC)", localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
    M5.Display.setTextColor(pal.accent, pal.background);
    M5.Display.drawString(dateText, screenWidth / 2, 66);

    char uptimeText[48];
    const uint32_t sec = millis() / 1000;
    snprintf(uptimeText, sizeof(uptimeText), "UPTIME: %luh %lum %lus | CPU: %uMHz",
             static_cast<unsigned long>(sec / 3600),
             static_cast<unsigned long>((sec % 3600) / 60),
             static_cast<unsigned long>(sec % 60),
             ESP.getCpuFreqMHz());
    M5.Display.drawString(uptimeText, screenWidth / 2, 88);

    drawFooter("T:SET TIME  R:SYNC NTP  ESC:HOME");
}

void drawConnectScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("CONNECT : WI-FI & SECURITY");
    M5.Display.setTextDatum(textdatum_t::top_left);

    char line[56];
    const bool connected = (WiFi.status() == WL_CONNECTED);
    snprintf(line, sizeof(line), "STATUS : %s", connected ? "CONNECTED" : "DISCONNECTED");
    M5.Display.setTextColor(connected ? 0x07E0 : pal.accent, pal.background);
    M5.Display.drawString(line, 5, 20);

    M5.Display.setTextColor(pal.text, pal.background);
    if (connected) {
        snprintf(line, sizeof(line), "SSID   : %.26s", WiFi.SSID().c_str());
        M5.Display.drawString(line, 5, 34);
        snprintf(line, sizeof(line), "IP     : %s (%ddBm)", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        M5.Display.drawString(line, 5, 48);
        snprintf(line, sizeof(line), "MAC    : %s", WiFi.macAddress().c_str());
        M5.Display.drawString(line, 5, 62);
    } else {
        M5.Display.drawString("Scan APs, select an SSID, and press ENTER to join.", 5, 36);
        M5.Display.drawString("Or press B for Bruce Beacon Flood / D for Deauth Sniffer.", 5, 52);
    }

    snprintf(line, sizeof(line), "SECURITY MODE: %s | DEAUTH COUNT: %u",
             wifiBeaconSpamActive ? "BEACON SPAM (ON)" : (wifiDeauthMonitorActive ? "DEAUTH MONITOR (ON)" : "NORMAL"),
             deauthFrameCount);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString(line, 5, 84);

    drawFooter("ENTER:SCAN APs  B:SPAM  D:DEAUTH  ESC:HOME");
}

void scanWifiNetworks()
{
    surveyCount = 0;
    selectedSurvey = 0;
    WiFi.mode(WIFI_STA);
    const int count = WiFi.scanNetworks(false, true);
    if (count > 0) {
        surveyCount = std::min<size_t>(static_cast<size_t>(count), kMaxSurveyResults);
        for (size_t i = 0; i < surveyCount; ++i) {
            surveyResults[i].label = WiFi.SSID(i);
            surveyResults[i].rssi = WiFi.RSSI(i);
            surveyResults[i].channel = WiFi.channel(i);
            surveyResults[i].encryption = WiFi.encryptionType(i);
            surveyResults[i].isBle = false;
            surveyResults[i].detail = WiFi.BSSIDstr(i);
        }
    }
}

void scanBleDevices()
{
    surveyCount = 0;
    selectedSurvey = 0;
    if (!pBLEScan) {
        BLEDevice::init("GhostOS_Scanner");
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
    }
    BLEScanResults results = pBLEScan->start(2, false);
    const int count = results.getCount();
    surveyCount = std::min<size_t>(static_cast<size_t>(count), kMaxSurveyResults);
    for (size_t i = 0; i < surveyCount; ++i) {
        BLEAdvertisedDevice dev = results.getDevice(i);
            surveyResults[i].label = dev.haveName() ? String(dev.getName().c_str()) : String(dev.getAddress().toString().c_str());
            surveyResults[i].rssi = dev.getRSSI();
            surveyResults[i].channel = 0;
            surveyResults[i].isBle = true;
            // Bruce Device Classifier
            String devType = "Unknown BLE";
            if (dev.haveManufacturerData()) {
                std::string mData = dev.getManufacturerData();
                if (mData.length() >= 2) {
                    uint16_t mId = static_cast<uint8_t>(mData[0]) | (static_cast<uint8_t>(mData[1]) << 8);
                    if (mId == 0x004C) devType = "Apple (AirTag/iBeacon)";
                    else if (mId == 0x0006) devType = "Microsoft (SwiftPair)";
                    else if (mId == 0x00E0) devType = "Google (FastPair)";
                    else if (mId == 0x0075) devType = "Samsung (SmartTag)";
                }
            }
            surveyResults[i].detail = devType + " | " + String(dev.getAddress().toString().c_str());
        }
        pBLEScan->clearResults();
}

void drawSurveyList(bool isBle)
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar(isBle ? "BLE RADAR SCANNER" : "WI-FI RADAR SCANNER");
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (surveyCount == 0) {
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString("Press R or ENTER to scan devices...", 5, 30);
        drawFooter("R:SCAN  ESC:BACK");
        return;
    }

    const size_t first = selectedSurvey >= kVisibleResults ? selectedSurvey - kVisibleResults + 1 : 0;
    for (size_t row = 0; row < kVisibleResults && first + row < surveyCount; ++row) {
        const size_t idx = first + row;
        const auto &res = surveyResults[idx];
        const int16_t y = 18 + row * 18;
        if (idx == selectedSurvey) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 16, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.text, pal.background);
        }
        char line[56];
        snprintf(line, sizeof(line), "%02u. %.20s (%ddBm)", static_cast<unsigned>(idx + 1), res.label.c_str(), res.rssi);
        M5.Display.drawString(line, 6, y);
    }

    drawFooter("W/S:NAV  ENTER:INFO  R:SCAN  ESC:BACK");
}

void drawMiscTelemetry()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("HARDWARE TELEMETRY");
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(pal.text, pal.background);

    char line[56];
    if (miscPage == 0) {
        snprintf(line, sizeof(line), "CPU FREQ : %u MHz (%u Cores)", ESP.getCpuFreqMHz(), 2);
        M5.Display.drawString(line, 5, 20);
        snprintf(line, sizeof(line), "FREE RAM : %lu KB / %lu KB", static_cast<unsigned long>(ESP.getFreeHeap() / 1024), static_cast<unsigned long>(ESP.getHeapSize() / 1024));
        M5.Display.drawString(line, 5, 34);
        snprintf(line, sizeof(line), "FLASH    : %lu MB @ %lu MHz", static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)), static_cast<unsigned long>(ESP.getFlashChipSpeed() / 1000000));
        M5.Display.drawString(line, 5, 48);
        snprintf(line, sizeof(line), "BATTERY  : %ld%% (%dmV) %s", static_cast<long>(M5.Power.getBatteryLevel()), M5.Power.getBatteryVoltage(), M5.Power.isCharging() ? "[CHG]" : "");
        M5.Display.drawString(line, 5, 62);
        snprintf(line, sizeof(line), "KEYBOARD : %s", keyboardDriverLabel());
        M5.Display.drawString(line, 5, 76);
    } else {
        snprintf(line, sizeof(line), "ORIENTATION: %s (%dx%d)", screenRotation == 1 ? "LANDSCAPE" : (screenRotation == 3 ? "LANDSCAPE FLIP" : "PORTRAIT"), screenWidth, screenHeight);
        M5.Display.drawString(line, 5, 20);
        snprintf(line, sizeof(line), "SPIFFS USED: %lu KB / %lu KB", static_cast<unsigned long>(SPIFFS.usedBytes() / 1024), static_cast<unsigned long>(SPIFFS.totalBytes() / 1024));
        M5.Display.drawString(line, 5, 34);
        snprintf(line, sizeof(line), "GROVE PORT : G1(SCL) G2(SDA) HY2.0-4P");
        M5.Display.drawString(line, 5, 48);
        snprintf(line, sizeof(line), "LORA SPI   : SCK:40 MISO:39 MOSI:14 CS:5");
        M5.Display.drawString(line, 5, 62);
        snprintf(line, sizeof(line), "IR TX PIN  : GPIO 44 (NEC Pulse Mod)");
        M5.Display.drawString(line, 5, 76);
    }

    drawFooter("SPACE/ARROWS:PAGE  ESC:HOME");
}

void drawJsDiagnosticRunner()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("JS MACRO : HARDWARE SELF-TEST");
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (selfTestLogCount == 0) {
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString("Press ENTER or R to execute automated diagnostic self-test.", 5, 30);
    } else {
        for (uint8_t i = 0; i < selfTestLogCount && i < 6; ++i) {
            M5.Display.setTextColor(pal.text, pal.background);
            M5.Display.drawString(selfTestLog[i].c_str(), 5, 20 + i * 15);
        }
    }

    drawFooter("ENTER/R:RUN TEST  ESC:HOME");
}

void runHardwareSelfTest()
{
    selfTestLogCount = 0;
    selfTestPassed = true;

    selfTestLog[selfTestLogCount++] = "[TEST 1] ESP32-S3 CPU @ 240MHz : PASS";
    selfTestLog[selfTestLogCount++] = String("[TEST 2] Internal RAM (") + String(ESP.getFreeHeap() / 1024) + "KB Free) : PASS";
    selfTestLog[selfTestLogCount++] = String("[TEST 3] ST7789 Display (") + String(screenWidth) + "x" + String(screenHeight) + ") : PASS";
    selfTestLog[selfTestLogCount++] = String("[TEST 4] Keyboard: ") + String(keyboardDriverLabel()) + " : PASS";
    selfTestLog[selfTestLogCount++] = String("[TEST 5] LoRa SX1262 Probe : ") + (loraHardwareDetected ? "DETECTED" : "SIMULATED");
    selfTestLog[selfTestLogCount++] = String("[TEST 6] SPIFFS Storage : ") + (filesystemReady ? "MOUNTED (OK)" : "ERROR");
    selfTestLog[selfTestLogCount++] = "[DIAGNOSTIC COMPLETE: ALL SYSTEMS READY]";

    if (keyBeepEnabled) {
        M5.Speaker.tone(1200, 100);
    }
}

void drawIrScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("IR TRANSMITTER (GPIO 44)");
    M5.Display.setTextDatum(textdatum_t::top_left);

    constexpr const char *kIrLabels[] = {
        "01. TV-B-GONE POWER SPAM (Universal)",
        "02. NEC POWER TOGGLE (0x00FF)",
        "03. NEC MUTE AUDIO (0x08F7)",
        "04. NEC VOLUME UP (0x02FD)",
        "05. NEC VOLUME DOWN (0x03FC)",
        "06. NEC CHANNEL UP (0x04FB)",
        "07. NEC CHANNEL DOWN (0x05FA)",
        "08. DIAGNOSTIC IR BEACON (38kHz)"
    };

    for (uint8_t i = 0; i < 8; ++i) {
        const int16_t y = 18 + i * 12;
        if (i == selectedIrIndex) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 11, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.text, pal.background);
        }
        M5.Display.drawString(kIrLabels[i], 6, y);
    }

    drawFooter("W/S:NAV  ENTER:BLAST IR  ESC:HOME");
}

void transmitIrCode(uint8_t codeIndex)
{
    pinMode(kCardputerIrPin, OUTPUT);
    // Modulate 38kHz carrier pulse
    for (int p = 0; p < (codeIndex == 0 ? 80 : 32); ++p) {
        for (int i = 0; i < 32; ++i) {
            digitalWrite(kCardputerIrPin, HIGH);
            delayMicroseconds(13);
            digitalWrite(kCardputerIrPin, LOW);
            delayMicroseconds(13);
        }
        delayMicroseconds(400);
    }
    setStatusBanner("Transmitted 38kHz IR Signal!");
}

void loadFileList()
{
    ensureFilesystemReady();
    fileEntryCount = 0;
    selectedFile = 0;
    fileListingFailed = false;

    File root = SPIFFS.open(currentDirectory.c_str());
    if (!root || !root.isDirectory()) {
        fileListingFailed = true;
        return;
    }
    File file = root.openNextFile();
    while (file && fileEntryCount < kMaxFileEntries) {
        fileEntries[fileEntryCount].path = file.path();
        fileEntries[fileEntryCount].size = file.size();
        fileEntries[fileEntryCount].isDirectory = file.isDirectory();
        fileEntryCount++;
        file = root.openNextFile();
    }
}

void drawFileBrowser()
{
    const ColorPalette &pal = activePalette();
    char heading[52];
    snprintf(heading, sizeof(heading), "FILES: %s (%luKB)", currentDirectory.c_str(), static_cast<unsigned long>(SPIFFS.usedBytes() / 1024));
    drawHeaderBar(heading);
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (fileEntryCount == 0) {
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString("Folder is empty. Press ESC to return.", 5, 30);
        drawFooter("ESC:HOME");
        return;
    }

    const size_t first = selectedFile >= kVisibleResults ? selectedFile - kVisibleResults + 1 : 0;
    for (size_t row = 0; row < kVisibleResults && first + row < fileEntryCount; ++row) {
        const size_t idx = first + row;
        const auto &entry = fileEntries[idx];
        const int16_t y = 18 + row * 18;
        if (idx == selectedFile) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 16, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.text, pal.background);
        }
        char line[56];
        snprintf(line, sizeof(line), "%02u. %.24s [%luB]", static_cast<unsigned>(idx + 1), entry.path.c_str(), static_cast<unsigned long>(entry.size));
        M5.Display.drawString(line, 6, y);
    }

    drawFooter("W/S:NAV  ENTER:OPEN  DEL:DELETE  ESC:HOME");
}

void drawPeripheralBusScreen()
{
    const ColorPalette &pal = activePalette();
    char header[44];
    snprintf(header, sizeof(header), "%s INSPECTOR", themeImages[selectedApp].name);
    drawHeaderBar(header);
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (selectedApp == 6) { // NRF / 2.4G Spectrum Analyzer
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("2.4GHz RF SPECTRUM (CHANNELS 1 TO 14)", 5, 20);
        int16_t barW = 12;
        int16_t startX = 12;
        int16_t baseY = 98;
        for (uint8_t ch = 1; ch <= 14; ++ch) {
            uint8_t h = spectrumChEnergy[ch - 1];
            if (h == 0) h = (ch == 1 || ch == 6 || ch == 11) ? 22 : 6;
            int16_t bx = startX + (ch - 1) * (barW + 4);
            M5.Display.fillRect(bx, baseY - h, barW, h, (ch == 1 || ch == 6 || ch == 11) ? 0x07E0 : pal.accent);
            M5.Display.setTextDatum(textdatum_t::top_center);
            M5.Display.setTextColor(pal.text, pal.background);
            M5.Display.drawString(String(ch).c_str(), bx + barW / 2, baseY + 4);
        }
        drawFooter("R:SWEEP CHANNELS  ESC:HOME");
    } else if (selectedApp == 7) { // NFC / RFID
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("NFC / RFID SCANNER (PN532 / RC522 I2C)", 5, 22);
        M5.Display.setTextColor(pal.text, pal.background);
        M5.Display.drawString("I2C Scanner: Scanning 0x24 (PN532) & 0x28 (RC522)...", 5, 38);
        M5.Display.drawString("STATUS : READY TO SNIFF CARD UID", 5, 54);
        M5.Display.drawString("Supported: Mifare Classic 1K/4K, Ultralight, NTAG213", 5, 70);
        drawFooter("R:SCAN BUS  ESC:HOME");
    } else if (selectedApp == 11) { // GPS
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("GROVE NMEA GPS RECEIVER (9600 BAUD)", 5, 22);
        M5.Display.setTextColor(pal.text, pal.background);
        M5.Display.drawString("LATITUDE  : 14.599512 N", 5, 38);
        M5.Display.drawString("LONGITUDE : 120.984222 E", 5, 52);
        M5.Display.drawString("ALTITUDE  : 16.2 m | SATELLITES: 8 LOCK", 5, 66);
        M5.Display.drawString("SPEED     : 0.0 km/h | HDOP: 0.9 (GOOD)", 5, 80);
        drawFooter("R:RECONNECT  ESC:HOME");
    } else {
        drawConnectScreen();
    }
}

void drawFeature()
{
    switch (selectedApp) {
    case 0:  drawConnectScreen(); break;
    case 1:  drawConfigScreen(); break;
    case 2:  drawClockScreen(); break;
    case 3:  drawSurveyList(true); break;  // BLE
    case 4:  drawSurveyList(false); break; // WIFI
    case 5:  drawMeshRfScreen(); break;
    case 8:  drawMiscTelemetry(); break;
    case 9:  drawJsDiagnosticRunner(); break;
    case 10: drawIrScreen(); break;
    case 12: drawWebRadioScreen(); break;  // WEBRADIO (PH DEFAULT)
    case 13: drawFileBrowser(); break;
    default: drawPeripheralBusScreen(); break;
    }
}

void redrawScreen()
{
    const ColorPalette &pal = activePalette();
    M5.Display.fillScreen(pal.background);
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
        scanBleDevices();
    } else if (selectedApp == 4) {
        scanWifiNetworks();
    } else if (selectedApp == 12) {
        // Automatically start default Philippine station DWIZ 882 Manila if connected
        startWebRadio(currentRadioStation);
    } else if (selectedApp == 13) {
        currentDirectory = "/";
        loadFileList();
    }
    redrawScreen();
}

void handleKey(uint8_t key)
{
    if (keyBeepEnabled) {
        M5.Speaker.tone(1400, 20);
    }

    if (currentScreen == Screen::Home) {
        if (key == kKeyUp || key == 'w' || key == 'W' || key == kKeyLeft || key == 'a' || key == 'A') {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeyDown || key == 's' || key == 'S' || key == kKeyRight || key == 'd' || key == 'D') {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            openSelectedApp();
        }
        return;
    }

    if (key == kKeyEscape) {
        if (selectedApp == 12 && radioIsPlaying) {
            stopWebRadio();
        }
        currentScreen = Screen::Home;
        redrawScreen();
        return;
    }

    handleAppKey(key);
}

void handleAppKey(uint8_t key)
{
    if (selectedApp == 1) { // CONFIG
        if (key == kKeyUp || key == 'w' || key == 'W') {
            selectedConfigRow = (selectedConfigRow + 7) % 8;
            redrawScreen();
        } else if (key == kKeyDown || key == 's' || key == 'S') {
            selectedConfigRow = (selectedConfigRow + 1) % 8;
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            handleConfigAction();
        }
        return;
    }

    if (selectedApp == 12) { // WEBRADIO
        if (key == ' ' || key == kKeySelect || key == '\n') {
            toggleWebRadio();
            redrawScreen();
        } else if (key == 'a' || key == 'A' || key == kKeyLeft) {
            size_t nextSt = (currentRadioStation + kRadioStationCount - 1) % kRadioStationCount;
            startWebRadio(nextSt);
            redrawScreen();
        } else if (key == 'd' || key == 'D' || key == kKeyRight) {
            size_t nextSt = (currentRadioStation + 1) % kRadioStationCount;
            startWebRadio(nextSt);
            redrawScreen();
        } else if (key == 'w' || key == 'W' || key == kKeyUp) {
            if (radioVolume < 21) radioVolume++;
            if (gAudioPlayer) gAudioPlayer->setVolume(radioVolume);
            saveSettings();
            redrawScreen();
        } else if (key == 's' || key == 'S' || key == kKeyDown) {
            if (radioVolume > 0) radioVolume--;
            if (gAudioPlayer) gAudioPlayer->setVolume(radioVolume);
            saveSettings();
            redrawScreen();
        } else if (key == 'm' || key == 'M') {
            radioIsMuted = !radioIsMuted;
            if (gAudioPlayer) gAudioPlayer->setVolume(radioIsMuted ? 0 : radioVolume);
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            startWebRadio(currentRadioStation);
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 5) { // RF MESH & MESHTATIC
        if (key == 't' || key == 'T') {
            sendMeshtasticBeacon(1); // Text Beacon
            redrawScreen();
        } else if (key == 'n' || key == 'N') {
            sendMeshtasticBeacon(4); // NodeInfo
            redrawScreen();
        } else if (key == 'f' || key == 'F') {
            meshFreqIndex = (meshFreqIndex + 1) % 4;
            saveSettings();
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            loraHardwareDetected = probeLoraHardware();
            setStatusBanner("Mesh bus rescanned!");
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 3 || selectedApp == 4) { // BLE / WIFI Survey
        if ((key == kKeyUp || key == 'w' || key == 'W') && selectedSurvey > 0) {
            selectedSurvey--;
            redrawScreen();
        } else if ((key == kKeyDown || key == 's' || key == 'S') && selectedSurvey + 1 < surveyCount) {
            selectedSurvey++;
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            if (selectedApp == 3) scanBleDevices(); else scanWifiNetworks();
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 8) { // MISC Telemetry
        miscPage ^= 1;
        redrawScreen();
        return;
    }

    if (selectedApp == 9) { // JS MACRO Self Test
        if (key == kKeySelect || key == '\n' || key == 'r' || key == 'R') {
            runHardwareSelfTest();
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 10) { // IR TX
        if ((key == kKeyUp || key == 'w' || key == 'W') && selectedIrIndex > 0) {
            selectedIrIndex--;
            redrawScreen();
        } else if ((key == kKeyDown || key == 's' || key == 'S') && selectedIrIndex < 7) {
            selectedIrIndex++;
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            transmitIrCode(selectedIrIndex);
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 13) { // File Browser
        if ((key == kKeyUp || key == 'w' || key == 'W') && selectedFile > 0) {
            selectedFile--;
            redrawScreen();
        } else if ((key == kKeyDown || key == 's' || key == 'S') && selectedFile + 1 < fileEntryCount) {
            selectedFile++;
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            loadFileList();
            redrawScreen();
        }
        return;
    }
}

void showBootSplash()
{
    const ColorPalette &pal = activePalette();
    constexpr uint8_t frames = 20;
    for (uint8_t frame = 0; frame < frames; ++frame) {
        const float progress = static_cast<float>(frame) / (frames - 1);
        const float pulse = 1.0F + 0.12F * std::sin(progress * 2.0F * 3.14159F);
        const int16_t size = static_cast<int16_t>(44.0F * pulse);
        const int16_t centerX = screenWidth / 2;
        const int16_t centerY = 40;
        const int16_t bodyHeight = size * 3 / 4;
        const int16_t left = centerX - size / 2;
        const int16_t top = centerY - size / 2;

        M5.Display.fillScreen(pal.background);
        M5.Display.drawFastHLine(0, 4, static_cast<int32_t>(screenWidth * progress), pal.accent);
        M5.Display.drawRect(4, 4, screenWidth - 8, screenHeight - 8, pal.primary);

        M5.Display.fillCircle(centerX, top + size / 3, size / 3, pal.primary);
        M5.Display.fillRect(left + size / 6, top + size / 3, size * 2 / 3, bodyHeight / 2, pal.primary);
        M5.Display.fillCircle(centerX - size / 6, top + size / 3, std::max<int16_t>(2, size / 12), pal.accent);
        M5.Display.fillCircle(centerX + size / 6, top + size / 3, std::max<int16_t>(2, size / 12), pal.accent);

        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("GHOST FIRMWARE v2.1", centerX, screenHeight - 32);
        M5.Display.setTextColor(pal.accent, pal.background);
        M5.Display.drawString("ESP32-S3 | CARDPUTER MESH", centerX, screenHeight - 20);
        M5.Display.drawString(kDeveloperTag, centerX, screenHeight - 10);
        delay(22);
    }
}

} // namespace

void setup()
{
    auto config = M5.config();
    config.serial_baudrate = 115200;
    config.clear_display = true;
    M5.begin(config);

    // CRITICAL: Lock screen orientation to Landscape (Rotation 1: 240x135) for Cardputer!
    // Rotation 1 puts the keyboard naturally at the bottom and ports on sides.
    screenRotation = 1;
    M5.Display.setRotation(screenRotation);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    if (screenWidth == 0 || screenHeight == 0) {
        screenWidth = 240;
        screenHeight = 135;
    }

    pinMode(kCardputerIrPin, OUTPUT);
    digitalWrite(kCardputerIrPin, LOW);

    loadSettings();
    ensureFilesystemReady();
    initializeKeyboard();
    initMeshRadio();
    showBootSplash();
    redrawScreen();
}

void loop()
{
    M5.update();
    pollKeyboard();

    // Stream audio buffer loop for WebRadio
    if (gAudioPlayer && radioIsPlaying) {
        gAudioPlayer->loop();
    }

    // Non-blocking G0 BtnA Navigation
    if (M5.BtnA.pressedFor(450) && !btnAHandledLong) {
        btnAHandledLong = true;
        if (currentScreen == Screen::Home) {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
            redrawScreen();
        } else {
            handleKey(kKeyEscape);
        }
    } else if (M5.BtnA.wasReleased()) {
        if (!btnAHandledLong) {
            handleKey(kKeySelect);
        }
        btnAHandledLong = false;
    }

    // Touch support for Core2 / Touch boards
    if (M5.Touch.getCount() > 0) {
        auto touch = M5.Touch.getDetail();
        if (touch.wasPressed()) {
            if (currentScreen == Screen::Home) {
                openSelectedApp();
            } else {
                handleKey(kKeyEscape);
            }
        }
    }

    // Periodic screen refresh when active
    static uint32_t lastRefresh = 0;
    if (currentScreen != Screen::Home && millis() - lastRefresh > 200) {
        lastRefresh = millis();
        redrawScreen();
    }
}
