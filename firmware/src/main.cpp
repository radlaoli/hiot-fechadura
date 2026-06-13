/*
 * --------------------------------------------------------------------------------------------------------------------
 * Example sketch/program showing how to read data from a PICC to serial.
 * --------------------------------------------------------------------------------------------------------------------
 * This is a MFRC522 library example; for further details and other examples
 * see: https://github.com/miguelbalboa/rfid
 *
 * Example sketch/program showing how to read data from a PICC (that is: a RFID
 * Tag or Card) using a MFRC522 based RFID Reader on the Arduino SPI interface.
 *
 * When the Arduino and the MFRC522 module are connected (see the pin layout
 * below), load this sketch into Arduino IDE then verify/compile and upload it.
 * To see the output: use Tools, Serial Monitor of the IDE (hit Ctrl+Shft+M).
 * When you present a PICC (that is: a RFID Tag or Card) at reading distance of
 * the MFRC522 Reader/PCD, the serial output will show the ID/UID, type and any
 * data blocks it can read. Note: you may see "Timeout in communication"
 * messages when removing the PICC from reading distance too early.
 *
 * If your reader supports it, this sketch/program will read all the PICCs
 * presented (that is: multiple tag reading). So if you stack two or more PICCs
 * on top of each other and present them to the reader, it will first output all
 * details of the first and then the next PICC. Note that this may take some
 * time as all data blocks are dumped, so keep the PICCs at reading distance
 * until complete.
 *
 * @license Released into the public domain.
 *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro
 * Pro Micro Signal      Pin          Pin           Pin       Pin        Pin Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5 RST
 * SPI SS      SDA(SS)      10            53        D10        10 10 SPI MOSI
 * MOSI         11 / ICSP-4   51        D11        ICSP-4           16 SPI MISO
 * MISO         12 / ICSP-1   50        D12        ICSP-1           14 SPI SCK
 * SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 * More pin layouts for other boards can be found here:
 * https://github.com/miguelbalboa/rfid#pin-layout
 */

#define MFRC522_SPICLOCK 1000000u

#include <MFRC522.h>
#include <SPI.h>

// Portenta C33 uses SPI on MOSI=8, MISO=10, SCK=9, and CS=7 in the board core.
// RST can be any free GPIO; D6 is a simple choice if you are not using it.
#if defined(ARDUINO_PORTENTA_C33)
constexpr uint8_t CS_PIN = 7;
constexpr uint8_t RST_PIN = 6;
#else
constexpr uint8_t CS_PIN = 10;
constexpr uint8_t RST_PIN = 9;
#endif

MFRC522 mfrc522(CS_PIN, RST_PIN);  // Create MFRC522 instance

void reportReaderHealth() {
    byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.print(F("RC522 Version: 0x"));
    Serial.println(version, HEX);

    bool selfTestPassed = mfrc522.PCD_PerformSelfTest();
    Serial.print(F("RC522 self-test: "));
    Serial.println(selfTestPassed ? F("PASS") : F("FAIL"));

    if (version == 0x00 || version == 0xFF || !selfTestPassed) {
        Serial.println(
            F("RC522 not responding reliably. Check 3.3V, GND, SDA/SS, SCK, "
              "MOSI, MISO, "
              "and RST wiring."));
    }
}

void setup() {
    Serial.begin(9200);
    SPI.begin();
    mfrc522.PCD_Init();
    delay(4);
    mfrc522.PCD_DumpVersionToSerial();

    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    // Reset the loop if no new card present on the sensor/reader. This saves
    // the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    digitalWrite(LED_BUILTIN, LOW);

    // Dump debug info about the card; PICC_HaltA() is automatically called
    mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

    delay(4);
    digitalWrite(LED_BUILTIN, HIGH);
}