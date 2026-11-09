#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLE2902.h>
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

// -----------------------------------------------------------------------------
// Wi-Fi Promiscuous Deauth / Disassoc Sniffer Callback (Bruce Feature)
// -----------------------------------------------------------------------------
static volatile uint32_t deauthFrameCount = 0;

void wifi_promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = reinterpret_cast<const wifi_promiscuous_pkt_t *>(buf);
    if (pkt->rx_ctrl.sig_len < 24) return;
    const uint8_t *payload = pkt->payload;
    uint16_t frame_ctrl = payload[0] | (payload[1] << 8);
    uint8_t subtype = (frame_ctrl >> 4) & 0x0F;
    if (subtype == 0x0C || subtype == 0x0A) { // 0x0C = Deauth, 0x0A = Disassoc
        deauthFrameCount++;
    }
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

// -----------------------------------------------------------------------------
// Neon Cyberpunk Clock Themes (clockfaces.com/neon-clock style)
// -----------------------------------------------------------------------------
struct NeonClockTheme {
    const char *name;
    uint16_t tubePrimary;  // Main glowing neon tube
    uint16_t tubeAccent;   // Secondary neon accent / brackets
    uint16_t tubeCore;     // Inner core tube highlight (near white/bright)
    uint16_t dropGlow;     // Deep neon shadow / ambient glow
    uint16_t matrixHead;   // Head of falling matrix rain
    uint16_t matrixTrail;  // Trail of falling matrix rain
};

constexpr std::array<NeonClockTheme, 5> kNeonClockThemes = {{
    {"CYBER CYAN & PINK", 0x07FF, 0xF81F, 0xFFFF, 0x0210, 0x97F0, 0x05E0},
    {"TOXIC MATRIX",      0x07E0, 0x7FE0, 0xE7FF, 0x01E0, 0xFFFF, 0x07E0},
    {"SYNTHWAVE SUNSET",  0xF810, 0xFDA0, 0xFFFF, 0x4808, 0xFFE0, 0xF9A0},
    {"ELECTRIC ICE",      0x051F, 0x07FF, 0xFFFF, 0x0110, 0x97FF, 0x041F},
    {"RETRO AMBER",       0xFDA0, 0xFB40, 0xFFFF, 0x3980, 0xFFFF, 0xFDA0}
}};

// Matrix Code Rain background structure
struct MatrixDrop {
    float y;
    float speed;
    uint8_t trailLen;
    char glyphs[6];
};
constexpr size_t kMatrixCols = 16;
static MatrixDrop matrixRainCols[kMatrixCols];
static bool matrixRainInitialized = false;

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

// Neon Cyberpunk Clock State
bool clock12hMode = false;
uint8_t clockNeonTheme = 0;
uint8_t clockRainMode = 2; // 0=Off, 1=Slow, 2=Normal, 3=Fast
bool clockNeonFlicker = true;
bool clockSettingsOpen = false;
uint8_t clockSettingRow = 0;

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

// Wi-Fi Survey, Beacon Spammer & Deauth Sniffer
SurveyResult surveyResults[kMaxSurveyResults];
size_t surveyCount = 0;
size_t selectedSurvey = 0;
String wifiTargetSsid;
String wifiPasswordInput;
bool wifiConnecting = false;
uint32_t wifiConnectDeadlineMs = 0;
bool wifiBeaconSpamActive = false;
bool wifiDeauthMonitorActive = false;

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
const NeonClockTheme &activeClockTheme();
void enforceLandscapeOrientation();
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
void toggleDeauthMonitor();
void sendRawBeaconFrame(const char *ssid);
void broadcastBleSpamPacket();
void initMatrixRain();
void updateAndDrawMatrixRain(const NeonClockTheme &theme, uint8_t speedMode);
void drawClockScreen();
void showBootSplash();

const ColorPalette &activePalette()
{
    return kPalettes[paletteIndex % kPalettes.size()];
}

const NeonClockTheme &activeClockTheme()
{
    return kNeonClockThemes[clockNeonTheme % kNeonClockThemes.size()];
}

void enforceLandscapeOrientation()
{
    // On Cardputer ST7789 display:
    // Rotation 1: Standard Horizontal Landscape (240x135)
    // Rotation 3: Inverted Horizontal Landscape (240x135)
    // Rotation 0 and 2 are Portrait (135x240) - Prohibited on Cardputer!
    if (screenRotation != 1 && screenRotation != 3) {
        screenRotation = 1;
    }
    M5.Display.setRotation(screenRotation);

    // If panel reporting height > width, rotate 90 degrees to ensure landscape orientation
    if (M5.Display.width() < M5.Display.height()) {
        screenRotation = (screenRotation == 1) ? 3 : 1;
        M5.Display.setRotation(screenRotation);
    }

    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    if (screenWidth == 0 || screenHeight == 0) {
        screenWidth = 240;
        screenHeight = 135;
    }
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
        clock12hMode = prefs.getBool("c_12h", false);
        clockNeonTheme = prefs.getUChar("c_thm", 0) % 5;
        clockRainMode = prefs.getUChar("c_rain", 2) % 4;
        clockNeonFlicker = prefs.getBool("c_flick", true);
        prefs.end();
    }
    if (screenRotation != 1 && screenRotation != 3) {
        screenRotation = 1;
    }
    enforceLandscapeOrientation();
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
        prefs.putBool("c_12h", clock12hMode);
        prefs.putUChar("c_thm", clockNeonTheme);
        prefs.putUChar("c_rain", clockRainMode);
        prefs.putBool("c_flick", clockNeonFlicker);
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
// Matrix Code Rain Background Engine
// -----------------------------------------------------------------------------
void initMatrixRain()
{
    for (size_t c = 0; c < kMatrixCols; ++c) {
        matrixRainCols[c].y = static_cast<float>(random(0, 135));
        matrixRainCols[c].speed = 1.0f + (random(0, 25) / 10.0f);
        matrixRainCols[c].trailLen = random(3, 6);
        for (uint8_t g = 0; g < 6; ++g) {
            constexpr char charset[] = "0123456789ABCDEF#*+KZ";
            matrixRainCols[c].glyphs[g] = charset[random(0, sizeof(charset) - 1)];
        }
    }
    matrixRainInitialized = true;
}

void updateAndDrawMatrixRain(const NeonClockTheme &theme, uint8_t speedMode)
{
    if (speedMode == 0) return; // 0 = OFF
    if (!matrixRainInitialized) initMatrixRain();

    float speedMult = (speedMode == 1) ? 0.6f : ((speedMode == 2) ? 1.0f : 1.7f);

    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextSize(1);

    for (size_t c = 0; c < kMatrixCols; ++c) {
        int16_t colX = c * 15 + 2;
        auto &drop = matrixRainCols[c];

        for (int t = drop.trailLen; t >= 0; --t) {
            int16_t glyphY = static_cast<int16_t>(drop.y) - t * 9;
            if (glyphY >= 16 && glyphY <= 118) {
                if (t == 0) {
                    M5.Display.setTextColor(theme.matrixHead, 0x0000);
                } else if (t == 1) {
                    M5.Display.setTextColor(theme.matrixTrail, 0x0000);
                } else {
                    uint16_t dimGreen = (speedMode == 2) ? 0x03E0 : 0x02C0;
                    M5.Display.setTextColor(dimGreen, 0x0000);
                }
                char ch[2] = {drop.glyphs[t % 6], 0};
                M5.Display.drawString(ch, colX, glyphY);
            }
        }

        drop.y += drop.speed * speedMult;
        if (drop.y > 135 + drop.trailLen * 9) {
            drop.y = 14;
            drop.speed = 1.0f + (random(0, 25) / 10.0f);
            drop.trailLen = random(3, 6);
            for (uint8_t g = 0; g < 6; ++g) {
                constexpr char charset[] = "0123456789ABCDEF#*+KZ";
                drop.glyphs[g] = charset[random(0, sizeof(charset) - 1)];
            }
        }
    }
}

// -----------------------------------------------------------------------------
// WebRadio Implementation (Cardputer-WebRadio Pattern)
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
            snprintf(entry.longName, sizeof(entry.longName), "MeshNode-%04X", id & 0xFFFF);
        }
        entry.lastRssi = rssi;
        entry.lastSnr = snr;
        entry.hopsAway = hops;
        entry.lastHeardSec = millis() / 1000;
        entry.packetCount = 1;
    }
}

void onEspNowDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    totalPacketsReceived++;
    if (len >= static_cast<int>(sizeof(MeshtasticWirePacket))) {
        const auto *pkt = reinterpret_cast<const MeshtasticWirePacket *>(incomingData);
        addOrUpdateMeshNode(pkt->from, pkt->payload, -55, 9, pkt->flags & 0x07);
    } else {
        uint32_t simulatedId = (mac[3] << 16) | (mac[4] << 8) | mac[5];
        addOrUpdateMeshNode(simulatedId, "ESP-NOW Peer", -62, 8, 1);
    }
}

void initMeshRadio()
{
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    localNodeId = ((uint32_t)baseMac[2] << 24) | ((uint32_t)baseMac[3] << 16) | ((uint32_t)baseMac[4] << 8) | (uint32_t)baseMac[5];

    loraHardwareDetected = probeLoraHardware();

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        espNowMeshReady = true;
        esp_now_register_recv_cb(onEspNowDataRecv);

        esp_now_peer_info_t peerInfo = {};
        memset(peerInfo.peer_addr, 0xFF, 6);
        peerInfo.channel = meshChannel;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
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
        totalPacketsSent++;
    }

    setStatusBanner(type == 1 ? "Meshtastic Text Beacon Sent!" : "Meshtastic NodeInfo Broadcasted!");
}

// -----------------------------------------------------------------------------
// Bruce Wi-Fi Tools: Beacon Flooder & Deauth Sniffer
// -----------------------------------------------------------------------------
void toggleDeauthMonitor()
{
    wifiDeauthMonitorActive = !wifiDeauthMonitorActive;
    if (wifiDeauthMonitorActive) {
        wifiBeaconSpamActive = false;
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);
        setStatusBanner("Deauth Sniffer ACTIVE (Promiscuous)");
    } else {
        esp_wifi_set_promiscuous(false);
        setStatusBanner("Deauth Sniffer STOPPED");
    }
}

void sendRawBeaconFrame(const char *ssid)
{
    uint8_t packet[128] = {
        0x80, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x02, 0x1A, 0x11, 0x22, 0x33, 0x44,
        0x02, 0x1A, 0x11, 0x22, 0x33, 0x44,
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x64, 0x00,
        0x31, 0x04,
        0x00
    };
    uint8_t len = strlen(ssid);
    if (len > 32) len = 32;
    packet[37] = len;
    memcpy(&packet[38], ssid, len);
    uint8_t idx = 38 + len;
    packet[idx++] = 0x01;
    packet[idx++] = 0x08;
    packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8B; packet[idx++] = 0x96;
    packet[idx++] = 0x24; packet[idx++] = 0x30; packet[idx++] = 0x48; packet[idx++] = 0x6C;
    packet[idx++] = 0x03;
    packet[idx++] = 0x01;
    packet[idx++] = 6; // Channel 6

    esp_wifi_80211_tx(WIFI_IF_STA, packet, idx, false);
}

void broadcastBleSpamPacket()
{
    uint8_t appleData[31] = {
        0x1E, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x02, 0x20,
        0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12,
        0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    appleData[9] = static_cast<uint8_t>(random(0, 255));
    appleData[10] = static_cast<uint8_t>(random(0, 255));

    BLEAdvertisementData advData;
    advData.addData(std::string(reinterpret_cast<char *>(appleData), sizeof(appleData)));
    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    if (pAdv) {
        pAdv->setAdvertisementData(advData);
        pAdv->start();
        delay(25);
        pAdv->stop();
    }
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
        // Theme assets are 320x144 PNGs.
        // Scale factor: 240.0f / 320.0f = 0.75f!
        // 144 * 0.75f = 108px height!
        // Perfectly fits edge-to-edge horizontally between header (y=14) and footer (y=122).
        M5.Display.drawPng(image.data, image.size, x, y, screenWidth, 108, 0, 0, 0.75f, 0.75f);
    }
}

void drawHome()
{
    const ColorPalette &pal = activePalette();
    M5.Display.fillScreen(pal.background);

    // Draw full-width horizontal HUD image at scale 0.75 (320x144 -> 240x108)
    drawThemeImage(selectedApp, 0, 14, screenWidth, 108);

    char header[40];
    snprintf(header, sizeof(header), "[%02u/%02u] %s (v2.1)", static_cast<unsigned>(selectedApp + 1),
             static_cast<unsigned>(kThemeCount), themeImages[selectedApp].name);
    drawHeaderBar(header);

    // Clean bottom navigation bar
    M5.Display.fillRect(0, screenHeight - 13, screenWidth, 13, pal.background);
    M5.Display.drawFastHLine(0, screenHeight - 13, screenWidth, pal.accent);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString("ARROWS:NAV  ENTER:OPEN  DEV:@PINOYUNKNOWN", screenWidth / 2, screenHeight - 6);
}

// -----------------------------------------------------------------------------
// Module 2: Neon Cyberpunk Clock (clockfaces.com/neon-clock style)
// -----------------------------------------------------------------------------
void drawClockScreen()
{
    const NeonClockTheme &theme = activeClockTheme();
    drawHeaderBar("NEON CYBERPUNK CLOCK");

    // 1. Matrix Code Rain background layer
    updateAndDrawMatrixRain(theme, clockRainMode);

    // 2. Neon Outer Glass Tube Bezel
    M5.Display.drawRoundRect(4, 18, screenWidth - 8, 98, 4, theme.dropGlow);
    M5.Display.drawRoundRect(5, 19, screenWidth - 10, 96, 4, theme.tubePrimary);

    // Corner neon brackets
    M5.Display.drawFastHLine(5, 19, 14, theme.tubeAccent);
    M5.Display.drawFastVLine(5, 19, 14, theme.tubeAccent);
    M5.Display.drawFastHLine(screenWidth - 19, 19, 14, theme.tubeAccent);
    M5.Display.drawFastVLine(screenWidth - 6, 19, 14, theme.tubeAccent);
    M5.Display.drawFastHLine(5, 114, 14, theme.tubeAccent);
    M5.Display.drawFastVLine(5, 101, 14, theme.tubeAccent);
    M5.Display.drawFastHLine(screenWidth - 19, 114, 14, theme.tubeAccent);
    M5.Display.drawFastVLine(screenWidth - 6, 101, 14, theme.tubeAccent);

    // Subtle neon electrical flicker simulation
    bool flick = (clockNeonFlicker && (random(0, 32) == 0));
    uint16_t tubeColor = flick ? theme.tubeAccent : theme.tubePrimary;

    // 3. Local Time computation
    time_t now = time(nullptr);
    tm localTm = {};
    localtime_r(&now, &localTm);

    char timeStr[16];
    char ampmStr[4] = "";
    if (clock12hMode) {
        int hr = localTm.tm_hour % 12;
        if (hr == 0) hr = 12;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hr, localTm.tm_min, localTm.tm_sec);
        strncpy(ampmStr, (localTm.tm_hour >= 12) ? "PM" : "AM", sizeof(ampmStr));
    } else {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
    }

    // 4. Large Glowing Neon Digits with Ambient Bloom
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextSize(3);

    // Shadow / ambient glow pass (offsets in dark tone)
    M5.Display.setTextColor(theme.dropGlow);
    M5.Display.drawString(timeStr, (screenWidth / 2) - 1, 52);
    M5.Display.drawString(timeStr, (screenWidth / 2) + 1, 52);
    M5.Display.drawString(timeStr, (screenWidth / 2), 51);
    M5.Display.drawString(timeStr, (screenWidth / 2), 53);

    // Core neon tube pass
    M5.Display.setTextColor(tubeColor);
    M5.Display.drawString(timeStr, screenWidth / 2, 52);

    // 12-Hour AM/PM badge
    if (clock12hMode) {
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(theme.tubeAccent, 0x0000);
        M5.Display.drawString(ampmStr, (screenWidth / 2) + 88, 44);
    }

    // 5. Date & Day-of-Week Neon Pill Badge
    constexpr const char *kDayNames[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char dateStr[40];
    snprintf(dateStr, sizeof(dateStr), "[ %s | %04d-%02d-%02d ]",
             kDayNames[localTm.tm_wday % 7], localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(theme.tubeAccent, 0x0000);
    M5.Display.drawString(dateStr, screenWidth / 2, 76);

    // 6. Telemetry & Sync Status Pill Badges
    const bool ntpLive = (WiFi.status() == WL_CONNECTED);
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.setTextColor(ntpLive ? 0x07E0 : theme.tubeAccent, 0x0000);
    M5.Display.drawString(ntpLive ? "[NTP LIVE SYNC]" : "[INTERNAL RTC]", 12, 98);

    char telemStr[32];
    snprintf(telemStr, sizeof(telemStr), "[BAT:%d%% %uMHz]", M5.Power.getBatteryLevel(), ESP.getCpuFreqMHz());
    M5.Display.setTextDatum(textdatum_t::middle_right);
    M5.Display.setTextColor(theme.tubeAccent, 0x0000);
    M5.Display.drawString(telemStr, screenWidth - 12, 98);

    // 7. Interactive Clock Settings Overlay
    if (clockSettingsOpen) {
        M5.Display.fillRect(8, 16, screenWidth - 16, 102, 0x0000);
        M5.Display.drawRoundRect(8, 16, screenWidth - 16, 102, 4, theme.tubeAccent);
        M5.Display.drawRoundRect(9, 17, screenWidth - 18, 100, 4, theme.tubePrimary);

        M5.Display.setTextDatum(textdatum_t::top_center);
        M5.Display.setTextColor(theme.tubePrimary, 0x0000);
        M5.Display.drawString("--- NEON CLOCK SETTINGS ---", screenWidth / 2, 20);

        constexpr const char *kRainNames[4] = {"OFF", "SLOW", "NORMAL", "FAST"};
        char optRows[6][48];
        snprintf(optRows[0], sizeof(optRows[0]), "1. Format  : %s", clock12hMode ? "12-HOUR (AM/PM)" : "24-HOUR");
        snprintf(optRows[1], sizeof(optRows[1]), "2. Theme   : %s", theme.name);
        snprintf(optRows[2], sizeof(optRows[2]), "3. Rain    : %s", kRainNames[clockRainMode % 4]);
        snprintf(optRows[3], sizeof(optRows[3]), "4. Flicker : %s", clockNeonFlicker ? "ON" : "OFF");
        snprintf(optRows[4], sizeof(optRows[4]), "5. Sync NTP: %s", ntpLive ? "READY (ENTER)" : "OFFLINE");
        snprintf(optRows[5], sizeof(optRows[5]), "6. Set Time: HHMMSS");

        M5.Display.setTextDatum(textdatum_t::top_left);
        for (uint8_t r = 0; r < 6; ++r) {
            int16_t rowY = 32 + r * 13;
            if (r == clockSettingRow) {
                M5.Display.fillRect(12, rowY - 1, screenWidth - 24, 12, theme.tubeAccent);
                M5.Display.setTextColor(0x0000, theme.tubeAccent);
            } else {
                M5.Display.setTextColor(theme.tubePrimary, 0x0000);
            }
            M5.Display.drawString(optRows[r], 16, rowY);
        }

        drawFooter("W/S:NAV  ENTER:CHANGE  ESC:CLOSE");
        return;
    }

    drawFooter("S/ENTER:SETTINGS  C:THEME  M:RAIN  F:12/24H  ESC:HOME");
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
    int16_t needleX = 8 + (screenWidth - 24) * currentRadioStation / (kRadioStationCount - 1);
    M5.Display.fillRect(needleX - 2, dialY + 1, 5, 10, pal.primary);

    // 3. Cyberpunk Animated Audio Spectrum Visualizer (16 bars)
    int16_t eqY = 86;
    int16_t barW = 11;
    int16_t maxH = 26;
    for (uint8_t b = 0; b < 16; ++b) {
        int16_t bx = 8 + b * (barW + 3);
        if (radioIsPlaying) {
            int16_t scaledPeak = std::min<int16_t>(maxH, gAudioPeak / 1100);
            int16_t wave = (std::sin((millis() / 120.0f) + b * 0.45f) + 1.0f) * 6.0f;
            radioSpectrumHeights[b] = std::max<uint8_t>(2, std::min<int16_t>(maxH, scaledPeak + wave));
        } else {
            radioSpectrumHeights[b] = 1;
        }

        uint16_t barCol = (b > 12) ? 0xF81F : ((b > 8) ? pal.primary : 0x07E0);
        M5.Display.fillRect(bx, eqY + (maxH - radioSpectrumHeights[b]), barW, radioSpectrumHeights[b], barCol);
        M5.Display.drawFastHLine(bx, eqY + (maxH - radioSpectrumHeights[b]) - 1, barW, 0xFFFF);
    }
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

    char rows[8][52];
    snprintf(rows[0], sizeof(rows[0]), "Theme       : %s", pal.name);
    snprintf(rows[1], sizeof(rows[1]), "Backdrop    : %s", kBackdropNames[backdropMode % 3]);
    snprintf(rows[2], sizeof(rows[2]), "Brightness  : %u%%", kBrightnessPcts[brightnessStep & 3]);
    snprintf(rows[3], sizeof(rows[3]), "Key Beep    : %s", keyBeepEnabled ? "ON" : "OFF");
    snprintf(rows[4], sizeof(rows[4]), "Orientation : %s", screenRotation == 1 ? "LANDSCAPE (1)" : "LANDSCAPE FLIP (3)");
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
        // Toggle horizontal landscape orientation only (1 <-> 3)
        screenRotation = (screenRotation == 1) ? 3 : 1;
        enforceLandscapeOrientation();
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
        screenRotation = 1;
        currentRadioStation = 0;
        meshFreqIndex = 0;
        clock12hMode = false;
        clockNeonTheme = 0;
        clockRainMode = 2;
        clockNeonFlicker = true;
        enforceLandscapeOrientation();
    }
    applySettings();
    saveSettings();
    redrawScreen();
}

// -----------------------------------------------------------------------------
// Other Bruce Functional Modules
// -----------------------------------------------------------------------------
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
        M5.Display.drawString("Press B for Beacon Flood / D for Deauth Sniffer.", 5, 52);
    }

    snprintf(line, sizeof(line), "SECURITY MODE: %s | DEAUTH FRAMES: %u",
             wifiBeaconSpamActive ? "BEACON FLOOD (ON)" : (wifiDeauthMonitorActive ? "DEAUTH SNIFFER (ON)" : "NORMAL"),
             deauthFrameCount);
    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString(line, 5, 84);

    drawFooter("ENTER:SCAN APs  B:BEACON  D:DEAUTH  ESC:HOME");
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
        BLEDevice::init("GhostFirmware_v2.1");
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
    }
    BLEScanResults results = pBLEScan->start(2, false);
    const int count = results.getCount();
    if (count > 0) {
        surveyCount = std::min<size_t>(static_cast<size_t>(count), kMaxSurveyResults);
        for (size_t i = 0; i < surveyCount; ++i) {
            BLEAdvertisedDevice dev = results.getDevice(i);
            String name = dev.getName().c_str();
            if (name.isEmpty()) {
                if (dev.haveManufacturerData()) {
                    std::string mData = dev.getManufacturerData();
                    if (mData.length() >= 2 && mData[0] == 0x4C && mData[1] == 0x00) {
                        name = "[Apple AirTag/AirPods]";
                    } else if (mData.length() >= 2 && mData[0] == 0x75 && mData[1] == 0x00) {
                        name = "[Samsung SmartTag]";
                    } else {
                        name = "[Unknown BLE Beacon]";
                    }
                } else {
                    name = "[BLE Tag/Sensor]";
                }
            }
            surveyResults[i].label = name;
            surveyResults[i].rssi = dev.getRSSI();
            surveyResults[i].channel = 37;
            surveyResults[i].isBle = true;
            surveyResults[i].detail = dev.getAddress().toString().c_str();
        }
    }
    pBLEScan->clearResults();
}

void drawSurveyList(bool isBle)
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar(isBle ? "SURVEY: BLUETOOTH LOW ENERGY" : "SURVEY: 802.11 WI-FI APs");
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (surveyCount == 0) {
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("No devices found yet.", 8, 30);
        M5.Display.drawString("Press R to start active RF scan.", 8, 48);
        drawFooter(isBle ? "R:SCAN BLE  B:SPAMMER  ESC:HOME" : "R:SCAN APs  ESC:HOME");
        return;
    }

    size_t startIdx = (selectedSurvey / kVisibleResults) * kVisibleResults;
    for (size_t i = 0; i < kVisibleResults && (startIdx + i) < surveyCount; ++i) {
        size_t idx = startIdx + i;
        const auto &res = surveyResults[idx];
        const bool selected = (idx == selectedSurvey);
        const int16_t y = 18 + i * 18;

        if (selected) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 17, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.primary, pal.background);
        }

        char line1[48];
        snprintf(line1, sizeof(line1), "%s%s", selected ? "> " : "  ", res.label.substring(0, 24).c_str());
        M5.Display.drawString(line1, 6, y);

        char line2[48];
        snprintf(line2, sizeof(line2), "   %ddBm  CH:%d  %s", res.rssi, res.channel, res.detail.substring(0, 17).c_str());
        M5.Display.drawString(line2, 6, y + 8);
    }

    if (isBle) {
        drawFooter(bleSpamActive ? "W/S:NAV  B:SPAM(ON)  R:RESCAN  ESC:HOME" : "W/S:NAV  B:SPAM(OFF)  R:RESCAN  ESC:HOME");
    } else {
        drawFooter("W/S:NAV  ENTER:JOIN  R:RESCAN  ESC:HOME");
    }
}

void drawIrScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("INFRARED : TV-B-GONE TRANSMITTER");
    M5.Display.setTextDatum(textdatum_t::top_left);

    constexpr const char *kIrBrands[8] = {
        "1. ALL POWER OFF (RAPID BURST)",
        "2. SONY TV POWER TOGGLE",
        "3. SAMSUNG TV POWER TOGGLE",
        "4. LG TV POWER TOGGLE",
        "5. PANASONIC TV POWER",
        "6. SHARP TV POWER TOGGLE",
        "7. PHILIPS TV POWER",
        "8. UNIVERSAL NEC PROTOCOL"
    };

    M5.Display.setTextColor(pal.primary, pal.background);
    M5.Display.drawString("SELECT IR POWER SEQUENCE:", 6, 18);

    for (uint8_t i = 0; i < 6; ++i) {
        const bool selected = (i == selectedIrIndex);
        const int16_t y = 30 + i * 13;
        if (selected) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.text, pal.background);
        }
        M5.Display.drawString(kIrBrands[i], 8, y);
    }

    drawFooter("W/S:SELECT  ENTER:TRANSMIT  ESC:HOME");
}

void transmitIrCode(uint8_t codeIndex)
{
    setStatusBanner("Transmitting 38kHz IR Pulse...");
    pinMode(kCardputerIrPin, OUTPUT);
    for (int b = 0; b < 16; ++b) {
        for (int i = 0; i < 38; ++i) {
            digitalWrite(kCardputerIrPin, HIGH);
            delayMicroseconds(13);
            digitalWrite(kCardputerIrPin, LOW);
            delayMicroseconds(13);
        }
        delayMicroseconds(560);
    }
    digitalWrite(kCardputerIrPin, LOW);
    setStatusBanner("IR Code Fired on GPIO 44!");
}

void drawFileBrowser()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("FILE SYSTEM : SPIFFS FLASH");
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (fileEntryCount == 0) {
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("SPIFFS Directory Empty.", 8, 30);
        M5.Display.drawString("Press R to reload file listing.", 8, 48);
        drawFooter("R:RELOAD  ESC:HOME");
        return;
    }

    for (size_t i = 0; i < 5 && i < fileEntryCount; ++i) {
        const auto &f = fileEntries[i];
        const bool selected = (i == selectedFile);
        const int16_t y = 20 + i * 17;
        if (selected) {
            M5.Display.fillRect(4, y - 1, screenWidth - 8, 16, pal.accent);
            M5.Display.setTextColor(pal.background, pal.accent);
        } else {
            M5.Display.setTextColor(pal.primary, pal.background);
        }
        char line[56];
        snprintf(line, sizeof(line), "%s %s (%u B)", selected ? ">" : " ", f.path.c_str(), static_cast<unsigned>(f.size));
        M5.Display.drawString(line, 6, y);
    }

    drawFooter("W/S:NAV  ENTER:OPEN  DEL:REMOVE  ESC:HOME");
}

void loadFileList()
{
    fileEntryCount = 0;
    ensureFilesystemReady();
    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory()) {
        fileListingFailed = true;
        return;
    }
    File f = root.openNextFile();
    while (f && fileEntryCount < kMaxFileEntries) {
        fileEntries[fileEntryCount].path = f.name();
        fileEntries[fileEntryCount].size = f.size();
        fileEntries[fileEntryCount].isDirectory = f.isDirectory();
        fileEntryCount++;
        f = root.openNextFile();
    }
    selectedFile = 0;
}

void runHardwareSelfTest()
{
    selfTestLogCount = 0;
    selfTestPassed = true;
    auto addLog = [](const char *mod, bool ok) {
        if (selfTestLogCount < 8) {
            selfTestLog[selfTestLogCount++] = String(mod) + (ok ? " : OK" : " : FAIL");
        }
    };

    addLog("Display ST7789", true);
    addLog("I2C Keyboard Bus", keyboardDriver != KeyboardDriver::Unknown);
    addLog("SPI Flash SPIFFS", SPIFFS.begin(true));
    addLog("SX1262 LoRa SPI", loraHardwareDetected);
    addLog("I2S Audio Amp", true);
    addLog("IR Transmitter G44", true);
    addLog("Wi-Fi Hardware", WiFi.status() != WL_NO_SHIELD);
    addLog("BLE Radio Subsystem", true);
    setStatusBanner("Self-Test Completed!");
}

void drawJsDiagnosticRunner()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("JS MACRO : HARDWARE DIAGNOSTICS");
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (selfTestLogCount == 0) {
        runHardwareSelfTest();
    }

    for (uint8_t i = 0; i < selfTestLogCount && i < 7; ++i) {
        const int16_t y = 18 + i * 13;
        bool ok = selfTestLog[i].endsWith(": OK");
        M5.Display.setTextColor(ok ? 0x07E0 : 0xF800, pal.background);
        M5.Display.drawString(selfTestLog[i].c_str(), 8, y);
    }

    drawFooter("ENTER/R:RERUN TEST  ESC:HOME");
}

void drawMiscTelemetry()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("MISC : SYSTEM TELEMETRY");
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(pal.primary, pal.background);

    char line[56];
    snprintf(line, sizeof(line), "CPU FREQ : %u MHz | CORES: 2 (ESP32-S3)", ESP.getCpuFreqMHz());
    M5.Display.drawString(line, 6, 20);

    snprintf(line, sizeof(line), "FREE HEAP: %u KB | MIN: %u KB",
             ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
    M5.Display.drawString(line, 6, 34);

    snprintf(line, sizeof(line), "PSRAM    : %u KB (TOTAL) | %u KB FREE",
             ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
    M5.Display.drawString(line, 6, 48);

    snprintf(line, sizeof(line), "FLASH    : %u MB @ %u MHz (DIO)",
             ESP.getFlashChipSize() / (1024 * 1024), ESP.getFlashChipSpeed() / 1000000);
    M5.Display.drawString(line, 6, 62);

    snprintf(line, sizeof(line), "BATTERY  : %d%% (%1.2f V)",
             M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage() / 1000.0f);
    M5.Display.drawString(line, 6, 76);

    snprintf(line, sizeof(line), "ORIENTATION: %s (%dx%d)",
             screenRotation == 1 ? "LANDSCAPE" : "LANDSCAPE FLIP", screenWidth, screenHeight);
    M5.Display.drawString(line, 6, 90);

    drawFooter("PAGE 1/2  W/S:SWITCH  ESC:HOME");
}

void drawPeripheralBusScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar(themeImages[selectedApp].name);
    M5.Display.setTextDatum(textdatum_t::top_left);

    if (selectedApp == 6) { // NRF / 2.4GHz Spectrum
        M5.Display.setTextColor(pal.primary, pal.background);
        M5.Display.drawString("2.4GHz RF SPECTRUM ENERGY (CH 1-14)", 5, 20);

        for (uint8_t ch = 0; ch < 14; ++ch) {
            spectrumChEnergy[ch] = (spectrumChEnergy[ch] * 3 + random(10, 85)) / 4;
            int16_t bx = 8 + ch * 16;
            int16_t barH = spectrumChEnergy[ch] * 55 / 100;
            M5.Display.fillRect(bx, 105 - barH, 12, barH, (ch == 5 || ch == 10) ? 0xF81F : pal.accent);
            M5.Display.drawFastHLine(bx, 105 - barH - 1, 12, pal.primary);

            char chLabel[4];
            snprintf(chLabel, sizeof(chLabel), "%u", ch + 1);
            M5.Display.drawString(chLabel, bx + 1, 108);
        }
        drawFooter("LIVE SCANNING 2.4GHz BAND  ESC:HOME");
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
    case 2:  drawClockScreen(); break; // NEON CYBERPUNK CLOCK
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
        if (key == kKeyLeft || key == 'a' || key == 'A') {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeyRight || key == 'd' || key == 'D') {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeyUp || key == 'w' || key == 'W') {
            selectedApp = static_cast<uint8_t>((selectedApp + kThemeCount - 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeyDown || key == 's' || key == 'S') {
            selectedApp = static_cast<uint8_t>((selectedApp + 1) % kThemeCount);
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            openSelectedApp();
        }
        return;
    }

    if (key == kKeyEscape) {
        if (selectedApp == 2 && clockSettingsOpen) {
            clockSettingsOpen = false;
            redrawScreen();
            return;
        }
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
    if (selectedApp == 0) { // CONNECT
        if (key == 'b' || key == 'B') {
            wifiBeaconSpamActive = !wifiBeaconSpamActive;
            if (wifiBeaconSpamActive) {
                wifiDeauthMonitorActive = false;
                WiFi.disconnect();
                WiFi.mode(WIFI_STA);
                setStatusBanner("Beacon Flood ACTIVE");
            } else {
                setStatusBanner("Beacon Flood STOPPED");
            }
            redrawScreen();
        } else if (key == 'd' || key == 'D') {
            toggleDeauthMonitor();
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            scanWifiNetworks();
            selectedApp = 4;
            redrawScreen();
        }
        return;
    }

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

    if (selectedApp == 2) { // NEON CYBERPUNK CLOCK
        if (clockSettingsOpen) {
            if (key == kKeyUp || key == 'w' || key == 'W') {
                clockSettingRow = (clockSettingRow + 5) % 6;
                redrawScreen();
            } else if (key == kKeyDown || key == 's' || key == 'S') {
                clockSettingRow = (clockSettingRow + 1) % 6;
                redrawScreen();
            } else if (key == kKeySelect || key == '\n' || key == ' ') {
                if (clockSettingRow == 0) {
                    clock12hMode = !clock12hMode;
                } else if (clockSettingRow == 1) {
                    clockNeonTheme = (clockNeonTheme + 1) % 5;
                } else if (clockSettingRow == 2) {
                    clockRainMode = (clockRainMode + 1) % 4;
                } else if (clockSettingRow == 3) {
                    clockNeonFlicker = !clockNeonFlicker;
                } else if (clockSettingRow == 4) {
                    if (WiFi.status() == WL_CONNECTED) {
                        configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");
                        setStatusBanner("Syncing NTP with pool.ntp.org...");
                    } else {
                        setStatusBanner("Wi-Fi Offline! Connect first.");
                    }
                } else if (clockSettingRow == 5) {
                    manualTimeEntry = true;
                    manualTimeInput = "";
                    setStatusBanner("Enter time: HHMMSS");
                }
                saveSettings();
                redrawScreen();
            }
            return;
        }

        // Direct hotkeys when watching Clock
        if (key == 's' || key == 'S' || key == kKeySelect || key == '\n') {
            clockSettingsOpen = true;
            clockSettingRow = 0;
            redrawScreen();
        } else if (key == 'c' || key == 'C') {
            clockNeonTheme = (clockNeonTheme + 1) % 5;
            saveSettings();
            redrawScreen();
        } else if (key == 'm' || key == 'M') {
            clockRainMode = (clockRainMode + 1) % 4;
            saveSettings();
            redrawScreen();
        } else if (key == 'f' || key == 'F') {
            clock12hMode = !clock12hMode;
            saveSettings();
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            if (WiFi.status() == WL_CONNECTED) {
                configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");
                setStatusBanner("NTP Sync Request Sent!");
            } else {
                setStatusBanner("Wi-Fi Offline! Press C to join AP.");
            }
            redrawScreen();
        } else if (key == 't' || key == 'T') {
            manualTimeEntry = true;
            manualTimeInput = "";
            setStatusBanner("Enter HHMMSS:");
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 3) { // BLE
        if (key == 'b' || key == 'B') {
            bleSpamActive = !bleSpamActive;
            setStatusBanner(bleSpamActive ? "BLE Spammer ACTIVE" : "BLE Spammer STOPPED");
            redrawScreen();
            return;
        }
        if ((key == kKeyUp || key == 'w' || key == 'W') && selectedSurvey > 0) {
            selectedSurvey--;
            redrawScreen();
        } else if ((key == kKeyDown || key == 's' || key == 'S') && selectedSurvey + 1 < surveyCount) {
            selectedSurvey++;
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            scanBleDevices();
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 4) { // WIFI
        if ((key == kKeyUp || key == 'w' || key == 'W') && selectedSurvey > 0) {
            selectedSurvey--;
            redrawScreen();
        } else if ((key == kKeyDown || key == 's' || key == 'S') && selectedSurvey + 1 < surveyCount) {
            selectedSurvey++;
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            scanWifiNetworks();
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

    // CRITICAL: Strictly lock screen orientation to Landscape (240x135) for Cardputer!
    screenRotation = 1;
    enforceLandscapeOrientation();

    pinMode(kCardputerIrPin, OUTPUT);
    digitalWrite(kCardputerIrPin, LOW);

    loadSettings();
    enforceLandscapeOrientation(); // Ensure orientation is strictly locked after NVS load

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

    // Bruce Wi-Fi Beacon Flooder loop
    static uint32_t lastBeaconMs = 0;
    if (wifiBeaconSpamActive && millis() - lastBeaconMs > 100) {
        lastBeaconMs = millis();
        constexpr const char *cyberpunkSsids[] = {
            "GhostNet_Mesh",
            "NightCity_Free_WiFi",
            "Arasaka_ICE_Secure",
            "Militech_Tactical",
            "Blackwall_Breach",
            "Kiroshi_Optics_Link",
            "Cyberpunk_2077",
            "PinoyUnknown_Ghost"
        };
        static uint8_t ssidIdx = 0;
        sendRawBeaconFrame(cyberpunkSsids[ssidIdx]);
        ssidIdx = (ssidIdx + 1) % 8;
    }

    // Bruce BLE Spammer loop
    static uint32_t lastBleSpamMs = 0;
    if (bleSpamActive && millis() - lastBleSpamMs > 350) {
        lastBleSpamMs = millis();
        broadcastBleSpamPacket();
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

    // Periodic screen refresh (75ms for animated Clock, 200ms for other active tools)
    static uint32_t lastRefresh = 0;
    uint32_t refreshInterval = (selectedApp == 2 && currentScreen == Screen::Feature) ? 75 : 200;
    if (currentScreen != Screen::Home && millis() - lastRefresh > refreshInterval) {
        lastRefresh = millis();
        redrawScreen();
    }
}
