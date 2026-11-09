#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_crt_bundle.h>
#include <esp_random.h>
#include <esp_tls.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>
#include <driver/gpio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>
#include <time.h>

namespace
{

constexpr uint8_t kKeyboardAddress = 0x34;
constexpr uint8_t kKeyboardAddressAlternate = 0x5F;
constexpr uint8_t kKeyCountRegister = 0x03;
constexpr uint8_t kKeyEventRegister = 0x04;
constexpr uint8_t kKeySelect = 0x0D;
constexpr uint8_t kKeyBackspace = 0x08;
constexpr uint8_t kKeyEscape = 0x1B;
constexpr uint8_t kKeyShift = 0x06;
constexpr uint8_t kKeyFunction = 0x02;
constexpr uint8_t kColumns = 8;
constexpr uint8_t kMaxEvents = 10;
constexpr uint32_t kPbkdfIterations = 150000;
constexpr size_t kPassphraseMinimum = 12;
constexpr size_t kMaxMessageBytes = 600;
constexpr size_t kMaxReplyBytes = 256;
constexpr size_t kInboxLimit = 8;
constexpr size_t kMaxRawEmailBytes = 2200;
constexpr size_t kMaxDecodedBodyBytes = 1400;
constexpr uint32_t kSendCooldownMs = 60000;
constexpr uint16_t kSmtpsPort = 465;
constexpr int16_t kBackground = 0x0000;
constexpr int16_t kCyan = 0x07FF;
constexpr int16_t kYellow = 0xFFE0;

constexpr uint8_t kKeyMap[56][3] = {
    {'`', '~', kKeyEscape}, {'\t', 0, 0},                         {0, 0, 0},             {0, 0, 0},
    {'1', '!', 0},           {'q', 'Q', 0x90},                    {0, 0, 0},             {0, 0, 0},
    {'2', '@', 0},           {'w', 'W', 0},                       {'a', 'A', 0},          {0, 0, 0},
    {'3', '#', 0},           {'e', 'E', 0},                       {'s', 'S', 0},          {'z', 'Z', 0},
    {'4', '$', 0},           {'r', 'R', 0},                       {'d', 'D', 0},          {'x', 'X', 0},
    {'5', '%', 0},           {'t', 'T', 0},                       {'f', 'F', 0},          {'c', 'C', 0},
    {'6', '^', 0},           {'y', 'Y', 0},                       {'g', 'G', 0},          {'v', 'V', 0},
    {'7', '&', 0},           {'u', 'U', 0},                       {'h', 'H', 0},          {'b', 'B', 0},
    {'8', '*', 0},           {'i', 'I', 0},                       {'j', 'J', 0},          {'n', 'N', 0},
    {'9', '(', 0},           {'o', 'O', 0},                       {'k', 'K', 0},          {'m', 'M', 0},
    {'0', ')', 0},           {'p', 'P', 0},                       {'l', 'L', 0},          {',', '<', 0xB4},
    {'_', '-', 0},           {'[', '{', 0},                       {';', ':', 0xB5},       {'.', '>', 0xB6},
    {'=', '+', 0},           {']', '}', 0},                       {'\'', '"', 0},        {'/', '?', 0xB7},
    {kKeyBackspace, 0, 0},   {'\\', '|', 0},                      {kKeySelect, 0, 0},     {' ', ' ', ' '}};

constexpr std::array<uint8_t, 16> kEnvelopeMagic = {'G', 'H', 'E', 'M', 'A', 'I', 'L', '1',
                                                    'C', 'R', 'E', 'D', 'S', '0', '0', '1'};
constexpr char kAad[] = "Ghost Email Firmware credential envelope v1";

enum class Screen : uint8_t {
    Splash,
    Welcome,
    CreatePassphrase,
    ConfirmPassphrase,
    Configure,
    Unlock,
    Menu,
    InboxLoading,
    Inbox,
    ReadingEmail,
    ComposeTo,
    ComposeSubject,
    ComposeBody,
    Review,
    ConfirmSend,
    Result
};

struct Credentials
{
    char wifiSsid[33];
    char wifiPassword[65];
    char smtpHost[96];
    char imapHost[96];
    char sender[96];
    char appPassword[97];
};

struct MailSummary
{
    uint32_t uid;
    char sender[82];
    char subject[82];
    char date[40];
};

struct Envelope
{
    uint8_t magic[16];
    uint8_t salt[16];
    uint8_t nonce[12];
    uint8_t ciphertext[sizeof(Credentials)];
    uint8_t tag[16];
};

Screen screen = Screen::Splash;
Preferences preferences;
Credentials credentials{};
Credentials newCredentials{};
std::array<MailSummary, kInboxLimit> inboxMessages{};
std::array<char, kMaxDecodedBodyBytes + 1> openedBody{};
uint8_t inboxCount = 0;
uint8_t selectedMail = 0;
size_t bodyScroll = 0;
std::array<char, 97> input{};
size_t inputLength = 0;
uint8_t formStep = 0;
uint8_t menuSelection = 0;
uint8_t keyboardModifiers = 0;
uint8_t keyboardAddress = kKeyboardAddress;
bool keyboardReady = false;
uint32_t wifiConnectStarted = 0;
bool credentialsExist = false;
bool wifiConnected = false;
bool sendInProgress = false;
bool sendSucceeded = false;
bool unlockFailed = false;
bool editingExisting = false;
String firstPassphrase;
String sessionPassphrase;
String recipient;
String subject;
String body;
String resultMessage;
uint32_t lastSuccessfulSendMs = 0;
bool hasSentThisBoot = false;

bool writeReg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

int readReg(uint8_t reg)
{
    Wire.beginTransmission(keyboardAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(keyboardAddress, static_cast<uint8_t>(1)) != 1) {
        return -1;
    }
    return Wire.read();
}

constexpr std::array<uint8_t, 3> kMatrixOutputPins = {8, 9, 11};
constexpr std::array<uint8_t, 7> kMatrixInputPins = {13, 15, 3, 4, 5, 6, 7};
enum class KeyboardDriver : uint8_t { None, GpioMatrix74HC138, Tca8418I2c };
KeyboardDriver keyboardDriver = KeyboardDriver::None;
std::array<bool, 56> previousKeyStates{};
uint32_t lastKeyScanMs = 0;
uint8_t stickyModifiers = 0;
uint8_t heldModifiers = 0;

void setMatrixOutput(uint8_t output)
{
    output &= 0x07;
    digitalWrite(kMatrixOutputPins[0], (output & 0x01) ? HIGH : LOW);
    digitalWrite(kMatrixOutputPins[1], (output & 0x02) ? HIGH : LOW);
    digitalWrite(kMatrixOutputPins[2], (output & 0x04) ? HIGH : LOW);
}

uint8_t readMatrixInputs()
{
    uint8_t mask = 0;
    for (size_t i = 0; i < kMatrixInputPins.size(); ++i) {
        if (digitalRead(kMatrixInputPins[i]) == LOW) {
            mask |= static_cast<uint8_t>(1U << i);
        }
    }
    return mask;
}

void initGpioMatrixKeyboard()
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

bool initKeyboard()
{
    Wire.begin(8, 9, 400000);
    Wire.beginTransmission(kKeyboardAddress);
    if (Wire.endTransmission() == 0) {
        keyboardAddress = kKeyboardAddress;
        constexpr uint8_t registers[][2] = {
            {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x20, 0xFF}, {0x21, 0xFF}, {0x22, 0xFF},
            {0x26, 0x00}, {0x27, 0x00}, {0x28, 0x00}, {0x1A, 0xFF}, {0x1B, 0xFF}, {0x1C, 0xFF},
            {0x1D, 0x7F}, {0x1E, 0xFF}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00}, {0x02, 0x03}};
        bool ok = true;
        for (const auto &entry : registers) {
            if (!writeReg(entry[0], entry[1])) {
                ok = false;
                break;
            }
        }
        if (ok) {
            while (readReg(kKeyCountRegister) > 0) {
                (void)readReg(kKeyEventRegister);
            }
            keyboardDriver = KeyboardDriver::Tca8418I2c;
            return true;
        }
    }
    Wire.end();

    initGpioMatrixKeyboard();
    return true;
}

void animateSplash()
{
    constexpr uint8_t frames = 28;
    const int16_t cx = M5.Display.width() / 2;
    for (uint8_t frame = 0; frame < frames; ++frame) {
        const int16_t pulse = static_cast<int16_t>(frame % 8);
        const int16_t radius = static_cast<int16_t>(37 + (pulse < 4 ? pulse : 7 - pulse));
        M5.Display.fillScreen(kBackground);
        M5.Display.drawRect(5, 5, M5.Display.width() - 10, M5.Display.height() - 10, kCyan);
        M5.Display.drawFastHLine(8, 12, static_cast<int32_t>((M5.Display.width() - 16) * frame / frames), kYellow);
        M5.Display.fillCircle(cx, 47, radius, kCyan);
        M5.Display.fillRect(cx - radius, 47, radius * 2, 43, kBackground);
        M5.Display.fillRect(cx - radius + 3, 49, radius * 2 - 6, 34, kCyan);
        M5.Display.fillTriangle(cx - radius + 3, 79, cx - 3, 79, cx - radius / 2, 100, kCyan);
        M5.Display.fillTriangle(cx + 3, 79, cx + radius - 3, 79, cx + radius / 2, 100, kCyan);
        M5.Display.fillCircle(cx - 13, 61, 4, kYellow);
        M5.Display.fillCircle(cx + 13, 61, 4, kYellow);
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(kYellow, kBackground);
        M5.Display.setTextSize(2);
        M5.Display.drawString("GHOST", cx, 111);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(kCyan, kBackground);
        M5.Display.drawString("PinoyUnknown", cx, 125);
        delay(28);
    }
}

bool deriveKey(const String &passphrase, const uint8_t *salt, uint8_t key[32])
{
    const mbedtls_md_info_t *digest = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (digest == nullptr) {
        return false;
    }
    mbedtls_md_context_t context;
    mbedtls_md_init(&context);
    const bool ready = mbedtls_md_setup(&context, digest, 1) == 0;
    const int result = ready ? mbedtls_pkcs5_pbkdf2_hmac(
                                   &context, reinterpret_cast<const uint8_t *>(passphrase.c_str()),
                                   passphrase.length(), salt, 16, kPbkdfIterations, 32, key)
                             : -1;
    mbedtls_md_free(&context);
    return result == 0;
}

bool encryptCredentials(const Credentials &plain, const String &passphrase)
{
    Envelope envelope{};
    memcpy(envelope.magic, kEnvelopeMagic.data(), kEnvelopeMagic.size());
    esp_fill_random(envelope.salt, sizeof(envelope.salt));
    esp_fill_random(envelope.nonce, sizeof(envelope.nonce));
    uint8_t key[32]{};
    if (!deriveKey(passphrase, envelope.salt, key)) {
        mbedtls_platform_zeroize(key, sizeof(key));
        return false;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int result = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (result == 0) {
        result = mbedtls_gcm_crypt_and_tag(
            &gcm, MBEDTLS_GCM_ENCRYPT, sizeof(plain), envelope.nonce, sizeof(envelope.nonce),
            reinterpret_cast<const uint8_t *>(kAad), sizeof(kAad) - 1,
            reinterpret_cast<const uint8_t *>(&plain), envelope.ciphertext, sizeof(envelope.tag),
            envelope.tag);
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));
    const size_t written =
        result == 0 ? preferences.putBytes("credential", &envelope, sizeof(envelope)) : 0;
    mbedtls_platform_zeroize(&envelope, sizeof(envelope));
    return written == sizeof(Envelope);
}

bool decryptCredentials(const String &passphrase, Credentials &plain)
{
    if (preferences.getBytesLength("credential") != sizeof(Envelope)) {
        return false;
    }
    Envelope envelope{};
    if (preferences.getBytes("credential", &envelope, sizeof(envelope)) != sizeof(envelope) ||
        memcmp(envelope.magic, kEnvelopeMagic.data(), kEnvelopeMagic.size()) != 0) {
        mbedtls_platform_zeroize(&envelope, sizeof(envelope));
        return false;
    }

    uint8_t key[32]{};
    if (!deriveKey(passphrase, envelope.salt, key)) {
        mbedtls_platform_zeroize(key, sizeof(key));
        mbedtls_platform_zeroize(&envelope, sizeof(envelope));
        return false;
    }

    Credentials decrypted{};
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int result = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (result == 0) {
        result = mbedtls_gcm_auth_decrypt(
            &gcm, sizeof(decrypted), envelope.nonce, sizeof(envelope.nonce),
            reinterpret_cast<const uint8_t *>(kAad), sizeof(kAad) - 1, envelope.tag,
            sizeof(envelope.tag), envelope.ciphertext, reinterpret_cast<uint8_t *>(&decrypted));
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));
    mbedtls_platform_zeroize(&envelope, sizeof(envelope));
    if (result != 0 || memchr(decrypted.wifiSsid, '\0', sizeof(decrypted.wifiSsid)) == nullptr ||
        memchr(decrypted.wifiPassword, '\0', sizeof(decrypted.wifiPassword)) == nullptr ||
        memchr(decrypted.smtpHost, '\0', sizeof(decrypted.smtpHost)) == nullptr ||
        memchr(decrypted.imapHost, '\0', sizeof(decrypted.imapHost)) == nullptr ||
        memchr(decrypted.sender, '\0', sizeof(decrypted.sender)) == nullptr ||
        memchr(decrypted.appPassword, '\0', sizeof(decrypted.appPassword)) == nullptr) {
        mbedtls_platform_zeroize(&decrypted, sizeof(decrypted));
        return false;
    }
    plain = decrypted;
    mbedtls_platform_zeroize(&decrypted, sizeof(decrypted));
    return true;
}

bool validEmail(const String &address)
{
    const int at = address.indexOf('@');
    if (address.length() < 5 || address.length() > 95 || at < 1 || at != address.lastIndexOf('@') ||
        at == static_cast<int>(address.length()) - 1) {
        return false;
    }
    const String local = address.substring(0, at);
    const String domain = address.substring(at + 1);
    if (local.startsWith(".") || local.endsWith(".") || local.indexOf("..") >= 0 ||
        domain.startsWith(".") || domain.endsWith(".") || domain.indexOf("..") >= 0) {
        return false;
    }
    for (size_t i = 0; i < local.length(); ++i) {
        const char c = local[i];
        const bool alphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9');
        if (!alphaNumeric && strchr(".!#$%&'*+-/=?^_`{|}~", c) == nullptr) {
            return false;
        }
    }
    for (size_t i = 0; i < domain.length(); ++i) {
        const char c = domain[i];
        const bool alphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9');
        if (!alphaNumeric && c != '.' && c != '-') {
            return false;
        }
        if (c == '-' && (i == 0 || i + 1 == domain.length() || domain[i - 1] == '.' ||
                         domain[i + 1] == '.')) {
            return false;
        }
    }
    if (domain.indexOf('.') < 1) {
        return false;
    }
    return true;
}

bool validSmtpHost(const char *host)
{
    if (host == nullptr || strlen(host) < 3 || strlen(host) > 95 || host[0] == '.' || host[0] == '-' ||
        host[strlen(host) - 1] == '.' || host[strlen(host) - 1] == '-') {
        return false;
    }
    for (const char *cursor = host; *cursor != '\0'; ++cursor) {
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') || *cursor == '.' || *cursor == '-')) {
            return false;
        }
        if (*cursor == '.' && (cursor[1] == '.' || cursor[1] == '-')) {
            return false;
        }
        if (*cursor == '-' && cursor > host && cursor[-1] == '.') {
            return false;
        }
    }
    return true;
}

bool validCredentials(const Credentials &value)
{
    return value.wifiSsid[0] != '\0' && validSmtpHost(value.smtpHost) &&
           validSmtpHost(value.imapHost) && validEmail(String(value.sender)) &&
           value.appPassword[0] != '\0';
}

void clearInput()
{
    mbedtls_platform_zeroize(input.data(), input.size());
    inputLength = 0;
}

void clearSessionSecrets()
{
    mbedtls_platform_zeroize(&credentials, sizeof(credentials));
    mbedtls_platform_zeroize(&newCredentials, sizeof(newCredentials));
    firstPassphrase = "";
    sessionPassphrase = "";
    clearInput();
    recipient = "";
    subject = "";
    body = "";
    mbedtls_platform_zeroize(inboxMessages.data(), sizeof(inboxMessages));
    mbedtls_platform_zeroize(openedBody.data(), openedBody.size());
    inboxCount = 0;
    selectedMail = 0;
    WiFi.disconnect(false, false);
    wifiConnected = false;
    screen = Screen::Unlock;
}

void startConnect()
{
    WiFi.disconnect(false, false);
    WiFi.persistent(false);
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(credentials.wifiSsid, credentials.wifiPassword);
    wifiConnected = false;
    wifiConnectStarted = millis();
    screen = Screen::Menu;
    resultMessage = "Connecting to saved Wi-Fi";
}

bool synchronizeClock()
{
    if (time(nullptr) >= 1735689600) {
        return true;
    }
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < 12000) {
        if (time(nullptr) >= 1735689600) {
            return true;
        }
        delay(100);
    }
    return false;
}

bool tlsWriteAll(esp_tls_t *tls, const void *data, size_t length)
{
    const auto *bytes = static_cast<const uint8_t *>(data);
    size_t written = 0;
    while (written < length) {
        const ssize_t count = esp_tls_conn_write(tls, bytes + written, length - written);
        if (count <= 0) {
            return false;
        }
        written += static_cast<size_t>(count);
    }
    return true;
}

bool waitForReply(esp_tls_t *tls, int expectedA, int expectedB = 0)
{
    char line[kMaxReplyBytes]{};
    size_t used = 0;
    uint32_t started = millis();
    uint8_t code = 0;
    while (static_cast<uint32_t>(millis() - started) < 15000) {
        uint8_t byte = 0;
        const ssize_t count = esp_tls_conn_read(tls, &byte, 1);
        if (count <= 0) {
            delay(5);
            continue;
        }
        if (byte == '\r') {
            continue;
        }
        if (byte != '\n') {
            if (used + 1 < sizeof(line)) {
                line[used++] = static_cast<char>(byte);
            }
            continue;
        }
        line[used] = '\0';
        if (used >= 3 && line[0] >= '0' && line[0] <= '5' && line[1] >= '0' && line[1] <= '9' &&
            line[2] >= '0' && line[2] <= '9') {
            code = static_cast<uint8_t>((line[0] - '0') * 100 + (line[1] - '0') * 10 + line[2] - '0');
            if (used < 4 || line[3] != '-') {
                return code == expectedA || (expectedB != 0 && code == expectedB);
            }
        }
        used = 0;
    }
    return false;
}

bool sendCommand(esp_tls_t *tls, const char *command, int expectedA, int expectedB = 0)
{
    const size_t length = strlen(command);
    return tlsWriteAll(tls, command, length) && tlsWriteAll(tls, "\r\n", 2) &&
           waitForReply(tls, expectedA, expectedB);
}

bool readTlsLine(esp_tls_t *tls, char *line, size_t capacity, uint32_t started)
{
    size_t used = 0;
    while (static_cast<uint32_t>(millis() - started) < 15000) {
        uint8_t byte = 0;
        const ssize_t count = esp_tls_conn_read(tls, &byte, 1);
        if (count <= 0) {
            delay(2);
            continue;
        }
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            line[used] = '\0';
            return true;
        }
        if (used + 1 >= capacity) {
            return false;
        }
        line[used++] = static_cast<char>(byte);
    }
    return false;
}

bool readTlsLiteral(esp_tls_t *tls, char *output, size_t capacity, size_t literalSize, size_t &stored,
                   uint32_t started)
{
    stored = 0;
    for (size_t i = 0; i < literalSize; ++i) {
        uint8_t byte = 0;
        bool received = false;
        while (static_cast<uint32_t>(millis() - started) < 15000) {
            if (esp_tls_conn_read(tls, &byte, 1) == 1) {
                received = true;
                break;
            }
            delay(2);
        }
        if (!received) {
            return false;
        }
        if (stored + 1 < capacity) {
            output[stored++] = static_cast<char>(byte);
        }
    }
    if (capacity > 0) {
        output[stored] = '\0';
    }
    return true;
}

bool imapCommand(esp_tls_t *tls, const char *tag, const char *command, char *literal,
                 size_t literalCapacity, size_t &literalStored, char *searchLine, size_t searchCapacity,
                 uint32_t *existsCount = nullptr)
{
    literalStored = 0;
    if (literalCapacity > 0) {
        literal[0] = '\0';
    }
    if (searchCapacity > 0) {
        searchLine[0] = '\0';
    }
    if (!tlsWriteAll(tls, command, strlen(command)) || !tlsWriteAll(tls, "\r\n", 2)) {
        return false;
    }

    char line[2048];
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < 15000) {
        if (!readTlsLine(tls, line, sizeof(line), started)) {
            return false;
        }
        if (strncmp(line, "* SEARCH ", 9) == 0 && searchCapacity > 0) {
            strlcpy(searchLine, line + 9, searchCapacity);
        } else if (existsCount != nullptr && line[0] == '*' && strstr(line, " EXISTS") != nullptr) {
            *existsCount = strtoul(line + 2, nullptr, 10);
        }

        const size_t tagLength = strlen(tag);
        if (strncmp(line, tag, tagLength) == 0 && line[tagLength] == ' ') {
            return strncmp(line + tagLength + 1, "OK", 2) == 0;
        }

        const char *brace = strrchr(line, '{');
        if (brace != nullptr) {
            char *end = nullptr;
            const unsigned long literalLength = strtoul(brace + 1, &end, 10);
            if (end != brace + 1 && *end == '}' && end[1] == '\0') {
                size_t stored = 0;
                if (!readTlsLiteral(tls, literal, literalCapacity, literalLength, stored, started)) {
                    return false;
                }
                literalStored = stored;
            }
        }
    }
    return false;
}

bool sendBase64(esp_tls_t *tls, const char *text, int expected)
{
    std::array<uint8_t, 196> encoded{};
    size_t length = 0;
    if (mbedtls_base64_encode(encoded.data(), encoded.size() - 2, &length,
                              reinterpret_cast<const uint8_t *>(text), strlen(text)) != 0) {
        mbedtls_platform_zeroize(encoded.data(), encoded.size());
        return false;
    }
    encoded[length++] = '\r';
    encoded[length++] = '\n';
    const bool ok = tlsWriteAll(tls, encoded.data(), length) && waitForReply(tls, expected);
    mbedtls_platform_zeroize(encoded.data(), encoded.size());
    return ok;
}

bool writeMessageData(esp_tls_t *tls)
{
    char headers[512];
    const int length = snprintf(headers, sizeof(headers),
                                "From: <%s>\r\nTo: <%s>\r\nSubject: %s\r\n"
                                "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n"
                                "Content-Transfer-Encoding: 8bit\r\n\r\n",
                                credentials.sender, recipient.c_str(), subject.c_str());
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(headers) ||
        !tlsWriteAll(tls, headers, static_cast<size_t>(length))) {
        return false;
    }

    size_t start = 0;
    while (start <= body.length()) {
        size_t end = body.indexOf('\n', start);
        if (end == static_cast<size_t>(-1)) {
            end = body.length();
        }
        const size_t lineLength = end - start;
        if (lineLength > 0 && body[start] == '.' && !tlsWriteAll(tls, ".", 1)) {
            return false;
        }
        if (lineLength > 0 &&
            !tlsWriteAll(tls, body.c_str() + start, lineLength)) {
            return false;
        }
        if (!tlsWriteAll(tls, "\r\n", 2)) {
            return false;
        }
        if (end == body.length()) {
            break;
        }
        start = end + 1;
    }
    return tlsWriteAll(tls, ".\r\n", 3) && waitForReply(tls, 250);
}

bool sendEmail()
{
    if (!validCredentials(credentials)) {
        resultMessage = "Saved email settings are invalid; reconfigure the account";
        return false;
    }
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        resultMessage = "Wi-Fi is not connected";
        return false;
    }
    if (!synchronizeClock()) {
        resultMessage = "Internet time sync failed; TLS certificate checks need time";
        return false;
    }
    if (!validEmail(recipient) || !validEmail(String(credentials.sender)) ||
        recipient.indexOf('\r') >= 0 || recipient.indexOf('\n') >= 0 ||
        subject.indexOf('\r') >= 0 || subject.indexOf('\n') >= 0 ||
        body.length() == 0 || body.length() > kMaxMessageBytes) {
        resultMessage = "Invalid address or message";
        return false;
    }
    if (hasSentThisBoot && static_cast<uint32_t>(millis() - lastSuccessfulSendMs) < kSendCooldownMs) {
        resultMessage = "Please wait before sending again";
        return false;
    }

    esp_tls_cfg_t config{};
    config.crt_bundle_attach = arduino_esp_crt_bundle_attach;
    config.timeout_ms = 12000;
    config.skip_common_name = false;
    esp_tls_t *tls = esp_tls_init();
    if (tls == nullptr) {
        resultMessage = "Unable to allocate TLS session";
        return false;
    }

    bool ok = esp_tls_conn_new_sync(credentials.smtpHost, strlen(credentials.smtpHost), kSmtpsPort,
                                    &config, tls) == 1;
    ok = ok && waitForReply(tls, 220);
    ok = ok && sendCommand(tls, "EHLO ghost-firmware.local", 250);
    ok = ok && sendCommand(tls, "AUTH LOGIN", 334);
    ok = ok && sendBase64(tls, credentials.sender, 334);
    ok = ok && sendBase64(tls, credentials.appPassword, 235);

    char command[192];
    if (ok) {
        snprintf(command, sizeof(command), "MAIL FROM:<%s>", credentials.sender);
        ok = sendCommand(tls, command, 250);
    }
    if (ok) {
        snprintf(command, sizeof(command), "RCPT TO:<%s>", recipient.c_str());
        ok = sendCommand(tls, command, 250, 251);
    }
    ok = ok && sendCommand(tls, "DATA", 354);
    ok = ok && writeMessageData(tls);
    if (ok) {
        sendCommand(tls, "QUIT", 221);
        resultMessage = "Email accepted by SMTP server";
        lastSuccessfulSendMs = millis();
        hasSentThisBoot = true;
    } else {
        resultMessage = "SMTP/TLS send failed; check settings";
    }
    esp_tls_conn_destroy(tls);
    return ok;
}

bool connectImap(esp_tls_t *&tls)
{
    if (!wifiConnected || WiFi.status() != WL_CONNECTED || !validSmtpHost(credentials.imapHost)) {
        return false;
    }
    if (!synchronizeClock()) {
        resultMessage = "Internet time sync failed; TLS certificate checks need time";
        return false;
    }
    esp_tls_cfg_t config{};
    config.crt_bundle_attach = arduino_esp_crt_bundle_attach;
    config.timeout_ms = 12000;
    config.skip_common_name = false;
    tls = esp_tls_init();
    if (tls == nullptr ||
        esp_tls_conn_new_sync(credentials.imapHost, strlen(credentials.imapHost), 993, &config, tls) != 1) {
        if (tls != nullptr) {
            esp_tls_conn_destroy(tls);
            tls = nullptr;
        }
        return false;
    }
    char greeting[256];
    if (!readTlsLine(tls, greeting, sizeof(greeting), millis()) || strncmp(greeting, "* OK", 4) != 0) {
        esp_tls_conn_destroy(tls);
        tls = nullptr;
        return false;
    }
    String quotedPassword = "\"";
    for (const char *cursor = credentials.appPassword; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '"') {
            quotedPassword += '\\';
        }
        quotedPassword += *cursor;
    }
    quotedPassword += '"';
    char command[320];
    snprintf(command, sizeof(command), "A001 LOGIN \"%s\" %s", credentials.sender, quotedPassword.c_str());
    size_t literalSize = 0;
    if (!imapCommand(tls, "A001", command, nullptr, 0, literalSize, nullptr, 0)) {
        esp_tls_conn_destroy(tls);
        tls = nullptr;
        return false;
    }
    if (!imapCommand(tls, "A002", "A002 SELECT INBOX", nullptr, 0, literalSize, nullptr, 0,
                     nullptr)) {
        esp_tls_conn_destroy(tls);
        tls = nullptr;
        return false;
    }
    return true;
}

void disconnectImap(esp_tls_t *tls)
{
    if (tls == nullptr) {
        return;
    }
    size_t literalSize = 0;
    imapCommand(tls, "A999", "A999 LOGOUT", nullptr, 0, literalSize, nullptr, 0);
    esp_tls_conn_destroy(tls);
}

void copyHeaderValue(const char *headers, const char *field, char *destination, size_t capacity)
{
    destination[0] = '\0';
    const size_t fieldLength = strlen(field);
    const char *cursor = headers;
    while (*cursor != '\0') {
        if ((*cursor == '\r' && cursor[1] == '\n') || *cursor == '\n') {
            break;
        }
        const char *lineEnd = strstr(cursor, "\r\n");
        if (lineEnd == nullptr) {
            lineEnd = cursor + strlen(cursor);
        }
        if (strncasecmp(cursor, field, fieldLength) == 0 && cursor[fieldLength] == ':') {
            const char *value = cursor + fieldLength + 1;
            while (value < lineEnd && (*value == ' ' || *value == '\t')) {
                ++value;
            }
            const size_t length = std::min(static_cast<size_t>(lineEnd - value), capacity - 1);
            memcpy(destination, value, length);
            destination[length] = '\0';
            return;
        }
        if (*lineEnd == '\0') {
            break;
        }
        cursor = lineEnd + (lineEnd[0] == '\r' && lineEnd[1] == '\n' ? 2 : 1);
    }
    if (destination[0] == '\0') {
        strlcpy(destination, "(unknown)", capacity);
    }
}

bool loadInbox()
{
    inboxCount = 0;
    selectedMail = 0;
    esp_tls_t *tls = nullptr;
    if (!connectImap(tls)) {
        if (resultMessage.length() == 0) {
            resultMessage = "IMAP/TLS login failed; check saved settings";
        }
        return false;
    }

    char searchResults[2048]{};
    size_t literalSize = 0;
    if (!imapCommand(tls, "A003", "A003 UID SEARCH ALL", nullptr, 0, literalSize,
                     searchResults, sizeof(searchResults))) {
        disconnectImap(tls);
        resultMessage = "Could not search the inbox";
        return false;
    }

    std::array<uint32_t, kInboxLimit> latestUids{};
    size_t uidCount = 0;
    char *cursor = searchResults;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        char *end = nullptr;
        const unsigned long uid = strtoul(cursor, &end, 10);
        if (end == cursor) {
            break;
        }
        if (uidCount < latestUids.size()) {
            latestUids[uidCount++] = static_cast<uint32_t>(uid);
        } else {
            memmove(latestUids.data(), latestUids.data() + 1, (latestUids.size() - 1) * sizeof(uint32_t));
            latestUids.back() = static_cast<uint32_t>(uid);
        }
        cursor = end;
    }

    char headers[1024];
    for (size_t i = uidCount; i > 0; --i) {
        MailSummary &mail = inboxMessages[inboxCount];
        mail = {};
        mail.uid = latestUids[i - 1];
        char command[96];
        char tag[8];
        snprintf(tag, sizeof(tag), "A%03u", static_cast<unsigned>(4 + inboxCount));
        snprintf(command, sizeof(command), "%s UID FETCH %lu (BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])",
                 tag,
                 static_cast<unsigned long>(mail.uid));
        if (!imapCommand(tls, tag, command, headers, sizeof(headers), literalSize, nullptr, 0)) {
            disconnectImap(tls);
            resultMessage = "Could not read inbox headers";
            return false;
        }
        copyHeaderValue(headers, "From", mail.sender, sizeof(mail.sender));
        copyHeaderValue(headers, "Subject", mail.subject, sizeof(mail.subject));
        copyHeaderValue(headers, "Date", mail.date, sizeof(mail.date));
        ++inboxCount;
    }
    disconnectImap(tls);
    resultMessage = inboxCount == 0 ? "Inbox is empty" : "Inbox updated";
    return true;
}

const char *findIgnoreCase(const char *text, const char *needle)
{
    const size_t needleLength = strlen(needle);
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (strncasecmp(cursor, needle, needleLength) == 0) {
            return cursor;
        }
    }
    return nullptr;
}

size_t decodeTransferEncoding(const char *inputText, size_t inputLength, const char *encoding, char *output,
                              size_t outputCapacity)
{
    if (outputCapacity == 0) {
        return 0;
    }
    if (findIgnoreCase(encoding, "base64") != nullptr) {
        std::array<uint8_t, kMaxRawEmailBytes> compact{};
        size_t compactLength = 0;
        for (size_t i = 0; i < inputLength && compactLength < compact.size(); ++i) {
            if (!isspace(static_cast<unsigned char>(inputText[i]))) {
                compact[compactLength++] = static_cast<uint8_t>(inputText[i]);
            }
        }
        size_t decodedLength = 0;
        if (mbedtls_base64_decode(reinterpret_cast<uint8_t *>(output), outputCapacity - 1, &decodedLength,
                                  compact.data(), compactLength) != 0) {
            mbedtls_platform_zeroize(compact.data(), compact.size());
            return 0;
        }
        output[decodedLength] = '\0';
        mbedtls_platform_zeroize(compact.data(), compact.size());
        return decodedLength;
    }

    size_t outputLength = 0;
    for (size_t i = 0; i < inputLength && outputLength + 1 < outputCapacity; ++i) {
        if (findIgnoreCase(encoding, "quoted-printable") != nullptr && inputText[i] == '=' &&
            i + 2 < inputLength) {
            if (inputText[i + 1] == '\r' && inputText[i + 2] == '\n') {
                i += 2;
                continue;
            }
            char hex[3] = {inputText[i + 1], inputText[i + 2], '\0'};
            char *end = nullptr;
            const long value = strtol(hex, &end, 16);
            if (end == hex + 2) {
                output[outputLength++] = static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        output[outputLength++] = inputText[i];
    }
    output[outputLength] = '\0';
    return outputLength;
}

size_t extractPlainText(const char *raw, size_t rawLength, char *output, size_t outputCapacity)
{
    const char *separator = strstr(raw, "\r\n\r\n");
    if (separator == nullptr) {
        strlcpy(output, "Message body is unavailable.", outputCapacity);
        return strlen(output);
    }
    const char *bodyStart = separator + 4;
    const size_t bodyLength = rawLength - static_cast<size_t>(bodyStart - raw);
    std::array<char, 192> contentType{};
    std::array<char, 64> encoding{};
    copyHeaderValue(raw, "Content-Type", contentType.data(), contentType.size());
    copyHeaderValue(raw, "Content-Transfer-Encoding", encoding.data(), encoding.size());

    const char *selectedStart = bodyStart;
    size_t selectedLength = bodyLength;
    if (findIgnoreCase(contentType.data(), "multipart/") != nullptr) {
        const char *boundaryKey = findIgnoreCase(contentType.data(), "boundary=");
        if (boundaryKey != nullptr) {
            boundaryKey += 9;
            const char quote = (*boundaryKey == '"' || *boundaryKey == '\'') ? *boundaryKey++ : '\0';
            size_t boundaryLength = 0;
            while (boundaryKey[boundaryLength] != '\0' &&
                   (quote ? boundaryKey[boundaryLength] != quote
                          : boundaryKey[boundaryLength] != ';' && boundaryKey[boundaryLength] != ' ')) {
                ++boundaryLength;
            }
            if (boundaryLength > 0 && boundaryLength < 96) {
                char delimiter[100];
                snprintf(delimiter, sizeof(delimiter), "--%.*s", static_cast<int>(boundaryLength), boundaryKey);
                const char *part = strstr(bodyStart, delimiter);
                while (part != nullptr && static_cast<size_t>(part - raw) < rawLength) {
                    const char *partHeaders = strstr(part, "\r\n");
                    if (partHeaders == nullptr) {
                        break;
                    }
                    partHeaders += 2;
                    const char *partBody = strstr(partHeaders, "\r\n\r\n");
                    if (partBody == nullptr) {
                        break;
                    }
                    partBody += 4;
                    const char *nextPart = strstr(partBody, delimiter);
                    if (nextPart == nullptr) {
                        break;
                    }
                    const char *partType = findIgnoreCase(partHeaders, "Content-Type: text/plain");
                    const char *partHeaderEnd = strstr(partHeaders, "\r\n\r\n");
                    if (partType != nullptr && partHeaderEnd != nullptr && partType < partHeaderEnd) {
                        selectedStart = partBody;
                        selectedLength = static_cast<size_t>(nextPart - partBody);
                        std::array<char, 192> partHeaderBuffer{};
                        const size_t partHeadersLength =
                            std::min(static_cast<size_t>(partHeaderEnd - partHeaders),
                                     partHeaderBuffer.size() - 1);
                        memcpy(partHeaderBuffer.data(), partHeaders, partHeadersLength);
                        copyHeaderValue(partHeaderBuffer.data(), "Content-Transfer-Encoding", encoding.data(),
                                        encoding.size());
                        break;
                    }
                    part = nextPart;
                }
            }
        }
    }
    if (selectedLength > 0 && selectedStart[selectedLength - 1] == '\n') {
        --selectedLength;
    }
    if (selectedLength > 0 && selectedStart[selectedLength - 1] == '\r') {
        --selectedLength;
    }
    if (selectedLength == 0) {
        strlcpy(output, "(empty message)", outputCapacity);
        return strlen(output);
    }
    return decodeTransferEncoding(selectedStart, selectedLength, encoding.data(), output, outputCapacity);
}

bool loadMessageBody(size_t index)
{
    if (index >= inboxCount) {
        return false;
    }
    esp_tls_t *tls = nullptr;
    if (!connectImap(tls)) {
        if (resultMessage.length() == 0) {
            resultMessage = "IMAP/TLS login failed";
        }
        return false;
    }
    char raw[kMaxRawEmailBytes + 1]{};
    size_t literalSize = 0;
    char command[96];
    snprintf(command, sizeof(command), "A003 UID FETCH %lu (BODY.PEEK[]<0.%u>)",
             static_cast<unsigned long>(inboxMessages[index].uid), static_cast<unsigned>(kMaxRawEmailBytes));
    const bool fetched = imapCommand(tls, "A003", command, raw, sizeof(raw), literalSize, nullptr, 0);
    disconnectImap(tls);
    if (!fetched) {
        resultMessage = "Could not fetch message body";
        return false;
    }
    if (extractPlainText(raw, literalSize, openedBody.data(), openedBody.size()) == 0) {
        strlcpy(openedBody.data(), "Message body is too large or uses an unsupported encoding.",
                openedBody.size());
    }
    mbedtls_platform_zeroize(raw, sizeof(raw));
    resultMessage = "Message opened";
    return true;
}

void drawText(const char *text, int16_t x, int16_t y, int16_t color = kCyan)
{
    M5.Display.setTextColor(color, kBackground);
    M5.Display.drawString(text, x, y);
}

void drawBase(const char *title)
{
    M5.Display.fillScreen(kBackground);
    M5.Display.drawRect(2, 2, M5.Display.width() - 4, M5.Display.height() - 4, kCyan);
    M5.Display.drawFastHLine(5, 15, M5.Display.width() - 10, kYellow);
    M5.Display.setTextDatum(textdatum_t::top_left);
    drawText(title, 7, 5, kYellow);
}

void drawInputScreen(const char *title, const char *hint, bool secret = false)
{
    drawBase(title);
    drawText(hint, 7, 29);
    String visible;
    if (secret) {
        for (size_t i = 0; i < inputLength; ++i) {
            visible += '*';
        }
    } else {
        visible = input.data();
    }
    M5.Display.fillRect(6, 51, M5.Display.width() - 12, 24, 0x2104);
    drawText(visible.c_str(), 9, 57, kYellow);
    drawText("ENTER: NEXT  BACKSPACE: EDIT", 7, 88);
    drawText("ESC: CANCEL / BACK", 7, 105);
    if (resultMessage.length() > 0) {
        drawText(resultMessage.substring(0, 36).c_str(), 7, 121, kYellow);
    } else if (!keyboardReady) {
        drawText("Cardputer keyboard not found", 7, 121, kYellow);
    }
}

void drawScreen()
{
    M5.Display.setTextSize(1);
    switch (screen) {
    case Screen::Welcome:
        drawBase("EMAIL-FIRMWARE");
        drawText("Authenticated SMTP sender", 8, 30);
        drawText("TLS only; one recipient per message", 8, 49);
        drawText("Create a separate unlock passphrase.", 8, 68);
        drawText("At least 12 characters; never saved.", 8, 85);
        drawText("ENTER: BEGIN", 8, 109, kYellow);
        break;
    case Screen::CreatePassphrase:
        drawInputScreen("CREATE UNLOCK PHRASE", "Use 12+ characters", true);
        break;
    case Screen::ConfirmPassphrase:
        drawInputScreen("CONFIRM UNLOCK PHRASE", "Type the same phrase again", true);
        break;
    case Screen::Configure: {
        static constexpr const char *hints[] = {
            "Wi-Fi network name (SSID)", "Wi-Fi password; blank for open Wi-Fi",
            "SMTP host (implicit TLS port 465)", "IMAP host (TLS port 993)",
            "Sender account email", "Provider app password"};
        drawInputScreen("ACCOUNT SETUP", hints[formStep], formStep == 1 || formStep == 5);
        char step[32];
        snprintf(step, sizeof(step), "FIELD %u / 6", static_cast<unsigned>(formStep + 1));
        drawText(step, 7, 77, kYellow);
        break;
    }
    case Screen::Unlock:
        drawInputScreen("UNLOCK SAVED ACCOUNT", "Unlock phrase; not stored", true);
        if (unlockFailed) {
            drawText("Unlock failed", 7, 121, kYellow);
        }
        break;
    case Screen::Menu:
        drawBase("EMAIL-FIRMWARE");
        drawText(wifiConnected ? "WI-FI: CONNECTED" : "WI-FI: CONNECTING",
                 8, 25, wifiConnected ? kCyan : kYellow);
        drawText(menuSelection == 0 ? "> NEW EMAIL" : "  NEW EMAIL", 8, 51,
                 menuSelection == 0 ? kYellow : kCyan);
        drawText(menuSelection == 1 ? "> INBOX" : "  INBOX", 8, 68,
                 menuSelection == 1 ? kYellow : kCyan);
        drawText(menuSelection == 2 ? "> ACCOUNT SETTINGS" : "  ACCOUNT SETTINGS", 8, 85,
                 menuSelection == 2 ? kYellow : kCyan);
        drawText(menuSelection == 3 ? "> LOCK ACCOUNT" : "  LOCK ACCOUNT", 8, 102,
                 menuSelection == 3 ? kYellow : kCyan);
        drawText("W/S: MOVE  N/I/C/L: OPEN", 7, 122);
        break;
    case Screen::InboxLoading:
        drawBase("INBOX");
        drawText("Connecting to mailbox over TLS...", 8, 48, kYellow);
        drawText("Please wait", 8, 69);
        break;
    case Screen::Inbox:
        drawBase("INBOX - LATEST 8");
        if (inboxCount == 0) {
            drawText("No messages found", 8, 45, kYellow);
        }
        for (uint8_t i = 0; i < inboxCount; ++i) {
            const bool selected = i == selectedMail;
            const String subjectLine = String(inboxMessages[i].subject).substring(0, 31);
            const String row = String(selected ? "> " : "  ") + subjectLine;
            drawText(row.c_str(), 7, 24 + i * 12, selected ? kYellow : kCyan);
        }
        drawText("W/S: MOVE  ENTER: READ  R: REFRESH", 7, 122);
        break;
    case Screen::ReadingEmail: {
        drawBase("READ MESSAGE");
        const MailSummary &mail = inboxMessages[selectedMail];
        drawText(("FROM: " + String(mail.sender)).substring(0, 37).c_str(), 7, 23);
        drawText(("SUBJ: " + String(mail.subject)).substring(0, 37).c_str(), 7, 35, kYellow);
        drawText(String(mail.date).substring(0, 37).c_str(), 7, 47);
        size_t cursor = bodyScroll;
        for (int row = 0; row < 6 && cursor < openedBody.size() && openedBody[cursor] != '\0'; ++row) {
            while (openedBody[cursor] == '\r' || openedBody[cursor] == '\n') {
                ++cursor;
            }
            if (openedBody[cursor] == '\0') {
                break;
            }
            const size_t lineStart = cursor;
            size_t lineLength = 0;
            while (openedBody[cursor] != '\0' && openedBody[cursor] != '\r' &&
                   openedBody[cursor] != '\n' && lineLength < 37) {
                ++cursor;
                ++lineLength;
            }
            const String line(openedBody.data() + lineStart);
            drawText(line.substring(0, lineLength).c_str(), 7, 62 + row * 10);
            if (openedBody[cursor] == '\r' || openedBody[cursor] == '\n') {
                ++cursor;
            }
        }
        drawText("W/S: SCROLL  ESC: INBOX", 7, 122, kYellow);
        break;
    }
    case Screen::ComposeTo:
        drawInputScreen("NEW EMAIL: RECIPIENT", "One recipient only");
        break;
    case Screen::ComposeSubject:
        drawInputScreen("NEW EMAIL: SUBJECT", "No line breaks; max 78 chars");
        break;
    case Screen::ComposeBody:
        drawInputScreen("NEW EMAIL: MESSAGE", "Max 600 chars; ENTER adds line");
        drawText("SHIFT+ENTER submits message", 7, 121);
        break;
    case Screen::Review:
        drawBase("REVIEW EMAIL");
        drawText(("FROM: " + String(credentials.sender)).c_str(), 7, 27);
        drawText(("TO: " + recipient).c_str(), 7, 43);
        drawText(("SUBJECT: " + subject).substring(0, 31).c_str(), 7, 59);
        drawText(("BODY: " + body).substring(0, 31).c_str(), 7, 75);
        drawText("ENTER: CONFIRM SEND", 7, 98, kYellow);
        drawText("ESC: CANCEL", 7, 115);
        break;
    case Screen::ConfirmSend:
        drawBase("CONFIRM SMTP SEND");
        drawText("Send this message to exactly one", 8, 33);
        drawText("recipient using your account?", 8, 50);
        drawText(("TO: " + recipient).substring(0, 30).c_str(), 8, 71);
        drawText("Y: SEND   N / ESC: CANCEL", 8, 100, kYellow);
        break;
    case Screen::Result:
        drawBase(sendInProgress ? "WORKING" : "RESULT");
        drawText(sendInProgress ? "Connecting via verified TLS..." : resultMessage.c_str(), 8, 44,
                 sendSucceeded ? kCyan : kYellow);
        drawText("ENTER / ESC: CONTINUE", 8, 88);
        break;
    case Screen::Splash:
        break;
    }
}

void refreshInbox()
{
    resultMessage = "";
    if (!wifiConnected) {
        resultMessage = "Connect to Wi-Fi before opening the inbox";
        sendSucceeded = false;
        screen = Screen::Result;
        return;
    }
    screen = Screen::InboxLoading;
    drawScreen();
    const bool loaded = loadInbox();
    sendSucceeded = loaded;
    screen = loaded ? Screen::Inbox : Screen::Result;
    drawScreen();
}

void openSelectedMail()
{
    if (inboxCount == 0 || selectedMail >= inboxCount) {
        return;
    }
    screen = Screen::InboxLoading;
    drawScreen();
    resultMessage = "";
    mbedtls_platform_zeroize(openedBody.data(), openedBody.size());
    bodyScroll = 0;
    const bool loaded = loadMessageBody(selectedMail);
    sendSucceeded = loaded;
    screen = loaded ? Screen::ReadingEmail : Screen::Result;
    drawScreen();
}

void beginSetup()
{
    formStep = 0;
    editingExisting = credentialsExist;
    resultMessage = "";
    newCredentials = {};
    clearInput();
    screen = Screen::Configure;
}

void saveSetup()
{
    if (!validCredentials(newCredentials)) {
        resultMessage = "Check account fields and try again";
        screen = Screen::Result;
        sendSucceeded = false;
        mbedtls_platform_zeroize(&newCredentials, sizeof(newCredentials));
        return;
    }
    preferences.begin("ghostmail", false);
    const String &activePassphrase = editingExisting ? sessionPassphrase : firstPassphrase;
    const bool saved = encryptCredentials(newCredentials, activePassphrase);
    preferences.end();
    if (!saved) {
        resultMessage = "Encrypted credential save failed";
        screen = Screen::Result;
        sendSucceeded = false;
        mbedtls_platform_zeroize(&newCredentials, sizeof(newCredentials));
        return;
    }
    credentials = newCredentials;
    if (!editingExisting) {
        sessionPassphrase = firstPassphrase;
    }
    mbedtls_platform_zeroize(&newCredentials, sizeof(newCredentials));
    firstPassphrase = "";
    credentialsExist = true;
    unlockFailed = false;
    startConnect();
}

void advanceConfiguration()
{
    if (inputLength == 0 && formStep != 1) {
        resultMessage = "This field cannot be empty";
        return;
    }
    char *target = nullptr;
    size_t capacity = 0;
    switch (formStep) {
    case 0:
        target = newCredentials.wifiSsid;
        capacity = sizeof(newCredentials.wifiSsid);
        break;
    case 1:
        target = newCredentials.wifiPassword;
        capacity = sizeof(newCredentials.wifiPassword);
        break;
    case 2:
        target = newCredentials.smtpHost;
        capacity = sizeof(newCredentials.smtpHost);
        if (!validSmtpHost(input.data())) {
            resultMessage = "Enter a DNS mail server hostname";
            return;
        }
        break;
    case 3:
        target = newCredentials.imapHost;
        capacity = sizeof(newCredentials.imapHost);
        if (!validSmtpHost(input.data())) {
            resultMessage = "Enter a DNS mail server hostname";
            return;
        }
        break;
    case 4:
        target = newCredentials.sender;
        capacity = sizeof(newCredentials.sender);
        if (!validEmail(String(input.data()))) {
            resultMessage = "Enter a valid sender email address";
            return;
        }
        break;
    case 5:
        target = newCredentials.appPassword;
        capacity = sizeof(newCredentials.appPassword);
        break;
    default:
        return;
    }
    if (inputLength >= capacity) {
        resultMessage = "Input is too long";
        return;
    }
    memcpy(target, input.data(), inputLength);
    target[inputLength] = '\0';
    clearInput();
    resultMessage = "";
    if (++formStep >= 6) {
        saveSetup();
    }
}

void beginCompose()
{
    resultMessage = "";
    recipient = "";
    subject = "";
    body = "";
    clearInput();
    screen = Screen::ComposeTo;
}

void advanceComposer()
{
    if (screen == Screen::ComposeTo) {
        recipient = String(input.data());
        if (!validEmail(recipient) || recipient.equalsIgnoreCase(credentials.sender)) {
            resultMessage = "Enter one valid address different from sender";
            return;
        }
        clearInput();
        screen = Screen::ComposeSubject;
    } else if (screen == Screen::ComposeSubject) {
        subject = String(input.data());
        if (subject.length() == 0 || subject.length() > 78) {
            resultMessage = "Subject must be 1-78 characters";
            return;
        }
        clearInput();
        screen = Screen::ComposeBody;
    } else if (screen == Screen::ComposeBody) {
        body = String(input.data());
        if (body.isEmpty() || body.length() > kMaxMessageBytes) {
            resultMessage = "Message must be 1-600 characters";
            return;
        }
        clearInput();
        screen = Screen::Review;
    }
    resultMessage = "";
}

void enterText(uint8_t key)
{
    if (key == kKeyBackspace) {
        if (inputLength > 0) {
            input[--inputLength] = '\0';
        }
        return;
    }
    if ((key == kKeySelect || key == '\n') && screen == Screen::ComposeBody) {
        if ((keyboardModifiers & 1U) != 0) {
            advanceComposer();
        } else if (inputLength < kMaxMessageBytes - 1) {
            input[inputLength++] = '\n';
            input[inputLength] = '\0';
        }
        return;
    }
    if (key == kKeySelect || key == '\n') {
        if (screen == Screen::Configure) {
            advanceConfiguration();
        } else if (screen == Screen::CreatePassphrase) {
            if (inputLength < kPassphraseMinimum) {
                resultMessage = "Use at least 12 characters";
                return;
            }
            firstPassphrase = String(input.data());
            clearInput();
            screen = Screen::ConfirmPassphrase;
        } else if (screen == Screen::ConfirmPassphrase) {
            if (firstPassphrase != String(input.data())) {
                firstPassphrase = "";
                clearInput();
                screen = Screen::CreatePassphrase;
                resultMessage = "Phrases did not match; try again";
                return;
            }
            clearInput();
            beginSetup();
        } else if (screen == Screen::Unlock) {
            String phrase(input.data());
            preferences.begin("ghostmail", true);
            const bool decrypted = decryptCredentials(phrase, credentials);
            const bool unlocked = decrypted && validCredentials(credentials);
            preferences.end();
            if (decrypted && !unlocked) {
                mbedtls_platform_zeroize(&credentials, sizeof(credentials));
            }
            if (unlocked) {
                sessionPassphrase = phrase;
            }
            phrase = "";
            clearInput();
            if (unlocked) {
                unlockFailed = false;
                startConnect();
            } else {
                unlockFailed = true;
            }
        } else {
            advanceComposer();
        }
        return;
    }
    const size_t maximum =
        screen == Screen::ComposeBody ? kMaxMessageBytes - 1 : input.size() - 1;
    if (key >= 0x20 && key <= 0x7E && inputLength < maximum) {
        input[inputLength++] = static_cast<char>(key);
        input[inputLength] = '\0';
    }
}

void onKey(uint8_t key)
{
    switch (screen) {
    case Screen::Welcome:
        if (key == kKeySelect || key == '\n') {
            screen = Screen::CreatePassphrase;
        }
        break;
    case Screen::CreatePassphrase:
    case Screen::ConfirmPassphrase:
    case Screen::Configure:
    case Screen::Unlock:
    case Screen::ComposeTo:
    case Screen::ComposeSubject:
    case Screen::ComposeBody:
        if (key == kKeyEscape && screen != Screen::CreatePassphrase && screen != Screen::Unlock) {
            clearInput();
            if (screen == Screen::Configure) {
                mbedtls_platform_zeroize(&newCredentials, sizeof(newCredentials));
            }
            screen = screen == Screen::ComposeTo || screen == Screen::ComposeSubject ||
                             screen == Screen::ComposeBody || (screen == Screen::Configure && editingExisting)
                         ? Screen::Menu
                         : Screen::Welcome;
            break;
        }
        enterText(key);
        break;
    case Screen::Menu:
        if (key == 'w' || key == 0xB5) {
            menuSelection = static_cast<uint8_t>((menuSelection + 3) % 4);
        } else if (key == 's' || key == 0xB6) {
            menuSelection = static_cast<uint8_t>((menuSelection + 1) % 4);
        } else if (key == 'l' || key == 'L' ||
                   (menuSelection == 3 && (key == kKeySelect || key == '\n'))) {
            clearSessionSecrets();
        } else if (key == 'n' || key == 'N' ||
                   (menuSelection == 0 && (key == kKeySelect || key == '\n'))) {
            beginCompose();
        } else if (key == 'i' || key == 'I' ||
                   (menuSelection == 1 && (key == kKeySelect || key == '\n'))) {
            refreshInbox();
        } else if (key == 'c' || key == 'C' ||
                   (menuSelection == 2 && (key == kKeySelect || key == '\n'))) {
            beginSetup();
        }
        break;
    case Screen::Inbox:
        if (key == 'w' || key == 0xB5) {
            if (inboxCount > 0) {
                selectedMail = static_cast<uint8_t>((selectedMail + inboxCount - 1) % inboxCount);
            }
        } else if (key == 's' || key == 0xB6) {
            if (inboxCount > 0) {
                selectedMail = static_cast<uint8_t>((selectedMail + 1) % inboxCount);
            }
        } else if (key == 'r' || key == 'R') {
            refreshInbox();
            break;
        } else if (key == kKeySelect || key == '\n') {
            openSelectedMail();
            break;
        } else if (key == kKeyEscape) {
            screen = Screen::Menu;
        }
        break;
    case Screen::ReadingEmail:
        if (key == kKeyEscape) {
            screen = Screen::Inbox;
        } else if (key == 'w' || key == 0xB5) {
            bodyScroll = bodyScroll > 80 ? bodyScroll - 80 : 0;
        } else if (key == 's' || key == 0xB6) {
            const size_t bodyLength = strnlen(openedBody.data(), openedBody.size());
            bodyScroll = std::min(bodyScroll + 80, bodyLength);
        }
        break;
    case Screen::Review:
        if (key == kKeySelect || key == '\n') {
            screen = Screen::ConfirmSend;
        } else if (key == kKeyEscape) {
            screen = Screen::Menu;
        }
        break;
    case Screen::ConfirmSend:
        if (key == 'y' || key == 'Y') {
            screen = Screen::Result;
            sendInProgress = true;
            drawScreen();
            sendSucceeded = sendEmail();
            sendInProgress = false;
        } else if (key == 'n' || key == 'N' || key == kKeyEscape) {
            screen = Screen::Menu;
        }
        break;
    case Screen::Result:
        if (key == kKeySelect || key == '\n' || key == kKeyEscape) {
            screen = credentialsExist ? Screen::Menu : Screen::Welcome;
        }
        break;
    case Screen::Splash:
    case Screen::InboxLoading:
        break;
    }
    drawScreen();
}

void pollTca8418Keyboard()
{
    const int count = readReg(kKeyCountRegister);
    if (count <= 0) {
        return;
    }
    for (int i = 0; i < std::min(count & 0x0F, static_cast<int>(kMaxEvents)); ++i) {
        const int event = readReg(static_cast<uint8_t>(kKeyEventRegister + i));
        if (event < 0) {
            return;
        }
        const uint8_t number = static_cast<uint8_t>(event & 0x7F);
        if (number == 0) {
            continue;
        }
        const uint8_t row = static_cast<uint8_t>((number - 1) / 10);
        const uint8_t column = static_cast<uint8_t>((number - 1) % 10);
        if (row >= 7 || column >= kColumns) {
            continue;
        }
        const uint8_t index = static_cast<uint8_t>(row * kColumns + column);
        const bool pressed = (event & 0x80) != 0;
        if (index == kKeyFunction) {
            if (pressed) {
                heldModifiers |= 0x02;
                stickyModifiers ^= 0x02;
            } else {
                heldModifiers &= ~0x02;
            }
            continue;
        }
        if (index == kKeyShift) {
            if (pressed) {
                heldModifiers |= 0x01;
                stickyModifiers ^= 0x01;
            } else {
                heldModifiers &= ~0x01;
            }
            continue;
        }
        if (pressed) {
            const uint8_t key = kKeyMap[index][(heldModifiers | stickyModifiers) & 0x03];
            stickyModifiers = 0;
            if (key != 0) {
                onKey(key);
            }
        }
    }
}

void pollGpioMatrixKeyboard()
{
    if (millis() - lastKeyScanMs < 18) {
        return;
    }
    lastKeyScanMs = millis();
    std::array<bool, 56> currentStates{};
    for (uint8_t i = 0; i < 8; ++i) {
        setMatrixOutput(i);
        delayMicroseconds(10);
        const uint8_t inputMask = readMatrixInputs();
        if (inputMask == 0) {
            continue;
        }
        for (uint8_t j = 0; j < 7; ++j) {
            if ((inputMask & (1U << j)) == 0) {
                continue;
            }
            const uint8_t x = (i > 3) ? static_cast<uint8_t>(2U * j) : static_cast<uint8_t>(2U * j + 1U);
            const uint8_t rawY = (i > 3) ? static_cast<uint8_t>(i - 4U) : i;
            const uint8_t y = static_cast<uint8_t>(3U - rawY);
            const uint8_t keyIndex = static_cast<uint8_t>(x * 4U + y);
            if (keyIndex < 56) {
                currentStates[keyIndex] = true;
            }
        }
    }
    heldModifiers = 0;
    if (currentStates[kKeyFunction]) {
        heldModifiers |= 0x02;
    }
    if (currentStates[kKeyShift]) {
        heldModifiers |= 0x01;
    }
    if (currentStates[kKeyFunction] && !previousKeyStates[kKeyFunction]) {
        stickyModifiers ^= 0x02;
    }
    if (currentStates[kKeyShift] && !previousKeyStates[kKeyShift]) {
        stickyModifiers ^= 0x01;
    }

    for (uint8_t keyIndex = 0; keyIndex < 56; ++keyIndex) {
        if (keyIndex == kKeyFunction || keyIndex == kKeyShift) {
            continue;
        }
        if (currentStates[keyIndex] && !previousKeyStates[keyIndex]) {
            const uint8_t key = kKeyMap[keyIndex][(heldModifiers | stickyModifiers) & 0x03];
            stickyModifiers = 0;
            if (key != 0) {
                previousKeyStates = currentStates;
                onKey(key);
                return;
            }
        }
    }
    previousKeyStates = currentStates;
}

void pollKeyboard()
{
    if (keyboardDriver == KeyboardDriver::Tca8418I2c) {
        pollTca8418Keyboard();
    } else if (keyboardDriver == KeyboardDriver::GpioMatrix74HC138) {
        pollGpioMatrixKeyboard();
    }
}

} // namespace

void setup()
{
    auto config = M5.config();
    M5.begin(config);
    M5.Display.setRotation(1);
    WiFi.persistent(false);
    preferences.begin("ghostmail", true);
    credentialsExist = preferences.getBytesLength("credential") == sizeof(Envelope);
    preferences.end();
    keyboardReady = initKeyboard();
    animateSplash();
    screen = credentialsExist ? Screen::Unlock : Screen::Welcome;
    drawScreen();
}

void loop()
{
    M5.update();
    pollKeyboard();
    if (M5.BtnA.wasPressed()) {
        if (screen == Screen::Welcome || screen == Screen::Review) {
            onKey(kKeySelect);
        } else if (screen == Screen::ConfirmSend) {
            onKey('y');
        } else if (screen == Screen::Menu || screen == Screen::Inbox) {
            onKey(kKeySelect);
        }
    }
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != wifiConnected) {
        wifiConnected = connected;
        if (screen == Screen::Menu) {
            drawScreen();
        }
    }
    if (!wifiConnected && screen == Screen::Menu &&
        static_cast<uint32_t>(millis() - wifiConnectStarted) >= 30000) {
        resultMessage = "Wi-Fi connection timed out; check saved settings";
        wifiConnectStarted = millis();
        screen = Screen::Result;
        sendSucceeded = false;
        drawScreen();
    }
    delay(10);
}
