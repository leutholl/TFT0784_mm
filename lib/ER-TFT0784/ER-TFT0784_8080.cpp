#include "ER-TFT0784_8080.h"
#include "SSD2828.h"
boolean ER_TFT0784_8080::begin(uint8_t baud_div) {

  // SSD2828 setup.
  // host GPIO pin numbers for the SSD2828 pin: CS, RST, SDI and SCK
  SSD2828 ssd2828 = SSD2828(_cs2828p, _rst2828p, _sdi2828p, _sck2828p);
  ssd2828.reset();
  delay(50); //LL
  ssd2828.initialize();
  delay(50); //LL
	ssd2828.release();
  delay(50); //LL
  Serial.println("SSD2828 initialized");
  return RA8876_8080::begin(baud_div);
}
