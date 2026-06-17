/*
 * Fechadura IoT — ESP32 + MFRC522 + Servo + Sensor Magnético + MQTT
 * ====================================================================
 * Responsabilidade: leitura de sensores e atuação no hardware.
 * Decisões de acesso (autorização, banco de dados) ficam no backend.
 *
 * O que este código faz:
 *  1. Lê o UID do cartão/tag NFC via MFRC522
 *  2. Publica o UID lido em fechadura/uid (backend decide se autoriza)
 *  3. Escuta fechadura/cmd — se receber "abrir", aciona o servo
 *  4. Monitora o sensor magnético e publica o estado da porta
 *
 * Tópicos MQTT publicados:
 *  fechadura/uid    → UID lido pelo leitor  {"uid":"DEADBEEF"}
 *  fechadura/porta  → estado da porta       {"open":true/false}
 *
 * Tópicos MQTT assinados:
 *  fechadura/cmd    → comando do backend    "abrir" | "fechar"
 *
 * Pinagem ESP32 DevKit ↔ MFRC522
 * ────────────────────────────────
 *  3.3V   → 3.3V
 *  GND    → GND
 *  GPIO5  → SDA (SS/CS)
 *  GPIO18 → SCK
 *  GPIO23 → MOSI
 *  GPIO19 → MISO
 *  GPIO4  → RST
 *
 * Outros periféricos:
 *  GPIO2  → Sinal do servo
 *  GPIO13 → Sensor magnético (NC: porta fechada = LOW com pull-up)
 * ====================================================================
 */

#include <Arduino.h>
#include <ESP32Servo.h>
#include <MFRC522.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>

// Preencher antes de compilar

static constexpr char     WIFI_SSID[]      = "SUA_REDE";
static constexpr char     WIFI_PASS[]      = "SUA_SENHA";

static constexpr char     MQTT_HOST[]      = "192.168.1.100";
static constexpr uint16_t MQTT_PORT        = 1883;
static constexpr char     MQTT_CLIENT_ID[] = "fechadura-esp32";

// Tópicos
static constexpr char TOPIC_UID[]   = "fechadura/uid";
static constexpr char TOPIC_PORTA[] = "fechadura/porta";
static constexpr char TOPIC_CMD[]   = "fechadura/cmd";

// Pinagem
static constexpr uint8_t RC522_SS_PIN  = 5;
static constexpr uint8_t RC522_RST_PIN = 4;
static constexpr uint8_t SERVO_PIN     = 2;
static constexpr uint8_t DOOR_PIN      = 13;

// Servo
static constexpr int SERVO_LOCKED   = 0;
static constexpr int SERVO_UNLOCKED = 90;

// Tempo máximo que o servo fica aberto aguardando o backend fechar (ms)
static constexpr uint32_t LOCK_OPEN_MS = 5000;

// ═══════════════════════════════════════════════════════════════
//  ESTADO GLOBAL
// ═══════════════════════════════════════════════════════════════

MFRC522      rfid(RC522_SS_PIN, RC522_RST_PIN);
Servo        lockServo;
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

volatile bool doorChanged  = false;
bool          lastDoorOpen = false;
bool          lockIsOpen   = false;
uint32_t      lockOpenedAt = 0;

// ═══════════════════════════════════════════════════════════════
//  ISR — sensor magnético
// ═══════════════════════════════════════════════════════════════

void IRAM_ATTR onDoorChange() {
    doorChanged = true;
}

// ═══════════════════════════════════════════════════════════════
//  FUNÇÕES
// ═══════════════════════════════════════════════════════════════

bool doorIsOpen() {
    // Sensor NC + INPUT_PULLUP: LOW = fechada, HIGH = aberta
    return digitalRead(DOOR_PIN) == HIGH;
}

String uidToString(const MFRC522::Uid& uid) {
    String s;
    for (uint8_t i = 0; i < uid.size; i++) {
        if (uid.uidByte[i] < 0x10) s += '0';
        s += String(uid.uidByte[i], HEX);
    }
    s.toUpperCase();
    return s;
}

// ── Servo ────────────────────────────────────────────────────────

void setLock(bool open) {
    lockIsOpen = open;
    lockServo.write(open ? SERVO_UNLOCKED : SERVO_LOCKED);
    Serial.printf("[LOCK] %s\n", open ? "ABERTA" : "FECHADA");
}

// Failsafe: fecha automaticamente se o backend não enviar "fechar"
void updateAutoLock() {
    if (lockIsOpen && millis() - lockOpenedAt >= LOCK_OPEN_MS) {
        setLock(false);
        Serial.println(F("[LOCK] Auto-fechamento (timeout)."));
    }
}

// ── MQTT ─────────────────────────────────────────────────────────

void mqttPublish(const char* topic, const String& payload, bool retained = false) {
    if (!mqtt.connected()) return;
    bool ok = mqtt.publish(topic, payload.c_str(), retained);
    Serial.printf("[MQTT] %s → %s [%s]\n", topic, payload.c_str(), ok ? "OK" : "FAIL");
}

// Publica apenas o UID — quem decide se abre é o backend
void publishUID(const String& uid) {
    String p = "{\"uid\":\"";
    p += uid;
    p += "\"}";
    mqttPublish(TOPIC_UID, p);
}

// Publica estado da porta (retained: subscriber sempre recebe o último estado)
void publishDoorState(bool open) {
    String p = "{\"open\":";
    p += open ? "true" : "false";
    p += "}";
    mqttPublish(TOPIC_PORTA, p, true);
}

// Callback: recebe comandos do backend
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();
    msg.toLowerCase();

    Serial.printf("[MQTT] CMD: %s\n", msg.c_str());

    if (msg == "abrir") {
        setLock(true);
        lockOpenedAt = millis();
    } else if (msg == "fechar") {
        setLock(false);
    }
}

// ── WiFi ─────────────────────────────────────────────────────────

bool connectWiFi(uint32_t timeoutMs = 15000) {
    Serial.printf("[WiFi] Conectando a '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            Serial.println(F("\n[WiFi] Timeout."));
            return false;
        }
        delay(500);
        Serial.print('.');
    }
    Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ── MQTT connect ─────────────────────────────────────────────────

bool mqttConnect() {
    Serial.printf("[MQTT] Conectando a %s:%d ...", MQTT_HOST, MQTT_PORT);
    bool ok = mqtt.connect(MQTT_CLIENT_ID);
    if (ok) {
        Serial.println(F(" OK"));
        mqtt.subscribe(TOPIC_CMD);
    } else {
        Serial.printf(" FALHOU (rc=%d)\n", mqtt.state());
    }
    return ok;
}

// ── RC522 diagnóstico ─────────────────────────────────────────────

void checkReaderHealth() {
    byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RC522] Versão: 0x%02X\n", version);
    bool ok = rfid.PCD_PerformSelfTest();
    Serial.printf("[RC522] Self-test: %s\n", ok ? "PASS" : "FAIL");
    if (version == 0x00 || version == 0xFF || !ok)
        Serial.println(F("[RC522] AVISO: verifique 3.3V, GND e fiação SPI."));
    rfid.PCD_Init();  // obrigatório após self-test
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n=== Fechadura IoT — ESP32 + MFRC522 ==="));

    // Servo
    lockServo.attach(SERVO_PIN);
    setLock(false);

    // Sensor magnético
    pinMode(DOOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(DOOR_PIN), onDoorChange, CHANGE);
    lastDoorOpen = doorIsOpen();
    Serial.printf("[DOOR] Estado inicial: %s\n", lastDoorOpen ? "ABERTA" : "FECHADA");

    // SPI + RC522
    SPI.begin();
    rfid.PCD_Init();
    delay(50);
    checkReaderHealth();

    // WiFi + MQTT
    if (connectWiFi()) {
        mqtt.setServer(MQTT_HOST, MQTT_PORT);
        mqtt.setCallback(onMqttMessage);
        mqttConnect();
    }

    Serial.println(F("[READY] Sistema pronto."));
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════

void loop() {
    // ── MQTT: mantém conexão ────────────────────────────────────
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) {
            static uint32_t lastReconnect = 0;
            if (millis() - lastReconnect > 5000) {
                lastReconnect = millis();
                mqttConnect();
            }
        } else {
            mqtt.loop();
        }
    }

    // ── Sensor magnético ────────────────────────────────────────
    if (doorChanged) {
        doorChanged = false;
        bool open = doorIsOpen();
        if (open != lastDoorOpen) {
            lastDoorOpen = open;
            Serial.printf("[DOOR] Porta %s\n", open ? "ABERTA" : "FECHADA");
            publishDoorState(open);
        }
    }

    // ── Auto-fechamento (failsafe) ──────────────────────────────
    updateAutoLock();

    // ── NFC: lê UID e publica — sem decisão de acesso ──────────
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial())   return;

    String uid = uidToString(rfid.uid);
    Serial.printf("\n[NFC] UID: %s\n", uid.c_str());
    publishUID(uid);  // backend recebe e decide se envia "abrir"

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(500);
}