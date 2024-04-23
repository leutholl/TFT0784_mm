#ifndef ER_TFT0784_d
#define ER_TFT0784_d
#include "RA8876_8080.h"
#include "SSD2828.h"

class ER_TFT0784_8080 : public RA8876_8080 {
   public:
    ER_TFT0784_8080(
        uint8_t _cs_2828_pin,
        uint8_t _rst_2828_pin,
        uint8_t _sdi_2828_pin,
        uint8_t _sck_2828_pin,
        uint8_t _8876_cs_pin,
        uint8_t _8876_dc_pin,
        uint8_t _8876_rst_pin,
        uint8_t _8876_wait_pin) : RA8876_8080(_8876_dc_pin,
                                              _8876_cs_pin,
                                              _8876_rst_pin,
                                              _8876_wait_pin),

                                  ssd2828(_cs_2828_pin, _rst_2828_pin, _sdi_2828_pin, _sck_2828_pin) {

                                  };

    boolean begin(uint8_t baud_div) {
        // SSD2828 setup.
        // host GPIO pin numbers for the SSD2828 pin: CS, RST, SDI and SCK
        // these pins are bitbanged and only used once during the initialization (here)
        // Some LCD screens need longer delays in ssd2828.initialize() (marked with //LL Wait Time)
        // The SSD2828 is picky on the video data sequence therefore RADIO chip
        // is started before initialization.

        delay(100);
        ssd2828.reset();
        delay(100);
        boolean res = RA8876_8080::begin(baud_div);
        delay(100);
        ssd2828.initialize();
        ssd2828.release();
        Serial.println("TFT initialized");
        return res;
    }

   private:
    SSD2828 ssd2828;
};
#endif
