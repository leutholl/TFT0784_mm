#include "GT911.h"

#include "Arduino.h"
#include "GT911FW.h"
#include "Wire.h"  //BUFFER NEEDS TO BE HIGHER THAN TYPICAL 32 BYTES

#define I2C Wire

// Interrupt handling
volatile uint8_t GT911IRQ = 0;

#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR
#endif

TS_Point touches[5];

void FASTRUN _GT911_irq_handler() {
    // noInterrupts();
    GT911IRQ = 1;
    // interrupts();
}

// Implementation
GT911::GT911() {
}

void GT911::setHandler(void (*handler)(int8_t, GTPoint *)) {
    touchHandler = handler;
}

bool GT911::begin(uint8_t interruptPin, uint8_t resetPin, uint8_t addr) {
    intPin = interruptPin;
    rstPin = resetPin;
    i2cAddr = addr;

    // Take chip some time to start
    msSleep(300);
    bool result = reset();
    msSleep(200);

    return result;
}

bool GT911::reset() {
    msSleep(1);

    pinOut(intPin);
    pinOut(rstPin);

    pinHold(intPin);
    pinHold(rstPin);

    /* begin select I2C slave addr */

    /* T2: > 10ms */
    msSleep(11);

    /* HIGH: 0x28/0x29 (0x14 7bit), LOW: 0xBA/0xBB (0x5D 7bit) */
    pinSet(intPin, i2cAddr == GT911_I2C_ADDR_28);

    /* T3: > 100us */
    usSleep(110);
    pinIn(rstPin);
    // if (!pinCheck(rstPin, HIGH))
    //   return false;

    /* T4: > 5ms */
    msSleep(6);
    pinHold(intPin);
    /* end select I2C slave addr */

    /* T5: 50ms */
    msSleep(51);
    pinIn(intPin);  // INT pin has no pullups so simple set to floating input

    attachInterrupt(intPin, _GT911_irq_handler, RISING);
    //  detachInterrupt(intPin, _GT911_irq_handler);

    return true;
}

uint8_t GT911::calcChecksum(uint8_t *buf, uint8_t len) {
    uint8_t ccsum = 0;
    for (uint8_t i = 0; i < len; i++) {
        ccsum += buf[i];
    }
    ccsum = (~ccsum) + 1;
    return ccsum;
}

uint8_t GT911::readChecksum() {
    uint16_t aStart = GT_REG_CFG;
    uint16_t aStop = 0x80FE;
    uint8_t len = aStop - aStart + 1;
    uint8_t buf[len];

    read(aStart, buf, len);
    return calcChecksum(buf, len);
}

uint8_t GT911::fwResolution(uint16_t maxX, uint16_t maxY) {
    uint8_t i;
    uint8_t len = 0x8100 - GT_REG_CFG + 1;
    uint8_t cfg[len];
    read(GT_REG_CFG, cfg, len);
    /*
    Serial.println("Reading Config");
    for (i = 0; i<len; i++) {
      Serial.print(GT_REG_CFG+i, HEX);
      Serial.print(" : ");
      Serial.println(cfg[i], HEX);
      cfg[i] = g911xFW[i];
    }
    */
    cfg[1] = (maxX & 0xff);
    cfg[2] = (maxX >> 8);
    cfg[3] = (maxY & 0xff);
    cfg[4] = (maxY >> 8);
    cfg[len - 2] = calcChecksum(cfg, len - 2);
    cfg[len - 1] = 1;
    /*
    Serial.println("Writing Config");
    for (i = 0; i<len; i++) {
      Serial.print(GT_REG_CFG+i, HEX);
      Serial.print(" : ");
      Serial.println(cfg[i], HEX);

    }
    delay(300);
    */
    uint8_t error = write(GT_REG_CFG, cfg, len);
    delay(200);  // wait while storing in flash
    return error;
}

uint8_t GT911::writeConfig(GTConfig *cfg) {
    uint8_t len = 0x8100 - GT_REG_CFG + 1;
    uint8_t out[len];

    memcpy(out, cfg, len - 2);

    out[len - 2] = calcChecksum(out, len - 2);
    out[len - 1] = 1;
    /*
    Serial.println("Writing Config");
    for (int i = 0; i<len; i++) {
      Serial.print(GT_REG_CFG+i, HEX);
      Serial.print(" : ");
      Serial.println(out[i], HEX);
    }
    */

    // uint8_t error = write(GT_REG_CFG, out, len);

    delay(200);  // wait while storing in flash
    // return error;
    return 0;
}

GTConfig *GT911::readConfig() {
    read(GT_REG_CFG, (uint8_t *)&config, sizeof(config));
    return &config;
}

GTInfo *GT911::readInfo() {
    read(GT_REG_DATA, (uint8_t *)&info, sizeof(config));
    return &info;
}

void GT911::onIRQ() {
    int16_t readResult = readInput((uint8_t *)points);
    if (readResult < 0) {
        //    Serial.print(millis());
        //    Serial.printf(" Error: %d\n", readResult);
        return;
    }

    contacts = readResult;

    if (contacts > 0) {
        if (touchHandler) {
            // touchHandler(contacts, points);
            for (uint8_t i = 0; i < contacts; i++) {
                touches[i] = getPoint(i);
            }
        }

        /*
        Serial.print(millis());
        Serial.print(" Contacts: ");
        Serial.println(contacts);
        */

        /*
    for (uint8_t i = 0; i < contacts; i++) {
      Serial.print("C ");
      Serial.print(i);
      Serial.print(": #");
      Serial.print(points[i].trackId);
      Serial.print(" ");
      Serial.print(points[i].x1);
      Serial.print("-");
      Serial.print(points[i].x2);
      Serial.print(", ");
      Serial.print(points[i].y1);
      Serial.print("-");
      Serial.print(points[i].y2);
      Serial.print(" s");
      Serial.print(points[i].area1);
      Serial.print("-");
      Serial.print(points[i].area2);
      Serial.println();
    }
    */
    }

    write(GT911_READ_COORD_ADDR, 0);  // Must clear buffer status
}

int16_t GT911::readInput(uint8_t *data) {
    int touch_num;
    int error;

    uint8_t regState[1];
    error = read(GT911_READ_COORD_ADDR, regState, 1);
    if (error) return -error;
    if (!(regState[0] & 0x80)) return -100;  // Try again error
    touch_num = regState[0] & 0x0f;
    if (touch_num > 0) {
        error = read(GT911_READ_COORD_ADDR + 1, data, GT911_CONTACT_SIZE * (touch_num));
        if (error) return -error;
    }
    return touch_num;
}

//  ----- Utils -----
void GT911::i2cStart(uint16_t reg) {
    I2C.beginTransmission(i2cAddr);
    I2C.write(highByte(reg));
    I2C.write(lowByte(reg));
}

void GT911::i2cRestart() {
    I2C.endTransmission(false);
    I2C.beginTransmission(i2cAddr);
}

uint8_t GT911::i2cStop() {
    return I2C.endTransmission(true);
}

uint8_t GT911::write(uint16_t reg, uint8_t *buf, size_t len) {
    uint8_t error;
    uint16_t startPos = 0;

    // THIS CAN WRITE MORE THAN THE STANDART 32BYTE BUFFER ON I2C LIBRARIES
    // MAKE SURE THE BUFFER IS INCREASED IN THE UNDERLAYING I2C DRIVER
    while (startPos < len) {
        i2cStart(reg + startPos);
        startPos += I2C.write(buf + startPos, len - startPos);
        error = I2C.endTransmission();
        if (error) {
            Serial.print("Error writing at: ");
            Serial.print(startPos, HEX);
            return error;
        }
    }
    return 0;
}

uint8_t GT911::write(uint16_t reg, uint8_t buf) {
    i2cStart(reg);
    I2C.write(buf);
    return I2C.endTransmission();
}

uint8_t GT911::read(uint16_t reg, uint8_t *buf, size_t len) {
    uint8_t res;
    i2cStart(reg);
    res = I2C.endTransmission(false);
    if (res != GT911_OK) {
        return res;
    }
    uint16_t pos = 0;
    I2C.requestFrom(i2cAddr, len);  // will set stop bit after transaction ends
    while (I2C.available()) {
        buf[pos] = I2C.read();
        pos++;
    }
    return 0;
}

void GT911::pinOut(uint8_t pin) {
    pinMode(pin, OUTPUT);
}

void GT911::pinIn(uint8_t pin) {
    pinMode(pin, INPUT);
}

void GT911::pinSet(uint8_t pin, uint8_t level) {
    digitalWrite(pin, level);
}

void GT911::pinHold(uint8_t pin) {
    pinSet(pin, LOW);
}

bool GT911::pinCheck(uint8_t pin, uint8_t level) {
    return digitalRead(pin) == level;
}

void GT911::msSleep(uint16_t milliseconds) {
    delay(milliseconds);
}

void GT911::usSleep(uint16_t microseconds) {
    delayMicroseconds(microseconds);
}

//*********   P U B L I C   *****************

void GT911::loop() {
    // noInterrupts();
    uint8_t irq = GT911IRQ;
    GT911IRQ = 0;
    // interrupts();

    if (irq) {
        onIRQ();
    }
}

TS_Point GT911::getPoint(uint8_t n) {
    loop();
    if ((contacts == 0) || (n >= contacts)) {
        return TS_Point(0, 0, 0, 0);
    } else {
        uint8_t *point = (uint8_t *)(&points[n]);
        /*  point structure:
            point[0] - id
            point[1] - x low
            point[2] - x high
            point[3] - y low
            point[4] - y high
            point[5] - area low
            point[6] - area high
            point[7] - reserved
        */

        return TS_Point(                             // this assumes correct packing of struct
            (((uint16_t)point[2]) << 8) + point[1],  // x
            (((uint16_t)point[4]) << 8) + point[3],  // y
            (((uint16_t)point[6]) << 8) + point[5],  // area
            point[0]);                               // id
    }
}

uint8_t GT911::touched() {
    loop();
    return contacts;
}

TS_Point::TS_Point(void) {
    x = y = z = id = 0;
}

TS_Point::TS_Point(int16_t _x, int16_t _y, int16_t _z, int8_t _id) {
    x = _x;
    y = _y;
    z = _z;
    id = _id;
}
