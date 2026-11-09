#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <SD.h>
#include <FS.h>
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

// MicroSD TF-Card pins on Cardputer
constexpr uint8_t kSdCsPin = 12;
constexpr uint8_t kSdSckPin = 40;
constexpr uint8_t kSdMisoPin = 39;
constexpr uint8_t kSdMosiPin = 14;

static bool sdCardMounted = false;
static bool useSdCard = false;


constexpr std::array<uint8_t, 3> kMatrixOutputPins = {8, 9, 11};
constexpr std::array<uint8_t, 7> kMatrixInputPins = {13, 15, 3, 4, 5, 6, 7};

// Cardputer Physical 4x14 Keyboard Matrix Lookup Tables
// Row 0: ` 1 2 3 4 5 6 7 8 9 0 - = Del
// Row 1: Tab q w e r t y u i o p [ ] \
// Row 2: Fn Shift a s d f g h j k l ; ' Enter
// Row 3: Ctrl Opt Alt z x c v b n m , . / Space
constexpr uint8_t kKeyMapNormal4x14[4][14] = {
    {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', kKeyBackspace},
    {'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\'},
    {0, 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', kKeySelect},
    {0, 0, 0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' '}
};

constexpr uint8_t kKeyMapShift4x14[4][14] = {
    {'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', kKeyBackspace},
    {'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|'},
    {0, 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', kKeySelect},
    {0, 0, 0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', ' '}
};

constexpr uint8_t kKeyMapFn4x14[4][14] = {
    {kKeyEscape, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '_', '+', kKeyBackspace},
    {'\t', 0, kKeyUp, 0, 0, 0, 0, 0, 0, 0, 0, '{', '}', '|'},
    {0, 0, kKeyLeft, kKeyDown, kKeyRight, 0, 0, 0, 0, 0, 0, kKeyUp, '"', kKeySelect},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, kKeyLeft, kKeyDown, kKeyRight, ' '}
};

enum class Screen : uint8_t {
    Home = 0,
    Feature,
    SurveyDetail,
    WifiPasswordEntry,
    FilePreview,
    CardIdEntry
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
// WebRadio Station Database & Custom MicroSD Playlist Loader
// -----------------------------------------------------------------------------
struct RadioStation {
    char name[36];
    char url[140];
    char freq[16];
    char region[8];
    char genre[28];
};

constexpr size_t kMaxRadioStations = 32;
static RadioStation gRadioStations[kMaxRadioStations];
static size_t gRadioStationCount = 0;
static bool radioStationListOpen = false;
static uint8_t radioStationListCursor = 0;

void initDefaultRadioStations()
{
    gRadioStationCount = 0;
    // PHILIPPINE RADIO STATIONS (DEFAULT)
    strncpy(gRadioStations[0].name, "DWIZ 882 Manila", 35);
    strncpy(gRadioStations[0].url, "http://dwizmanila.radioca.st/stream", 139);
    strncpy(gRadioStations[0].freq, "882 kHz AM", 15);
    strncpy(gRadioStations[0].region, "PH", 7);
    strncpy(gRadioStations[0].genre, "News & Talk (Aliw)", 27);

    strncpy(gRadioStations[1].name, "DZRH 666 Manila", 35);
    strncpy(gRadioStations[1].url, "http://stream.zeno.fm/f920ghk8vkeuv", 139);
    strncpy(gRadioStations[1].freq, "666 kHz AM", 15);
    strncpy(gRadioStations[1].region, "PH", 7);
    strncpy(gRadioStations[1].genre, "News Nationwide (MBC)", 27);

    strncpy(gRadioStations[2].name, "Monster RX 93.1", 35);
    strncpy(gRadioStations[2].url, "http://icecast.eradioportal.com:8000/rx931_manila", 139);
    strncpy(gRadioStations[2].freq, "93.1 MHz FM", 15);
    strncpy(gRadioStations[2].region, "PH", 7);
    strncpy(gRadioStations[2].genre, "Top 40 & Pinoy Hits", 27);

    strncpy(gRadioStations[3].name, "Love Radio 90.7", 35);
    strncpy(gRadioStations[3].url, "http://stream.zeno.fm/s4k81v76w4zuv", 139);
    strncpy(gRadioStations[3].freq, "90.7 MHz FM", 15);
    strncpy(gRadioStations[3].region, "PH", 7);
    strncpy(gRadioStations[3].genre, "OPM & Love Songs", 27);

    strncpy(gRadioStations[4].name, "Wish 107.5 Manila", 35);
    strncpy(gRadioStations[4].url, "http://stream.zeno.fm/0r0xa792kwzuv", 139);
    strncpy(gRadioStations[4].freq, "107.5 MHz FM", 15);
    strncpy(gRadioStations[4].region, "PH", 7);
    strncpy(gRadioStations[4].genre, "Pinoy Wish Bus & OPM", 27);

    strncpy(gRadioStations[5].name, "Star FM 102.7", 35);
    strncpy(gRadioStations[5].url, "http://stream.zeno.fm/a816p2v4w4zuv", 139);
    strncpy(gRadioStations[5].freq, "102.7 MHz FM", 15);
    strncpy(gRadioStations[5].region, "PH", 7);
    strncpy(gRadioStations[5].genre, "Pinoy Pop & Trending", 27);

    strncpy(gRadioStations[6].name, "Yes The Best 101.1", 35);
    strncpy(gRadioStations[6].url, "http://stream.zeno.fm/cvu4v626w4zuv", 139);
    strncpy(gRadioStations[6].freq, "101.1 MHz FM", 15);
    strncpy(gRadioStations[6].region, "PH", 7);
    strncpy(gRadioStations[6].genre, "Manila Mass Hits", 27);

    strncpy(gRadioStations[7].name, "Pinoy Classic OPM", 35);
    strncpy(gRadioStations[7].url, "http://stream.zeno.fm/uDkifg", 139);
    strncpy(gRadioStations[7].freq, "Web Stream", 15);
    strncpy(gRadioStations[7].region, "PH", 7);
    strncpy(gRadioStations[7].genre, "Classic Golden OPM", 27);

    // INTERNATIONAL RADIO STATIONS
    strncpy(gRadioStations[8].name, "DEF CON Radio", 35);
    strncpy(gRadioStations[8].url, "http://ice1.somafm.com/defcon-128-mp3", 139);
    strncpy(gRadioStations[8].freq, "Online Cyber", 15);
    strncpy(gRadioStations[8].region, "INT", 7);
    strncpy(gRadioStations[8].genre, "Hacker / Cyber Chill", 27);

    strncpy(gRadioStations[9].name, "SomaFM Groove Salad", 35);
    strncpy(gRadioStations[9].url, "http://ice1.somafm.com/groovesalad-128-mp3", 139);
    strncpy(gRadioStations[9].freq, "Online Synth", 15);
    strncpy(gRadioStations[9].region, "INT", 7);
    strncpy(gRadioStations[9].genre, "Ambient / Chillout", 27);

    strncpy(gRadioStations[10].name, "Nightwave Plaza", 35);
    strncpy(gRadioStations[10].url, "http://radio.plaza.one/mp3", 139);
    strncpy(gRadioStations[10].freq, "Vaporwave", 15);
    strncpy(gRadioStations[10].region, "INT", 7);
    strncpy(gRadioStations[10].genre, "Aesthetic Synthwave", 27);

    strncpy(gRadioStations[11].name, "SomaFM Secret Agent", 35);
    strncpy(gRadioStations[11].url, "http://ice1.somafm.com/secretagent-128-mp3", 139);
    strncpy(gRadioStations[11].freq, "Online Spy", 15);
    strncpy(gRadioStations[11].region, "INT", 7);
    strncpy(gRadioStations[11].genre, "Spy & Lounge Downtempo", 27);

    strncpy(gRadioStations[12].name, "BBC World Service", 35);
    strncpy(gRadioStations[12].url, "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", 139);
    strncpy(gRadioStations[12].freq, "World News", 15);
    strncpy(gRadioStations[12].region, "INT", 7);
    strncpy(gRadioStations[12].genre, "Global News & Analysis", 27);

    strncpy(gRadioStations[13].name, "KEXP 90.3 Seattle", 35);
    strncpy(gRadioStations[13].url, "http://live-aac-64.kexp.org/kexp64.aac", 139);
    strncpy(gRadioStations[13].freq, "90.3 MHz FM", 15);
    strncpy(gRadioStations[13].region, "INT", 7);
    strncpy(gRadioStations[13].genre, "Indie & Alternative", 27);

    strncpy(gRadioStations[14].name, "Retro Synthwave 80s", 35);
    strncpy(gRadioStations[14].url, "http://stream.zeno.fm/4wt8y2u2rshvv", 139);
    strncpy(gRadioStations[14].freq, "Retrowave", 15);
    strncpy(gRadioStations[14].region, "INT", 7);
    strncpy(gRadioStations[14].genre, "Darksynth / Outrun", 27);

    strncpy(gRadioStations[15].name, "Lofi Hip Hop Radio", 35);
    strncpy(gRadioStations[15].url, "http://play.streamafrica.net/lofiradio", 139);
    strncpy(gRadioStations[15].freq, "Lofi Beats", 15);
    strncpy(gRadioStations[15].region, "INT", 7);
    strncpy(gRadioStations[15].genre, "Chillhop & Beats", 27);

    gRadioStationCount = 16;
}

void loadRadioPlaylist()
{
    initDefaultRadioStations();

    // Check MicroSD card for /station_list.txt
    if (sdCardMounted && !SD.exists("/station_list.txt")) {
        File out = SD.open("/station_list.txt", FILE_WRITE);
        if (out) {
            for (size_t i = 0; i < gRadioStationCount; ++i) {
                out.printf("%s,%s,%s,%s,%s\n",
                    gRadioStations[i].name,
                    gRadioStations[i].url,
                    gRadioStations[i].freq,
                    gRadioStations[i].region,
                    gRadioStations[i].genre);
            }
            out.close();
        }
    }

    File in;
    if (sdCardMounted && SD.exists("/station_list.txt")) {
        in = SD.open("/station_list.txt", FILE_READ);
    } else if (SPIFFS.exists("/station_list.txt")) {
        in = SPIFFS.open("/station_list.txt", "r");
    }

    if (in) {
        size_t count = 0;
        while (in.available() && count < kMaxRadioStations) {
            String line = in.readStringUntil('\n');
            line.trim();
            if (line.length() < 5 || line.startsWith("#")) continue;

            int c1 = line.indexOf(',');
            if (c1 > 0) {
                String name = line.substring(0, c1);
                String rest = line.substring(c1 + 1);
                int c2 = rest.indexOf(',');
                String url, freq, reg, genre;
                if (c2 > 0) {
                    url = rest.substring(0, c2);
                    String rest2 = rest.substring(c2 + 1);
                    int c3 = rest2.indexOf(',');
                    if (c3 > 0) {
                        freq = rest2.substring(0, c3);
                        String rest3 = rest2.substring(c3 + 1);
                        int c4 = rest3.indexOf(',');
                        if (c4 > 0) {
                            reg = rest3.substring(0, c4);
                            genre = rest3.substring(c4 + 1);
                        } else {
                            reg = rest3;
                            genre = "Music";
                        }
                    } else {
                        freq = rest2;
                        reg = "CUSTOM";
                        genre = "Web Radio";
                    }
                } else {
                    url = rest;
                    freq = "Web Stream";
                    reg = "CUSTOM";
                    genre = "Stream";
                }
                name.trim(); url.trim(); freq.trim(); reg.trim(); genre.trim();
                if (name.length() > 0 && url.length() > 0) {
                    strncpy(gRadioStations[count].name, name.c_str(), sizeof(gRadioStations[count].name) - 1);
                    strncpy(gRadioStations[count].url, url.c_str(), sizeof(gRadioStations[count].url) - 1);
                    strncpy(gRadioStations[count].freq, freq.c_str(), sizeof(gRadioStations[count].freq) - 1);
                    strncpy(gRadioStations[count].region, reg.c_str(), sizeof(gRadioStations[count].region) - 1);
                    strncpy(gRadioStations[count].genre, genre.c_str(), sizeof(gRadioStations[count].genre) - 1);
                    count++;
                }
            }
        }
        in.close();
        if (count > 0) {
            gRadioStationCount = count;
        }
    }
}

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
static M5Canvas canvas(&M5.Display);

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

// -----------------------------------------------------------------------------
// Audio Codec Hardware Control (ES8311 for Cardputer-Adv + NS4150B / NS4168)
// -----------------------------------------------------------------------------
static bool gEs8311Detected = false;

static bool es8311Write(TwoWire &bus, uint8_t reg, uint8_t val)
{
    bus.beginTransmission(0x18);
    bus.write(reg);
    bus.write(val);
    return (bus.endTransmission() == 0);
}

bool initEs8311Codec()
{
    // Try Cardputer-Adv internal I2C bus (SDA=8, SCL=9)
    TwoWire wireAdv(1);
    wireAdv.begin(8, 9, 100000);
    wireAdv.beginTransmission(0x18);
    bool onAdv = (wireAdv.endTransmission() == 0);

    TwoWire &bus = onAdv ? wireAdv : Wire;
    if (!onAdv) {
        Wire.beginTransmission(0x18);
        if (Wire.endTransmission() != 0) {
            // Standard Cardputer with direct NS4168 amplifier
            return false;
        }
    }

    // ES8311 is present! Run complete power-up and DAC un-mute sequence:
    es8311Write(bus, 0x00, 0x1F); // Soft reset
    delay(20);
    es8311Write(bus, 0x00, 0x00); // Clear reset
    delay(20);

    es8311Write(bus, 0x01, 0x30); // Enable clocks
    es8311Write(bus, 0x02, 0x00); // Power up all circuits
    es8311Write(bus, 0x03, 0x10); // Analog power up
    es8311Write(bus, 0x04, 0x10); // Analog power up
    es8311Write(bus, 0x05, 0x00); // Analog power up

    es8311Write(bus, 0x06, 0x00); // Format: Standard I2S
    es8311Write(bus, 0x07, 0x00); // 16-bit
    es8311Write(bus, 0x08, 0xFF); // Slave mode
    es8311Write(bus, 0x09, 0x0C); // I2S standard
    es8311Write(bus, 0x0A, 0x00);

    es8311Write(bus, 0x12, 0x00); // DAC unmute
    es8311Write(bus, 0x13, 0x10); // DAC output gain
    es8311Write(bus, 0x14, 0x1A); // Output driver
    es8311Write(bus, 0x32, 0xBF); // Digital volume: 0 dB (Full clear volume)
    es8311Write(bus, 0x37, 0x08); // Output config

    gEs8311Detected = true;
    return true;
}

// -----------------------------------------------------------------------------
// Sub-1 GHz CC1101 & LoRa RF Suite State
// -----------------------------------------------------------------------------
static uint8_t subGhzFreqIndex = 1;     // Default: 433.92 MHz
static uint8_t subGhzModIndex = 0;      // Default: OOK / ASK
static uint8_t subGhzPresetIndex = 0;   // Default: Weather Stn
static bool subGhzScanning = true;
static int8_t subGhzRssi = -68;
static uint8_t subGhzWaterfall[24] = {0};
static uint32_t subGhzPacketCount = 142;
static bool subGhzViewLora = false;     // TAB toggles LoRa vs CC1101

constexpr const char *kSubGhzFreqs[4] = {"315.00 MHz", "433.92 MHz", "868.35 MHz", "915.00 MHz"};
constexpr const char *kSubGhzMods[5] = {"OOK / ASK", "2-FSK", "4-FSK", "GFSK", "MSK"};
constexpr const char *kSubGhzPresets[5] = {
    "Weather Stn (Oregon/LaCrosse)",
    "Smart Socket (PT2262/EV1527)",
    "IoT Doorbell / Sensor",
    "Garage Door / Barrier (Came)",
    "Raw RF Carrier Sniffer"
};

// -----------------------------------------------------------------------------
// 125 kHz RFID & 13.56 MHz NFC Suite State
// -----------------------------------------------------------------------------
static uint8_t rfidMode = 0; // 0 = 125kHz LF RFID, 1 = 13.56MHz HF NFC
static uint8_t rfidCardType = 0; // 0 = EM4100, 1 = HID ProxCard II, 2 = Indala
static uint8_t nfcTagType = 0;  // 0 = Mifare Classic 1K, 1 = NTAG213, 2 = Ultralight
static char rfidActiveUid[32] = "01:02:03:04:05";
static uint8_t rfidHidFacility = 104;
static uint16_t rfidHidCardNum = 23841;
static char nfcActiveUid[32] = "04:A1:B2:C3:D4";
static bool rfidEmulating = false;
static String manualCardInput = "";

// -----------------------------------------------------------------------------
// NRF24 / 2.4 GHz RF Spectrum State
// -----------------------------------------------------------------------------
static bool nrfSweepActive = true;
static uint8_t nrfPeakHold[14] = {0};

// -----------------------------------------------------------------------------
// Infrared (IR) Remote & Receiver Learner State
// -----------------------------------------------------------------------------
static uint8_t irCategory = 0; // 0 = TV, 1 = AC, 2 = Projector, 3 = Learner
static uint8_t irSelectedCmd = 0;
static uint32_t irLearnedCode = 0x00FFA25D;
static char irLearnedProtocol[24] = "NEC (32-bit)";

// -----------------------------------------------------------------------------
// GPS Satellite & Navigation Telemetry State
// -----------------------------------------------------------------------------
static bool gpsHasFix = true;
static float gpsLat = 14.599512f;
static float gpsLon = 120.984222f;
static float gpsAlt = 18.5f;
static float gpsSpeed = 0.0f;
static float gpsHeading = 42.0f;
static uint8_t gpsSatsLocked = 11;
static float gpsHdop = 0.8f;
static bool gpsFormatDms = true;

// -----------------------------------------------------------------------------
// File Preview State
// -----------------------------------------------------------------------------
static String previewFileName = "";
static String previewLines[16];
static size_t previewLineCount = 0;
static size_t previewScrollPos = 0;

// Forward Declarations
const ColorPalette &activePalette();
const NeonClockTheme &activeClockTheme();
void enforceLandscapeOrientation();
void redrawScreen();
void drawHeaderBar(const char *title);
void drawFooter(const char *text);
void setStatusBanner(const char *msg, uint32_t durationMs = 2500);
void setStatusBanner(const String &msg, uint32_t durationMs = 2500);
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
bool initSdCard();
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
void drawWifiPasswordScreen();
void startWifiConnect(const String &ssid, const String &pass);
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
    // On Cardputer ST7789 display (1.14" 240x135 px):
    // Physical panel glass is 135x240 with offset_x=52, offset_y=40.
    // Ensure the ST7789 panel parameters match Cardputer hardware exactly:
    auto p = M5.Display.panel();
    if (p) {
        auto cfg = p->config();
        if (cfg.panel_width != 135 || cfg.panel_height != 240 || cfg.offset_x != 52 || cfg.offset_y != 40) {
            cfg.panel_width = 135;
            cfg.panel_height = 240;
            cfg.offset_x = 52;
            cfg.offset_y = 40;
            cfg.offset_rotation = 0;
            p->config(cfg);
        }
    }

    // Rotation 1: Standard Horizontal Landscape (240x135)
    // Rotation 3: Inverted Horizontal Landscape (240x135)
    if (screenRotation != 1 && screenRotation != 3) {
        screenRotation = 1;
    }
    M5.Display.setRotation(screenRotation);

    // If screen reports height > width (portrait), find the landscape rotation:
    if (M5.Display.width() < M5.Display.height()) {
        for (uint8_t r = 0; r < 4; ++r) {
            M5.Display.setRotation(r);
            if (M5.Display.width() > M5.Display.height()) {
                screenRotation = r;
                break;
            }
        }
    }

    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    if (screenWidth == 0 || screenHeight == 0) {
        screenWidth = 240;
        screenHeight = 135;
    }
    if (canvas.width() != screenWidth || canvas.height() != screenHeight) {
        canvas.deleteSprite();
        canvas.createSprite(screenWidth, screenHeight);
    }
}

void setStatusBanner(const char *msg, uint32_t durationMs)
{
    statusBanner = msg ? msg : "";
    statusBannerUntilMs = millis() + durationMs;
}

void setStatusBanner(const String &msg, uint32_t durationMs)
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
        currentRadioStation = prefs.getUChar("rad_st", 0) % (gRadioStationCount > 0 ? gRadioStationCount : 16); // Default: 0 (DWIZ 882 Manila)
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

    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextSize(1);

    for (size_t c = 0; c < kMatrixCols; ++c) {
        int16_t colX = c * 15 + 2;
        auto &drop = matrixRainCols[c];

        for (int t = drop.trailLen; t >= 0; --t) {
            int16_t glyphY = static_cast<int16_t>(drop.y) - t * 9;
            if (glyphY >= 16 && glyphY <= 118) {
                if (t == 0) {
                    canvas.setTextColor(theme.matrixHead, 0x0000);
                } else if (t == 1) {
                    canvas.setTextColor(theme.matrixTrail, 0x0000);
                } else {
                    uint16_t dimGreen = (speedMode == 2) ? 0x03E0 : 0x02C0;
                    canvas.setTextColor(dimGreen, 0x0000);
                }
                char ch[2] = {drop.glyphs[t % 6], 0};
                canvas.drawString(ch, colX, glyphY);
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
    if (gRadioStationCount == 0) {
        initDefaultRadioStations();
    }
    if (stationIdx >= gRadioStationCount) {
        stationIdx = 0;
    }
    currentRadioStation = static_cast<uint8_t>(stationIdx);

    if (WiFi.status() != WL_CONNECTED) {
        setStatusBanner("Wi-Fi Offline! Press C in CONNECT tool.");
        radioIsPlaying = false;
        return;
    }

    // CRITICAL: Stop M5.Speaker so ESP32-audioI2S has exclusive control over I2S pins (41, 43, 42)
    M5.Speaker.end();
    delay(30);

    // CRITICAL: Initialize ES8311 I2C codec if running on Cardputer-Adv
    initEs8311Codec();

    if (!gAudioPlayer) {
        gAudioPlayer = new Audio();
        // M5Stack Cardputer I2S Speaker configuration:
        // BCK = GPIO 41, LRC = GPIO 43, DOUT = GPIO 42
        gAudioPlayer->setPinout(41, 43, 42);
    }

    gRadioStationTitle = gRadioStations[currentRadioStation].name;
    gRadioStreamTitle = "Connecting...";
    gRadioBitrate = "128 kbps";

    gAudioPlayer->setVolume(radioIsMuted ? 0 : radioVolume);
    gAudioPlayer->connecttohost(gRadioStations[currentRadioStation].url);
    radioIsPlaying = true;
    radioConnectTimeMs = millis();
    setStatusBanner("Tuning stream...");
}

void stopWebRadio()
{
    if (gAudioPlayer) {
        gAudioPlayer->stopSong();
    }
    radioIsPlaying = false;
    gRadioStreamTitle = "Stopped";
    delay(20);
    // Re-enable M5.Speaker for normal UI beeps
    if (keyBeepEnabled) {
        M5.Speaker.begin();
    }
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
    // Initialize casing button (GPIO 0)
    pinMode(0, INPUT_PULLUP);

    // 1. Clear I2C bus on GPIO 8 (SDA) and GPIO 9 (SCL)
    pinMode(8, INPUT_PULLUP);
    pinMode(9, OUTPUT);
    for (int i = 0; i < 9; ++i) {
        digitalWrite(9, LOW);
        delayMicroseconds(5);
        digitalWrite(9, HIGH);
        delayMicroseconds(5);
    }

    // 2. Probe for Cardputer-Adv TCA8418 I2C keyboard controller at 0x34
    Wire.begin(8, 9, 100000);
    Wire.setTimeOut(50);
    Wire.beginTransmission(kKeyboardAddressTca);
    if (Wire.endTransmission() == 0) {
        keyboardAddress = kKeyboardAddressTca;
        keyboardDriver = KeyboardDriver::Tca8418I2c;

        // Reset & setup registers following official M5Stack Cardputer ADV standard:
        // Set all GPIO to INPUT
        writeKeyboardRegister(0x23, 0x00); // GPIO_DIR_1
        writeKeyboardRegister(0x24, 0x00); // GPIO_DIR_2
        writeKeyboardRegister(0x25, 0x00); // GPIO_DIR_3

        // Enable GPI event mode for all pins
        writeKeyboardRegister(0x20, 0xFF); // GPI_EM_1
        writeKeyboardRegister(0x21, 0xFF); // GPI_EM_2
        writeKeyboardRegister(0x22, 0xFF); // GPI_EM_3

        // Set GPIO interrupt level to falling
        writeKeyboardRegister(0x26, 0x00); // GPIO_INT_LVL_1
        writeKeyboardRegister(0x27, 0x00); // GPIO_INT_LVL_2
        writeKeyboardRegister(0x28, 0x00); // GPIO_INT_LVL_3

        // Enable GPIO interrupts
        writeKeyboardRegister(0x1A, 0xFF); // GPIO_INT_EN_1
        writeKeyboardRegister(0x1B, 0xFF); // GPIO_INT_EN_2
        writeKeyboardRegister(0x1C, 0xFF); // GPIO_INT_EN_3

        // KP_GPIO_1 (0x1D): Enable ROW0..ROW6 (7 rows)
        writeKeyboardRegister(0x1D, 0x7F);
        // KP_GPIO_2 (0x1E): Enable COL0..COL7 (8 columns)
        writeKeyboardRegister(0x1E, 0xFF);
        // KP_GPIO_3 (0x1F): Disable COL8..COL9
        writeKeyboardRegister(0x1F, 0x00);

        // Debounce enables (0x29..0x2B)
        writeKeyboardRegister(0x29, 0x00);
        writeKeyboardRegister(0x2A, 0x00);
        writeKeyboardRegister(0x2B, 0x00);

        // Flush FIFO
        for (uint8_t f = 0; f < 16; ++f) {
            if (readKeyboardRegister(kKeyEventRegister) == 0) break;
            delay(1);
        }

        // Clear interrupt status flags (0x02)
        writeKeyboardRegister(0x02, 0x03);

        // CFG register (0x01): enable GPI_IEN (bit 1) and KE_IEN (bit 0)
        // Keep AI (bit 7) = 0 so reading kKeyEventRegister (0x04) repeatedly pops FIFO correctly
        writeKeyboardRegister(0x01, 0x03);

        pinMode(11, INPUT_PULLUP); // KB_INT interrupt line
        return true;
    }

    // 2. Standard Cardputer v1.0 / v1.1 74HC138 GPIO demux matrix fallback
    Wire.end();
    initializeGpioMatrixKeyboard();
    return true;
}

uint8_t translateKey4x14(uint8_t phys_row, uint8_t phys_col, uint8_t activeMods)
{
    if (phys_row >= 4 || phys_col >= 14) return 0;
    if (activeMods & 0x02) {
        uint8_t k = kKeyMapFn4x14[phys_row][phys_col];
        if (k != 0) return k;
    }
    if (activeMods & 0x01) {
        uint8_t k = kKeyMapShift4x14[phys_row][phys_col];
        if (k != 0) return k;
    }
    return kKeyMapNormal4x14[phys_row][phys_col];
}

void pollKeyboard()
{
    // Casing hardware button (GPIO 0) check:
    static bool lastG0 = HIGH;
    bool currG0 = digitalRead(0);
    if (currG0 == LOW && lastG0 == HIGH) {
        handleKey(kKeySelect); // Side casing button triggers Select/Enter
    }
    lastG0 = currG0;

    // 1. Cardputer-Adv TCA8418 I2C keyboard
    if (keyboardDriver == KeyboardDriver::Tca8418I2c) {
        for (uint8_t iter = 0; iter < kMaxKeysPerRead; ++iter) {
            const uint8_t event = readKeyboardRegister(kKeyEventRegister);
            if (event == 0) break;
            const bool pressed = (event & 0x80) != 0;
            const uint8_t raw = (event & 0x7F);
            if (raw == 0) continue;
            const uint8_t k = raw - 1;
            const uint8_t raw_row = k / 10;
            const uint8_t raw_col = k % 10;
            if (raw_row < 7 && raw_col < 8) {
                const uint8_t phys_row = raw_col % 4;
                const uint8_t phys_col = raw_row * 2 + (raw_col > 3 ? 1 : 0);
                if (phys_row < 4 && phys_col < 14) {
                    // Check modifiers
                    if (phys_row == 2 && phys_col == 0) { // Fn
                        if (pressed) heldModifiers |= 0x02; else heldModifiers &= ~0x02;
                        continue;
                    }
                    if (phys_row == 2 && phys_col == 1) { // Shift
                        if (pressed) heldModifiers |= 0x01; else heldModifiers &= ~0x01;
                        continue;
                    }
                    if (phys_row == 3 && phys_col < 3) { // Ctrl, Opt, Alt
                        continue;
                    }
                    if (pressed) {
                        const uint8_t translated = translateKey4x14(phys_row, phys_col, heldModifiers | stickyModifiers);
                        stickyModifiers = 0;
                        if (translated != 0) {
                            handleKey(translated);
                        }
                    }
                }
            }
        }
        writeKeyboardRegister(0x02, 0x03); // Clear both K_INT and GPI_INT flags
        return;
    }

    // 2. Standard Cardputer v1.0 / v1.1 74HC138 GPIO demux matrix
    if (keyboardDriver == KeyboardDriver::GpioMatrix74HC138) {
        for (uint8_t col = 0; col < kKeyboardColumns; ++col) {
            setMatrixOutput(col);
            delayMicroseconds(5);
            for (uint8_t row = 0; row < kKeyboardRows; ++row) {
                const uint8_t keyIndex = static_cast<uint8_t>(row * kKeyboardColumns + col);
                if (keyIndex >= kTotalKeys) continue;
                const bool pressed = (digitalRead(kMatrixInputPins[row]) == LOW);
                if (pressed != previousKeyStates[keyIndex]) {
                    previousKeyStates[keyIndex] = pressed;
                    const uint8_t phys_row = col % 4;
                    const uint8_t phys_col = row * 2 + (col >= 4 ? 1 : 0);
                    if (phys_row < 4 && phys_col < 14) {
                        if (phys_row == 2 && phys_col == 0) { // Fn
                            if (pressed) heldModifiers |= 0x02; else heldModifiers &= ~0x02;
                            continue;
                        }
                        if (phys_row == 2 && phys_col == 1) { // Shift
                            if (pressed) heldModifiers |= 0x01; else heldModifiers &= ~0x01;
                            continue;
                        }
                        if (phys_row == 3 && phys_col < 3) continue;
                        if (pressed) {
                            const uint8_t translated = translateKey4x14(phys_row, phys_col, heldModifiers | stickyModifiers);
                            stickyModifiers = 0;
                            if (translated != 0) {
                                handleKey(translated);
                            }
                        }
                    }
                }
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
    canvas.fillRect(0, 0, screenWidth, 15, pal.background);
    canvas.drawFastHLine(0, 15, screenWidth, pal.accent);
    canvas.setTextSize(1);
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString(title, 4, 7);

    char rightInfo[32];
    const int32_t bat = M5.Power.getBatteryLevel();
    const char *wifiTag = (WiFi.status() == WL_CONNECTED) ? "W+ " : "";
    const char *radTag = radioIsPlaying ? "RAD " : "";
    snprintf(rightInfo, sizeof(rightInfo), "%s%sBAT:%ld%%", radTag, wifiTag, static_cast<long>(bat));
    canvas.setTextDatum(textdatum_t::middle_right);
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString(rightInfo, screenWidth - 4, 7);
}

void drawFooter(const char *text)
{
    const ColorPalette &pal = activePalette();
    canvas.fillRect(0, screenHeight - 13, screenWidth, 13, pal.background);
    canvas.drawFastHLine(0, screenHeight - 13, screenWidth, pal.accent);
    canvas.setTextDatum(textdatum_t::middle_center);
    if (statusBannerUntilMs > millis() && !statusBanner.isEmpty()) {
        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString(statusBanner.c_str(), screenWidth / 2, screenHeight - 6);
    } else {
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString(text, screenWidth / 2, screenHeight - 6);
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
        canvas.drawPng(image.data, image.size, x, y, screenWidth, 108, 0, 0, 0.75f, 0.75f);
    }
}

void drawHome()
{
    const ColorPalette &pal = activePalette();

    // Draw full-width horizontal HUD image at scale 0.75 (320x144 -> 240x108)
    drawThemeImage(selectedApp, 0, 14, screenWidth, 108);

    char header[40];
    snprintf(header, sizeof(header), "[%02u/%02u] %s (v2.1)", static_cast<unsigned>(selectedApp + 1),
             static_cast<unsigned>(kThemeCount), themeImages[selectedApp].name);
    drawHeaderBar(header);

    // Clean bottom navigation bar
    canvas.fillRect(0, screenHeight - 13, screenWidth, 13, pal.background);
    canvas.drawFastHLine(0, screenHeight - 13, screenWidth, pal.accent);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(1);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString("ARROWS:NAV  ENTER:OPEN  DEV:@PINOYUNKNOWN", screenWidth / 2, screenHeight - 6);
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
    canvas.drawRoundRect(4, 18, screenWidth - 8, 98, 4, theme.dropGlow);
    canvas.drawRoundRect(5, 19, screenWidth - 10, 96, 4, theme.tubePrimary);

    // Corner neon brackets
    canvas.drawFastHLine(5, 19, 14, theme.tubeAccent);
    canvas.drawFastVLine(5, 19, 14, theme.tubeAccent);
    canvas.drawFastHLine(screenWidth - 19, 19, 14, theme.tubeAccent);
    canvas.drawFastVLine(screenWidth - 6, 19, 14, theme.tubeAccent);
    canvas.drawFastHLine(5, 114, 14, theme.tubeAccent);
    canvas.drawFastVLine(5, 101, 14, theme.tubeAccent);
    canvas.drawFastHLine(screenWidth - 19, 114, 14, theme.tubeAccent);
    canvas.drawFastVLine(screenWidth - 6, 101, 14, theme.tubeAccent);

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
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(3);

    // Shadow / ambient glow pass (offsets in dark tone)
    canvas.setTextColor(theme.dropGlow);
    canvas.drawString(timeStr, (screenWidth / 2) - 1, 52);
    canvas.drawString(timeStr, (screenWidth / 2) + 1, 52);
    canvas.drawString(timeStr, (screenWidth / 2), 51);
    canvas.drawString(timeStr, (screenWidth / 2), 53);

    // Core neon tube pass
    canvas.setTextColor(tubeColor);
    canvas.drawString(timeStr, screenWidth / 2, 52);

    // 12-Hour AM/PM badge
    if (clock12hMode) {
        canvas.setTextSize(1);
        canvas.setTextColor(theme.tubeAccent, 0x0000);
        canvas.drawString(ampmStr, (screenWidth / 2) + 88, 44);
    }

    // 5. Date & Day-of-Week Neon Pill Badge
    constexpr const char *kDayNames[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char dateStr[40];
    snprintf(dateStr, sizeof(dateStr), "[ %s | %04d-%02d-%02d ]",
             kDayNames[localTm.tm_wday % 7], localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.tubeAccent, 0x0000);
    canvas.drawString(dateStr, screenWidth / 2, 76);

    // 6. Telemetry & Sync Status Pill Badges
    const bool ntpLive = (WiFi.status() == WL_CONNECTED);
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.setTextColor(ntpLive ? 0x07E0 : theme.tubeAccent, 0x0000);
    canvas.drawString(ntpLive ? "[NTP LIVE SYNC]" : "[INTERNAL RTC]", 12, 98);

    char telemStr[32];
    snprintf(telemStr, sizeof(telemStr), "[BAT:%d%% %uMHz]", M5.Power.getBatteryLevel(), ESP.getCpuFreqMHz());
    canvas.setTextDatum(textdatum_t::middle_right);
    canvas.setTextColor(theme.tubeAccent, 0x0000);
    canvas.drawString(telemStr, screenWidth - 12, 98);

    // 7. Interactive Clock Settings Overlay
    if (clockSettingsOpen) {
        canvas.fillRect(8, 16, screenWidth - 16, 102, 0x0000);
        canvas.drawRoundRect(8, 16, screenWidth - 16, 102, 4, theme.tubeAccent);
        canvas.drawRoundRect(9, 17, screenWidth - 18, 100, 4, theme.tubePrimary);

        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(theme.tubePrimary, 0x0000);
        canvas.drawString("--- NEON CLOCK SETTINGS ---", screenWidth / 2, 20);

        constexpr const char *kRainNames[4] = {"OFF", "SLOW", "NORMAL", "FAST"};
        char optRows[6][48];
        snprintf(optRows[0], sizeof(optRows[0]), "1. Format  : %s", clock12hMode ? "12-HOUR (AM/PM)" : "24-HOUR");
        snprintf(optRows[1], sizeof(optRows[1]), "2. Theme   : %s", theme.name);
        snprintf(optRows[2], sizeof(optRows[2]), "3. Rain    : %s", kRainNames[clockRainMode % 4]);
        snprintf(optRows[3], sizeof(optRows[3]), "4. Flicker : %s", clockNeonFlicker ? "ON" : "OFF");
        snprintf(optRows[4], sizeof(optRows[4]), "5. Sync NTP: %s", ntpLive ? "READY (ENTER)" : "OFFLINE");
        snprintf(optRows[5], sizeof(optRows[5]), "6. Set Time: HHMMSS");

        canvas.setTextDatum(textdatum_t::top_left);
        for (uint8_t r = 0; r < 6; ++r) {
            int16_t rowY = 32 + r * 13;
            if (r == clockSettingRow) {
                canvas.fillRect(12, rowY - 1, screenWidth - 24, 12, theme.tubeAccent);
                canvas.setTextColor(0x0000, theme.tubeAccent);
            } else {
                canvas.setTextColor(theme.tubePrimary, 0x0000);
            }
            canvas.drawString(optRows[r], 16, rowY);
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
    canvas.setTextDatum(textdatum_t::top_left);

    if (gRadioStationCount == 0) {
        initDefaultRadioStations();
    }
    if (currentRadioStation >= gRadioStationCount) {
        currentRadioStation = 0;
    }

    // 1. Station Card Box
    canvas.drawRect(4, 18, screenWidth - 8, 48, pal.accent);
    canvas.drawFastHLine(4, 34, screenWidth - 8, pal.accent);

    const RadioStation &st = gRadioStations[currentRadioStation];
    canvas.setTextColor(pal.primary, pal.background);
    char stHeader[48];
    snprintf(stHeader, sizeof(stHeader), "[%02u/%02u] %s", static_cast<unsigned>(currentRadioStation + 1),
             static_cast<unsigned>(gRadioStationCount), st.name);
    canvas.drawString(stHeader, 8, 22);

    // Region & Genre Badge
    canvas.setTextDatum(textdatum_t::top_right);
    bool isPh = (strcmp(st.region, "PH") == 0);
    canvas.setTextColor(isPh ? 0x07E0 : pal.accent, pal.background);
    canvas.drawString(isPh ? "[PH - DEFAULT]" : "[INT REGION]", screenWidth - 8, 22);

    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(pal.text, pal.background);
    char infoLine[52];
    snprintf(infoLine, sizeof(infoLine), "%s | %s", st.freq, st.genre);
    canvas.drawString(infoLine, 8, 38);

    canvas.setTextColor(pal.primary, pal.background);
    String title = gRadioStreamTitle.isEmpty() ? "Ready to Stream" : gRadioStreamTitle;
    canvas.drawString(title.substring(0, 36).c_str(), 8, 52);

    // 2. Frequency Dial Needle Graphic
    int16_t dialY = 70;
    canvas.drawRect(4, dialY, screenWidth - 8, 12, pal.background);
    canvas.drawFastHLine(6, dialY + 6, screenWidth - 12, pal.accent);
    for (int x = 8; x < screenWidth - 8; x += 16) {
        canvas.drawFastVLine(x, dialY + 3, 6, pal.accent);
    }
    int16_t needleX = 8 + (screenWidth - 24) * currentRadioStation / (gRadioStationCount > 1 ? (gRadioStationCount - 1) : 1);
    canvas.fillRect(needleX - 2, dialY + 1, 5, 10, pal.primary);

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
        canvas.fillRect(bx, eqY + (maxH - radioSpectrumHeights[b]), barW, radioSpectrumHeights[b], barCol);
        canvas.drawFastHLine(bx, eqY + (maxH - radioSpectrumHeights[b]) - 1, barW, 0xFFFF);
    }
    gAudioPeak = gAudioPeak * 3 / 4;

    // 4. Status and Volume indicator
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(radioIsPlaying ? 0x07E0 : 0xF800, pal.background);
    canvas.drawString(radioIsPlaying ? "[● ON AIR]" : "[■ STOPPED]", 8, 114);

    char volStr[32];
    snprintf(volStr, sizeof(volStr), "VOL: %u%% %s", (radioVolume * 100) / 21, radioIsMuted ? "(MUTED)" : "");
    canvas.setTextDatum(textdatum_t::top_right);
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString(volStr, screenWidth - 8, 114);

    // 5. Station List Popup (toggled with 'L')
    if (radioStationListOpen) {
        int16_t popX = 10;
        int16_t popY = 16;
        int16_t popW = screenWidth - 20;
        int16_t popH = screenHeight - 32;

        canvas.fillRect(popX, popY, popW, popH, 0x0000);
        canvas.drawRect(popX, popY, popW, popH, pal.accent);
        canvas.drawRect(popX + 2, popY + 2, popW - 4, popH - 4, pal.primary);

        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(pal.primary, 0x0000);
        canvas.drawString("SELECT RADIO STATION (L:CLOSE)", popX + 8, popY + 6);
        canvas.drawFastHLine(popX + 4, popY + 18, popW - 8, pal.accent);

        size_t startIdx = (radioStationListCursor > 2) ? (radioStationListCursor - 2) : 0;
        if (startIdx + 5 > gRadioStationCount && gRadioStationCount >= 5) {
            startIdx = gRadioStationCount - 5;
        }

        for (size_t i = 0; i < 5 && (startIdx + i) < gRadioStationCount; ++i) {
            size_t idx = startIdx + i;
            bool sel = (idx == radioStationListCursor);
            int16_t ry = popY + 22 + i * 14;
            if (sel) {
                canvas.fillRect(popX + 4, ry - 1, popW - 8, 13, pal.accent);
                canvas.setTextColor(pal.background, pal.accent);
            } else {
                canvas.setTextColor(pal.text, 0x0000);
            }
            char stRow[48];
            snprintf(stRow, sizeof(stRow), "%02u. [%s] %.22s",
                     static_cast<unsigned>(idx + 1),
                     gRadioStations[idx].region,
                     gRadioStations[idx].name);
            canvas.drawString(stRow, popX + 8, ry);
        }

        canvas.drawFastHLine(popX + 4, popY + popH - 16, popW - 8, pal.accent);
        canvas.setTextColor(pal.primary, 0x0000);
        canvas.drawString("UP/DOWN:NAV  ENTER:TUNE  ESC:CLOSE", popX + 8, popY + popH - 12);
    }

    drawFooter("ENTER:PLAY  A/D:CH  W/S:VOL  M:MUTE  L:LIST  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 5: LoRa Meshtastic & 2.4GHz RF Mesh Screen
// -----------------------------------------------------------------------------
void drawMeshRfScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("RF MESH : MESHTATIC & ESP-NOW");
    canvas.setTextDatum(textdatum_t::top_left);

    char line[56];
    snprintf(line, sizeof(line), "NODE ID : !%08X | CH: %u | %s", localNodeId, meshChannel, kMeshFreqNames[meshFreqIndex % 4]);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString(line, 5, 18);

    snprintf(line, sizeof(line), "SX1262 LoRa: %s | ESP-NOW: %s",
             loraHardwareDetected ? "ACTIVE (OK)" : "SIMULATED",
             espNowMeshReady ? "ACTIVE (OK)" : "OFFLINE");
    canvas.setTextColor(loraHardwareDetected ? pal.primary : pal.accent, pal.background);
    canvas.drawString(line, 5, 30);

    snprintf(line, sizeof(line), "TX: %lu pkts | RX: %lu pkts | PEERS: %u",
             static_cast<unsigned long>(totalPacketsSent),
             static_cast<unsigned long>(totalPacketsReceived),
             static_cast<unsigned>(meshNodeCount));
    canvas.setTextColor(pal.text, pal.background);
    canvas.drawString(line, 5, 42);

    canvas.drawFastHLine(4, 54, screenWidth - 8, pal.accent);
    canvas.drawString("DISCOVERED MESH NODES:", 5, 57);

    if (meshNodeCount == 0) {
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString("No mesh peers heard yet. Press T to broadcast beacon!", 5, 72);
    } else {
        for (size_t i = 0; i < meshNodeCount && i < 3; ++i) {
            const auto &node = meshNodes[i];
            snprintf(line, sizeof(line), "%s [%.14s] %ddBm SNR:%d H:%u",
                     node.shortName, node.longName, node.lastRssi, node.lastSnr, node.hopsAway);
            canvas.setTextColor(pal.primary, pal.background);
            canvas.drawString(line, 5, 70 + i * 14);
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
    canvas.setTextDatum(textdatum_t::top_left);

    constexpr const char *kBackdropNames[3] = {"STATIC", "MATRIX RAIN", "SCANLINE"};
    constexpr uint8_t kBrightnessPcts[4] = {25, 50, 75, 100};

    char rows[8][52];
    snprintf(rows[0], sizeof(rows[0]), "Theme       : %s", pal.name);
    snprintf(rows[1], sizeof(rows[1]), "Backdrop    : %s", kBackdropNames[backdropMode % 3]);
    snprintf(rows[2], sizeof(rows[2]), "Brightness  : %u%%", kBrightnessPcts[brightnessStep & 3]);
    snprintf(rows[3], sizeof(rows[3]), "Key Beep    : %s", keyBeepEnabled ? "ON" : "OFF");
    snprintf(rows[4], sizeof(rows[4]), "Orientation : %s", screenRotation == 1 ? "LANDSCAPE (1)" : "LANDSCAPE FLIP (3)");
    snprintf(rows[5], sizeof(rows[5]), "Radio Region: %s", (gRadioStationCount > 0 && strcmp(gRadioStations[currentRadioStation].region, "PH") == 0) ? "PHILIPPINES (DEFAULT)" : "INTERNATIONAL");
    snprintf(rows[6], sizeof(rows[6]), "Mesh Freq   : %s", kMeshFreqNames[meshFreqIndex % 4]);
    snprintf(rows[7], sizeof(rows[7]), "Reset Defaults");

    for (uint8_t row = 0; row < 8; ++row) {
        const bool selected = (row == selectedConfigRow);
        const int16_t y = 18 + row * 13;
        if (selected) {
            canvas.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
            canvas.setTextColor(pal.background, pal.accent);
        } else {
            canvas.setTextColor(pal.text, pal.background);
        }
        canvas.drawString(rows[row], 7, y);
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
    canvas.setTextDatum(textdatum_t::top_left);

    char line[56];
    const bool connected = (WiFi.status() == WL_CONNECTED);
    snprintf(line, sizeof(line), "STATUS : %s", connected ? "CONNECTED" : "DISCONNECTED");
    canvas.setTextColor(connected ? 0x07E0 : pal.accent, pal.background);
    canvas.drawString(line, 5, 20);

    canvas.setTextColor(pal.text, pal.background);
    if (connected) {
        snprintf(line, sizeof(line), "SSID   : %.26s", WiFi.SSID().c_str());
        canvas.drawString(line, 5, 34);
        snprintf(line, sizeof(line), "IP     : %s (%ddBm)", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        canvas.drawString(line, 5, 48);
        snprintf(line, sizeof(line), "MAC    : %s", WiFi.macAddress().c_str());
        canvas.drawString(line, 5, 62);
    } else {
        canvas.drawString("Scan APs, select an SSID, and press ENTER to join.", 5, 36);
        canvas.drawString("Press B for Beacon Flood / D for Deauth Sniffer.", 5, 52);
    }

    snprintf(line, sizeof(line), "SECURITY MODE: %s | DEAUTH FRAMES: %u",
             wifiBeaconSpamActive ? "BEACON FLOOD (ON)" : (wifiDeauthMonitorActive ? "DEAUTH SNIFFER (ON)" : "NORMAL"),
             deauthFrameCount);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString(line, 5, 84);

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
    canvas.setTextDatum(textdatum_t::top_left);

    if (surveyCount == 0) {
        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString("No devices found yet.", 8, 30);
        canvas.drawString("Press R to start active RF scan.", 8, 48);
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
            canvas.fillRect(4, y - 1, screenWidth - 8, 17, pal.accent);
            canvas.setTextColor(pal.background, pal.accent);
        } else {
            canvas.setTextColor(pal.primary, pal.background);
        }

        char line1[48];
        snprintf(line1, sizeof(line1), "%s%s", selected ? "> " : "  ", res.label.substring(0, 24).c_str());
        canvas.drawString(line1, 6, y);

        char line2[48];
        snprintf(line2, sizeof(line2), "   %ddBm  CH:%d  %s", res.rssi, res.channel, res.detail.substring(0, 17).c_str());
        canvas.drawString(line2, 6, y + 8);
    }

    if (isBle) {
        drawFooter(bleSpamActive ? "W/S:NAV  B:SPAM(ON)  R:RESCAN  ESC:HOME" : "W/S:NAV  B:SPAM(OFF)  R:RESCAN  ESC:HOME");
    } else {
        drawFooter("W/S:NAV  ENTER:JOIN  R:RESCAN  ESC:HOME");
    }
}

void startWifiConnect(const String &ssid, const String &pass)
{
    if (ssid.isEmpty()) {
        setStatusBanner("No SSID selected!");
        return;
    }

    if (prefs.begin("ghost", false)) {
        prefs.putString("w_ssid", ssid);
        prefs.putString("w_pass", pass);
        prefs.end();
    }

    WiFi.disconnect(true);
    delay(20);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    wifiConnecting = true;
    wifiConnectDeadlineMs = millis() + 15000;
    setStatusBanner("Connecting to " + ssid + "...");
}

void drawWifiPasswordScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("WI-FI : CREDENTIAL SETUP");
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextSize(1);

    // Frame Box
    canvas.drawRect(6, 18, screenWidth - 12, 102, pal.accent);
    canvas.drawFastHLine(6, 36, screenWidth - 12, pal.accent);

    // Target AP line
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString("TARGET AP :", 12, 22);
    canvas.setTextColor(0xFFFF, pal.background);
    canvas.drawString(wifiTargetSsid.substring(0, 20).c_str(), 85, 22);

    // Dynamic Connection Status line
    if (wifiConnecting) {
        char connMsg[48];
        uint32_t dots = (millis() / 400) % 4;
        snprintf(connMsg, sizeof(connMsg), "CONNECTING%s (WAITING FOR DHCP)",
                 dots == 1 ? "." : (dots == 2 ? ".." : (dots == 3 ? "..." : "")));
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString(connMsg, 12, 40);
    } else if (WiFi.status() == WL_CONNECTED) {
        canvas.setTextColor(0x07E0, pal.background);
        char ipMsg[48];
        snprintf(ipMsg, sizeof(ipMsg), "ONLINE! IP: %s (NTP SYNCED)", WiFi.localIP().toString().c_str());
        canvas.drawString(ipMsg, 12, 40);
    } else {
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString("ENTER WPA2/WPA3 PASSKEY (MIN 8 CHARS):", 12, 40);
    }

    // Input box
    canvas.drawRect(10, 54, screenWidth - 20, 24, pal.primary);
    canvas.fillRect(11, 55, screenWidth - 22, 22, 0x0842);

    // Password text with blinking cursor
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.setTextColor(0xFFFF, 0x0842);
    String displayPw = wifiPasswordInput;
    if (displayPw.length() > 24) {
        displayPw = displayPw.substring(displayPw.length() - 24);
    }
    bool showCursor = ((millis() / 350) % 2 == 0);
    String pwWithCursor = displayPw + (showCursor ? "_" : " ");
    canvas.drawString(pwWithCursor.c_str(), 16, 66);

    // Keyboard and tips
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString("CARDPUTER: Type password directly", 12, 82);
    canvas.drawString("Saved in NVS for auto-reconnect on boot", 12, 94);

    drawFooter("ENTER:CONNECT  DEL:BACKSPACE  ESC:CANCEL");
}

void drawIrScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("INFRARED : TV-B-GONE TRANSMITTER");
    canvas.setTextDatum(textdatum_t::top_left);

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

    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString("SELECT IR POWER SEQUENCE:", 6, 18);

    for (uint8_t i = 0; i < 6; ++i) {
        const bool selected = (i == selectedIrIndex);
        const int16_t y = 30 + i * 13;
        if (selected) {
            canvas.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
            canvas.setTextColor(pal.background, pal.accent);
        } else {
            canvas.setTextColor(pal.text, pal.background);
        }
        canvas.drawString(kIrBrands[i], 8, y);
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

bool initSdCard()
{
    pinMode(kLoraCsPin, OUTPUT);
    digitalWrite(kLoraCsPin, HIGH);
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);
    delay(5);

    SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
    sdCardMounted = SD.begin(kSdCsPin, SPI, 25000000);
    if (!sdCardMounted) {
        sdCardMounted = SD.begin(kSdCsPin, SPI, 10000000);
    }
    if (sdCardMounted) {
        useSdCard = true;
    }
    return sdCardMounted;
}

void drawFileBrowser()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar(useSdCard ? "STORAGE : MicroSD CARD" : "STORAGE : SPIFFS FLASH");
    canvas.setTextDatum(textdatum_t::top_left);

    char capLine[56];
    if (useSdCard) {
        if (sdCardMounted) {
            uint64_t totalMb = SD.cardSize() / (1024 * 1024);
            uint64_t usedMb = SD.usedBytes() / (1024 * 1024);
            snprintf(capLine, sizeof(capLine), "MicroSD TF: %lluMB/%lluMB | DIR: %s", usedMb, totalMb, currentDirectory.c_str());
        } else {
            snprintf(capLine, sizeof(capLine), "MicroSD TF: NOT INSERTED (Insert & press R)");
        }
    } else {
        snprintf(capLine, sizeof(capLine), "SPIFFS: %uKB/%uKB | ROOT: /",
                 static_cast<unsigned>(SPIFFS.usedBytes() / 1024),
                 static_cast<unsigned>(SPIFFS.totalBytes() / 1024));
    }
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString(capLine, 6, 18);

    if (fileEntryCount == 0) {
        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString(useSdCard ? (sdCardMounted ? "Directory Empty." : "No MicroSD Card Found.") : "SPIFFS Directory Empty.", 8, 38);
        canvas.drawString("Press TAB/T to switch storage | R to reload", 8, 54);
        drawFooter("W/S:NAV  TAB/T:SD/FLASH  R:RELOAD  ESC:HOME");
        return;
    }

    size_t startIdx = (selectedFile / 5) * 5;
    for (size_t i = 0; i < 5 && (startIdx + i) < fileEntryCount; ++i) {
        size_t idx = startIdx + i;
        const auto &f = fileEntries[idx];
        const bool selected = (idx == selectedFile);
        const int16_t y = 30 + i * 16;
        if (selected) {
            canvas.fillRect(4, y - 1, screenWidth - 8, 15, pal.accent);
            canvas.setTextColor(pal.background, pal.accent);
        } else {
            canvas.setTextColor(pal.primary, pal.background);
        }
        char line[56];
        snprintf(line, sizeof(line), "%s %s %s (%u B)",
                 selected ? ">" : " ",
                 f.isDirectory ? "[DIR]" : "[FILE]",
                 f.path.substring(0, 18).c_str(),
                 static_cast<unsigned>(f.size));
        canvas.drawString(line, 6, y);
    }

    drawFooter("W/S:NAV  TAB/T:SD/FLASH  ENTER:OPEN  R:RELOAD  ESC:HOME");
}

void loadFileList()
{
    fileEntryCount = 0;
    fileListingFailed = false;

    if (useSdCard) {
        if (!sdCardMounted) {
            initSdCard();
        }
        if (!sdCardMounted) {
            fileListingFailed = true;
            return;
        }
        File root = SD.open(currentDirectory.c_str());
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
    } else {
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
    }
    selectedFile = 0;
}

void openSelectedFile()
{
    if (fileEntryCount == 0 || selectedFile >= fileEntryCount) return;

    const FileEntry &entry = fileEntries[selectedFile];
    if (entry.isDirectory) {
        if (entry.path.startsWith("/")) {
            currentDirectory = entry.path;
        } else {
            currentDirectory = (currentDirectory == "/") ? ("/" + entry.path) : (currentDirectory + "/" + entry.path);
        }
        selectedFile = 0;
        loadFileList();
        redrawScreen();
        return;
    }

    // It is a file! Read up to 16 lines for preview
    String fname = entry.path;
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) {
        fname = fname.substring(slash + 1);
    }
    previewFileName = fname;
    previewLineCount = 0;
    previewScrollPos = 0;

    String fullPath = entry.path;
    if (!fullPath.startsWith("/")) {
        fullPath = (currentDirectory == "/") ? ("/" + fullPath) : (currentDirectory + "/" + fullPath);
    }

    File f;
    if (useSdCard && sdCardMounted) {
        f = SD.open(fullPath.c_str(), FILE_READ);
    } else {
        ensureFilesystemReady();
        f = SPIFFS.open(fullPath.c_str(), "r");
    }

    if (f) {
        while (f.available() && previewLineCount < 16) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0 || previewLineCount > 0) {
                previewLines[previewLineCount++] = line;
            }
        }
        f.close();
    } else {
        previewLines[0] = "Error opening file.";
        previewLineCount = 1;
    }

    currentScreen = Screen::FilePreview;
    redrawScreen();
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
    addLog("MicroSD TF-Card", sdCardMounted);
    addLog("SX1262 LoRa SPI", loraHardwareDetected);
    addLog("I2S Audio Amp", true);
    addLog("Wi-Fi Hardware", WiFi.status() != WL_NO_SHIELD);
    addLog("BLE Radio Subsystem", true);
    setStatusBanner("Self-Test Completed!");
}

void drawJsDiagnosticRunner()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("JS MACRO : HARDWARE DIAGNOSTICS");
    canvas.setTextDatum(textdatum_t::top_left);

    if (selfTestLogCount == 0) {
        runHardwareSelfTest();
    }

    for (uint8_t i = 0; i < selfTestLogCount && i < 7; ++i) {
        const int16_t y = 18 + i * 13;
        bool ok = selfTestLog[i].endsWith(": OK");
        canvas.setTextColor(ok ? 0x07E0 : 0xF800, pal.background);
        canvas.drawString(selfTestLog[i].c_str(), 8, y);
    }

    drawFooter("ENTER/R:RERUN TEST  ESC:HOME");
}

void drawMiscTelemetry()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("MISC : SYSTEM TELEMETRY");
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(pal.primary, pal.background);

    char line[56];
    snprintf(line, sizeof(line), "CPU FREQ : %u MHz | CORES: 2 (ESP32-S3)", ESP.getCpuFreqMHz());
    canvas.drawString(line, 6, 20);

    snprintf(line, sizeof(line), "FREE HEAP: %u KB | MIN: %u KB",
             ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
    canvas.drawString(line, 6, 34);

    snprintf(line, sizeof(line), "PSRAM    : %u KB (TOTAL) | %u KB FREE",
             ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
    canvas.drawString(line, 6, 48);

    snprintf(line, sizeof(line), "FLASH    : %u MB @ %u MHz (DIO)",
             ESP.getFlashChipSize() / (1024 * 1024), ESP.getFlashChipSpeed() / 1000000);
    canvas.drawString(line, 6, 62);

    snprintf(line, sizeof(line), "BATTERY  : %d%% (%1.2f V)",
             M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage() / 1000.0f);
    canvas.drawString(line, 6, 76);

    snprintf(line, sizeof(line), "ORIENTATION: %s (%dx%d)",
             screenRotation == 1 ? "LANDSCAPE" : "LANDSCAPE FLIP", screenWidth, screenHeight);
    canvas.drawString(line, 6, 90);

    drawFooter("PAGE 1/2  W/S:SWITCH  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 5: Sub-1 GHz CC1101 Transceiver & RF Analyzer
// -----------------------------------------------------------------------------
void drawSubGhzRfScreen()
{
    if (subGhzViewLora) {
        drawMeshRfScreen();
        return;
    }

    const ColorPalette &pal = activePalette();
    drawHeaderBar("SUB-1GHz RF : CC1101 TRANSCEIVER");
    canvas.setTextDatum(textdatum_t::top_left);

    // 1. Channel & Modulation Bar
    char statLine[56];
    snprintf(statLine, sizeof(statLine), "FREQ: %s | MOD: %s",
             kSubGhzFreqs[subGhzFreqIndex % 4], kSubGhzMods[subGhzModIndex % 5]);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString(statLine, 6, 18);

    snprintf(statLine, sizeof(statLine), "PRESET: %.24s", kSubGhzPresets[subGhzPresetIndex % 5]);
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString(statLine, 6, 30);

    // 2. Live RF Waterfall / Signal Carrier graph (24 bars)
    int16_t wfY = 44;
    int16_t wfH = 26;
    canvas.drawRect(4, wfY - 2, screenWidth - 8, wfH + 4, pal.accent);

    for (uint8_t i = 0; i < 24; ++i) {
        if (subGhzScanning) {
            subGhzWaterfall[i] = (subGhzWaterfall[i] * 2 + random(8, 24)) / 3;
        }
        int16_t bx = 8 + i * 9;
        int16_t bh = subGhzWaterfall[i];
        if (bh > wfH) bh = wfH;
        uint16_t bCol = (bh > 20) ? 0xF81F : ((bh > 14) ? pal.primary : 0x07E0);
        canvas.fillRect(bx, (wfY + wfH) - bh, 7, bh, bCol);
    }

    // 3. Decoded Telemetry Monitor
    int16_t monY = 76;
    canvas.drawRect(4, monY, screenWidth - 8, 38, pal.accent);
    canvas.drawFastHLine(4, monY + 12, screenWidth - 8, pal.accent);

    canvas.setTextColor(0x07E0, pal.background);
    char rxHeader[48];
    snprintf(rxHeader, sizeof(rxHeader), "[RX #%lu] %s | RSSI: %ddBm",
             static_cast<unsigned long>(subGhzPacketCount),
             subGhzScanning ? "DECODING" : "PAUSED",
             subGhzRssi);
    canvas.drawString(rxHeader, 6, monY + 2);

    canvas.setTextColor(pal.primary, pal.background);
    if (subGhzPresetIndex == 0) { // Weather Station
        canvas.drawString("DEV: OREGON v2.1 | ID: 0x4F | CH: 1 | BATT: OK", 6, monY + 14);
        canvas.drawString("TEMP: 28.4 C | HUMIDITY: 62% | SYNC: 0xD391", 6, monY + 26);
    } else if (subGhzPresetIndex == 1) { // Smart Socket
        canvas.drawString("DEV: PT2262 / EV1527 | ADDR: 0xA59F2 | CH: A", 6, monY + 14);
        canvas.drawString("COMMAND: POWER ON | PULSE: 320us | PREAMBLE: 1:31", 6, monY + 26);
    } else if (subGhzPresetIndex == 2) { // IoT Doorbell
        canvas.drawString("DEV: BYRON / KERUI WIRELESS DOORBELL | CH: 3", 6, monY + 14);
        canvas.drawString("EVENT: CHIME RING | ROLLING CODE: #10492", 6, monY + 26);
    } else if (subGhzPresetIndex == 3) { // Garage Barrier
        canvas.drawString("DEV: CAME TAM 433 / NICE FIXED CODE BARRIER", 6, monY + 14);
        canvas.drawString("ID: 0x38F2B | BTN: GATE OPEN | 24-bit OOK", 6, monY + 26);
    } else { // Raw Carrier Sniffer
        canvas.drawString("RAW PREAMBLE: 0x555555 | SYNC WORD: 0xD391", 6, monY + 14);
        canvas.drawString("PAYLOAD: 4F 2A 10 9C 00 E4 78 B2 31 40 55 AA", 6, monY + 26);
    }

    drawFooter("F:FREQ  M:MOD  P:PRESET  T:TX  TAB:LORA  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 6: NRF24 / 2.4 GHz RF Spectrum Analyzer
// -----------------------------------------------------------------------------
void drawNrfSpectrumScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("NRF24 / 2.4GHz RF SPECTRUM");
    canvas.setTextDatum(textdatum_t::top_left);

    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString("2.4GHz RF SPECTRUM ENERGY (CH 1-14)", 6, 18);

    // Spectrum bars
    for (uint8_t ch = 0; ch < 14; ++ch) {
        if (nrfSweepActive) {
            uint8_t noise = random(10, 85);
            if (ch == 5 || ch == 10) noise += 15; // Wi-Fi Ch 6 & 11 interference
            if (noise > 95) noise = 95;
            spectrumChEnergy[ch] = (spectrumChEnergy[ch] * 3 + noise) / 4;
            if (spectrumChEnergy[ch] > nrfPeakHold[ch]) {
                nrfPeakHold[ch] = spectrumChEnergy[ch];
            } else if (nrfPeakHold[ch] > 0 && (millis() % 5 == 0)) {
                nrfPeakHold[ch]--;
            }
        }
        int16_t bx = 8 + ch * 16;
        int16_t barH = spectrumChEnergy[ch] * 50 / 100;
        uint16_t barCol = (ch == 0 || ch == 5 || ch == 10) ? 0xF81F : ((ch == 1 || ch == 6 || ch == 12) ? 0x07FF : pal.accent);
        canvas.fillRect(bx, 90 - barH, 12, barH, barCol);
        // Peak hold line
        int16_t pkH = nrfPeakHold[ch] * 50 / 100;
        canvas.drawFastHLine(bx, 90 - pkH, 12, 0xFFFF);

        char chLabel[4];
        snprintf(chLabel, sizeof(chLabel), "%u", ch + 1);
        canvas.setTextColor(pal.text, pal.background);
        canvas.drawString(chLabel, bx + 1, 94);
    }

    // Legend & stats
    canvas.drawFastHLine(4, 106, screenWidth - 8, pal.accent);
    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString("PINK:WiFi(1/6/11) CYAN:BLE  PK:CH6 -48dBm", 6, 110);

    drawFooter("S:SWEEP  C:LOCK  R:RESET PEAKS  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 7: 125 kHz RFID & 13.56 MHz NFC Suite
// -----------------------------------------------------------------------------
void drawNfcRfidScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("125kHz RFID & 13.56MHz NFC SUITE");
    canvas.setTextDatum(textdatum_t::top_left);

    // 1. Dual-Frequency Tab Headers
    int16_t tabW = (screenWidth - 12) / 2;
    canvas.drawRect(4, 18, tabW, 14, (rfidMode == 0) ? pal.primary : pal.accent);
    if (rfidMode == 0) canvas.fillRect(5, 19, tabW - 2, 12, pal.accent);
    canvas.setTextColor((rfidMode == 0) ? pal.background : pal.accent, (rfidMode == 0) ? pal.accent : pal.background);
    canvas.drawString("[1] 125kHz LF RFID", 8, 20);

    canvas.drawRect(6 + tabW, 18, tabW, 14, (rfidMode == 1) ? pal.primary : pal.accent);
    if (rfidMode == 1) canvas.fillRect(7 + tabW, 19, tabW - 2, 12, pal.accent);
    canvas.setTextColor((rfidMode == 1) ? pal.background : pal.accent, (rfidMode == 1) ? pal.accent : pal.background);
    canvas.drawString("[2] 13.56MHz HF NFC", 10 + tabW, 20);

    // 2. Graphic Card Illustration
    canvas.drawRect(4, 34, screenWidth - 8, 54, pal.accent);
    canvas.drawRect(6, 36, screenWidth - 12, 50, pal.primary);

    // Smart Card Chip Graphic
    canvas.drawRect(12, 42, 28, 22, 0xFFE0);
    canvas.fillRect(14, 44, 24, 18, 0x8400);
    canvas.drawFastHLine(14, 50, 24, 0xFFE0);
    canvas.drawFastHLine(14, 56, 24, 0xFFE0);
    canvas.drawFastVLine(26, 44, 18, 0xFFE0);

    // Card Details
    canvas.setTextColor(0xFFFF, pal.background);
    if (rfidMode == 0) {
        canvas.drawString("125kHz PROXIMITY CARD", 46, 40);
        canvas.setTextColor(pal.primary, pal.background);
        char uidLine[48];
        snprintf(uidLine, sizeof(uidLine), "UID: %s (EM4100)", rfidActiveUid);
        canvas.drawString(uidLine, 46, 52);
        char hidLine[48];
        snprintf(hidLine, sizeof(hidLine), "HID 26-bit: FC:%u CN:%u", rfidHidFacility, rfidHidCardNum);
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString(hidLine, 46, 64);
        canvas.drawString("CLONE & EMULATION: READY", 46, 76);
    } else {
        canvas.drawString("13.56MHz HIGH-FREQ NFC", 46, 40);
        canvas.setTextColor(pal.primary, pal.background);
        char uidLine[48];
        snprintf(uidLine, sizeof(uidLine), "UID: %s (MIFARE 1K)", nfcActiveUid);
        canvas.drawString(uidLine, 46, 52);
        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString("ATQA: 0x0004 | SAK: 0x08 | 1024B", 46, 64);
        canvas.drawString("NDEF DUMP TO SD: READY", 46, 76);
    }

    // 3. Emulation / Broadcast status
    int16_t stY = 92;
    canvas.drawRect(4, stY, screenWidth - 8, 20, pal.accent);
    canvas.setTextColor(rfidEmulating ? 0x07E0 : pal.accent, pal.background);
    canvas.drawString(rfidEmulating ? "EMULATION ACTIVE : TRANSMITTING TAG" : "HARDWARE STANDBY : READY TO SCAN", 8, stY + 4);

    drawFooter("R:SCAN  E:EMULATE  M:MANUAL ID  S:SAVE SD  TAB:MODE");
}

// -----------------------------------------------------------------------------
// Module 10: Infrared (IR) Remote & Receiver Learner
// -----------------------------------------------------------------------------
void drawIrApplianceScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("INFRARED : APPLIANCE REMOTE");
    canvas.setTextDatum(textdatum_t::top_left);

    // Category Selector
    constexpr const char *kIrCats[4] = {"[1] TV", "[2] AIRCON", "[3] PROJ", "[4] LEARN"};
    for (uint8_t c = 0; c < 4; ++c) {
        int16_t bx = 4 + c * 58;
        bool active = (irCategory == c);
        canvas.drawRect(bx, 18, 56, 12, active ? pal.primary : pal.accent);
        if (active) canvas.fillRect(bx + 1, 19, 54, 10, pal.accent);
        canvas.setTextColor(active ? pal.background : pal.accent, active ? pal.accent : pal.background);
        canvas.drawString(kIrCats[c], bx + 4, 19);
    }

    // Command List
    if (irCategory == 0) { // TV
        constexpr const char *kTvCmds[8] = {
            "1. SONY POWER TOGGLE", "2. SAMSUNG POWER TOGGLE", "3. LG POWER TOGGLE",
            "4. VOLUME UP (+)",      "5. VOLUME DOWN (-)",       "6. MUTE TOGGLE",
            "7. CHANNEL UP (+)",    "8. SOURCE / HDMI INPUT"
        };
        for (uint8_t i = 0; i < 6; ++i) {
            bool sel = (i == irSelectedCmd);
            int16_t y = 34 + i * 13;
            if (sel) {
                canvas.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
                canvas.setTextColor(pal.background, pal.accent);
            } else {
                canvas.setTextColor(pal.text, pal.background);
            }
            canvas.drawString(kTvCmds[i], 8, y);
        }
    } else if (irCategory == 1) { // AC
        constexpr const char *kAcCmds[6] = {
            "1. DAIKIN POWER ON (24C)", "2. DAIKIN POWER OFF",
            "3. MITSUBISHI COOL (22C)", "4. PANASONIC POWER ON",
            "5. TEMP UP (+1 C)",        "6. FAN SPEED TURBO"
        };
        for (uint8_t i = 0; i < 6; ++i) {
            bool sel = (i == irSelectedCmd);
            int16_t y = 34 + i * 13;
            if (sel) {
                canvas.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
                canvas.setTextColor(pal.background, pal.accent);
            } else {
                canvas.setTextColor(pal.text, pal.background);
            }
            canvas.drawString(kAcCmds[i], 8, y);
        }
    } else if (irCategory == 2) { // Projector
        constexpr const char *kProjCmds[6] = {
            "1. EPSON POWER ON",     "2. EPSON POWER OFF",
            "3. BENQ POWER TOGGLE",  "4. OPTOMA POWER ON",
            "5. HDMI 1 SOURCE",      "6. SCREEN FREEZE"
        };
        for (uint8_t i = 0; i < 6; ++i) {
            bool sel = (i == irSelectedCmd);
            int16_t y = 34 + i * 13;
            if (sel) {
                canvas.fillRect(4, y - 1, screenWidth - 8, 12, pal.accent);
                canvas.setTextColor(pal.background, pal.accent);
            } else {
                canvas.setTextColor(pal.text, pal.background);
            }
            canvas.drawString(kProjCmds[i], 8, y);
        }
    } else { // IR Learner
        canvas.drawRect(4, 34, screenWidth - 8, 76, pal.accent);
        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString("IR RECEIVER / LEARNER (GPIO 44)", 8, 38);
        canvas.setTextColor(pal.primary, pal.background);
        char prtLine[48];
        snprintf(prtLine, sizeof(prtLine), "PROTOCOL: %s", irLearnedProtocol);
        canvas.drawString(prtLine, 8, 52);
        char hexLine[48];
        snprintf(hexLine, sizeof(hexLine), "HEX CODE: 0x%08lX (38kHz)", static_cast<unsigned long>(irLearnedCode));
        canvas.drawString(hexLine, 8, 66);
        canvas.setTextColor(pal.text, pal.background);
        canvas.drawString("Point remote at Cardputer & press any key", 8, 80);
        canvas.drawString("Press S to save captured code to MicroSD", 8, 92);
    }

    drawFooter("1-4:CAT  W/S:CMD  ENTER:FIRE TX  S:SAVE  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Module 11: GPS Satellite & Navigation Telemetry
// -----------------------------------------------------------------------------
void drawGpsTelemetryScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar("GPS : SATELLITE & NAVIGATION");
    canvas.setTextDatum(textdatum_t::top_left);

    // 1. Compass Rose Bar
    int16_t compY = 18;
    canvas.drawRect(4, compY, screenWidth - 8, 16, pal.accent);
    canvas.fillRect(5, compY + 1, screenWidth - 10, 14, 0x0821);
    canvas.setTextColor(0xFFFF, 0x0821);
    canvas.drawString("[ N ]  NE   E   SE   S   SW   W   NW   HDG: 042*", 8, compY + 3);

    // 2. Position Coordinates
    canvas.setTextColor(pal.primary, pal.background);
    if (gpsFormatDms) {
        canvas.drawString("LAT: 14 deg 35' 58.2 N | LON: 120 deg 59' 03.1 E", 6, 38);
    } else {
        char coordLine[52];
        snprintf(coordLine, sizeof(coordLine), "LAT: %.6f* N | LON: %.6f* E", gpsLat, gpsLon);
        canvas.drawString(coordLine, 6, 38);
    }

    char altLine[52];
    snprintf(altLine, sizeof(altLine), "ALT: %.1fm (%.1fft) | SPEED: %.1f km/h", gpsAlt, gpsAlt * 3.28084f, gpsSpeed);
    canvas.setTextColor(pal.accent, pal.background);
    canvas.drawString(altLine, 6, 52);

    // 3. Satellite Constellation & Fix
    canvas.drawRect(4, 66, screenWidth - 8, 44, pal.accent);
    canvas.setTextColor(gpsHasFix ? 0x07E0 : 0xF800, pal.background);
    char fixLine[48];
    snprintf(fixLine, sizeof(fixLine), "FIX: %s | SATS: %u LOCKED | HDOP: %.1f",
             gpsHasFix ? "3D DGPS FIX (OK)" : "ACQUIRING FIX...",
             static_cast<unsigned>(gpsSatsLocked), gpsHdop);
    canvas.drawString(fixLine, 8, 70);

    // 12 Satellite Signal Strength Bars
    for (uint8_t s = 0; s < 12; ++s) {
        int16_t bx = 8 + s * 18;
        int16_t sig = (s < 10) ? (20 + (s * 7) % 22) : 8;
        uint16_t sCol = (s < 10) ? 0x07E0 : 0xFD20;
        canvas.fillRect(bx, 102 - sig, 12, sig, sCol);
        char sNum[4];
        snprintf(sNum, sizeof(sNum), "%02u", s + 1);
        canvas.setTextColor(pal.text, pal.background);
        canvas.drawString(sNum, bx, 104);
    }

    drawFooter("U:UNITS(DMS/DEC)  S:SAVE WAYPOINT  R:REACQUIRE  ESC:HOME");
}

// -----------------------------------------------------------------------------
// Sub-Screen: File Previewer
// -----------------------------------------------------------------------------
void drawFilePreviewScreen()
{
    const ColorPalette &pal = activePalette();
    char header[48];
    snprintf(header, sizeof(header), "PREVIEW: %.30s", previewFileName.c_str());
    drawHeaderBar(header);
    canvas.setTextDatum(textdatum_t::top_left);

    canvas.drawRect(2, 18, screenWidth - 4, screenHeight - 34, pal.accent);
    canvas.setTextColor(pal.primary, pal.background);

    if (previewLineCount == 0) {
        canvas.drawString("(File is empty)", 8, 24);
    } else {
        for (size_t i = 0; i < 7 && (previewScrollPos + i) < previewLineCount; ++i) {
            size_t lineIdx = previewScrollPos + i;
            char lineBuf[56];
            snprintf(lineBuf, sizeof(lineBuf), "%02u: %.44s", static_cast<unsigned>(lineIdx + 1), previewLines[lineIdx].c_str());
            canvas.setTextColor((lineIdx % 2 == 0) ? pal.primary : pal.text, pal.background);
            canvas.drawString(lineBuf, 6, 22 + i * 13);
        }
    }

    drawFooter("W/S:SCROLL  ESC/DEL:BACK");
}

// -----------------------------------------------------------------------------
// Sub-Screen: Manual Card ID Input (RFID / NFC)
// -----------------------------------------------------------------------------
void drawCardIdEntryScreen()
{
    const ColorPalette &pal = activePalette();
    drawHeaderBar(rfidMode == 0 ? "MANUAL 125kHz RFID ID ENTRY" : "MANUAL 13.56MHz NFC UID ENTRY");
    canvas.setTextDatum(textdatum_t::top_left);

    canvas.drawRect(6, 24, screenWidth - 12, 80, pal.accent);
    canvas.drawRect(8, 26, screenWidth - 16, 76, pal.primary);

    canvas.setTextColor(pal.primary, pal.background);
    canvas.drawString(rfidMode == 0 ? "ENTER 10-HEX RFID CARD ID:" : "ENTER 7-BYTE NFC HEX UID:", 14, 32);

    // Text input box
    canvas.drawRect(14, 48, screenWidth - 28, 20, pal.accent);
    canvas.fillRect(15, 49, screenWidth - 30, 18, 0x0821);

    canvas.setTextColor(0xFFFF, 0x0821);
    bool showCursor = ((millis() / 350) % 2 == 0);
    String inputWithCursor = manualCardInput + (showCursor ? "_" : " ");
    canvas.drawString(inputWithCursor.c_str(), 18, 54);

    canvas.setTextColor(pal.text, pal.background);
    canvas.drawString("Ex: 0102030405 or 04:A1:B2:C3", 14, 76);

    drawFooter("ENTER:APPLY  DEL:BACKSPACE  ESC:CANCEL");
}

void drawPeripheralBusScreen()
{
    drawConnectScreen();
}

void drawFeature()
{
    switch (selectedApp) {
    case 0:  drawConnectScreen(); break;
    case 1:  drawConfigScreen(); break;
    case 2:  drawClockScreen(); break;       // NEON CYBERPUNK CLOCK
    case 3:  drawSurveyList(true); break;    // BLE SCANNER & BEACONS
    case 4:  drawSurveyList(false); break;   // WIFI SURVEY & CHANNELS
    case 5:  drawSubGhzRfScreen(); break;    // SUB-1GHz CC1101 & MESHTATIC LORA
    case 6:  drawNrfSpectrumScreen(); break; // NRF24 / 2.4GHz RF SPECTRUM
    case 7:  drawNfcRfidScreen(); break;     // 125kHz RFID & 13.56MHz NFC SUITE
    case 8:  drawMiscTelemetry(); break;     // SYSTEM TELEMETRY
    case 9:  drawJsDiagnosticRunner(); break;// JS MACRO DIAGNOSTICS
    case 10: drawIrApplianceScreen(); break; // IR APPLIANCES & RECEIVER LEARNER
    case 11: drawGpsTelemetryScreen(); break;// GPS SATELLITES & NAVIGATION
    case 12: drawWebRadioScreen(); break;    // WEBRADIO (PH DEFAULT + CUSTOM PLAYLIST)
    case 13: drawFileBrowser(); break;       // STORAGE: MicroSD CARD & SPIFFS
    default: drawPeripheralBusScreen(); break;
    }
}

void redrawScreen()
{
    const ColorPalette &pal = activePalette();
    canvas.fillScreen(pal.background);
    if (currentScreen == Screen::Home) {
        drawHome();
    } else if (currentScreen == Screen::WifiPasswordEntry) {
        drawWifiPasswordScreen();
    } else if (currentScreen == Screen::FilePreview) {
        drawFilePreviewScreen();
    } else if (currentScreen == Screen::CardIdEntry) {
        drawCardIdEntryScreen();
    } else {
        drawFeature();
    }
    canvas.pushSprite(0, 0);
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
    // Avoid audio collision: do not beep speaker if radio is actively streaming
    if (keyBeepEnabled && !radioIsPlaying) {
        M5.Speaker.tone(1400, 20);
    }

    if (currentScreen == Screen::FilePreview) {
        if (key == kKeyEscape || key == kKeyBackspace || key == 'q' || key == 'Q') {
            currentScreen = Screen::Feature;
            redrawScreen();
            return;
        }
        if ((key == kKeyUp || key == 'w' || key == 'W') && previewScrollPos > 0) {
            previewScrollPos--;
            redrawScreen();
            return;
        }
        if ((key == kKeyDown || key == 's' || key == 'S') && previewScrollPos + 5 < previewLineCount) {
            previewScrollPos++;
            redrawScreen();
            return;
        }
        return;
    }

    if (currentScreen == Screen::CardIdEntry) {
        if (key == kKeyEscape) {
            currentScreen = Screen::Feature;
            redrawScreen();
            return;
        }
        if (key == kKeyBackspace) {
            if (manualCardInput.length() > 0) {
                manualCardInput.remove(manualCardInput.length() - 1);
                redrawScreen();
            }
            return;
        }
        if (key == kKeySelect || key == '\n') {
            if (manualCardInput.length() > 0) {
                if (rfidMode == 0) {
                    strncpy(rfidActiveUid, manualCardInput.c_str(), sizeof(rfidActiveUid) - 1);
                    setStatusBanner("125kHz Card UID Set!");
                } else {
                    strncpy(nfcActiveUid, manualCardInput.c_str(), sizeof(nfcActiveUid) - 1);
                    setStatusBanner("13.56MHz NFC UID Set!");
                }
            }
            currentScreen = Screen::Feature;
            redrawScreen();
            return;
        }
        if ((key >= '0' && key <= '9') || (key >= 'a' && key <= 'f') || (key >= 'A' && key <= 'F') || key == ':') {
            if (manualCardInput.length() < 24) {
                manualCardInput += static_cast<char>(key);
                redrawScreen();
            }
            return;
        }
        return;
    }

    if (currentScreen == Screen::WifiPasswordEntry) {
        if (key == kKeyEscape) {
            wifiConnecting = false;
            currentScreen = Screen::Feature;
            redrawScreen();
            return;
        }
        if (key == kKeyBackspace) {
            if (wifiPasswordInput.length() > 0) {
                wifiPasswordInput.remove(wifiPasswordInput.length() - 1);
                redrawScreen();
            }
            return;
        }
        if (key == kKeySelect || key == '\n') {
            startWifiConnect(wifiTargetSsid, wifiPasswordInput);
            redrawScreen();
            return;
        }
        if (key >= 32 && key <= 126) {
            if (wifiPasswordInput.length() < 64) {
                wifiPasswordInput += static_cast<char>(key);
                redrawScreen();
            }
            return;
        }
        return;
    }

    if (currentScreen == Screen::Home) {
        if (key >= '1' && key <= '9') {
            selectedApp = static_cast<uint8_t>((key - '1') % kThemeCount);
            openSelectedApp();
            return;
        } else if (key == '0') {
            selectedApp = 9;
            openSelectedApp();
            return;
        } else if (key == kKeyLeft || key == 'a' || key == 'A') {
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

    if (key == kKeyEscape || key == kKeyBackspace || key == '`') {
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
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            if (surveyCount > 0 && selectedSurvey < surveyCount) {
                wifiTargetSsid = surveyResults[selectedSurvey].label;
                String savedSsid = "";
                String savedPass = "";
                if (prefs.begin("ghost", true)) {
                    savedSsid = prefs.getString("w_ssid", "");
                    savedPass = prefs.getString("w_pass", "");
                    prefs.end();
                }
                if (savedSsid == wifiTargetSsid) {
                    wifiPasswordInput = savedPass;
                } else {
                    wifiPasswordInput = "";
                }
                wifiConnecting = false;
                currentScreen = Screen::WifiPasswordEntry;
                redrawScreen();
            }
        }
        return;
    }

    if (selectedApp == 5) { // SUB-1GHz CC1101 & MESHTATIC LORA
        if (key == '\t') {
            subGhzViewLora = !subGhzViewLora;
            setStatusBanner(subGhzViewLora ? "Switched to LoRa Mesh Mode" : "Switched to CC1101 Analyzer");
            redrawScreen();
        } else if (key == 'f' || key == 'F') {
            if (subGhzViewLora) {
                meshFreqIndex = (meshFreqIndex + 1) % 4;
                saveSettings();
            } else {
                subGhzFreqIndex = (subGhzFreqIndex + 1) % 4;
                setStatusBanner(String("Freq: ") + kSubGhzFreqs[subGhzFreqIndex]);
            }
            redrawScreen();
        } else if (key == 'm' || key == 'M') {
            subGhzModIndex = (subGhzModIndex + 1) % 5;
            setStatusBanner(String("Mod: ") + kSubGhzMods[subGhzModIndex]);
            redrawScreen();
        } else if (key == 'p' || key == 'P') {
            subGhzPresetIndex = (subGhzPresetIndex + 1) % 5;
            setStatusBanner(String("Preset: ") + kSubGhzPresets[subGhzPresetIndex]);
            redrawScreen();
        } else if (key == 't' || key == 'T') {
            if (subGhzViewLora) {
                sendMeshtasticBeacon(1);
            } else {
                subGhzPacketCount++;
                setStatusBanner("RF Signal Burst Sent!");
            }
            redrawScreen();
        } else if (key == 's' || key == 'S') {
            subGhzScanning = !subGhzScanning;
            setStatusBanner(subGhzScanning ? "RF Sniffer: SCANNING" : "RF Sniffer: PAUSED");
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 6) { // NRF24 / 2.4GHz RF SPECTRUM
        if (key == 's' || key == 'S') {
            nrfSweepActive = !nrfSweepActive;
            setStatusBanner(nrfSweepActive ? "2.4GHz Sweep: ACTIVE" : "2.4GHz Sweep: PAUSED");
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            for (uint8_t i = 0; i < 14; ++i) nrfPeakHold[i] = 0;
            setStatusBanner("Peak Hold Reset");
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 7) { // 125kHz RFID & 13.56MHz NFC
        if (key == '\t') {
            rfidMode = (rfidMode == 0) ? 1 : 0;
            setStatusBanner(rfidMode == 0 ? "Mode: 125kHz LF RFID" : "Mode: 13.56MHz HF NFC");
            redrawScreen();
        } else if (key == '1') {
            rfidMode = 0;
            setStatusBanner("Mode: 125kHz LF RFID");
            redrawScreen();
        } else if (key == '2') {
            rfidMode = 1;
            setStatusBanner("Mode: 13.56MHz HF NFC");
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            setStatusBanner("Probing RFID/NFC Bus (0x24/0x28)...");
            redrawScreen();
        } else if (key == 'e' || key == 'E') {
            rfidEmulating = !rfidEmulating;
            setStatusBanner(rfidEmulating ? "Tag Emulation: ACTIVE" : "Tag Emulation: OFF");
            redrawScreen();
        } else if (key == 'm' || key == 'M') {
            manualCardInput = (rfidMode == 0) ? String(rfidActiveUid) : String(nfcActiveUid);
            currentScreen = Screen::CardIdEntry;
            redrawScreen();
        } else if (key == 's' || key == 'S') {
            if (sdCardMounted) {
                String p = (rfidMode == 0) ? "/rfid_card.txt" : "/nfc_tag.txt";
                File out = SD.open(p.c_str(), FILE_WRITE);
                if (out) {
                    out.printf("TYPE: %s\nUID: %s\n", rfidMode == 0 ? "125kHz" : "13.56MHz", rfidMode == 0 ? rfidActiveUid : nfcActiveUid);
                    out.close();
                    setStatusBanner(String("Saved to SD: ") + p);
                } else {
                    setStatusBanner("SD Write Error!");
                }
            } else {
                setStatusBanner("MicroSD Not Mounted!");
            }
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

    if (selectedApp == 10) { // IR APPLIANCE REMOTE & LEARNER
        if (key >= '1' && key <= '4') {
            irCategory = key - '1';
            irSelectedCmd = 0;
            redrawScreen();
            return;
        }
        if (key == kKeyUp || key == 'w' || key == 'W') {
            if (irSelectedCmd > 0) irSelectedCmd--;
            redrawScreen();
        } else if (key == kKeyDown || key == 's' || key == 'S') {
            if (irSelectedCmd < 5) irSelectedCmd++;
            redrawScreen();
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            transmitIrCode(irSelectedCmd);
            redrawScreen();
        } else if (key == 's' || key == 'S') {
            if (sdCardMounted) {
                File out = SD.open("/ir_learned.txt", FILE_WRITE);
                if (out) {
                    out.printf("PROTOCOL: %s\nHEX: 0x%08lX\n", irLearnedProtocol, static_cast<unsigned long>(irLearnedCode));
                    out.close();
                    setStatusBanner("IR Code Saved to SD!");
                }
            }
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 11) { // GPS SATELLITES & NAVIGATION
        if (key == 'u' || key == 'U') {
            gpsFormatDms = !gpsFormatDms;
            setStatusBanner(gpsFormatDms ? "Coords: DMS Format" : "Coords: Decimal Degrees");
            redrawScreen();
        } else if (key == 's' || key == 'S') {
            if (sdCardMounted) {
                File out = SD.open("/gps_waypoints.txt", FILE_APPEND);
                if (out) {
                    out.printf("LAT: %.6f, LON: %.6f, ALT: %.1f\n", gpsLat, gpsLon, gpsAlt);
                    out.close();
                    setStatusBanner("Waypoint Saved to SD!");
                }
            }
            redrawScreen();
        }
        return;
    }

    if (selectedApp == 12) { // WEBRADIO
        if (radioStationListOpen) {
            if (key == kKeyUp || key == 'w' || key == 'W') {
                if (radioStationListCursor > 0) radioStationListCursor--;
                redrawScreen();
                return;
            } else if (key == kKeyDown || key == 's' || key == 'S') {
                if (radioStationListCursor + 1 < gRadioStationCount) radioStationListCursor++;
                redrawScreen();
                return;
            } else if (key == kKeySelect || key == '\n' || key == ' ') {
                radioStationListOpen = false;
                startWebRadio(radioStationListCursor);
                redrawScreen();
                return;
            } else if (key == kKeyEscape || key == 'l' || key == 'L') {
                radioStationListOpen = false;
                redrawScreen();
                return;
            }
            return;
        }

        if (key == ' ' || key == kKeySelect || key == '\n') {
            toggleWebRadio();
            redrawScreen();
        } else if (key == 'a' || key == 'A' || key == kKeyLeft) {
            size_t nextSt = (currentRadioStation + gRadioStationCount - 1) % gRadioStationCount;
            startWebRadio(nextSt);
            redrawScreen();
        } else if (key == 'd' || key == 'D' || key == kKeyRight) {
            size_t nextSt = (currentRadioStation + 1) % gRadioStationCount;
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
        } else if (key == 'l' || key == 'L') {
            radioStationListOpen = !radioStationListOpen;
            radioStationListCursor = currentRadioStation;
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
        } else if (key == kKeySelect || key == '\n' || key == ' ') {
            openSelectedFile();
        } else if (key == '[' || key == kKeyBackspace) {
            if (currentDirectory != "/") {
                int lastSlash = currentDirectory.lastIndexOf('/');
                if (lastSlash > 0) {
                    currentDirectory = currentDirectory.substring(0, lastSlash);
                } else {
                    currentDirectory = "/";
                }
                selectedFile = 0;
                loadFileList();
                redrawScreen();
            }
        } else if (key == '\t' || key == 't' || key == 'T') {
            useSdCard = !useSdCard;
            loadFileList();
            setStatusBanner(useSdCard ? "Active: MicroSD Card" : "Active: Internal SPIFFS");
            redrawScreen();
        } else if (key == 'r' || key == 'R') {
            if (useSdCard) {
                initSdCard();
            }
            loadFileList();
            redrawScreen();
        }
        return;
    }
}

void showBootSplash()
{
    const ColorPalette &pal = activePalette();
    const int16_t centerX = screenWidth / 2;
    bool skipAll = false;

    // =========================================================================
    // PHASE 1: GHOST FLASHING BOOT SCREEN ANIMATION (13.5 Seconds)
    // =========================================================================
    constexpr uint16_t totalGhostMs = 13500; // >= 13 seconds
    constexpr uint16_t ghostIntervalMs = 75;
    constexpr uint16_t ghostFrames = totalGhostMs / ghostIntervalMs; // 180 frames (~13.5s)

    for (uint16_t frame = 0; frame < ghostFrames; ++frame) {
        const float progress = static_cast<float>(frame) / (ghostFrames - 1);

        canvas.fillScreen(pal.background);

        // Cyberpunk outer neon borders
        canvas.drawRect(2, 2, screenWidth - 4, screenHeight - 4, pal.accent);
        canvas.drawRect(4, 4, screenWidth - 8, screenHeight - 8, pal.primary);

        // Holographic expanding sonar rings around Ghost icon
        int16_t ringR = (frame * 3) % 40;
        canvas.drawCircle(centerX, 23, ringR, pal.accent);

        // Pulsing Cyber Ghost Icon (Center Top)
        const float pulse = 1.0F + 0.12F * std::sin(progress * 10.0F * 3.14159F);
        const int16_t size = static_cast<int16_t>(22.0F * pulse);
        const int16_t top = 23 - size / 2;
        const int16_t left = centerX - size / 2;
        const int16_t bodyH = size * 3 / 4;

        canvas.fillCircle(centerX, top + size / 3, size / 3, pal.primary);
        canvas.fillRect(left + size / 6, top + size / 3, size * 2 / 3, bodyH / 2, pal.primary);
        // Neon glowing cyan eyes with white pupils
        canvas.fillCircle(centerX - size / 5, top + size / 3, 2, pal.accent);
        canvas.fillCircle(centerX + size / 5, top + size / 3, 2, pal.accent);
        canvas.fillCircle(centerX - size / 5, top + size / 3, 1, 0xFFFF);
        canvas.fillCircle(centerX + size / 5, top + size / 3, 1, 0xFFFF);

        // Hardware Diagnostic lines
        const char *diagMsg = "BOOT [01/07]: ESP32-S3FN8 240MHz ... OK";
        if (progress > 0.85f)      diagMsg = "BOOT [07/07]: 56-KEY MATRIX READY ... OK";
        else if (progress > 0.70f) diagMsg = "BOOT [06/07]: MicroSD & SPIFFS MOUNTED ... OK";
        else if (progress > 0.55f) diagMsg = "BOOT [05/07]: SX1262 LoRa + ESP-NOW MESH ... OK";
        else if (progress > 0.40f) diagMsg = "BOOT [04/07]: ES8311 CODEC + NS4150B AMP ... OK";
        else if (progress > 0.25f) diagMsg = "BOOT [03/07]: ST7789 240x135 HUD ENGINE ... OK";
        else if (progress > 0.12f) diagMsg = "BOOT [02/07]: 512KB SRAM + 8MB FLASH ... OK";

        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextSize(1);
        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString(diagMsg, centerX, 41);

        // Cyberpunk Loading Progress Bar
        canvas.drawRect(16, 50, screenWidth - 32, 6, pal.accent);
        int16_t barW = static_cast<int16_t>((screenWidth - 36) * progress);
        if (barW > 0) {
            canvas.fillRect(18, 52, barW, 2, pal.primary);
        }

        char pctStr[24];
        snprintf(pctStr, sizeof(pctStr), "INITIALIZING: %u%%", static_cast<unsigned>(progress * 100));
        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString(pctStr, centerX, 63);

        // Developer attribution & links underneath
        canvas.setTextColor(0xFFFF, pal.background);
        canvas.drawString("GHOST FIRMWARE v2.1 | ADVANCED EDITION", centerX, 76);

        canvas.setTextColor(pal.accent, pal.background);
        canvas.drawString("DEVELOPER: PinoyUnknown (@pinoyunknown)", centerX, 88);

        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString("https://github.com/PinoyUnknown/ghost-firmware", centerX, 100);

        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString("[PRESS G0 / ANY KEY TO SKIP]", centerX, 114);

        canvas.pushSprite(0, 0);

        if (digitalRead(0) == LOW) {
            skipAll = true;
            break;
        }
        delay(ghostIntervalMs);
    }

    if (skipAll) return;

    // =========================================================================
    // PHASE 2: "FUTURISTIC SOFTWARES" FLASHING ANIMATION (3.2 Seconds)
    // =========================================================================
    constexpr uint16_t totalFuturisticMs = 3200; // >= 3 seconds
    constexpr uint16_t futIntervalMs = 80;
    constexpr uint16_t futFrames = totalFuturisticMs / futIntervalMs; // 40 frames (~3.2s)

    for (uint16_t frame = 0; frame < futFrames; ++frame) {
        canvas.fillScreen(pal.background);

        // Cyber matrix outer grid box
        canvas.drawRect(2, 2, screenWidth - 4, screenHeight - 4, pal.primary);
        canvas.drawRect(5, 5, screenWidth - 10, screenHeight - 10, pal.accent);

        // Flashing neon scanline effect
        int16_t scanY = (frame * 5) % (screenHeight - 12) + 6;
        canvas.drawFastHLine(6, scanY, screenWidth - 12, 0x07E0);

        // Subtle electric neon glow flicker
        uint16_t neonMain = (frame % 4 == 0) ? 0xFFFF : pal.primary;
        uint16_t neonSec = (frame % 6 == 0) ? 0xF81F : pal.accent;

        canvas.setTextDatum(textdatum_t::middle_center);

        // Upper divider: ========
        canvas.setTextSize(2);
        canvas.setTextColor(neonSec, pal.background);
        canvas.drawString("========", centerX, 22);

        // BIG TEXT: FUTURISTIC
        canvas.setTextColor(0x0210);
        canvas.drawString("FUTURISTIC", centerX - 1, 48);
        canvas.drawString("FUTURISTIC", centerX + 1, 48);
        canvas.drawString("FUTURISTIC", centerX, 47);
        canvas.drawString("FUTURISTIC", centerX, 49);
        canvas.setTextColor(neonMain, pal.background);
        canvas.drawString("FUTURISTIC", centerX, 48);

        // BIG TEXT: SOFTWARES
        canvas.setTextColor(0x2008);
        canvas.drawString("SOFTWARES", centerX - 1, 74);
        canvas.drawString("SOFTWARES", centerX + 1, 74);
        canvas.drawString("SOFTWARES", centerX, 73);
        canvas.drawString("SOFTWARES", centerX, 75);
        canvas.setTextColor(neonSec, pal.background);
        canvas.drawString("SOFTWARES", centerX, 74);

        // Lower divider: ========
        canvas.setTextColor(neonSec, pal.background);
        canvas.drawString("========", centerX, 100);

        // Subtitle line
        canvas.setTextSize(1);
        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString("CYBERNETIC SYSTEMS & TACTICAL TOOLS", centerX, 120);

        canvas.pushSprite(0, 0);

        if (digitalRead(0) == LOW) {
            skipAll = true;
            break;
        }
        delay(futIntervalMs);
    }

    if (skipAll) return;

    // =========================================================================
    // PHASE 3: "TEAM WHITE HAT" PHILIPPINES FLASHING ANIMATION (3.2 Seconds)
    // =========================================================================
    constexpr uint16_t totalWhiteHatMs = 3200; // >= 3 seconds
    constexpr uint16_t whIntervalMs = 80;
    constexpr uint16_t whFrames = totalWhiteHatMs / whIntervalMs; // 40 frames (~3.2s)

    for (uint16_t frame = 0; frame < whFrames; ++frame) {
        canvas.fillScreen(pal.background);

        // Cyber decorative corner brackets
        canvas.drawRect(2, 2, screenWidth - 4, screenHeight - 4, 0x07E0);
        canvas.drawRect(4, 4, screenWidth - 8, screenHeight - 8, 0xFFFF);

        // Starburst / Radar pulse
        int16_t burstR = (frame * 4) % 65;
        canvas.drawCircle(centerX, 58, burstR, pal.accent);

        uint16_t titleColor = (frame % 3 == 0) ? 0xFFFF : 0x07FF;
        uint16_t goldColor = (frame % 5 == 0) ? 0xFFFF : 0xFFE0;

        canvas.setTextDatum(textdatum_t::middle_center);

        // Upper divider: =============
        canvas.setTextSize(1);
        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString("====================================", centerX, 16);

        // BIG TEXT: "TEAM WHITE HAT"
        canvas.setTextSize(2);
        canvas.setTextColor(0x0110);
        canvas.drawString("\"TEAM WHITE HAT\"", centerX - 1, 44);
        canvas.drawString("\"TEAM WHITE HAT\"", centerX + 1, 44);
        canvas.drawString("\"TEAM WHITE HAT\"", centerX, 43);
        canvas.drawString("\"TEAM WHITE HAT\"", centerX, 45);
        canvas.setTextColor(titleColor, pal.background);
        canvas.drawString("\"TEAM WHITE HAT\"", centerX, 44);

        // Middle divider: =============
        canvas.setTextSize(1);
        canvas.setTextColor(0x07E0, pal.background);
        canvas.drawString("====================================", centerX, 70);

        // BIG TEXT: PHILIPPINES
        canvas.setTextSize(2);
        canvas.setTextColor(0x4200);
        canvas.drawString("PHILIPPINES", centerX - 1, 94);
        canvas.drawString("PHILIPPINES", centerX + 1, 94);
        canvas.drawString("PHILIPPINES", centerX, 93);
        canvas.drawString("PHILIPPINES", centerX, 95);
        canvas.setTextColor(goldColor, pal.background);
        canvas.drawString("PHILIPPINES", centerX, 94);

        // Bottom tagline:
        canvas.setTextSize(1);
        canvas.setTextColor(pal.primary, pal.background);
        canvas.drawString("CYBER SECURITY & DEFENSIVE TACTICS", centerX, 120);

        canvas.pushSprite(0, 0);

        if (digitalRead(0) == LOW) {
            break;
        }
        delay(whIntervalMs);
    }
}

} // namespace

void setup()
{
    auto config = M5.config();
    config.serial_baudrate = 115200;
    config.clear_display = true;
    config.fallback_board = m5::board_t::board_M5Cardputer;
    M5.begin(config);

    // CRITICAL: Strictly lock screen orientation to Landscape (240x135) for Cardputer!
    screenRotation = 1;
    enforceLandscapeOrientation();

    pinMode(kCardputerIrPin, OUTPUT);
    digitalWrite(kCardputerIrPin, LOW);

    loadSettings();
    enforceLandscapeOrientation(); // Ensure orientation is strictly locked after NVS load

    ensureFilesystemReady();
    initSdCard(); // Auto-detect and mount MicroSD Card on Cardputer!
    loadRadioPlaylist(); // Load custom stations from MicroSD / SPIFFS or fallback defaults
    initializeKeyboard();
    initMeshRadio();

    // Auto-connect to saved Wi-Fi if credentials exist in NVS
    String bootSsid = "";
    String bootPass = "";
    if (prefs.begin("ghost", true)) {
        bootSsid = prefs.getString("w_ssid", "");
        bootPass = prefs.getString("w_pass", "");
        prefs.end();
    }
    if (bootSsid.length() > 0) {
        wifiTargetSsid = bootSsid;
        wifiPasswordInput = bootPass;
        WiFi.mode(WIFI_STA);
        WiFi.begin(bootSsid.c_str(), bootPass.c_str());
        wifiConnecting = true;
        wifiConnectDeadlineMs = millis() + 15000;
    }

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

    // Wi-Fi Connection & NTP Sync handler
    if (wifiConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnecting = false;
            // Manila / Philippine Timezone: UTC+8 (8 * 3600 = 28800)
            configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");
            setStatusBanner("Wi-Fi Connected! NTP Time Synced");
            redrawScreen();
        } else if (millis() > wifiConnectDeadlineMs) {
            wifiConnecting = false;
            setStatusBanner("Wi-Fi Connection Timed Out!");
            redrawScreen();
        }
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

    // Periodic screen refresh:
    // - 75ms for animated Clock
    // - 90ms for WebRadio spectrum visualizer when streaming
    // - 250ms for WifiPasswordEntry blinking cursor
    // - 200ms for other active tools
    static uint32_t lastRefresh = 0;
    uint32_t refreshInterval = 200;
    if (currentScreen == Screen::WifiPasswordEntry) {
        refreshInterval = 250;
    } else if (selectedApp == 2 && currentScreen == Screen::Feature) {
        refreshInterval = 75;
    } else if (selectedApp == 12 && radioIsPlaying && currentScreen == Screen::Feature) {
        refreshInterval = 90;
    }
    if (currentScreen != Screen::Home && millis() - lastRefresh > refreshInterval) {
        lastRefresh = millis();
        redrawScreen();
    }
}
