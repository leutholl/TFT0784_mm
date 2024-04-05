#ifndef ER_TFT0784_d
#define ER_TFT0784_d
#include "RA8876_8080.h"

#define FLEXIO2_MM_DMA 1

class ER_TFT0784_8080 : public RA8876_8080 {
	public:

#if (FLEXIO2_MM_DMA==1)
		ER_TFT0784_8080(          
								  const uint8_t cs_2828_pin = 31,
								  const uint8_t rst_2828_pin = 32,
								  const uint8_t sdi_2828_pin = 34,
								  const uint8_t sck_2828_pin = 35) :
				RA8876_8080(11,16,17) //default pin 10,11,12
		{
			_cs2828p  = cs_2828_pin;
			_rst2828p = rst_2828_pin;
			_sdi2828p = sdi_2828_pin;
			_sck2828p = sck_2828_pin;
		}
#else
		ER_TFT0784_8080(          
								  const uint8_t cs_2828_pin = 24,
								  const uint8_t rst_2828_pin = 25,
								  const uint8_t sdi_2828_pin = 26,
								  const uint8_t sck_2828_pin = 27) :

				RA8876_8080() //default pin 10,11,12
		{
			_cs2828p  = cs_2828_pin;
			_rst2828p = rst_2828_pin;
			_sdi2828p = sdi_2828_pin;
			_sck2828p = sck_2828_pin;
		}
#endif
		boolean begin(uint8_t baud_div);

	private:
		uint8_t _cs2828p;
		uint8_t _rst2828p;
		uint8_t _sdi2828p;
		uint8_t _sck2828p;
};
#endif

