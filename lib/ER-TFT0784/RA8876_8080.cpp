//**************************************************************//
//**************************************************************//
/*
 * Ra8876LiteTeensy.cpp
 * Modified Version of: File Name : RA8876_8080.cpp                                   
 *			Author    : RAiO Application Team                             
 *			Edit Date : 09/13/2017
 *			Version   : v2.0  1.modify bte_DestinationMemoryStartAddr bug 
 *                 			  2.modify ra8876SdramInitial Auto_Refresh
 *                 			  3.modify ra8876PllInitial 
 * 	  	      : For Teensy 3.x and T4
 *                    : By Warren Watson
 *                    : 06/07/2018 - 11/31/2019
 *                    : Copyright (c) 2017-2019 Warren Watson.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//**************************************************************//

#include "RA8876_8080.h"
#include "Arduino.h"



// Create a parameter save structure for all 10 screen pages	
tftSave_t	screenSave1,
			screenSave2,
			screenSave3,
			screenSave4,
			screenSave5,
			screenSave6,
			screenSave7,
			screenSave8,
			screenSave9;
			//screenSave10;
// Create pointers to each screen save structs
tftSave_t *screenPage1 = &screenSave1;
tftSave_t *screenPage2 = &screenSave2;
tftSave_t *screenPage3 = &screenSave3;
tftSave_t *screenPage4 = &screenSave4;
tftSave_t *screenPage5 = &screenSave5;
tftSave_t *screenPage6 = &screenSave6;
tftSave_t *screenPage7 = &screenSave7;
tftSave_t *screenPage8 = &screenSave8;
tftSave_t *screenPage9 = &screenSave9;
//tftSave_t *screenPage10 = &screenSave10; // Not used at this time


//**************************************************************//
// RA8876_8080()
//**************************************************************//
// Create RA8876 driver instance 8080 IF
//**************************************************************//
RA8876_8080::RA8876_8080(const uint8_t DCp, const uint8_t CSp, const uint8_t RSTp)
{
	_cs = CSp;
	_rst = RSTp;
    _dc = DCp;
Serial.print("_cs = ");
Serial.println(_cs,DEC);
}

FLASHMEM boolean RA8876_8080::begin(uint8_t baud_div) 
{ 
  Serial.printf("RA8876_8080::begin(%d)\n", baud_div);
  Serial.printf("RA8876_8080::_cs = %d\n", _cs);
  Serial.printf("RA8876_8080::_dc = %d\n", _dc);
  Serial.printf("RA8876_8080::_rst = %d\n", _rst);

  switch (baud_div) {
	case 1:  _baud_div = 240;
			  break;
    case 2:  _baud_div = 120;
              break;
    case 4:  _baud_div = 60;
              break;
    case 8:  _baud_div = 30;
              break;
    case 12: _baud_div = 20;
              break;
    case 20: _baud_div = 12;
              break;
    case 24: _baud_div = 10;
              break;
    case 30: _baud_div = 8;
              break;
    case 40: _baud_div = 6;
              break;
    case 60: _baud_div = 4;
              break;
    case 120: _baud_div = 2;
              break;
   default: _baud_div = 20; // 12Mhz
              break;           
  }
    pinMode(_cs, OUTPUT); // CS
    pinMode(_dc, OUTPUT); // DC
    pinMode(_rst, OUTPUT); // RST
    *(portControlRegister(_cs)) = 0xFF;
    *(portControlRegister(_dc)) = 0xFF;
    *(portControlRegister(_rst)) = 0xFF;
  
    digitalWriteFast(_cs, HIGH);
    digitalWriteFast(_dc, HIGH);
    digitalWriteFast(_rst, HIGH);

    delay(15);
    digitalWrite(_rst, LOW);
    delay(15);
    digitalWriteFast(_rst, HIGH);
    delay(100);

    FlexIO_Init();

	if(!checkIcReady())
		return false;

	//read ID code must disable pll, 01h bit7 set 0
    if(BUS_WIDTH == 16) {
	  lcdRegDataWrite(0x01,0x09);
	} else {
	  lcdRegDataWrite(0x01,0x08);
	}
	delay(1);
	if ((lcdRegDataRead(0xff) != 0x76)&&(lcdRegDataRead(0xff) != 0x77)) {
		return false;
    }

	// Initialize RA8876 to default settings
	if(!ra8876Initialize()) {
		return false;
    }

    return true;
}

FASTRUN void RA8876_8080::CSLow() 
{
  digitalWriteFast(_cs, LOW);       //Select TFT
}

FASTRUN void RA8876_8080::CSHigh() 
{
  digitalWriteFast(_cs, HIGH);       //Deselect TFT
}

FASTRUN void RA8876_8080::DCLow() 
{
  digitalWriteFast(_dc, LOW);       //Writing command to TFT
}

FASTRUN void RA8876_8080::DCHigh() 
{
  digitalWriteFast(_dc, HIGH);       //Writing data to TFT
}

FASTRUN void RA8876_8080::microSecondDelay()
{
  for (uint32_t i=0; i<99; i++) __asm__("nop\n\t");
}

FASTRUN void RA8876_8080::gpioWrite(){
  #if (FLEXIO2_MM_DMA)
	pFlex->setIOPinToFlexMode(10);
    pinMode(12, OUTPUT);
    digitalWriteFast(12, HIGH);
  #else
  pFlex->setIOPinToFlexMode(36); // ReConfig /WR pin
  pinMode(37, OUTPUT);  // Set /RD pin to output
  digitalWriteFast(37, HIGH); // Set /RD pin High
  #endif
}

FASTRUN void RA8876_8080::gpioRead(){
  #if (FLEXIO2_MM_DMA)
	pFlex->setIOPinToFlexMode(12);
    pinMode(10, OUTPUT);
    digitalWriteFast(10, HIGH);
  #else
    pFlex->setIOPinToFlexMode(37); // ReConfig /RD pin
    pinMode(36, OUTPUT);  // Set /WR pin to output
    digitalWriteFast(36, HIGH); // Set /WR pin High
  #endif
}

FASTRUN void RA8876_8080::FlexIO_Init()
{
  /* Get a FlexIO channel */
	#if (FLEXIO2_MM_DMA)
    pFlex = FlexIOHandler::flexIOHandler_list[1]; // use FlexIO2
	#else
	pFlex = FlexIOHandler::flexIOHandler_list[2]; // use FlexIO3
	#endif

    /* Pointer to the port structure in the FlexIO channel */
    p = &pFlex->port();
    /* Pointer to the hardware structure in the FlexIO channel */
    hw = &pFlex->hardware();

	#if (FLEXIO2_MM_DMA)
	/* Basic pin setup */
	pinMode(10, OUTPUT); // FlexIO3:0 WR
	pinMode(12, OUTPUT); // FlexIO3:1 RD
	pinMode(40, OUTPUT); // FlexIO3:4 D0
	pinMode(41, OUTPUT); // FlexIO3:5 |
	pinMode(42, OUTPUT); // FlexIO3:6 |
	pinMode(43, OUTPUT); // FlexIO3:7 |
	pinMode(44, OUTPUT); // FlexIO3:8 |
	pinMode(45, OUTPUT); // FlexIO3:9 |
	pinMode(6, OUTPUT); // FlexIO3:10 |
	pinMode(9, OUTPUT); // FlexIO3:11 D7
	digitalWriteFast(10,HIGH);
	//FlexIO3 
	#else
    /* Basic pin setup */
    pinMode(19, OUTPUT); // FlexIO3:0 D0
    pinMode(18, OUTPUT); // FlexIO3:1 |
    pinMode(14, OUTPUT); // FlexIO3:2 |
    pinMode(15, OUTPUT); // FlexIO3:3 |
    pinMode(40, OUTPUT); // FlexIO3:4 |
    pinMode(41, OUTPUT); // FlexIO3:5 |
    pinMode(17, OUTPUT); // FlexIO3:6 |
    pinMode(16, OUTPUT); // FlexIO3:7 D7
		#if (BUS_WIDTH == 16)
			pinMode(22, OUTPUT); // FlexIO3:8 D8
			pinMode(23, OUTPUT); // FlexIO3:9  |
			pinMode(20, OUTPUT); // FlexIO3:10 |
			pinMode(21, OUTPUT); // FlexIO3:11 |
			pinMode(38, OUTPUT); // FlexIO3:12 |
			pinMode(39, OUTPUT); // FlexIO3:13 |
			pinMode(26, OUTPUT); // FlexIO3:14 |
			pinMode(27, OUTPUT); // FlexIO3:15 D15
		#endif
	

	pinMode(36, OUTPUT);
	digitalWriteFast(36, HIGH);
	pinMode(37, OUTPUT);
	digitalWriteFast(37, HIGH);
	#endif

    



    /* High speed and drive strength configuration */


	//LL enable for production

	
#if (FLEXIO2_MM_DMA)
	//for mm
	
    *(portControlRegister(10)) = 0xFF;
    *(portControlRegister(12)) = 0xFF; 
    *(portControlRegister(40)) = 0xFF;
    *(portControlRegister(41)) = 0xFF;
    *(portControlRegister(42)) = 0xFF;
    *(portControlRegister(43)) = 0xFF;
    *(portControlRegister(44)) = 0xFF;
    *(portControlRegister(45)) = 0xFF;
    *(portControlRegister(6)) = 0xFF;
    *(portControlRegister(9)) = 0xFF;
	

#else
	// not not mm
    *(portControlRegister(36)) = 0xFF;
    *(portControlRegister(37)) = 0xFF; 

    *(portControlRegister(19)) = 0xFF;
    *(portControlRegister(18)) = 0xFF;
    *(portControlRegister(14)) = 0xFF;
    *(portControlRegister(15)) = 0xFF;
    *(portControlRegister(40)) = 0xFF;
    *(portControlRegister(41)) = 0xFF;
    *(portControlRegister(17)) = 0xFF;
    *(portControlRegister(16)) = 0xFF;

    #if (BUS_WIDTH == 16)
    *(portControlRegister(22)) = 0xFF;
    *(portControlRegister(23)) = 0xFF;
    *(portControlRegister(20)) = 0xFF;
    *(portControlRegister(21)) = 0xFF;
    *(portControlRegister(38)) = 0xFF;
    *(portControlRegister(39)) = 0xFF;
    *(portControlRegister(26)) = 0xFF;
    *(portControlRegister(27)) = 0xFF;
    #endif
#endif

	


    /* Set clock */
    //LL  pFlex->setClockSettings(3, 1, 0); // (480 MHz source, 1+1, 1+0) >> 480/2/1 >> 240Mhz
	pFlex->setClockSettings(3, 1, 0); // (480 MHz source, 1+1, 1+0) >> 480/2/1 >> 240Mhz

	#if (FLEXIO2_MM_DMA)
	  	pFlex->setIOPinToFlexMode(10);
		pFlex->setIOPinToFlexMode(40);
		pFlex->setIOPinToFlexMode(41);
		pFlex->setIOPinToFlexMode(42);
		pFlex->setIOPinToFlexMode(43);
		pFlex->setIOPinToFlexMode(44);
		pFlex->setIOPinToFlexMode(45);
		pFlex->setIOPinToFlexMode(6);
		pFlex->setIOPinToFlexMode(9);
		digitalWriteFast(12, HIGH);
	#else

    /* Set up pin mux */
    pFlex->setIOPinToFlexMode(36);
    pFlex->setIOPinToFlexMode(37);

    pFlex->setIOPinToFlexMode(19);
    pFlex->setIOPinToFlexMode(18);
    pFlex->setIOPinToFlexMode(14);
    pFlex->setIOPinToFlexMode(15);
    pFlex->setIOPinToFlexMode(40);
    pFlex->setIOPinToFlexMode(41);
    pFlex->setIOPinToFlexMode(17);
    pFlex->setIOPinToFlexMode(16);

		#if (BUS_WIDTH == 16)
		pFlex->setIOPinToFlexMode(22);
		pFlex->setIOPinToFlexMode(23);
		pFlex->setIOPinToFlexMode(20);
		pFlex->setIOPinToFlexMode(21);
		pFlex->setIOPinToFlexMode(38);
		pFlex->setIOPinToFlexMode(39);
		pFlex->setIOPinToFlexMode(26);
		pFlex->setIOPinToFlexMode(27);
		#endif
	#endif

    hw->clock_gate_register |= hw->clock_gate_mask  ;
    /* Enable the FlexIO with fast access */
	#if (FLEXIO2_MM_DMA)
		p->CTRL = FLEXIO_CTRL_FLEXEN | FLEXIO_CTRL_FASTACC;
	#else
    	p->CTRL = FLEXIO_CTRL_FLEXEN;
	#endif
    
}

FASTRUN void RA8876_8080::FlexIO_Config_SnglBeat_Read() {

	//Serial.println("FlexIO_Config_SnglBeat_Read");

    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |=  FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    gpioRead();

	#if (FLEXIO2_MM_DMA)
	/* Configure the shifters */
			p->SHIFTCFG[3] = 
				//FLEXIO_SHIFTCFG_INSRC                                                  /* Shifter input */
				FLEXIO_SHIFTCFG_SSTOP(0)                                              /* Shifter stop bit disabled */
			| FLEXIO_SHIFTCFG_SSTART(0)                                             /* Shifter start bit disabled and loading data on enabled */
			| FLEXIO_SHIFTCFG_PWIDTH(7);                                            /* Bus width */
			
			p->SHIFTCTL[3] = 
				FLEXIO_SHIFTCTL_TIMSEL(0)                                              /* Shifter's assigned timer index */
			| FLEXIO_SHIFTCTL_TIMPOL*(1)                                             /* Shift on posedge of shift clock */
			| FLEXIO_SHIFTCTL_PINCFG(0)                                              /* Shifter's pin configured as input */
			| FLEXIO_SHIFTCTL_PINSEL(4)                  // was 4                             /* Shifter's pin start index */
			| FLEXIO_SHIFTCTL_PINPOL*(0)                                             /* Shifter's pin active high */
			| FLEXIO_SHIFTCTL_SMOD(1);                                               /* Shifter mode as recieve */

			/* Configure the timer for shift clock */
			p->TIMCMP[0] = 
				(((1 * 2) - 1) << 8)                                                   /* TIMCMP[15:8] = number of beats x 2 – 1 */
			| (((60)/2) - 1);                               //was 30                         /* TIMCMP[7:0] = baud rate divider / 2 – 1 ::: 30 = 8Mhz with current controller speed */

			p->TIMCFG[0] = 
				FLEXIO_TIMCFG_TIMOUT(0)                                                /* Timer output logic one when enabled and not affected by reset */
			| FLEXIO_TIMCFG_TIMDEC(0)                                                /* Timer decrement on FlexIO clock, shift clock equals timer output */
			| FLEXIO_TIMCFG_TIMRST(0)                                                /* Timer never reset */
			| FLEXIO_TIMCFG_TIMDIS(2)                                                /* Timer disabled on timer compare */
			| FLEXIO_TIMCFG_TIMENA(2)                                                /* Timer enabled on trigger high */
			| FLEXIO_TIMCFG_TSTOP(1)                                                 /* Timer stop bit disabled */
			| FLEXIO_TIMCFG_TSTART*(0);                                              /* Timer start bit disabled */
			
			p->TIMCTL[0] = 
				FLEXIO_TIMCTL_TRGSEL((((3) << 2) | 1))                                 /* Timer trigger selected as shifter's status flag */
			| FLEXIO_TIMCTL_TRGPOL*(1)                                               /* Timer trigger polarity as active low */
			| FLEXIO_TIMCTL_TRGSRC*(1)                                               /* Timer trigger source as internal */
			| FLEXIO_TIMCTL_PINCFG(3)                                                /* Timer' pin configured as output */
			| FLEXIO_TIMCTL_PINSEL(1)                   //was 1                             /* Timer' pin index: WR pin */
			| FLEXIO_TIMCTL_PINPOL*(1)                                               /* Timer' pin active low */
			| FLEXIO_TIMCTL_TIMOD(1);                                                /* Timer mode as dual 8-bit counters baud/bit */

		
			/* Enable FlexIO */
		p->CTRL |= FLEXIO_CTRL_FLEXEN;     
	#else
    /* Configure the shifters */
    p->SHIFTCFG[3] = 
         FLEXIO_SHIFTCFG_SSTOP(0)                                              /* Shifter stop bit disabled */
       | FLEXIO_SHIFTCFG_SSTART(0)                                             /* Shifter start bit disabled and loading data on enabled */
       | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH-1);                                  /* Bus width */
     
    p->SHIFTCTL[3] = 
        FLEXIO_SHIFTCTL_TIMSEL(0)                                              /* Shifter's assigned timer index */
      | FLEXIO_SHIFTCTL_TIMPOL*(1)                                             /* Shift on negative edge of shift clock */
      | FLEXIO_SHIFTCTL_PINCFG(1)                                              /* Shifter's pin configured as output */
      | FLEXIO_SHIFTCTL_PINSEL(0)                                              /* Shifter's pin start index */
      | FLEXIO_SHIFTCTL_PINPOL*(0)                                             /* Shifter's pin active high */
      | FLEXIO_SHIFTCTL_SMOD(1);                                               /* Shifter mode as receive */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] = 
        (((1 * 2) - 1) << 8)                                                   /* TIMCMP[15:8] = number of beats x 2 – 1 */
      | ((60/2) - 1);                                                          /* TIMCMP[7:0] = baud rate divider / 2 – 1 */
    
    p->TIMCFG[0] = 
        FLEXIO_TIMCFG_TIMOUT(0)                                                /* Timer output logic one when enabled and not affected by reset */
      | FLEXIO_TIMCFG_TIMDEC(0)                                                /* Timer decrement on FlexIO clock, shift clock equals timer output */
      | FLEXIO_TIMCFG_TIMRST(0)                                                /* Timer never reset */
      | FLEXIO_TIMCFG_TIMDIS(2)                                                /* Timer disabled on timer compare */
      | FLEXIO_TIMCFG_TIMENA(2)                                                /* Timer enabled on trigger high */
      | FLEXIO_TIMCFG_TSTOP(1)                                                 /* Timer stop bit enabled */
      | FLEXIO_TIMCFG_TSTART*(0);                                              /* Timer start bit disabled */
    
    p->TIMCTL[0] = 
        FLEXIO_TIMCTL_TRGSEL((((3) << 2) | 1))                                 /* Timer trigger selected as shifter's status flag */
      | FLEXIO_TIMCTL_TRGPOL*(1)                                               /* Timer trigger polarity as active low */
      | FLEXIO_TIMCTL_TRGSRC*(1)                                               /* Timer trigger source as internal */
      | FLEXIO_TIMCTL_PINCFG(3)                                                /* Timer' pin configured as output */
      | FLEXIO_TIMCTL_PINSEL(19)                                               /* Timer' pin index: RD pin */
      | FLEXIO_TIMCTL_PINPOL*(1)                                               /* Timer' pin active low */
      | FLEXIO_TIMCTL_TIMOD(1);                                                /* Timer mode as dual 8-bit counters baud/bit */
 
    /* Enable FlexIO */
   p->CTRL |= FLEXIO_CTRL_FLEXEN;      
#endif
}


FASTRUN void RA8876_8080::FlexIO_Config_SnglBeat()
{
//	Serial.println("FlexIO_Config_SnglBeat");

	gpioWrite();

    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    #if (FLEXIO2_MM_DMA)

		/* Configure the shifters */
		p->SHIFTCFG[0] = 
		FLEXIO_SHIFTCFG_INSRC*(1)                                                    /* Shifter input */
		|FLEXIO_SHIFTCFG_SSTOP(0)                                               /* Shifter stop bit disabled */
		| FLEXIO_SHIFTCFG_SSTART(0)                                             /* Shifter start bit disabled and loading data on enabled */
		| FLEXIO_SHIFTCFG_PWIDTH(7);                                            /* Bus width */
		
		p->SHIFTCTL[0] = 
			FLEXIO_SHIFTCTL_TIMSEL(0)                                              /* Shifter's assigned timer index */
		| FLEXIO_SHIFTCTL_TIMPOL*(0)                                             /* Shift on posedge of shift clock */
		| FLEXIO_SHIFTCTL_PINCFG(3)                                              /* Shifter's pin configured as output */
		| FLEXIO_SHIFTCTL_PINSEL(4)        //was 4                                      /* Shifter's pin start index */
		| FLEXIO_SHIFTCTL_PINPOL*(0)                                             /* Shifter's pin active high */
		| FLEXIO_SHIFTCTL_SMOD(2);                                               /* Shifter mode as transmit */

		/* Configure the timer for shift clock */
		p->TIMCMP[0] = 
			(((1 * 2) - 1) << 8)                                                   /* TIMCMP[15:8] = number of beats x 2 – 1 */
		| ((_baud_div/2) - 1);                                                    /* TIMCMP[7:0] = baud rate divider / 2 – 1 */
		
		p->TIMCFG[0] = 
			FLEXIO_TIMCFG_TIMOUT(0)                                                /* Timer output logic one when enabled and not affected by reset */
		| FLEXIO_TIMCFG_TIMDEC(0)                                                /* Timer decrement on FlexIO clock, shift clock equals timer output */
		| FLEXIO_TIMCFG_TIMRST(0)                                                /* Timer never reset */
		| FLEXIO_TIMCFG_TIMDIS(2)                                                /* Timer disabled on timer compare */
		| FLEXIO_TIMCFG_TIMENA(2)                                                /* Timer enabled on trigger high */
		| FLEXIO_TIMCFG_TSTOP(0)                                                 /* Timer stop bit disabled */
		| FLEXIO_TIMCFG_TSTART*(0);                                              /* Timer start bit disabled */
		
		p->TIMCTL[0] = 
			FLEXIO_TIMCTL_TRGSEL((((0) << 2) | 1))                                 /* Timer trigger selected as shifter's status flag */
		| FLEXIO_TIMCTL_TRGPOL*(1)                                               /* Timer trigger polarity as active low */
		| FLEXIO_TIMCTL_TRGSRC*(1)                                               /* Timer trigger source as internal */
		| FLEXIO_TIMCTL_PINCFG(3)                                                /* Timer' pin configured as output */
		| FLEXIO_TIMCTL_PINSEL(0)                   //was 0                             /* Timer' pin index: WR pin */
		| FLEXIO_TIMCTL_PINPOL*(1)                                               /* Timer' pin active low */
		| FLEXIO_TIMCTL_TIMOD(1);                                                /* Timer mode as dual 8-bit counters baud/bit */

		/* Enable FlexIO */
	p->CTRL |= FLEXIO_CTRL_FLEXEN;      


	#else

    /* Configure the shifters */
    p->SHIFTCFG[0] = 
       FLEXIO_SHIFTCFG_INSRC*(1)                                               /* Shifter input */
       |FLEXIO_SHIFTCFG_SSTOP(0)                                               /* Shifter stop bit disabled */
       | FLEXIO_SHIFTCFG_SSTART(0)                                             /* Shifter start bit disabled and loading data on enabled */
       | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH - 1); // Was 7 for 8 bit bus         /* Bus width */

    p->SHIFTCTL[0] = 
        FLEXIO_SHIFTCTL_TIMSEL(0)                                              /* Shifter's assigned timer index */
      | FLEXIO_SHIFTCTL_TIMPOL*(0)                                             /* Shift on posedge of shift clock */
      | FLEXIO_SHIFTCTL_PINCFG(3)                                              /* Shifter's pin configured as output */
      | FLEXIO_SHIFTCTL_PINSEL(0)                                              /* Shifter's pin start index */
      | FLEXIO_SHIFTCTL_PINPOL*(0)                                             /* Shifter's pin active high */
      | FLEXIO_SHIFTCTL_SMOD(2);                                               /* Shifter mode as transmit */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] = 
        (((1 * 2) - 1) << 8)                                                   /* TIMCMP[15:8] = number of beats x 2 – 1 */
      | ((_baud_div/2) - 1);                                                   /* TIMCMP[7:0] = baud rate divider / 2 – 1 */
    
    p->TIMCFG[0] = 
        FLEXIO_TIMCFG_TIMOUT(0)                                                /* Timer output logic one when enabled and not affected by reset */
      | FLEXIO_TIMCFG_TIMDEC(0)                                                /* Timer decrement on FlexIO clock, shift clock equals timer output */
      | FLEXIO_TIMCFG_TIMRST(0)                                                /* Timer never reset */
      | FLEXIO_TIMCFG_TIMDIS(2)                                                /* Timer disabled on timer compare */
      | FLEXIO_TIMCFG_TIMENA(2)                                                /* Timer enabled on trigger high */
      | FLEXIO_TIMCFG_TSTOP(0)                                                 /* Timer stop bit disabled */
      | FLEXIO_TIMCFG_TSTART*(0);                                              /* Timer start bit disabled */
    
    p->TIMCTL[0] = 
        FLEXIO_TIMCTL_TRGSEL((((0) << 2) | 1))                                 /* Timer trigger selected as shifter's status flag */
      | FLEXIO_TIMCTL_TRGPOL*(1)                                               /* Timer trigger polarity as active low */
      | FLEXIO_TIMCTL_TRGSRC*(1)                                               /* Timer trigger source as internal */
      | FLEXIO_TIMCTL_PINCFG(3)                                                /* Timer' pin configured as output */
      | FLEXIO_TIMCTL_PINSEL(18)                                               /* Timer' pin index: WR pin */
      | FLEXIO_TIMCTL_PINPOL*(1)                                               /* Timer' pin active low */
      | FLEXIO_TIMCTL_TIMOD(1);                                                /* Timer mode as dual 8-bit counters baud/bit */
    
    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;      
#endif

}

FASTRUN void RA8876_8080::FlexIO_Clear_Config_SnglBeat(){
	//Serial.println("FlexIO_Clear_Config_SnglBeat");
    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

	for (int i = 0; i < SHIFTNUM; i++) {
		p->SHIFTCFG[i] = 0;
		p->SHIFTCTL[i] = 0;
	}
    //p->SHIFTCFG[0] = 0;                                 
    //p->SHIFTCTL[0] = 0;
    p->SHIFTSTAT = (1 << 0);
	for (int i = 0; i < SHIFTNUM; i++) {
		p->TIMCMP[i] = 0;
		p->TIMCFG[i] = 0;
	}
    //p->TIMCMP[0] = 0;
    //p->TIMCFG[0] = 0;
    p->TIMSTAT = (1U << 0);                                          /* Timer start bit disabled */
    //p->TIMCTL[0] = 0;      
	for (int i = 0; i < SHIFTNUM; i++) {
		p->TIMCTL[i] = 0;
	}
    
	  /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;     

}

FASTRUN void RA8876_8080::FlexIO_Config_MultiBeat()
{


	//FlexIO_Clear_Config_SnglBeat();
	//Serial.println("FlexIO_Config_MultiBeat");
    uint8_t beats = SHIFTNUM * BEATS_PER_SHIFTER;                                     //Number of beats = number of shifters * beats per shifter
    /* Disable and reset FlexIO */
    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    gpioWrite();


#if (FLEXIO2_MM_DMA)
	uint32_t i;

	  p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)                                                     /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0U)                                               /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3U)                                                  /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(4)                                                   /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0U)                                               /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2U);                                                   /* shifter mode transmit */


	for(i=0; i<=SHIFTNUM-1; i++)
    {
        p->SHIFTCFG[i] = 
        FLEXIO_SHIFTCFG_INSRC*(1U)                                                /* Shifter input from next shifter's output */
      | FLEXIO_SHIFTCFG_SSTOP(0U)                                                 /* Shifter stop bit disabled */
      | FLEXIO_SHIFTCFG_SSTART(0U)                                                /* Shifter start bit disabled and loading data on enabled */
      | FLEXIO_SHIFTCFG_PWIDTH(8U-1U);                                            /* 8 bit shift width */
    }

	

  
    for(i=1; i<=SHIFTNUM-1; i++)
    {
        p->SHIFTCTL[i] = 
        FLEXIO_SHIFTCTL_TIMSEL(0)                                                 /* Shifter's assigned timer index */
      | FLEXIO_SHIFTCTL_TIMPOL*(0U)                                               /* Shift on posedge of shift clock */
      | FLEXIO_SHIFTCTL_PINCFG(0U)                                                /* Shifter's pin configured as output disabled */
      | FLEXIO_SHIFTCTL_PINSEL(0)                                                 /* Shifter's pin start index */
      | FLEXIO_SHIFTCTL_PINPOL*(0U)                                               /* Shifter's pin active high */
      | FLEXIO_SHIFTCTL_SMOD(2U);                                                 /* shifter mode transmit */          
    }

    /* Configure the timer for shift clock */
    p->TIMCMP[0] = 
        ((beats * 2U - 1) << 8)                                       /* TIMCMP[15:8] = number of beats x 2 – 1 */
      | (_baud_div / 2U - 1U);                                                       /* TIMCMP[7:0] = shift clock divide ratio / 2 - 1 */ //try 4 was _baud_div
      
    p->TIMCFG[0] =   FLEXIO_TIMCFG_TIMOUT(0U)                                     /* Timer output logic one when enabled and not affected by reset */
      | FLEXIO_TIMCFG_TIMDEC(0U)                                                  /* Timer decrement on FlexIO clock, shift clock equals timer output */
      | FLEXIO_TIMCFG_TIMRST(0U)                                                  /* Timer never reset */
      | FLEXIO_TIMCFG_TIMDIS(2U)                                                  /* Timer disabled on timer compare */
      | FLEXIO_TIMCFG_TIMENA(2U)                                                  /* Timer enabled on trigger high */
      | FLEXIO_TIMCFG_TSTOP(0U)                                                   /* Timer stop bit disabled */
      | FLEXIO_TIMCFG_TSTART*(0U);                                                /* Timer start bit disabled */

	  p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL(((SHIFTNUM - 1) << 2) | 1U)                              /* Timer trigger selected as highest shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1U)                                                 /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1U)                                                 /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3U)                                                    /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(0)                                                    /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1U)                                                 /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1U);                                                    /* Timer mode 8-bit baud counter */


		/*
		Serial.printf("CCM_CDCDR: %x\n", CCM_CDCDR);
		Serial.printf("VERID:%x PARAM:%x CTRL:%x PIN: %x\n", IMXRT_FLEXIO2_S.VERID, IMXRT_FLEXIO2_S.PARAM, IMXRT_FLEXIO2_S.CTRL, IMXRT_FLEXIO2_S.PIN);
		Serial.printf("SHIFTSTAT:%x SHIFTERR=%x TIMSTAT=%x\n", IMXRT_FLEXIO2_S.SHIFTSTAT, IMXRT_FLEXIO2_S.SHIFTERR, IMXRT_FLEXIO2_S.TIMSTAT);
		Serial.printf("SHIFTSIEN:%x SHIFTEIEN=%x TIMIEN=%x\n", IMXRT_FLEXIO2_S.SHIFTSIEN, IMXRT_FLEXIO2_S.SHIFTEIEN, IMXRT_FLEXIO2_S.TIMIEN);
		Serial.printf("SHIFTSDEN:%x SHIFTSTATE=%x\n", IMXRT_FLEXIO2_S.SHIFTSDEN, IMXRT_FLEXIO2_S.SHIFTSTATE);
		for(int i=0; i<SHIFTNUM; i++){
			Serial.printf("SHIFTCTL[%d]:%x \n", i, IMXRT_FLEXIO2_S.SHIFTCTL[i]);
			} 

		for(int i=0; i<SHIFTNUM; i++){
			Serial.printf("SHIFTCFG[%d]:%x \n", i, IMXRT_FLEXIO2_S.SHIFTCFG[i]);
			}   
		
		Serial.printf("TIMCTL:%x %x %x %x\n", IMXRT_FLEXIO2_S.TIMCTL[0], IMXRT_FLEXIO2_S.TIMCTL[1], IMXRT_FLEXIO2_S.TIMCTL[2], IMXRT_FLEXIO2_S.TIMCTL[3]);
		Serial.printf("TIMCFG:%x %x %x %x\n", IMXRT_FLEXIO2_S.TIMCFG[0], IMXRT_FLEXIO2_S.TIMCFG[1], IMXRT_FLEXIO2_S.TIMCFG[2], IMXRT_FLEXIO2_S.TIMCFG[3]);
		Serial.printf("TIMCMP:%x %x %x %x\n", IMXRT_FLEXIO2_S.TIMCMP[0], IMXRT_FLEXIO2_S.TIMCMP[1], IMXRT_FLEXIO2_S.TIMCMP[2], IMXRT_FLEXIO2_S.TIMCMP[3]);
		*/
    /* Enable FlexIO */
   p->CTRL |= FLEXIO_CTRL_FLEXEN;
   p->SHIFTSDEN |= 1U << (SHIFTER_DMA_REQUEST); // enable DMA trigger when shifter status flag is set on shifter SHIFTER_DMA_REQUEST
#else

    /* Configure the shifters */
    for (int i = 0; i <= SHIFTNUM - 1; i++)
    {
        p->SHIFTCFG[i] =
            FLEXIO_SHIFTCFG_INSRC * (1U)                                              /* Shifter input from next shifter's output */
            | FLEXIO_SHIFTCFG_SSTOP(0U)                                               /* Shifter stop bit disabled */
            | FLEXIO_SHIFTCFG_SSTART(0U)                                              /* Shifter start bit disabled and loading data on enabled */
            | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH - 1);                                  /* 8 bit shift width */
    }
	

    p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)                                                     /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0U)                                               /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3U)                                                  /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(0)                                                   /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0U)                                               /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2U);                                                   /* shifter mode transmit */

    for (int i = 1; i <= SHIFTNUM - 1; i++)
    {
        p->SHIFTCTL[i] =
            FLEXIO_SHIFTCTL_TIMSEL(0)                                                 /* Shifter's assigned timer index */
            | FLEXIO_SHIFTCTL_TIMPOL * (0U)                                           /* Shift on posedge of shift clock */
            | FLEXIO_SHIFTCTL_PINCFG(0U)                                              /* Shifter's pin configured as output disabled */
            | FLEXIO_SHIFTCTL_PINSEL(0)                                               /* Shifter's pin start index */
            | FLEXIO_SHIFTCTL_PINPOL * (0U)                                           /* Shifter's pin active high */
            | FLEXIO_SHIFTCTL_SMOD(2U);
    }

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        ((beats * 2U - 1) << 8)                                                       /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | (_baud_div / 2U - 1U);                                                      /* TIMCMP[7:0] = shift clock divide ratio / 2 - 1 */

    p->TIMCFG[0] =   FLEXIO_TIMCFG_TIMOUT(0U)                                         /* Timer output logic one when enabled and not affected by reset */
                     | FLEXIO_TIMCFG_TIMDEC(0U)                                       /* Timer decrement on FlexIO clock, shift clock equals timer output */
                     | FLEXIO_TIMCFG_TIMRST(0U)                                       /* Timer never reset */
                     | FLEXIO_TIMCFG_TIMDIS(2U)                                       /* Timer disabled on timer compare */
                     | FLEXIO_TIMCFG_TIMENA(2U)                                       /* Timer enabled on trigger high */
                     | FLEXIO_TIMCFG_TSTOP(0U)                                        /* Timer stop bit disabled */
                     | FLEXIO_TIMCFG_TSTART * (0U);                                   /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL(((SHIFTNUM - 1) << 2) | 1U)                              /* Timer trigger selected as highest shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1U)                                                 /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1U)                                                 /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3U)                                                    /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(18)                                                    /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1U)                                                 /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1U);                                                    /* Timer mode 8-bit baud counter */
    /* Enable FlexIO */
   p->CTRL |= FLEXIO_CTRL_FLEXEN;

   // configure interrupts
    attachInterruptVector(hw->flex_irq, ISR);
    NVIC_ENABLE_IRQ(hw->flex_irq);
    NVIC_SET_PRIORITY(hw->flex_irq, FLEXIO_ISR_PRIORITY);

    // disable interrupts until later
    p->SHIFTSIEN &= ~(1 << SHIFTER_IRQ);
    p->TIMIEN &= ~(1 << TIMER_IRQ);

#endif

    
}

FASTRUN void RA8876_8080::SglBeatWR_nPrm_8(uint32_t const cmd, const uint8_t *value = NULL, uint32_t const length = 0)
{
	Serial.println("SglBeatWR_nPrm_8");
  while(WR_IRQTransferDone == false)
  {
    //Wait for any DMA transfers to complete
    
  }
  
    FlexIO_Config_SnglBeat();
     uint32_t i;
    /* Assert CS, RS pins */
    
    //delay(1);
    CSLow();
    DCLow();

    /* Write command index */
    p->SHIFTBUF[0] = cmd;

    /*Wait for transfer to be completed */
    while(0 == (p->SHIFTSTAT & (1 << 0)))
    {
    }
    while(0 == (p->TIMSTAT & (1 << 0)))
            {  
            }

    /* De-assert RS pin */
    
    microSecondDelay();
    DCHigh();
    microSecondDelay();
    if(length)
    {
        for(i = 0; i < length; i++)
        {    
            p->SHIFTBUF[0] = *value++;
            while(0 == (p->SHIFTSTAT & (1 << 0)))
            {  
            }
        }
        while(0 == (p->TIMSTAT & (1 << 0)))
            {  
            }
    }
    microSecondDelay();
    CSHigh();
    
    /* De-assert CS pin */
}

FASTRUN void RA8876_8080::SglBeatWR_nPrm_16(uint32_t const cmd, const uint16_t *value, uint32_t const length)
{
	Serial.print("SglBeatWR_nPrm_16");	
 while(WR_IRQTransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }
  Serial.println("SglBeatWR_nPrm_16");
    FlexIO_Config_SnglBeat();
    uint16_t buf;
    /* Assert CS, RS pins */
    CSLow();
    DCLow();
 
    /* Write command index */
    p->SHIFTBUF[0] = cmd;
 
    /*Wait for transfer to be completed */
    while(0 == (p->TIMSTAT & (1 << 0)))
            {  
            }
    /* De-assert RS pin */
	microSecondDelay();
    DCHigh();
    microSecondDelay();

    if(length)
    {
		for(uint32_t i=0; i<length-1U; i++)
		{
			buf = *value++;
				while(0 == (p->SHIFTSTAT & (1U << 0)))
				{
				}
			p->SHIFTBUF[0] = buf >> 8;
			//Write the last byte
				while(0 == (p->SHIFTSTAT & (1U << 0)))
				{
				}
		p->SHIFTBUF[0] = buf & 0xFF;
		}
		buf = *value++;
		/* Write the last byte */
		while(0 == (p->SHIFTSTAT & (1U << 0)))
			{
			}
		p->SHIFTBUF[0] = buf >> 8;

		while(0 == (p->SHIFTSTAT & (1U << 0)))
		{
		}
		p->TIMSTAT |= (1U << 0);

		p->SHIFTBUF[0] = buf & 0xFF;

		/*Wait for transfer to be completed */
		while(0 == (p->TIMSTAT |= (1U << 0)))
		{
		}
    }
	microSecondDelay();
    CSHigh();
}

FASTRUN uint8_t RA8876_8080::readCommand(const uint8_t cmd){
  while(WR_IRQTransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }

    CSLow();
    FlexIO_Config_SnglBeat();
    DCLow();

    /* Write command index */
    p->SHIFTBUF[0] = cmd;

    /*Wait for transfer to be completed */
    while(0 == (p->SHIFTSTAT & (1 << 0)))
    {
    }
    while(0 == (p->TIMSTAT & (1 << 0)))
            {  
            }
    /* De-assert RS pin */
    microSecondDelay();
    DCHigh();
    FlexIO_Config_SnglBeat_Read();
    uint8_t dummy;
    uint8_t data = 0;

    while (0 == (p->SHIFTSTAT & (1 << 3)))
        {
        }
    dummy = p->SHIFTBUFBYS[3];
    while (0 == (p->SHIFTSTAT & (1 << 3)))
        {
        }
    data = p->SHIFTBUFBYS[3];
    FlexIO_Clear_Config_SnglBeat();

//    Serial.printf("Dummy 0x%x, data 0x%x\n", dummy, data);
    
    //Set FlexIO back to Write mode
    FlexIO_Config_SnglBeat();
  CSHigh();
  return data;


}

FASTRUN uint8_t RA8876_8080::readStatus(void){
  while(WR_IRQTransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }

    CSLow();
    DCLow();
    /* De-assert RS pin */
//    microSecondDelay();
    FlexIO_Config_SnglBeat_Read();
    uint8_t dummy;
    uint8_t data = 0;
    while (0 == (p->SHIFTSTAT & (1 << 3)))
        {
        }
    dummy = p->SHIFTBUFBYS[3];
    while (0 == (p->SHIFTSTAT & (1 << 3)))
        {
        }
    data = p->SHIFTBUFBYS[3];
    FlexIO_Clear_Config_SnglBeat();

  //  Serial.printf("Dummy 0x%x, data 0x%x\n", dummy, data);
    
    //Set FlexIO back to Write mode
    FlexIO_Config_SnglBeat();
  DCHigh();
  CSHigh();
  return data;


}

RA8876_8080 * RA8876_8080::IRQcallback = nullptr;
RA8876_8080 * RA8876_8080::dmaCallback = nullptr;
DMAChannel RA8876_8080::flexDma;

FASTRUN void RA8876_8080::MulBeatWR_nPrm_DMA(uint32_t const cmd,  const void *value, uint32_t const length) 
{
  while(WR_DMATransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }
    uint32_t BeatsPerMinLoop = SHIFTNUM * sizeof(uint32_t) / sizeof(uint8_t);      // Number of shifters * number of 8 bit values per shifter
    uint32_t majorLoopCount, minorLoopBytes;
    uint32_t destinationModulo = 31-(__builtin_clz(SHIFTNUM*sizeof(uint32_t))); // defines address range for circular DMA destination buffer 

    FlexIO_Config_SnglBeat();
    CSLow();
    DCLow();

    /* Write command index */
    p->SHIFTBUF[0] = cmd;

    /*Wait for transfer to be completed */

    while(0 == (p->TIMSTAT & (1 << 0)))
            {  
            }
    microSecondDelay();
    /* De-assert RS pin */
    DCHigh();
    microSecondDelay();


  if (length < 8){
    Serial.println ("In DMA but to Short to multibeat");
    const uint16_t * newValue = (uint16_t*)value;
    uint16_t buf;
    for(uint32_t i=0; i<length; i++)
      {
        buf = *newValue++;
          while(0 == (p->SHIFTSTAT & (1U << 0)))
          {
          }
          p->SHIFTBUF[0] = buf >> 8;


          while(0 == (p->SHIFTSTAT & (1U << 0)))
          {
          }
          p->SHIFTBUF[0] = buf & 0xFF;
          
      }        
      //Wait for transfer to be completed 
      while(0 == (p->TIMSTAT & (1U << 0)))
      {
      }

    microSecondDelay();
    CSHigh();

  }

  else{
    //memcpy(framebuff, value, length); 
    //arm_dcache_flush((void*)framebuff, sizeof(framebuff)); // always flush cache after writing to DMAMEM variable that will be accessed by DMA
    
    FlexIO_Config_MultiBeat();
    
    MulBeatCountRemain = length % BeatsPerMinLoop;
    MulBeatDataRemain = (uint16_t*)value + ((length - MulBeatCountRemain)); // pointer to the next unused byte (overflow if MulBeatCountRemain = 0)
    TotalSize = (length - MulBeatCountRemain)*2;               /* in bytes */
    minorLoopBytes = SHIFTNUM * sizeof(uint32_t);
    majorLoopCount = TotalSize/minorLoopBytes;
    //Serial.printf("Length(16bit): %d, Count remain(16bit): %d, Data remain: %d, TotalSize(8bit): %d, majorLoopCount: %d \n",length, MulBeatCountRemain, MulBeatDataRemain, TotalSize, majorLoopCount );

    /* Configure FlexIO with multi-beat write configuration */
    flexDma.begin();

    /* Setup DMA transfer with on-the-fly swapping of MSB and LSB in 16-bit data:
     *  Within each minor loop, read 16-bit values from buf in reverse order, then write 32bit values to SHIFTBUFBYS[i] in reverse order.
     *  Result is that every pair of bytes are swapped, while half-words are unswapped.
     *  After each minor loop, advance source address using minor loop offset. */
    int destinationAddressOffset, destinationAddressLastOffset, sourceAddressOffset, sourceAddressLastOffset, minorLoopOffset;
    volatile void *destinationAddress, *sourceAddress;

    DMA_CR |= DMA_CR_EMLM; // enable minor loop mapping

    sourceAddress = (uint16_t*)value + minorLoopBytes/sizeof(uint16_t) - 1; // last 16bit address within current minor loop
    sourceAddressOffset = -sizeof(uint16_t); // read values in reverse order
    minorLoopOffset = 2*minorLoopBytes; // source address offset at end of minor loop to advance to next minor loop
    sourceAddressLastOffset = minorLoopOffset - TotalSize; // source address offset at completion to reset to beginning
    destinationAddress = (uint32_t*)&p->SHIFTBUFBYS[SHIFTNUM-1]; // last 32bit shifter address (with reverse byte order)
    destinationAddressOffset = -sizeof(uint32_t); // write words in reverse order
    destinationAddressLastOffset = 0;

    flexDma.TCD->SADDR = sourceAddress;
    flexDma.TCD->SOFF = sourceAddressOffset;
    flexDma.TCD->SLAST = sourceAddressLastOffset;
    flexDma.TCD->DADDR = destinationAddress;
    flexDma.TCD->DOFF = destinationAddressOffset;
    flexDma.TCD->DLASTSGA = destinationAddressLastOffset;
    flexDma.TCD->ATTR =
        DMA_TCD_ATTR_SMOD(0U)
      | DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_16BIT) // 16bit reads
      | DMA_TCD_ATTR_DMOD(destinationModulo)
      | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT); // 32bit writes
    flexDma.TCD->NBYTES_MLOFFYES = 
     //   DMA_TCD_NBYTES_SMLOE
       DMA_TCD_NBYTES_MLOFFYES_MLOFF(minorLoopOffset)
      | DMA_TCD_NBYTES_MLOFFYES_NBYTES(minorLoopBytes);
    flexDma.TCD->CITER = majorLoopCount; // Current major iteration count
    flexDma.TCD->BITER = majorLoopCount; // Starting major iteration count

    flexDma.triggerAtHardwareEvent(hw->shifters_dma_channel[SHIFTER_DMA_REQUEST]);
    flexDma.disableOnCompletion();
    flexDma.interruptAtCompletion();
    flexDma.clearComplete();
    
    //Serial.println("Dma setup done");

    /* Start data transfer by using DMA */
    WR_DMATransferDone = false;
    flexDma.attachInterrupt(dmaISR);
    flexDma.enable();
    //Serial.println("Starting transfer");
    dmaCallback = this;
   }
}

FASTRUN void RA8876_8080::MultiBeat_ROP_DMA(void *value, uint32_t length) 
{
  while(WR_DMATransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }
    uint32_t BeatsPerMinLoop = SHIFTNUM * sizeof(uint32_t) / sizeof(uint8_t);      // Number of shifters * number of 8 bit values per shifter
    uint32_t majorLoopCount, minorLoopBytes;
    uint32_t destinationModulo = 31-(__builtin_clz(SHIFTNUM*sizeof(uint32_t))); // defines address range for circular DMA destination buffer 

	//delayMicroseconds(1);



  if (length < 8){
    Serial.println ("In DMA but to Short to multibeat");
    const uint16_t * newValue = (uint16_t*)value;
    uint16_t buf;
    for(uint32_t i=0; i<length; i++)
      {
        buf = *newValue++;
          while(0 == (p->SHIFTSTAT & (1U << 0)))
          {
          }
          p->SHIFTBUF[0] = buf >> 8;


          while(0 == (p->SHIFTSTAT & (1U << 0)))
          {
          }
          p->SHIFTBUF[0] = buf & 0xFF;
          
      }        
      //Wait for transfer to be completed 
      while(0 == (p->TIMSTAT & (1U << 0)))
      {
      }

  }

  else{
    //memcpy(framebuff, value, length); 
    //arm_dcache_flush((void*)framebuff, sizeof(framebuff)); // always flush cache after writing to DMAMEM variable that will be accessed by DMA
    
	//flexDma.disable();
	//FlexIO_Clear_Config_SnglBeat();
    FlexIO_Config_MultiBeat();

    
    MulBeatCountRemain = length % BeatsPerMinLoop;
    MulBeatDataRemain = (uint16_t*)value + ((length - MulBeatCountRemain)); // pointer to the next unused byte (overflow if MulBeatCountRemain = 0)
    TotalSize = ((length*2) - MulBeatCountRemain);               /* in bytes */
    minorLoopBytes = SHIFTNUM * sizeof(uint32_t);
    majorLoopCount = TotalSize/minorLoopBytes;
    //Serial.printf("Length(16bit): %d, Count remain(16bit): %d, Data remain: %d, TotalSize(8bit): %d, majorLoopCount: %d \n",length, MulBeatCountRemain, MulBeatDataRemain, TotalSize, majorLoopCount );

	
	//simple code
	/*
	flexDma.sourceBuffer((uint32_t*)value, length);
	flexDma.destinationBuffer((uint32_t*)&p->SHIFTBUF[SHIFTNUM-1], sizeof(uint32_t)*SHIFTNUM);
	flexDma.transferSize(SHIFTNUM*sizeof(uint32_t));
	flexDma.transferCount(majorLoopCount);

	flexDma.triggerAtHardwareEvent(hw->shifters_dma_channel[SHIFTER_DMA_REQUEST]);
    flexDma.disableOnCompletion();
    flexDma.interruptAtCompletion();
    flexDma.clearComplete();
	
    
    //Serial.println("Dma setup done");

  
    WR_DMATransferDone = false;
    flexDma.attachInterrupt(dmaISR);
    flexDma.enable();
    //Serial.println("Starting transfer");
    dmaCallback = this;

	return;


    */
   


    /* Setup DMA transfer with on-the-fly swapping of MSB and LSB in 16-bit data:
     *  Within each minor loop, read 16-bit values from buf in reverse order, then write 32bit values to SHIFTBUFBYS[i] in reverse order.
     *  Result is that every pair of bytes are swapped, while half-words are unswapped.
     *  After each minor loop, advance source address using minor loop offset. */
    int destinationAddressOffset, destinationAddressLastOffset, sourceAddressOffset, sourceAddressLastOffset, minorLoopOffset;
	volatile void *sourceAddress;
    volatile void *destinationAddress;
	
    DMA_CR |= DMA_CR_EMLM; // enable minor loop mapping



	/* ORIG */
	/*
    sourceAddress = (uint16_t*)value + minorLoopBytes/sizeof(uint16_t) - 1; // last 16bit address within current minor loop
    sourceAddressOffset = -sizeof(uint16_t); // read values in reverse order
    minorLoopOffset = 2*minorLoopBytes; // source address offset at end of minor loop to advance to next minor loop
    sourceAddressLastOffset = minorLoopOffset - TotalSize; // source address offset at completion to reset to beginning
    destinationAddress = (uint32_t*)&p->SHIFTBUF[SHIFTNUM-1]; // last 32bit shifter address (with reverse byte order)
    destinationAddressOffset = -sizeof(uint32_t); // write words in reverse order
    destinationAddressLastOffset = 0;
	*/
	

	
	sourceAddress = (uint16_t*)value + minorLoopBytes/sizeof(uint16_t) - 1; // last 16bit address within current minor loop
    sourceAddressOffset = -sizeof(uint16_t); // read values in reverse order
    minorLoopOffset = 2*minorLoopBytes; // source address offset at end of minor loop to advance to next minor loop
    sourceAddressLastOffset = minorLoopOffset - TotalSize; // source address offset at completion to reset to beginning
    destinationAddress = (uint32_t*)&p->SHIFTBUFHWS[SHIFTNUM-1]; // last 32bit shifter address (with reverse byte order)
    destinationAddressOffset = -sizeof(uint32_t); // write words in reverse order
    destinationAddressLastOffset = 0;
	
	
    /*
   	sourceAddress = (uint16_t*)value; // Start from the beginning of the data
	//sourceAddressOffset = sizeof(uint32_t); // Increment to read values in forward order
	sourceAddressOffset = sizeof(uint16_t); // Increment to read values in forward order
	minorLoopOffset = 0; // No offset needed for the beginning of the minor loop
	sourceAddressLastOffset = 0; //TotalSize - sizeof(uint16_t); // Offset at completion to reset to the beginning

	// Adjust destination address calculation for correct byte order
	destinationAddress = (uint32_t*)&p->SHIFTBUF[SHIFTNUM-1]; // Last 32-bit shifter address (without reverse byte order)
	destinationAddressOffset = sizeof(uint32_t); // Increment to write words in forward order
	destinationAddressLastOffset = 0; // No offset needed for the last address); // write words in reverse order
	*/
    flexDma.begin();
    flexDma.TCD->SADDR = sourceAddress;
    flexDma.TCD->SOFF = sourceAddressOffset;
    flexDma.TCD->SLAST = sourceAddressLastOffset;
    flexDma.TCD->DADDR = destinationAddress;
    flexDma.TCD->DOFF = destinationAddressOffset;
    flexDma.TCD->DLASTSGA = destinationAddressLastOffset;
    flexDma.TCD->ATTR =
	    DMA_TCD_ATTR_SMOD(0U)
      | DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_16BIT) // 16bit reads
      | DMA_TCD_ATTR_DMOD(destinationModulo)
      | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT); // 32bit writes
    flexDma.TCD->NBYTES_MLOFFYES = 
	    DMA_TCD_NBYTES_SMLOE
      | DMA_TCD_NBYTES_MLOFFYES_MLOFF(minorLoopOffset)
      | DMA_TCD_NBYTES_MLOFFYES_NBYTES(minorLoopBytes);
    flexDma.TCD->CITER = majorLoopCount; // Current major iteration count
    flexDma.TCD->BITER = majorLoopCount; // Starting major iteration count

    flexDma.triggerAtHardwareEvent(hw->shifters_dma_channel[SHIFTER_DMA_REQUEST]);
    flexDma.disableOnCompletion();
    flexDma.interruptAtCompletion();
    flexDma.clearComplete();

	//arm_dcache_delete((void*)sourceAddress, sizeof(uint32_t)); // always flush cache after writing to DMAMEM variable that will be accessed by DMA
    //arm_dcache_delete((void*)value, sizeof(uint32_t)); // always flush cache after writing to DMAMEM variable that will be accessed by DMA
    
    //Serial.println("Dma setup done");

    /* Start data transfer by using DMA */
    WR_DMATransferDone = false;
    flexDma.attachInterrupt(dmaISR);
    
    //Serial.println("Starting transfer");
    dmaCallback = this;
	flexDma.enable();
   }
}

FLASHMEM void RA8876_8080::onCompleteCB(CBF callback)
{
  _callback = callback;
  isCB = true;
}

FASTRUN void RA8876_8080::_onCompleteCB()
{
  if (_callback){
        _callback();
      }
      return;
}

FASTRUN void RA8876_8080::flexIRQ_Callback(){

	//Serial.print('.');
  
 if (p->TIMSTAT & (1 << TIMER_IRQ)) { // interrupt from end of burst
		
        p->TIMSTAT = (1 << TIMER_IRQ); // clear timer interrupt signal
        bursts_to_complete--;
        if (bursts_to_complete == 0) { 
            p->TIMIEN &= ~(1 << TIMER_IRQ); // disable timer interrupt
            asm("dsb");
			
            WR_IRQTransferDone = true;
			//free((void*)readPtr);
            microSecondDelay();
            CSHigh();
            _onCompleteCB();
			Serial.println("in last callback");
            return;
        }
    }

    if (p->SHIFTSTAT & (1 << SHIFTER_IRQ)) { // interrupt from empty shifter buffer
        // note, the interrupt signal is cleared automatically when writing data to the shifter buffers
        if (bytes_remaining == 0) { // just started final burst, no data to load
            p->SHIFTSIEN &= ~(1 << SHIFTER_IRQ); // disable shifter interrupt signal
			Serial.print('f');
        } else if (bytes_remaining < BYTES_PER_BURST) { // just started second-to-last burst, load data for final burst
            uint8_t beats = bytes_remaining / BYTES_PER_BEAT;
            p->TIMCMP[0] = ((beats * 2U - 1) << 8) | (_baud_div / 2U - 1); // takes effect on final burst
            readPtr = finalBurstBuffer;
            bytes_remaining = 0;
            for (int i = 0; i < SHIFTNUM; i++) {
                uint32_t data = *readPtr++;
                p->SHIFTBUFBYS[i] = ((data >> 16) & 0xFFFF) | ((data << 16) & 0xFFFF0000);
				//p->SHIFTBUFBYS[i] = ((0xFFFF >> 16) & 0xFFFF) | ((0xFFFF << 16) & 0xFFFF0000);
            }
        } else {
			//Serial.print("c - ");
            bytes_remaining -= BYTES_PER_BURST;
			//Serial.printf("bytes_remaining: %d \n", bytes_remaining);
            for (int i = 0; i < SHIFTNUM; i++) {
                uint32_t data = *readPtr++;
				//Serial.printf("data: %x \n", data);
                p->SHIFTBUFBYS[i] = ((data >> 16) & 0xFFFF) | ((data << 16) & 0xFFFF0000);
				//p->SHIFTBUFBYS[i] = ((0xFFFF >> 16) & 0xFFFF) | ((0xFFFF << 16) & 0xFFFF0000);
				//Serial.printf("SHIFTBUFBYS[%u]: %x \n", i, p->SHIFTBUFBYS[i]);
        }
    }
  }
    asm("dsb");
}

FASTRUN void RA8876_8080::ISR()
{
  asm("dsb");
  IRQcallback->flexIRQ_Callback();
}

FASTRUN void RA8876_8080::dmaISR()
{
  flexDma.clearInterrupt();
  asm volatile ("dsb"); // prevent interrupt from re-entering
  dmaCallback->flexDma_Callback();
}

FASTRUN void RA8876_8080::flexDma_Callback()
{
    //Serial.printf("DMA callback start triggred \n");
    
    /* the interrupt is called when the final DMA transfer completes writing to the shifter buffers, which would generally happen while
    data is still in the process of being shifted out from the second-to-last major iteration. In this state, all the status flags are cleared.
    when the second-to-last major iteration is fully shifted out, the final data is transfered from the buffers into the shifters which sets all the status flags.
    if you have only one major iteration, the status flags will be immediately set before the interrupt is called, so the while loop will be skipped. */
    
	
	while(0 == (p->SHIFTSTAT & (1U << (SHIFTNUM-1))))
    {
    }
	
    
    /* Wait the last multi-beat transfer to be completed. Clear the timer flag
    before the completing of the last beat. The last beat may has been completed
    at this point, then code would be dead in the while() below. So mask the
    while() statement and use the software delay .*/
    p->TIMSTAT |= (1U << 0U);

    // Wait timer flag to be set to ensure the completing of the last beat.
	

    
    delayMicroseconds(200);
    
    if(MulBeatCountRemain)
    {
      //Serial.printf("MulBeatCountRemain in DMA callback: %d, MulBeatDataRemain %x \n", MulBeatCountRemain,MulBeatDataRemain);
      uint16_t value;

		/*
		Serial.println("Remaining values of MulBeatDataRemains:");
		for (uint32_t i = 0; i < MulBeatCountRemain; i++) {
			Serial.printf("Value %d: %x\n", i, MulBeatDataRemain[i]);
		}
		*/
        /* Configure FlexIO with 1-beat write configuration */
        FlexIO_Config_SnglBeat();
        

       // Serial.printf("Starting single beat completion: %d \n", MulBeatCountRemain);

        /* Use polling method for data transfer */
        for(uint32_t i=0; i<(MulBeatCountRemain); i++)
        {
          value = *MulBeatDataRemain++;
            

            while(0 == (p->SHIFTSTAT & (1U << 0)))
            {
            }
            p->SHIFTBUF[0] = value & 0xFF;
			while(0 == (p->SHIFTSTAT & (1U << 0)))
            {
            }
            p->SHIFTBUF[0] = value >> 8;
        }
        p->TIMSTAT |= (1U << 0);
        /*
        value = *MulBeatDataRemain++;
        //Write the last byte 
        
        while(0 == (p->SHIFTSTAT & (1U << 0)))
            {
            }
        p->SHIFTBUF[0] = value >> 8;

        while(0 == (p->SHIFTSTAT & (1U << 0)))
        {
        }
        p->TIMSTAT |= (1U << 0);

        p->SHIFTBUF[0] = value & 0xFF;
        */
        /*Wait for transfer to be completed */
        while(0 == (p->TIMSTAT |= (1U << 0)))
        {
        }
       // Serial.println("Finished single beat completion");
    }

   // delayMicroseconds(10);
   // CSHigh();
    /* the for loop is probably not sufficient to complete the transfer. Shifting out all 32 bytes takes (32 beats)/(6 MHz) = 5.333 microseconds which is over 3000 CPU cycles.
    If you really need to wait in this callback until all the data has been shifted out, the while loop is probably the correct solution and I don't think it risks an infinite loop.
    however, it seems like a waste of time to wait here, since the process otherwise completes in the background and the shifter buffers are ready to receive new data while the transfer completes.
    I think in most applications you could continue without waiting. You can start a new DMA transfer as soon as the first one completes (no need to wait for FlexIO to finish shifting). */

    WR_DMATransferDone = true;
//    flexDma.disable(); // not necessary because flexDma is already configured to disable on completion
    if(isCB){
      //Serial.printf("custom callback triggred \n");
    	_onCompleteCB();
    
    }
  //  Serial.printf("DMA callback end triggred \n");
}

void RA8876_8080::DMAerror(){
  if(flexDma.error()){
    Serial.print("DMA error: ");
    Serial.println(DMA_ES, HEX);
  } 
}

FASTRUN void RA8876_8080::pushPixels16bit(const uint16_t * pcolors, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  while(WR_IRQTransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }
  uint32_t area = (x2-x1+1)*(y2-y1+1);

  /*
  if (!((_lastx1 == x1) && (_lastx2 == x2) && (_lasty1 == y1) && (_lasty2 == y2))) {
  setAddrWindow( x1, y1, x2, y2);
     _lastx1 = x1;  _lastx2 = x2;  _lasty1 = y1;  _lasty2 = y2;
  }
  */
 
 
  SglBeatWR_nPrm_16(RA8876_MRWDP, pcolors, area);
}


FASTRUN void RA8876_8080::pushPixels16bitDMA(const uint16_t * pcolors, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2){
  while(WR_DMATransferDone == false)
  {
    //Wait for any DMA transfers to complete
  }
  uint32_t area = (x2-x1+1)*(y2-y1+1);
  if (!((_lastx1 == x1) && (_lastx2 == x2) && (_lasty1 == y1) && (_lasty2 == y2))) {
	setActiveWindow(x1, y1, x2, y2);
	 _lastx1 = x1;  _lastx2 = x2;  _lasty1 = y1;  _lasty2 = y2;
  }
  /*
  setAddrWindow(x1, y1, x2, y2);
     _lastx1 = x1;  _lastx2 = x2;  _lasty1 = y1;  _lasty2 = y2;
  }
  */

  MulBeatWR_nPrm_DMA(RA8876_MRWDP, pcolors, area);
}

uint16_t *RA8876_8080::rotateImageRect(int16_t w, int16_t h, const uint16_t *pcolors, int16_t rotation) 
{
	uint16_t *rotated_colors_alloc = (uint16_t *)malloc(w * h * 2);
	int16_t x, y;
	if (!rotated_colors_alloc) 
		return nullptr; 
    uint16_t *rotated_colors_aligned = (uint16_t *)(((uintptr_t)rotated_colors_alloc + 32) & ~((uintptr_t)(31)));

	if ((rotation < 0) || (rotation > 3)) rotation = _rotation;  // just use current one. 
	Serial.printf("rotateImageRect %d %d %d %x %x\n", w, h, rotation, pcolors, rotated_colors_aligned);
	switch (rotation) {
		case 0:
			memcpy((uint8_t *)rotated_colors_aligned, (uint8_t*)pcolors, w*h*2);
			break;
		case 1:
			// reorder 
			for(y = 0; y < h; y++) {
				for(x = 0; x < w; x++) {
					rotated_colors_aligned[x*h+y] =*pcolors++;
				}
			}
			break;
		case 2:
			int y_offset;
			for(y = 0; y < h; y++) {
				y_offset = y*w;	// reverse the rows
				for(x = 1; x <= w; x++) {
					rotated_colors_aligned[y_offset + (w-x)] = *pcolors++;
				}
			}
			break;
		case 3:
			// reorder 
			for(y = 1; y <= h; y++) {
				for(x = 0; x < w; x++) {
					rotated_colors_aligned[x*h+(h-y)] = *pcolors++;
				}
			}
			break;
	}

	return rotated_colors_alloc;
}

// This one assumes that the data was previously arranged such that you can just ROP it out...
FASTRUN void RA8876_8080::writeRotatedRect(int16_t x, int16_t y, int16_t w, int16_t h, void *pcolors) 
{

//Serial.printf("writeRotatedRect %d %d %d %d (%x)\n", x, y, w, h, pcolors);
	uint16_t start_x = (x != CENTER) ? x : (_width - w) / 2;
	uint16_t start_y = (y != CENTER) ? y : (_height - h) / 2;

    //const uint16_t *pcolors_aligned = (uint16_t *)(((uintptr_t)pcolors + 32) & ~((uintptr_t)(31)));
	//const uint16_t *pcolors_aligned = pcolors;

	// print the first 4 values of the array
//	for (uint32_t i = 0; i < w*h; i++) Serial.printf("pcolors[%u] = 0x%x\n", i, pcolors[i]);

	switch (_rotation) {
		case 0: 
		case 2:
			// Same as normal writeRect
          if(BUS_WIDTH == 8) {
			//Serial.println("writeRotatedRect 16bpp with no rotation");
		    bteMpuWriteWithROPData16_MultiBeat_DMA(currentPage, width(), x, y,  //Source 1 is ignored for ROP 12
                          currentPage, width(), x, y, w, h,     //destination address, pagewidth, x/y, width/height
                          RA8876_BTE_ROP_CODE_12,
						  pcolors);
	       /*
		    bteMpuWriteWithROPData8(currentPage, width(), start_x, start_y,  //Source 1 is ignored for ROP 12
                          currentPage, width(), start_x, start_y, w, h,     //destination address, pagewidth, x/y, width/height
                          RA8876_BTE_ROP_CODE_12,
                          (const unsigned char *)pcolors_aligned);
						  */
          } else {
			Serial.println("writeRotatedRect 16bpp with no rotation");
		    bteMpuWriteWithROPData16(currentPage, width(), start_x, start_y,  //Source 1 is ignored for ROP 12
                          currentPage, width(), start_x, start_y, w, h,     //destination address, pagewidth, x/y, width/height
                          RA8876_BTE_ROP_CODE_12,
						  (volatile unsigned short *)pcolors);
	      }
			break;
		case 1:
		case 3:
          if(BUS_WIDTH == 8) {
		    bteMpuWriteWithROPData8(currentPage, height(), start_y, start_x,  //Source 1 is ignored for ROP 12
                          currentPage, height(),  start_y, start_x, h, w,     //destination address, pagewidth, x/y, width/height
                          RA8876_BTE_ROP_CODE_12,
                          (unsigned char *)pcolors);
          } else {
		    bteMpuWriteWithROPData16(currentPage, height(), start_y, start_x,  //Source 1 is ignored for ROP 12
                          currentPage, height(),  start_y, start_x, h, w,     //destination address, pagewidth, x/y, width/height
                          RA8876_BTE_ROP_CODE_12,
                          (unsigned short *)pcolors);
	      }
			break;
	}

}

// This one assumes that the data was previously arranged such that you can just ROP it out...
FASTRUN uint16_t RA8876_8080::lvglRotateAndROP(int16_t x1, int16_t y1, int16_t w, int16_t h, const uint16_t *pcolors) 
{

	//Serial.printf("writeRotatedRect2 %d %d %d %d (%x)\n", x1, y1, w, h, pcolors);

	

	uint16_t *rotated_colors_alloc = (uint16_t *)malloc(w * h *2+32);
	Serial.printf("allocated bytes %d\n", w * h *2+32);
	int16_t x, y;
	if (!rotated_colors_alloc) {
		Serial.printf("Failed to allocate memory for rotated colors %d\n", w * h *2+32);
		return 0; 
	}

    uint16_t *rotated_colors_aligned = (uint16_t *)(((uintptr_t)rotated_colors_alloc + 32) & ~((uintptr_t)(31)));


	for(y = 1; y <= h; y++) {
		for(x = 0; x < w; x++) {
			rotated_colors_aligned[x*h+(h-y)] = *pcolors++;
		}
	}
	

	/*
	uint16_t *rotated_colors_alloc = (uint16_t *)malloc(w * h * 2);
	int16_t x, y;
	if (!rotated_colors_alloc) {
		Serial.printf("Failed to allocate memory for rotated colors %d\n", w * h *2+32);
		return 0; 
	}

    //uint16_t *rotated_colors_aligned = (uint16_t *)(((uintptr_t)rotated_colors_alloc + 32) & ~((uintptr_t)(31)));


	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			rotated_colors_alloc[x*h+(h-y)] = *pcolors++;
		}
	}
	*/

	if(BUS_WIDTH == 8) {
		//while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
		//Serial.println("sending ROP command");
		//bteMpuWriteWithROP(currentPage, height(), y1, x1, currentPage, height(), y1, x1, h, w, RA8876_BTE_ROP_CODE_12);
		CSLow();
  		DCHigh();
		//Serial.println("sending ROP data");
		//MultiBeatWrite_8_IRQ((const unsigned char *)rotated_colors_aligned, w*h*2);

		
		bteMpuWriteWithROPData8(currentPage, height(), y1, x1,  //Source 1 is ignored for ROP 12
						currentPage, height(),  y1, x1, h, w,     //destination address, pagewidth, x/y, width/height
						RA8876_BTE_ROP_CODE_12,
						( const unsigned char *)rotated_colors_aligned);
		
	} else {
		/*
		Serial.println("sending ROP command");
		
		bteMpuWriteWithROP(currentPage, height(), y1, x1,  //Source 1 is ignored for ROP 12
						currentPage, height(),  y1, x1, h, w,     //destination address, pagewidth, x/y, width/height
						RA8876_BTE_ROP_CODE_12);
		CSLow();
  		DCHigh();
		Serial.println("sending ROP data");
		//MultiBeatWrite_16_IRQ((const unsigned short *)rotated_colors_aligned, w*h*2);
		*/
		CSLow();
  		DCHigh();
		bteMpuWriteWithROPData16(currentPage, height(), y1, x1,  //Source 1 is ignored for ROP 12
						currentPage, height(),  y1, x1, h, w,     //destination address, pagewidth, x/y, width/height
						RA8876_BTE_ROP_CODE_12,
						rotated_colors_aligned);
		
	}

	free((void*)rotated_colors_alloc);
	Serial.println("finished ROP data");

	return 1;

}

//**************************************************************//
// Send data from the microcontroller to the RA8876
// Does a Raster OPeration to combine with an image already in memory
// For a simple overwrite operation, use ROP 12
//**************************************************************//
void RA8876_8080::bteMpuWriteWithROPData8(ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,ru32 des_addr,ru16 des_image_width,
ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru8 rop_code,const uint8_t *data)
{

  bteMpuWriteWithROP(s1_addr, s1_image_width, s1_x, s1_y, des_addr, des_image_width, des_x, des_y, width, height, rop_code);

  ru16 i,j;

  bool useMultiBeat = true;

  
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  CSLow();
  DCHigh();
  

  Serial.printf("bteMpuWriteWithROPData8: %d pixels a 16bit\n", width*height);
  //for (uint32_t i = 0; i < width*height*2; i++) Serial.printf("data[%u] = 0x%x\n", i, data[i]);

  //MultiBeatWrite_8_IRQ(data, width*height);
  //return;


  
  
  if (!useMultiBeat) {
	//single beat mode
	FlexIO_Config_SnglBeat();
	
	for(j=0;j<height;j++) {
		for(i=0;i<width;i++) {
		if(_rotation & 1) delayNanoseconds(40); //was 20
		p->SHIFTBUF[0] = *data++; //increments in pointer type size
		/*Wait for transfer to be completed */
		while(0 == (p->SHIFTSTAT & (1 << 0))) {}
		while(0 == (p->TIMSTAT & (1 << 0))) {}
		p->SHIFTBUF[0] = *data++;
		/*Wait for transfer to be completed */
		while(0 == (p->SHIFTSTAT & (1 << 0))) {}
		while(0 == (p->TIMSTAT & (1 << 0))) {}
		}
	}
	microSecondDelay();
	DCLow();
	/* De-assert /CS pin */
	CSHigh();
  } else {
	MultiBeat_ROP_DMA((void*)data, width*height);
	//dma Callback will be called when the transfer is done and will set DC and CS
  }

  
}

//**************************************************************//
// For 16-bit byte-reversed data.
// Note this is 4-5 milliseconds slower than the 8-bit version above
// as the bulk byte-reversing SPI transfer operation is not available
// on all Teensys.
//**************************************************************//
FASTRUN void RA8876_8080::bteMpuWriteWithROPData16(ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,ru32 des_addr,ru16 des_image_width,
ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru8 rop_code, volatile unsigned short *data)
{

  

  bteMpuWriteWithROP(s1_addr, s1_image_width, s1_x, s1_y, des_addr, des_image_width, des_x, des_y, width, height, rop_code);

  ru16 i,j;
  bool useMultiBeat = true;

  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  microSecondDelay();
  CSLow();
  DCHigh();
  microSecondDelay();

 Serial.printf("bteMpuWriteWithROPData16: %d pixels a 16bit\n", width*height);

 unsigned char __attribute__((aligned(8))) value1;
 unsigned char __attribute__((aligned(8))) value2;

 if (!useMultiBeat) {

		//while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
		FlexIO_Config_SnglBeat();

		for(j=0;j<height;j++) {
			for(i=0;i<width;i++) {
			//if(_rotation & 1) delayNanoseconds(40);
			value1 = *data >> 8;
			value2 = *++data;
			//uint16_t value2 = *data++;
			//value1 = 0xF8D0;
			p->SHIFTBUF[0] = (uint32_t)((0x000000 << 24) | (value2));
			/*Wait for transfer to be completed */
			while(0 == (p->SHIFTSTAT & (1 << 0))) {}
			while(0 == (p->TIMSTAT & (1 << 0))) {}
			p->SHIFTBUF[0] = (uint32_t)((0x000000 << 24) | (value1));
			/*Wait for transfer to be completed */
			while(0 == (p->SHIFTSTAT & (1 << 0))) {}
			while(0 == (p->TIMSTAT & (1 << 0))) {}
			}
		}
		microSecondDelay();
		DCLow();
		/* De-assert /CS pin */
		CSHigh();
 } else {
	MultiBeat_ROP_DMA((void*)data, width*height);
	//dma Callback will be called when the transfer is done and will set DC and CS
  }
}

FASTRUN void RA8876_8080::bteMpuWriteWithROPData16_MultiBeat_DMA(ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,ru32 des_addr,ru16 des_image_width,
ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru8 rop_code, void *data)
{

  while(WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete

// log all params with print
  
  bteMpuWriteWithROP(s1_addr, s1_image_width, s1_x, s1_y, des_addr, des_image_width, des_x, des_y, width, height, rop_code);
    
  ru16 i,j;
  bool useMultiBeat = true;
  CSLow();
  delayMicroseconds(10);
  DCHigh();

 	Serial.printf("bteMpuWriteWithROPData16: %d pixels a 16bit\n", width*height); 
	MultiBeat_ROP_DMA(data, width*height);
	//dma Callback will be called when the transfer is done and will set DC and CS
}
//**************************************************************//
//write data after setting, using lcdDataWrite() or lcdDataWrite16bbp()
//**************************************************************//
FASTRUN void RA8876_8080::bteMpuWriteWithROP(ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,ru32 des_addr,ru16 des_image_width,ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru8 rop_code)
{
  check2dBusy();
  graphicMode(true);
  bte_Source1_MemoryStartAddr(s1_addr);
  bte_Source1_ImageWidth(s1_image_width);
  bte_Source1_WindowStartXY(s1_x,s1_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  //lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_8BPP<<5|RA8876_S1_COLOR_DEPTH_8BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL1,rop_code<<4|RA8876_BTE_MPU_WRITE_WITH_ROP);//91h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();


}

//**************************************************************//
// Initialize RA8876 to default settings.
// Return true on success.
//**************************************************************//
boolean RA8876_8080::ra8876Initialize() {
	
	// Init PLL
	if(!ra8876PllInitial()) {
		return false;
    }
	// Init SDRAM
	if(!ra8876SdramInitial()) {
		return false;
	}

//	lcdRegWrite(RA8876_CCR);//01h
//  lcdDataWrite(RA8876_PLL_ENABLE<<7|RA8876_WAIT_MASK<<6|RA8876_KEY_SCAN_DISABLE<<5|RA8876_TFT_OUTPUT24<<3
  
  lcdRegWrite(RA8876_CCR);//01h
  if(BUS_WIDTH == 16) {
    lcdDataWrite(RA8876_PLL_ENABLE<<7|RA8876_WAIT_NO_MASK<<6|RA8876_KEY_SCAN_DISABLE<<5|RA8876_TFT_OUTPUT24<<3
    |RA8876_I2C_MASTER_DISABLE<<2|RA8876_SERIAL_IF_DISABLE<<1|RA8876_HOST_DATA_BUS_16BIT);
  } else {
    lcdDataWrite(RA8876_PLL_ENABLE<<7|RA8876_WAIT_NO_MASK<<6|RA8876_KEY_SCAN_DISABLE<<5|RA8876_TFT_OUTPUT24<<3
    |RA8876_I2C_MASTER_DISABLE<<2|RA8876_SERIAL_IF_DISABLE<<1|RA8876_HOST_DATA_BUS_8BIT);
  }
  lcdRegWrite(RA8876_MACR);//02h
  lcdDataWrite(RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1);

  lcdRegWrite(RA8876_ICR);//03h
  lcdDataWrite(RA8877_LVDS_FORMAT<<3|RA8876_GRAPHIC_MODE<<2|RA8876_MEMORY_SELECT_IMAGE);

  lcdRegWrite(RA8876_MPWCTR);//10h
  lcdDataWrite(RA8876_PIP1_WINDOW_DISABLE<<7|RA8876_PIP2_WINDOW_DISABLE<<6|RA8876_SELECT_CONFIG_PIP1<<4
  |RA8876_IMAGE_COLOCR_DEPTH_16BPP<<2|TFT_MODE);

  lcdRegWrite(RA8876_PIPCDEP);//11h
  lcdDataWrite(RA8876_PIP1_COLOR_DEPTH_16BPP<<2|RA8876_PIP2_COLOR_DEPTH_16BPP);
  
  lcdRegWrite(RA8876_AW_COLOR);//5Eh
  lcdDataWrite(RA8876_CANVAS_BLOCK_MODE<<2|RA8876_CANVAS_COLOR_DEPTH_16BPP);
  
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_S0_COLOR_DEPTH_16BPP);//92h
  
  /*TFT timing configure*/
  lcdRegWrite(RA8876_DPCR);//12h
  lcdDataWrite(XPCLK_INV<<7|RA8876_DISPLAY_OFF<<6|RA8876_OUTPUT_RGB);
  
	/* TFT timing configure (1024x600) */
	lcdRegWrite(RA8876_DPCR);//12h
	lcdDataWrite(XPCLK_INV<<7|RA8876_DISPLAY_OFF<<6|RA8876_OUTPUT_RGB);
	
	lcdRegWrite(RA8876_PCSR);//13h
	lcdDataWrite(XHSYNC_INV<<7|XVSYNC_INV<<6|XDE_INV<<5);

	lcdHorizontalWidthVerticalHeight(HDW,VDH);
	Serial.printf("HDW: %x, VDH: %x\n", HDW, VDH);
	lcdHorizontalNonDisplay(HND);
	lcdHsyncStartPosition(HST);
	lcdHsyncPulseWidth(HPW);
	lcdVerticalNonDisplay(VND);
	lcdVsyncStartPosition(VST);
	lcdVsyncPulseWidth(VPW);

	
	// Init Global Variables
	_width = 	SCREEN_WIDTH;
	_height = 	SCREEN_HEIGHT;
	
	_rotation = 0;
	currentPage = PAGE1_START_ADDR; // set default screen page address
	pageOffset = 0;
gCursorX = 0;
	gCursorY = 0;
	_cursorX = 0;
	_cursorY = 0;
	CharPosX = 0;
	CharPosY = 0;
	_FNTwidth = 8; // Default font width
	_FNTheight = 16; // Default font height;
	_scaleX = 1;
	_scaleY = 1;
	_textMode = true;
prompt_size = 1 * (_FNTwidth * _scaleX); // prompt ">"
	vdata = 0;  // Used in tft_print()
	leftmarg = 0;
	topmarg = 0;
rightmarg =  (uint8_t)(_width / (_FNTwidth  * _scaleX));
	bottommarg = (uint8_t)(_height / (_FNTheight * _scaleY));
	_scrollXL = 0;
	_scrollXR = _width;
	_scrollYT = 0;
	_scrollYB = _height;
_cursorX = _scrollXL;
	_cursorY = _scrollYT;
	CharPosX = _scrollXL;
	CharPosY = _scrollYT;
	tab_size = 8;
_cursorXsize = _FNTwidth;
	_cursorYsize = _FNTheight;
	_TXTForeColor = COLOR65K_WHITE;
	_TXTBackColor = COLOR65K_DARKBLUE;
	RA8876_BUSY = false;
//_FNTrender = false;
	/* set-->  _TXTparameters  <--
	0:_extFontRom = false;
	1:_autoAdvance = true;
	2:_textWrap = user defined
	3:_fontFullAlig = false;
	4:_fontRotation = false;//not used
	5:_alignXToCenter = false;
	6:_alignYToCenter = false;
	7: render         = false;
	*/
	_TXTparameters = 0b00000010;
	
	_activeWindowXL = 0;
	_activeWindowXR = _width;
	_activeWindowYT = 0;
	_activeWindowYB = _height;
  
	_portrait = false;

	_backTransparent = false;	
	
	displayOn(true);	// Turn on TFT display
	UDFont = false;     // Turn off user define fonts
	
	// Setup graphic cursor
	Set_Graphic_Cursor_Color_1(0xff); // Foreground color
	Set_Graphic_Cursor_Color_2(0x00); // Outline color
	//Graphic_cursor_initial();  // Initialize Graphic Cursor
    Select_Graphic_Cursor_2(); // Select Arrow Graphic Cursor
	// Set default foreground and background colors
	//setTextColor(_TXTForeColor, _TXTBackColor);	
	// Position text cursor to default
	//setTextCursor(_scrollXL, _scrollYT);
	// Setup Text Cursor
	cursorInit();
	// Set Margins to default settings
	//setTMargins(0, 0, 0, 0); // Left Side, Top Side, Right Side, Bottom Side

	// This must be called before usage of fillScreen() which calls drawSquareFill().
	// drawSquareFill() is ineffective otherwise. 
	setClipRect();

	// Save Default Screen Parameters
	saveTFTParams(screenPage1);
	saveTFTParams(screenPage2);
	saveTFTParams(screenPage3);
	saveTFTParams(screenPage4);
	saveTFTParams(screenPage5);
	saveTFTParams(screenPage6);
	saveTFTParams(screenPage7);
	saveTFTParams(screenPage8);
	saveTFTParams(screenPage9);
//	saveTFTParams(screenPage10);

// Initialize all screen colors to default values
	currentPage = 999; // Don't repeat screen page 1 init.
	selectScreen(PAGE1_START_ADDR);	// Init page 1 screen
	fillScreen(COLOR65K_DARKBLUE);     // Not sure why we need to clear the screen twice
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE2_START_ADDR);	// Init page 2 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE3_START_ADDR);	// Init page 3 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE4_START_ADDR);	// Init page 4 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE5_START_ADDR);	// Init page 5 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE6_START_ADDR);	// Init page 6 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE7_START_ADDR);	// Init page 7 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE8_START_ADDR);	// Init page 8 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE9_START_ADDR);	// Init page 9 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE10_START_ADDR);	// Init page 10 screen
	fillScreen(COLOR65K_DARKBLUE);
	//fillStatusLine(COLOR65K_DARKBLUE);
	selectScreen(PAGE1_START_ADDR); // back to page 1 screen

	
	// Set graphic mouse cursor to center of screen
	gcursorxy(width() / 2, height() / 2);
		
	setOrigin();
	//setTextSize(1, 1);

	Serial.println("RA8876 Initialized");
	
  return true;
}

//**********************************************************************
// This section defines the low level parallel 8080 access routines.
// It uses "BUS_WIDTH" (8 or 16) to decide which drivers to use.  
// If "USE_8080_IF" is defined the 8080 bus drivers are used otherwise
// the 4 wire SPI bus is used.
//**********************************************************************

//**********************************************************************
void RA8876_8080::LCD_CmdWrite(unsigned char cmd)
{	
//  startSend();
//  _pspi->transfer16(0x00);
//  _pspi->transfer(cmd);
//  endSend(true);
}

//**************************************************************//
// Write to a RA8876 register
//**************************************************************//
void RA8876_8080::lcdRegWrite(ru8 reg, bool finalize) 
{
//  ru16 _data = (RA8876_SPI_CMDWRITE16 | reg);
//  startSend();
//  _pspi->transfer16(_data);
//  endSend(finalize);
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  FlexIO_Config_SnglBeat();
  CSLow();
  DCLow();
  /* Write command index */
  p->SHIFTBUF[0] = reg;
  /*Wait for transfer to be completed */
  while(0 == (p->SHIFTSTAT & (1 << 0))) {}
  while(0 == (p->TIMSTAT & (1 << 0))) {}
  /* De-assert RS pin */
  DCHigh();
  CSHigh();
}


//**************************************************************//
// Write RA8876 Data
//**************************************************************//
void RA8876_8080::lcdDataWrite(ru8 data, bool finalize) 
{
//  ru16 _data = (RA8876_SPI_DATAWRITE16 | data);
//  startSend();
//  _pspi->transfer16(_data);
//  endSend(finalize);
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  FlexIO_Config_SnglBeat();
  CSLow();
  DCHigh();
  /* Write command index */
  p->SHIFTBUF[0] = data;
  //Serial.printf("Data 0x%x SHIFTBUF 0x%x\n", data, p->SHIFTBUF[0]);
  /*Wait for transfer to be completed */
  while(0 == (p->SHIFTSTAT & (1 << 0))) {}
  while(0 == (p->TIMSTAT & (1 << 0))) {}
  /* De-assert /CS pin */
  CSHigh();
}

//**************************************************************//
// Read RA8876 Data
//**************************************************************//
ru8 RA8876_8080::lcdDataRead(bool finalize) 
{
//  ru16 _data = (RA8876_SPI_DATAREAD16 | 0x00);
//  startSend();
//  ru8 data = _pspi->transfer16(_data);
//  endSend(finalize);
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  CSLow();
  /* De-assert RS pin */
  DCHigh();
  FlexIO_Config_SnglBeat_Read();
  uint16_t dummy;
  uint16_t data = 0;
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  dummy = p->SHIFTBUFBYS[3];
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  data = p->SHIFTBUFBYS[3];
  //Set FlexIO back to Write mode
  FlexIO_Config_SnglBeat();
  CSHigh();
  if(BUS_WIDTH == 8) { 
    return data;
  } else {
	return dummy = (data >> 8) | (data & 0xff); // High byte to low byte and mask.
  }
}

//**************************************************************//
// Read RA8876 status register
//**************************************************************//
ru8 RA8876_8080::lcdStatusRead(bool finalize) 
{
//  startSend();
//  ru8 data = _pspi->transfer16(RA8876_SPI_STATUSREAD16);
//  endSend(finalize);
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  CSLow();
  /* De-assert RS pin */
  DCLow();
  FlexIO_Config_SnglBeat_Read();
  uint16_t dummy;
  uint16_t data = 0;
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  dummy = p->SHIFTBUFBYS[3];
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  data = p->SHIFTBUFBYS[3];
  DCHigh();
  CSHigh();
  //  Serial.printf("Dummy 0x%x, data 0x%x\n", dummy, data);
  //Set FlexIO back to Write mode
  FlexIO_Config_SnglBeat();
  if(BUS_WIDTH == 16)
    return data >> 8;
  else
    return data;
}

//**************************************************************//
// Write Data to a RA8876 register
//**************************************************************//
void RA8876_8080::lcdRegDataWrite(ru8 reg, ru8 data, bool finalize)
{
  lcdRegWrite(reg);
  (BUS_WIDTH == 8) ? lcdDataWrite(data) : lcdDataWrite16(data,false);
  //ru8 res = lcdRegDataRead(reg);
  //Serial.printf("Reg 0x%x, Data 0x%x, Read 0x%x\n", reg, data, res);
  //if (data != res) Serial.println("Error writing data to RA8876 register");
}

//**************************************************************//
// Read a RA8876 register Data
//**************************************************************//
ru8 RA8876_8080::lcdRegDataRead(ru8 reg, bool finalize)
{
  lcdRegWrite(reg, finalize);
  return lcdDataRead();
}

//**************************************************************//
// support SPI interface to write 16bpp data after Regwrite 04h
//**************************************************************//
void RA8876_8080::lcdDataWrite16bbp(ru16 data, bool finalize) 
{
  if(BUS_WIDTH == 8) {
    lcdDataWrite(data & 0xff);
    lcdDataWrite(data >> 8);
  } else {
    lcdDataWrite16(data, false );
  }
}

//**************************************************************//
// Read RA8876 Data
//**************************************************************//
uint16_t RA8876_8080::lcdDataRead16(bool finalize) 
{
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  CSLow();
  /* De-assert RS pin */
  DCHigh();
  FlexIO_Config_SnglBeat_Read();
  uint16_t dummy;
  uint16_t data = 0;
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  dummy = p->SHIFTBUFBYS[3];
  while (0 == (p->SHIFTSTAT & (1 << 3))) {}
  data = p->SHIFTBUFBYS[3];
  //Set FlexIO back to Write mode
  FlexIO_Config_SnglBeat();
  CSHigh();
  return data;
}

//**************************************************************//
// Write RA8876 Data 
//**************************************************************//
void RA8876_8080::lcdDataWrite16(uint16_t data, bool finalize) 
{
  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  FlexIO_Config_SnglBeat();
  CSLow();
  DCHigh();
  p->SHIFTBUF[0] = data;
  /*Wait for transfer to be completed */
  while(0 == (p->SHIFTSTAT & (1 << 0))) {}
  while(0 == (p->TIMSTAT & (1 << 0))) {}
  /* De-assert /CS pin */
  CSHigh();
}

//**************************************************************//
//RA8876 register 
//**************************************************************//
/*[Status Register] bit7  Host Memory Write FIFO full
0: Memory Write FIFO is not full.
1: Memory Write FIFO is full.
Only when Memory Write FIFO is not full, MPU may write another one pixel.*/ 
//**************************************************************//
void RA8876_8080::checkWriteFifoNotFull(void)
{  ru16 i;  
   for(i=0;i<10000;i++) //Please according to your usage to modify i value.
   {
    if( (lcdStatusRead()&0x80)==0 ){break;}
   }
}

//**************************************************************//
/*[Status Register] bit6  Host Memory Write FIFO empty
0: Memory Write FIFO is not empty.
1: Memory Write FIFO is empty.
When Memory Write FIFO is empty, MPU may write 8bpp data 64
pixels, or 16bpp data 32 pixels, 24bpp data 16 pixels directly.*/
//**************************************************************//
void RA8876_8080::checkWriteFifoEmpty(void)
{ ru16 i;
   for(i=0;i<10000;i++)   //Please according to your usage to modify i value.
   {
    if( (lcdStatusRead()&0x40)==0x40 ){break;}
   }
}

//**************************************************************//
/*[Status Register] bit5  Host Memory Read FIFO full
0: Memory Read FIFO is not full.
1: Memory Read FIFO is full.
When Memory Read FIFO is full, MPU may read 8bpp data 32
pixels, or 16bpp data 16 pixels, 24bpp data 8 pixels directly.*/
//**************************************************************//
void RA8876_8080::checkReadFifoNotFull(void)
{ ru16 i;
  for(i=0;i<10000;i++)  //Please according to your usage to modify i value.
  {if( (lcdStatusRead()&0x20)==0x00 ){break;}}
}

//**************************************************************//
/*[Status Register] bit4   Host Memory Read FIFO empty
0: Memory Read FIFO is not empty.
1: Memory Read FIFO is empty.*/
//**************************************************************//
void RA8876_8080::checkReadFifoNotEmpty(void)
{ ru16 i;
  for(i=0;i<10000;i++)// //Please according to your usage to modify i value. 
  {if( (lcdStatusRead()&0x10)==0x00 ){break;}}
}

//**************************************************************//
/*[Status Register] bit3   Core task is busy
Following task is running:
BTE, Geometry engine, Serial flash DMA, Text write or Graphic write
0: task is done or idle.   1: task is busy
A typical task like drawing a rectangle might take 30-150 microseconds, 
    depending on size
A large filled rectangle might take 3300 microseconds
*****************************************************************/
void RA8876_8080::check2dBusy(void)  
{  
   //unsigned long start = millis();
   ru32 i; 
   for(i=0;i<100000;i++)   //Please according to your usage to modify i value.
   { 
    delayMicroseconds(1);
    if( (lcdStatusRead()&0x08)==0x00 )
    {
		//Serial.printf("check2dBusy waiting time %d\n", millis() - start);
		return;}
   }
   Serial.println("2D ready failed (16-bit)");
}  


//**************************************************************//
/*[Status Register] bit2   SDRAM ready for access
0: SDRAM is not ready for access   1: SDRAM is ready for access*/	
//**************************************************************//
boolean RA8876_8080::checkSdramReady(void) {
 ru32 i;
 for(i=0;i<1000000;i++) //Please according to your usage to modify i value.
 { 
   delayMicroseconds(1);
   if( (lcdStatusRead()&0x04)==0x04 )
    {
		return true;
	}
 }
 return false;
}

//**************************************************************//
/*[Status Register] bit1  Operation mode status
0: Normal operation state  1: Inhibit operation state
Inhibit operation state means internal reset event keep running or
initial display still running or chip enter power saving state.	*/
//**************************************************************//
boolean RA8876_8080::checkIcReady(void) {
  ru32 i;
  for(i=0;i<1000000;i++)  //Please according to your usage to modify i value.
   {
     delayMicroseconds(1);
     if( (lcdStatusRead()&0x02)==0x00 )
    {
//Serial.printf("checkIcReady(void)\n");

		return true;
	}     
   }
   return false;
}
//**********************************************************************
// End 8080 parallel driver section
//**********************************************************************

//**************************************************************************************
//**************************************************************//
// Initialize PLL
//**************************************************************//
//[05h] [06h] [07h] [08h] [09h] [0Ah]
//------------------------------------//----------------------------------*/
boolean RA8876_8080::ra8876PllInitial(void) 
{
/*(1) 10MHz <= OSC_FREQ <= 15MHz 
  (2) 10MHz <= (OSC_FREQ/PLLDIVM) <= 40MHz
  (3) 250MHz <= [OSC_FREQ/(PLLDIVM+1)]x(PLLDIVN+1) <= 600MHz
PLLDIVM:0
PLLDIVN:1~63
PLLDIVK:CPLL & MPLL = 1/2/4/8.SPLL = 1/2/4/8/16/32/64/128.
ex:
 OSC_FREQ = 10MHz
 Set X_DIVK=2
 Set X_DIVM=0
 => (X_DIVN+1)=(XPLLx4)/10*/
//ru16 x_Divide,PLLC1,PLLC2;
//ru16 pll_m_lo, pll_m_hi;
//ru8 temp;

	// Set tft output pixel clock
		if(SCAN_FREQ>=79)								//&&(SCAN_FREQ<=100))
		{
			lcdRegDataWrite(0x05,0x04);				//PLL Divided by 4
			lcdRegDataWrite(0x06,(SCAN_FREQ*4/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>=63)&&(SCAN_FREQ<=78))
		{
			lcdRegDataWrite(0x05,0x05);				//PLL Divided by 4
			lcdRegDataWrite(0x06,(SCAN_FREQ*8/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>=40)&&(SCAN_FREQ<=62))
		{								  	
			lcdRegDataWrite(0x05,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x06,(SCAN_FREQ*8/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>=32)&&(SCAN_FREQ<=39))
		{								  	
			lcdRegDataWrite(0x05,0x07);				//PLL Divided by 8
			lcdRegDataWrite(0x06,(SCAN_FREQ*16/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>=16)&&(SCAN_FREQ<=31))
		{								  	
			lcdRegDataWrite(0x05,0x16);				//PLL Divided by 16
			lcdRegDataWrite(0x06,(SCAN_FREQ*16/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>=8)&&(SCAN_FREQ<=15))
		{
			lcdRegDataWrite(0x05,0x26);				//PLL Divided by 32
			lcdRegDataWrite(0x06,(SCAN_FREQ*32/OSC_FREQ)-1);
		}
		else if((SCAN_FREQ>0)&&(SCAN_FREQ<=7))
		{
			lcdRegDataWrite(0x05,0x36);				//PLL Divided by 64
//			lcdRegDataWrite(0x06,(SCAN_FREQ*64/OSC_FREQ)-1);
			lcdRegDataWrite(0x06,((SCAN_FREQ*64/OSC_FREQ)-1) & 0x3f);
		}								    
		// Set internal Buffer Ram clock
		if(DRAM_FREQ>=158)							//
		{
			lcdRegDataWrite(0x07,0x02);				//PLL Divided by 4
			lcdRegDataWrite(0x08,(DRAM_FREQ*2/OSC_FREQ)-1);
		}
		else if((DRAM_FREQ>=125)&&(DRAM_FREQ<=157))							
		{
			lcdRegDataWrite(0x07,0x03);				//PLL Divided by 4
			lcdRegDataWrite(0x08,(DRAM_FREQ*4/OSC_FREQ)-1);
		}
		else if((DRAM_FREQ>=79)&&(DRAM_FREQ<=124))					
		{
			lcdRegDataWrite(0x07,0x04);				//PLL Divided by 4
			lcdRegDataWrite(0x08,(DRAM_FREQ*4/OSC_FREQ)-1);
		}
		else if((DRAM_FREQ>=63)&&(DRAM_FREQ<=78))					
		{
			lcdRegDataWrite(0x07,0x05);				//PLL Divided by 4
			lcdRegDataWrite(0x08,(DRAM_FREQ*8/OSC_FREQ)-1);
		}
		else if((DRAM_FREQ>=40)&&(DRAM_FREQ<=62))
		{								  	
			lcdRegDataWrite(0x07,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x08,(DRAM_FREQ*8/OSC_FREQ)-1);
		}
		else if((DRAM_FREQ>=32)&&(DRAM_FREQ<=39))
		{								  	
			lcdRegDataWrite(0x07,0x07);				//PLL Divided by 16
			lcdRegDataWrite(0x08,(DRAM_FREQ*16/OSC_FREQ)-1);
		}
		else if(DRAM_FREQ<=31)
		{
			lcdRegDataWrite(0x07,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x08,(30*8/OSC_FREQ)-1);	//set to 30MHz if out off range
		}
		// Set Core clock
		if(CORE_FREQ>=158)
		{
			lcdRegDataWrite(0x09,0x02);				//PLL Divided by 2
			lcdRegDataWrite(0x0A,(CORE_FREQ*2/OSC_FREQ)-1);
		}
		else if((CORE_FREQ>=125)&&(CORE_FREQ<=157))
		{
			lcdRegDataWrite(0x09,0x03);				//PLL Divided by 4
			lcdRegDataWrite(0x0A,(CORE_FREQ*4/OSC_FREQ)-1);
		}
		else if((CORE_FREQ>=79)&&(CORE_FREQ<=124))					
		{
			lcdRegDataWrite(0x09,0x04);				//PLL Divided by 4
			lcdRegDataWrite(0x0A,(CORE_FREQ*4/OSC_FREQ)-1);
		}
		else if((CORE_FREQ>=63)&&(CORE_FREQ<=78))					
		{
			lcdRegDataWrite(0x09,0x05);				//PLL Divided by 8
			lcdRegDataWrite(0x0A,(CORE_FREQ*8/OSC_FREQ)-1);
		}
		else if((CORE_FREQ>=40)&&(CORE_FREQ<=62))
		{								  	
			lcdRegDataWrite(0x09,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x0A,(CORE_FREQ*8/OSC_FREQ)-1);
		}
		else if((CORE_FREQ>=32)&&(CORE_FREQ<=39))
		{								  	
			lcdRegDataWrite(0x09,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x0A,(CORE_FREQ*8/OSC_FREQ)-1);
		}
		else if(CORE_FREQ<=31)
		{
			lcdRegDataWrite(0x09,0x06);				//PLL Divided by 8
			lcdRegDataWrite(0x0A,(30*8/OSC_FREQ)-1);	//set to 30MHz if out off range
		}

	delay(1);
	lcdRegWrite(0x01);
	delay(2);
	lcdDataWrite(0x80);
	delay(2); //wait for pll stable
	if((lcdDataRead()&0x80)==0x80)
		return true;
	else
		return false; 
}

//**************************************************************//
// Initialize SDRAM
//**************************************************************//
boolean RA8876_8080::ra8876SdramInitial(void)
{
ru8	CAS_Latency;
ru16	Auto_Refresh;

#ifdef IS42SM16160D
  if(DRAM_FREQ<=133)	
  CAS_Latency=2;
  else 				
  CAS_Latency=3;

  Auto_Refresh=(64*DRAM_FREQ*1000)/(8192);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0xf9);        
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x09);
 #endif

 #ifdef IS42S16320B	
  if(DRAM_FREQ<=133)	
  CAS_Latency=2;
  else 				
  CAS_Latency=3;	
  
  Auto_Refresh=(64*DRAM_FREQ*1000)/(8192);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x32);	
  lcdRegDataWrite(0xe1,CAS_Latency);
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x09);
 #endif

#ifdef IS42S16400F
  if(DRAM_FREQ<143)	
  CAS_Latency=2;
  else 
  CAS_Latency=3;
  
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x28);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef M12L32162A
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x08);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x09);
 #endif

 #ifdef M12L2561616A
  CAS_Latency=3;	
  Auto_Refresh=(64*DRAM_FREQ*1000)/(8192);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x31);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef M12L64164A
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x28);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x09);
 #endif

 #ifdef W9825G6JH
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x31);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef W9812G6JH
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);	
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x29);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef MT48LC4M16A
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
	Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x28);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef K4S641632N
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x28);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
#endif

#ifdef K4S281632K
  CAS_Latency=3;
  Auto_Refresh=(64*DRAM_FREQ*1000)/(4096);	
  Auto_Refresh=Auto_Refresh-2; 
  lcdRegDataWrite(0xe0,0x29);      
  lcdRegDataWrite(0xe1,CAS_Latency);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,Auto_Refresh);
  lcdRegDataWrite(0xe3,Auto_Refresh>>8);
  lcdRegDataWrite(0xe4,0x01);
 #endif

 #ifdef INTERNAL_LT7683
  lcdRegDataWrite(0xe0,0x29);      
  lcdRegDataWrite(0xe1,0x03);      //CAS:2=0x02，CAS:3=0x03
  lcdRegDataWrite(0xe2,0x1A);
  lcdRegDataWrite(0xe3,0x06);
  lcdRegDataWrite(0xe4,0x01);
 #endif
  return checkSdramReady();
}

//**************************************************************//
// Turn Display ON/Off (true = ON)
//**************************************************************//
void RA8876_8080::displayOn(boolean on)
{
	unsigned char temp;
	
	// Maybe preserve some of the bits. 
	temp = lcdRegDataRead(RA8876_DPCR) & (1 << 3);

  if(on)
   lcdRegDataWrite(RA8876_DPCR, XPCLK_INV<<7|RA8876_DISPLAY_ON<<6|RA8876_OUTPUT_RGB|temp);
  else
   lcdRegDataWrite(RA8876_DPCR, XPCLK_INV<<7|RA8876_DISPLAY_OFF<<6|RA8876_OUTPUT_RGB|temp);
   
  delay(20);
 }

//**************************************************************//
// Turn Backlight ON/Off (true = ON)
//**************************************************************//
void RA8876_8080::backlight(boolean on)
{

  if(on) {
	//Enable_PWM0_Interrupt();
	//Clear_PWM0_Interrupt_Flag();
 	//Mask_PWM0_Interrupt_Flag();
	//Select_PWM0_Clock_Divided_By_2();
 	//Select_PWM0();
// 	pwm_ClockMuxReg(0, RA8876_PWM_TIMER_DIV2, 0, RA8876_XPWM0_OUTPUT_PWM_TIMER0);
 	//Enable_PWM0_Dead_Zone();
	//Auto_Reload_PWM0();
	//Start_PWM0();


//	pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_OFF, RA8876_PWM_TIMER1_AUTO_RELOAD,RA8876_PWM_TIMER1_STOP,
//					RA8876_PWM_TIMER0_DEAD_ZONE_ENABLE, RA8876_PWM_TIMER1_INVERTER_OFF,
//					RA8876_PWM_TIMER0_AUTO_RELOAD, RA8876_PWM_TIMER0_START);

	//pwm0_Duty(0x0000);
	//pwm_Prescaler(128);

	pwm_Prescaler(RA8876_PRESCALER); //if core_freq = 120MHz, pwm base clock = 120/(3+1) = 30MHz
	pwm_ClockMuxReg(RA8876_PWM_TIMER_DIV4, RA8876_PWM_TIMER_DIV4, RA8876_XPWM1_OUTPUT_PWM_TIMER1,RA8876_XPWM0_OUTPUT_PWM_TIMER0);
//pwm timer clock = 30/4 = 7.5MHz

	pwm0_ClocksPerPeriod(256); // pwm0 = 7.5MHz/1024 = 7.3KHz
	pwm0_Duty(10);//pwm0 set 10/1024 duty

	pwm1_ClocksPerPeriod(256); // pwm1 = 7.5MHz/256 = 29.2KHz
	pwm1_Duty(5); //pwm1 set 5/256 duty

	pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_ON,RA8876_PWM_TIMER1_AUTO_RELOAD,RA8876_PWM_TIMER1_START,RA8876_PWM_TIMER0_DEAD_ZONE_DISABLE
	, RA8876_PWM_TIMER0_INVERTER_ON, RA8876_PWM_TIMER0_AUTO_RELOAD,RA8876_PWM_TIMER0_START);

  }
  else 
  {
	pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_OFF, RA8876_PWM_TIMER1_AUTO_RELOAD,RA8876_PWM_TIMER1_STOP,
					RA8876_PWM_TIMER0_DEAD_ZONE_ENABLE, RA8876_PWM_TIMER1_INVERTER_OFF,
					RA8876_PWM_TIMER0_AUTO_RELOAD, RA8876_PWM_TIMER0_STOP);

  }

}

void RA8876_8080::setBrightness(ru8 brightness)
{
	/*
  pwm_ClockMuxReg(0, RA8876_PWM_TIMER_DIV2, 0, RA8876_XPWM0_OUTPUT_PWM_TIMER0);
  pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_OFF, RA8876_PWM_TIMER1_AUTO_RELOAD,RA8876_PWM_TIMER1_STOP,
					RA8876_PWM_TIMER0_DEAD_ZONE_ENABLE, RA8876_PWM_TIMER1_INVERTER_OFF,
					RA8876_PWM_TIMER0_AUTO_RELOAD, RA8876_PWM_TIMER0_START);
					*/
  pwm0_Duty(0x00FF - brightness);
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdHorizontalWidthVerticalHeight(ru16 width,ru16 height)
{
	unsigned char temp;
	temp=(width/8)-1;
	lcdRegDataWrite(RA8876_HDWR,temp);
	temp=width%8;
	lcdRegDataWrite(RA8876_HDWFTR,temp);
	temp=height-1;
	lcdRegDataWrite(RA8876_VDHR0,temp);
	temp=(height-1)>>8;
	lcdRegDataWrite(RA8876_VDHR1,temp);
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdHorizontalNonDisplay(ru16 numbers)
{
	ru8 temp;
	if(numbers<8)
	{
		lcdRegDataWrite(RA8876_HNDR,0x00);
		lcdRegDataWrite(RA8876_HNDFTR,numbers);
	} else {
		temp=(numbers/8)-1;
		lcdRegDataWrite(RA8876_HNDR,temp);
		temp=numbers%8;
		lcdRegDataWrite(RA8876_HNDFTR,temp);
	}	
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdHsyncStartPosition(ru16 numbers)
{
	ru8 temp;
	if(numbers<8)
	{
		lcdRegDataWrite(RA8876_HSTR,0x00);
	} else {
		temp=(numbers/8)-1;
		lcdRegDataWrite(RA8876_HSTR,temp);
	}
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdHsyncPulseWidth(ru16 numbers)
{
	ru8 temp;
	if(numbers<8)
	{
		lcdRegDataWrite(RA8876_HPWR,0x00);
	} else {
		temp=(numbers/8)-1;
		lcdRegDataWrite(RA8876_HPWR,temp);
	}
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdVerticalNonDisplay(ru16 numbers)
{
	ru8 temp;
	temp=numbers-1;
	lcdRegDataWrite(RA8876_VNDR0,temp);
	lcdRegDataWrite(RA8876_VNDR1,temp>>8);
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdVsyncStartPosition(ru16 numbers)
{
	ru8 temp;
	temp=numbers-1;
	lcdRegDataWrite(RA8876_VSTR,temp);
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::lcdVsyncPulseWidth(ru16 numbers)
{
	ru8 temp;
	temp=numbers-1;
	lcdRegDataWrite(RA8876_VPWR,temp);
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::displayImageStartAddress(ru32 addr)	
{
	lcdRegDataWrite(RA8876_MISA0,addr);//20h
	lcdRegDataWrite(RA8876_MISA1,addr>>8);//21h 
	lcdRegDataWrite(RA8876_MISA2,addr>>16);//22h  
	lcdRegDataWrite(RA8876_MISA3,addr>>24);//23h 
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::displayImageWidth(ru16 width)	
{
	lcdRegDataWrite(RA8876_MIW0,width); //24h
	lcdRegDataWrite(RA8876_MIW1,width>>8); //25h 
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::displayWindowStartXY(ru16 x0,ru16 y0)	
{
	lcdRegDataWrite(RA8876_MWULX0,x0);//26h
	lcdRegDataWrite(RA8876_MWULX1,x0>>8);//27h
	lcdRegDataWrite(RA8876_MWULY0,y0);//28h
	lcdRegDataWrite(RA8876_MWULY1,y0>>8);//29h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::canvasImageStartAddress(ru32 addr)	
{
	lcdRegDataWrite(RA8876_CVSSA0,addr);//50h
	lcdRegDataWrite(RA8876_CVSSA1,addr>>8);//51h
	lcdRegDataWrite(RA8876_CVSSA2,addr>>16);//52h
	lcdRegDataWrite(RA8876_CVSSA3,addr>>24);//53h  
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::canvasImageWidth(ru16 width)	
{
	lcdRegDataWrite(RA8876_CVS_IMWTH0,width);//54h
	lcdRegDataWrite(RA8876_CVS_IMWTH1,width>>8); //55h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::activeWindowXY(ru16 x0,ru16 y0)	
{
	lcdRegDataWrite(RA8876_AWUL_X0,x0);//56h
	lcdRegDataWrite(RA8876_AWUL_X1,x0>>8);//57h 
	lcdRegDataWrite(RA8876_AWUL_Y0,y0);//58h
	lcdRegDataWrite(RA8876_AWUL_Y1,y0>>8);//59h 
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::activeWindowWH(ru16 width,ru16 height)	
{
	lcdRegDataWrite(RA8876_AW_WTH0,width);//5ah
	lcdRegDataWrite(RA8876_AW_WTH1,width>>8);//5bh
	lcdRegDataWrite(RA8876_AW_HT0,height);//5ch
	lcdRegDataWrite(RA8876_AW_HT1,height>>8);//5dh  
}

//**************************************************************//
//**************************************************************//
void  RA8876_8080::setPixelCursor(ru16 x,ru16 y)
{
	switch (_rotation) {
		case 1: swapvals(x,y); break;
		case 2: x = _width-x; break;
		case 3: rotateCCXY(x,y); break;
	}
	lcdRegDataWrite(RA8876_CURH0,x); //5fh
	lcdRegDataWrite(RA8876_CURH1,x>>8);//60h
	lcdRegDataWrite(RA8876_CURV0,y);//61h
	lcdRegDataWrite(RA8876_CURV1,y>>8);//62h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::linearAddressSet(ru32 addr)	
{
	lcdRegDataWrite(RA8876_CURH0,addr); //5fh
	lcdRegDataWrite(RA8876_CURH1,addr>>8); //60h
	lcdRegDataWrite(RA8876_CURV0,addr>>16); //61h
	lcdRegDataWrite(RA8876_CURV1,addr>>24); //62h
}

//**************************************************************//
//**************************************************************//
ru8 RA8876_8080::vmemReadData(ru32 addr)	
{
	ru8 vmemData = 0;
  
	graphicMode(true);
	Memory_Linear_Mode();
	linearAddressSet(addr);
	ramAccessPrepare();
	vmemData = lcdDataRead(); // dummyread to alert SPI
	vmemData = lcdDataRead(); // read byte
	check2dBusy();
	Memory_XY_Mode();
	return vmemData;
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::vmemWriteData(ru32 addr, ru8 vmemData)	
{
	graphicMode(true);
	Memory_Linear_Mode();
	linearAddressSet(addr);
	ramAccessPrepare();
//  checkWriteFifoNotFull();//if high speed mcu and without Xnwait check
	lcdDataWrite(vmemData);
//  checkWriteFifoEmpty();//if high speed mcu and without Xnwait check
	graphicMode(false);
	Memory_XY_Mode();
	graphicMode(false);
}

//**************************************************************//
//**************************************************************//
ru16 RA8876_8080::vmemReadData16(ru32 addr)	
{
	ru16 vmemData = 0;
  
	vmemData = (vmemReadData(addr) & 0xff); // lo byte
	vmemData |= vmemReadData(addr+1) << 8; // hi byte
	return vmemData;
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::vmemWriteData16(ru32 addr, ru16 vmemData)	
{
	vmemWriteData(addr,vmemData); // lo byte
	vmemWriteData(addr+1,vmemData>>8); // hi byte
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source0_MemoryStartAddr(ru32 addr)	
{
	lcdRegDataWrite(RA8876_S0_STR0,addr);//93h
	lcdRegDataWrite(RA8876_S0_STR1,addr>>8);//94h
	lcdRegDataWrite(RA8876_S0_STR2,addr>>16);//95h
	lcdRegDataWrite(RA8876_S0_STR3,addr>>24);////96h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source0_ImageWidth(ru16 width)	
{
	lcdRegDataWrite(RA8876_S0_WTH0,width);//97h
	lcdRegDataWrite(RA8876_S0_WTH1,width>>8);//98h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source0_WindowStartXY(ru16 x0,ru16 y0)	
{
	lcdRegDataWrite(RA8876_S0_X0,x0);//99h
	lcdRegDataWrite(RA8876_S0_X1,x0>>8);//9ah
	lcdRegDataWrite(RA8876_S0_Y0,y0);//9bh
	lcdRegDataWrite(RA8876_S0_Y1,y0>>8);//9ch
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source1_MemoryStartAddr(ru32 addr)	
{
	lcdRegDataWrite(RA8876_S1_STR0,addr);//9dh
	lcdRegDataWrite(RA8876_S1_STR1,addr>>8);//9eh
	lcdRegDataWrite(RA8876_S1_STR2,addr>>16);//9fh
	lcdRegDataWrite(RA8876_S1_STR3,addr>>24);//a0h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source1_ImageWidth(ru16 width)	
{
	lcdRegDataWrite(RA8876_S1_WTH0,width);//a1h
	lcdRegDataWrite(RA8876_S1_WTH1,width>>8);//a2h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bte_Source1_WindowStartXY(ru16 x0,ru16 y0)	
{
	lcdRegDataWrite(RA8876_S1_X0,x0);//a3h
	lcdRegDataWrite(RA8876_S1_X1,x0>>8);//a4h
	lcdRegDataWrite(RA8876_S1_Y0,y0);//a5h
	lcdRegDataWrite(RA8876_S1_Y1,y0>>8);//a6h
}

//**************************************************************//
//**************************************************************//
void  RA8876_8080::bte_DestinationMemoryStartAddr(ru32 addr)	
{
	lcdRegDataWrite(RA8876_DT_STR0,addr);//a7h
	lcdRegDataWrite(RA8876_DT_STR1,addr>>8);//a8h
	lcdRegDataWrite(RA8876_DT_STR2,addr>>16);//a9h
	lcdRegDataWrite(RA8876_DT_STR3,addr>>24);//aah
}

//**************************************************************//
//**************************************************************//
void  RA8876_8080::bte_DestinationImageWidth(ru16 width)	
{
	lcdRegDataWrite(RA8876_DT_WTH0,width);//abh
	lcdRegDataWrite(RA8876_DT_WTH1,width>>8);//ach
}

//**************************************************************//
//**************************************************************//
void  RA8876_8080::bte_DestinationWindowStartXY(ru16 x0,ru16 y0)	
{
	lcdRegDataWrite(RA8876_DT_X0,x0);//adh
	lcdRegDataWrite(RA8876_DT_X1,x0>>8);//aeh
	lcdRegDataWrite(RA8876_DT_Y0,y0);//afh
	lcdRegDataWrite(RA8876_DT_Y1,y0>>8);//b0h
}

//**************************************************************//
//**************************************************************//
void  RA8876_8080::bte_WindowSize(ru16 width, ru16 height)
{
	lcdRegDataWrite(RA8876_BTE_WTH0,width);//b1h
	lcdRegDataWrite(RA8876_BTE_WTH1,width>>8);//b2h
	lcdRegDataWrite(RA8876_BTE_HIG0,height);//b3h
	lcdRegDataWrite(RA8876_BTE_HIG1,height>>8);//b4h
}

//**************************************************************//
// Window alpha allows 2 source images to be blended together
// with a constant blend factor (alpha) over the entire picture.
// Also called "picture mode".
// 
// alpha can be from 0 to 32
//**************************************************************//
void  RA8876_8080::bte_WindowAlpha(ru8 alpha)
{
	lcdRegDataWrite(RA8876_APB_CTRL,alpha);//b5h
}

//**************************************************************//
// These 8 bits determine prescaler value for Timer 0 and 1.*/
// Time base is “Core_Freq / (Prescaler + 1)”*/
//**************************************************************//
void RA8876_8080::pwm_Prescaler(ru8 prescaler)
{
	lcdRegDataWrite(RA8876_PSCLR,prescaler);//84h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm_ClockMuxReg(ru8 pwm1_clk_div, ru8 pwm0_clk_div, ru8 xpwm1_ctrl, ru8 xpwm0_ctrl)
{
	lcdRegDataWrite(RA8876_PMUXR,pwm1_clk_div<<6|pwm0_clk_div<<4|xpwm1_ctrl<<2|xpwm0_ctrl);//85h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm_Configuration(ru8 pwm1_inverter,ru8 pwm1_auto_reload,ru8 pwm1_start,ru8 
                       pwm0_dead_zone, ru8 pwm0_inverter, ru8 pwm0_auto_reload,ru8 pwm0_start) {
	lcdRegDataWrite(RA8876_PCFGR,pwm1_inverter<<6|pwm1_auto_reload<<5|pwm1_start<<4|pwm0_dead_zone<<3|
	pwm0_inverter<<2|pwm0_auto_reload<<1|pwm0_start);//86h                
}   

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm0_Duty(ru16 duty)
{
	lcdRegDataWrite(RA8876_TCMPB0L,duty);//88h 
	lcdRegDataWrite(RA8876_TCMPB0H,duty>>8);//89h 
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm0_ClocksPerPeriod(ru16 clocks_per_period)
{
	lcdRegDataWrite(RA8876_TCNTB0L,clocks_per_period);//8ah
	lcdRegDataWrite(RA8876_TCNTB0H,clocks_per_period>>8);//8bh
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm1_Duty(ru16 duty)
{
	lcdRegDataWrite(RA8876_TCMPB1L,duty);//8ch 
	lcdRegDataWrite(RA8876_TCMPB1H,duty>>8);//8dh
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::pwm1_ClocksPerPeriod(ru16 clocks_per_period)
{
	lcdRegDataWrite(RA8876_TCNTB1L,clocks_per_period);//8eh
	lcdRegDataWrite(RA8876_TCNTB1F,clocks_per_period>>8);//8fh
}

//**************************************************************//
// Call this before reading or writing to SDRAM
//**************************************************************//
void  RA8876_8080::ramAccessPrepare(void)
{
	//check2dBusy();
	checkWriteFifoNotFull();
	checkWriteFifoEmpty();
	lcdRegWrite(RA8876_MRWDP); //04h
	//checkWriteFifoNotFull();
	
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::foreGroundColor16bpp(ru16 color, bool finalize)
{
	lcdRegDataWrite(RA8876_FGCR,color>>8, false);//d2h
	lcdRegDataWrite(RA8876_FGCG,color>>3, false);//d3h
	lcdRegDataWrite(RA8876_FGCB,color<<3, finalize);//d4h
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::backGroundColor16bpp(ru16 color, bool finalize)
{
	lcdRegDataWrite(RA8876_BGCR,color>>8, false);//d5h
	lcdRegDataWrite(RA8876_BGCG,color>>3, false);//d6h
	lcdRegDataWrite(RA8876_BGCB,color<<3, finalize);//d7h
}

//***************************************************//
/*                 GRAPHIC FUNCTIONS                 */
//***************************************************//

//**************************************************************//
/* Turn RA8876 graphic mode ON/OFF (True = ON)                  */
/* Inverse of text mode                                         */
//**************************************************************//
void RA8876_8080::graphicMode(boolean on)
{
	if(on)
		lcdRegDataWrite(RA8876_ICR,RA8877_LVDS_FORMAT<<3|RA8876_GRAPHIC_MODE<<2|RA8876_MEMORY_SELECT_IMAGE);//03h  //switch to graphic mode
	else
		lcdRegDataWrite(RA8876_ICR,RA8877_LVDS_FORMAT<<3|RA8876_TEXT_MODE<<2|RA8876_MEMORY_SELECT_IMAGE);//03h  //switch back to text mode

}

//**************************************************************//
/* Read a 16bpp pixel                                           */
//**************************************************************//
ru16 RA8876_8080::getPixel(ru16 x,ru16 y)
{
	ru16 rdata = 0;
	graphicMode(true);
	setPixelCursor(x, y);		// set memory address
	ramAccessPrepare();			// Setup SDRAM Access
	rdata = lcdDataRead();		// dummyread to alert SPI
	rdata = lcdDataRead();		// read low byte
	rdata |= lcdDataRead()<<8;	// add high byte 
	return rdata;
}

//**************************************************************//
/* Write a 16bpp pixel                                          */
//**************************************************************//
void  RA8876_8080::drawPixel(ru16 x,ru16 y,ru16 color)
{
	graphicMode(true);
	setPixelCursor(x,y);
	ramAccessPrepare();
	lcdDataWrite(color);
	lcdDataWrite(color>>8);
	//lcdDataWrite16bbp(color);
}

//**************************************************************//
/* Write 16bpp(RGB565) picture data for user operation          */
/* Not recommended for future use - use BTE instead             */
//**************************************************************//
void  RA8876_8080::putPicture_16bpp(ru16 x,ru16 y,ru16 width, ru16 height)
{
	graphicMode(true);
	activeWindowXY(x,y);
	activeWindowWH(width,height);
	setPixelCursor(x,y);
	ramAccessPrepare();
    // Now your program has to send the image data pixels
}

//*******************************************************************//
/* write 16bpp(RGB565) picture data in byte format from data pointer */
/* Not recommended for future use - use BTE instead                  */
//*******************************************************************//
void  RA8876_8080::putPicture_16bppData8(ru16 x,ru16 y,ru16 width, ru16 height, const unsigned char *data)
{
	ru16 i,j;

	putPicture_16bpp(x, y, width, height);

  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  FlexIO_Config_SnglBeat();
  CSLow();
  DCHigh();
  for(j=0;j<height;j++) {
	for(i=0;i<width;i++) {
      p->SHIFTBUF[0] = *data++;
      /*Wait for transfer to be completed */
      while(0 == (p->SHIFTSTAT & (1 << 0))) {}
      while(0 == (p->TIMSTAT & (1 << 0))) {}
      p->SHIFTBUF[0] = *data++;
      /*Wait for transfer to be completed */
      while(0 == (p->SHIFTSTAT & (1 << 0))) {}
      while(0 == (p->TIMSTAT & (1 << 0))) {}
    }
  }
  /* De-assert /CS pin */
  CSHigh();
	checkWriteFifoEmpty();				//if high speed mcu and without Xnwait check
	activeWindowXY(0,0);
    activeWindowWH(_width,_height);
}

//****************************************************************//
/* Write 16bpp(RGB565) picture data word format from data pointer */
/* Not recommended for future use - use BTE instead               */
//****************************************************************//
void  RA8876_8080::putPicture_16bppData16(ru16 x,ru16 y,ru16 width, ru16 height, const unsigned short *data)
{
	ru16 i,j;
	putPicture_16bpp(x, y, width, height);

  while(WR_IRQTransferDone == false) {} //Wait for any DMA transfers to complete
  FlexIO_Config_SnglBeat();
  CSLow();
  DCHigh();
  for(j=0;j<height;j++) {
	for(i=0;i<width;i++) {
      p->SHIFTBUF[0] = *data++;
      /*Wait for transfer to be completed */
      while(0 == (p->SHIFTSTAT & (1 << 0))) {}
      while(0 == (p->TIMSTAT & (1 << 0))) {}
    }
  }
  /* De-assert /CS pin */
  CSHigh();
	checkWriteFifoEmpty();//if high speed mcu and without Xnwait check
	activeWindowXY(0,0);
	activeWindowWH(_width,_height);
}


//***************************************************//
/*                 GRAPHIC FUNCTIONS                    */
//***************************************************//

//***************************************************//
/* Select Character Generator RAM                    */
//***************************************************//
void RA8876_8080::Memory_Select_CGRAM(void) // this may not be right
{
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_ICR); //0x03
//    temp &= cClrb1; // manual says this bit is clear
    temp |= cSetb0;
    temp &= cClrb0; // manual says this bit is set ?? Figure 14-10
	lcdRegDataWrite(RA8876_ICR, temp);
}

//***************************************************//
/* Select display buffer RAM                         */
//***************************************************//
void RA8876_8080::Memory_Select_SDRAM(void)
{
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_ICR); //0x03
    temp &= cClrb1;
    temp &= cClrb0;	// B
	lcdRegDataWrite(RA8876_ICR, temp);
}

//***************************************************//
/* Select graphic cursor RAM                         */
//***************************************************//
void RA8876_8080::Memory_Select_Graphic_Cursor_RAM(void)
{
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_ICR); //0x03
    temp |= cSetb1;
    temp &= cClrb0;
	lcdRegDataWrite(RA8876_ICR, temp);
}

//***************************************************//
/* Select RAM X-Y addressing mode                    */
//***************************************************//
void RA8876_8080::Memory_XY_Mode(void)	
{
/*
Canvas addressing mode
	0: Block mode (X-Y coordination addressing)
	1: linear mode
*/
	unsigned char temp;

	temp = lcdRegDataRead(RA8876_AW_COLOR);
	temp &= cClrb2;
	lcdRegDataWrite(RA8876_AW_COLOR, temp);
}

//***************************************************//
/* Select RAM linear addressing mode                 */
//***************************************************//
void RA8876_8080::Memory_Linear_Mode(void)	
{
/*
Canvas addressing mode
	0: Block mode (X-Y coordination addressing)
	1: linear mode
*/
	unsigned char temp;

	temp = lcdRegDataRead(RA8876_AW_COLOR);
	temp |= cSetb2;
	lcdRegDataWrite(RA8876_AW_COLOR, temp);
}

//***************************************************//
/* Enable Graphic Cursor                             */
//***************************************************//
void RA8876_8080::Enable_Graphic_Cursor(void)	
{
/*
Graphic Cursor Enable
	0 : Graphic Cursor disable.
	1 : Graphic Cursor enable.
*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp |= cSetb4;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//***************************************************//
/* Disable Graphic Cursor                            */
//***************************************************//
void RA8876_8080::Disable_Graphic_Cursor(void)	
{
/*
Graphic Cursor Enable
	0 : Graphic Cursor disable.
	1 : Graphic Cursor enable.
*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp &= cClrb4;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Select one of four available graphic cursors  */
//************************************************/
void RA8876_8080::Select_Graphic_Cursor_1(void)	
{
/*
Graphic Cursor Selection Bit
Select one from four graphic cursor types. (00b to 11b)
	00b : Graphic Cursor Set 1.
	01b : Graphic Cursor Set 2.
	10b : Graphic Cursor Set 3.
	11b : Graphic Cursor Set 4.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp &= cClrb3;
	temp &= cClrb2;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Select one of four available graphic cursors  */
//************************************************/
void RA8876_8080::Select_Graphic_Cursor_2(void)	
{
/*
Graphic Cursor Selection Bit
Select one from four graphic cursor types. (00b to 11b)
	00b : Graphic Cursor Set 1.
	01b : Graphic Cursor Set 2.
	10b : Graphic Cursor Set 3.
	11b : Graphic Cursor Set 4.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp &= cClrb3;
	temp |= cSetb2;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Select one of four available graphic cursors  */
//************************************************/
void RA8876_8080::Select_Graphic_Cursor_3(void)	
{
/*
Graphic Cursor Selection Bit
Select one from four graphic cursor types. (00b to 11b)
	00b : Graphic Cursor Set 1.
	01b : Graphic Cursor Set 2.
	10b : Graphic Cursor Set 3.
	11b : Graphic Cursor Set 4.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp |= cSetb3;
	temp &= cClrb2;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Select one of four available graphic cursors  */
//************************************************/
void RA8876_8080::Select_Graphic_Cursor_4(void)	
{
/*
Graphic Cursor Selection Bit
Select one from four graphic cursor types. (00b to 11b)
	00b : Graphic Cursor Set 1.
	01b : Graphic Cursor Set 2.
	10b : Graphic Cursor Set 3.
	11b : Graphic Cursor Set 4.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); //0x3c
	temp |= cSetb3;
	temp |= cSetb2;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Position graphic cursor                       */
//************************************************/
void RA8876_8080::Graphic_Cursor_XY(int16_t WX,int16_t HY)
{
/*
REG[40h] Graphic Cursor Horizontal Location[7:0]
REG[41h] Graphic Cursor Horizontal Location[12:8]
REG[42h] Graphic Cursor Vertical Location[7:0]
REG[43h] Graphic Cursor Vertical Location[12:8]
Reference main Window coordinates.
*/	
	gCursorX = WX;
	gCursorY = HY;

	if(WX < 0 && WX > -32) WX = 0; //cursor partially visible off the left/top of the screen is coerced onto the screen
	if(HY < 0 && HY > -32) HY = 0;

	lcdRegDataWrite(RA8876_GCHP0,WX, false);
	lcdRegDataWrite(RA8876_GCHP1,WX>>8, false);

	lcdRegDataWrite(RA8876_GCVP0,HY, false);
	lcdRegDataWrite(RA8876_GCVP1,HY>>8, true);
}

//************************************************/
/* Set graphic cursor foreground color           */
//************************************************/
void RA8876_8080::Set_Graphic_Cursor_Color_1(unsigned char temp)
{
/*
REG[44h] Graphic Cursor Color 0 with 256 Colors
RGB Format [7:0] = RRRGGGBB.
*/	
	lcdRegDataWrite(RA8876_GCC0,temp);
}

//************************************************/
/* Set graphic cursor outline color              */
//************************************************/
void RA8876_8080::Set_Graphic_Cursor_Color_2(unsigned char temp)
{
/*
REG[45h] Graphic Cursor Color 1 with 256 Colors
RGB Format [7:0] = RRRGGGBB.
*/	
	lcdRegDataWrite(RA8876_GCC1,temp);
}

//************************************************/
/* turn on text cursor                           */
//************************************************/
void RA8876_8080::Enable_Text_Cursor(void)	
{
/*
Text Cursor Enable
	0 : Disable.
	1 : Enable.
Text cursor & Graphic cursor cannot enable simultaneously.
Graphic cursor has higher priority then Text cursor if enabled simultaneously.
*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_GTCCR); // 0x3c
	temp |= cSetb1;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* turn off text cursor                          */
//************************************************/
void RA8876_8080::Disable_Text_Cursor(void)	
{
/*
Text Cursor Enable
	0 : Disable.
	1 : Enable.
Text cursor & Graphic cursor cannot enable simultaneously.
Graphic cursor has higher priority then Text cursor if enabled simultaneously.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); // 0x3c
	temp &= cClrb1;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Turn on blinking text cursor                  */
//************************************************/
void RA8876_8080::Enable_Text_Cursor_Blinking(void)	
{
/*
Text Cursor Blinking Enable
	0 : Disable.
	1 : Enable.
*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_GTCCR); // 0x3c
	temp |= cSetb0;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Turn off blinking text cursor                  */
//************************************************/
void RA8876_8080::Disable_Text_Cursor_Blinking(void)	
{
/*
Text Cursor Blinking Enable
	0 : Disable.
	1 : Enable.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_GTCCR); // 0x3c
	temp &= cClrb0;
	lcdRegDataWrite(RA8876_GTCCR,temp);
}

//************************************************/
/* Set text cursor blink rate                    */
//************************************************/
void RA8876_8080::Blinking_Time_Frames(unsigned char temp)	
{
/*
Text Cursor Blink Time Setting (Unit: Frame)
	00h : 1 frame time.
	01h : 2 frames time.
	02h : 3 frames time.
	:
	FFh : 256 frames time.
*/
	lcdRegDataWrite(RA8876_BTCR,temp); // 0x3d
}

//************************************************/
/* Set text cursor horizontal and vertical size  */
//************************************************/
void RA8876_8080::Text_Cursor_H_V(unsigned short WX,unsigned short HY)
{
/*
REG[3Eh]
Text Cursor Horizontal Size Setting[4:0]
Unit : Pixel
Zero-based number. Value 0 means 1 pixel.
Note : When font is enlarged, the cursor setting will multiply the
same times as the font enlargement.
REG[3Fh]
Text Cursor Vertical Size Setting[4:0]
Unit : Pixel
Zero-based number. Value 0 means 1 pixel.
Note : When font is enlarged, the cursor setting will multiply the
same times as the font enlargement.
*/
	lcdRegDataWrite(RA8876_CURHS,WX, false); // 0x3e
	lcdRegDataWrite(RA8876_CURVS,HY, true); // 0x3f
}

//************************************************/
/* Turn on color bar test screen                 */
//************************************************/
void RA8876_8080::Color_Bar_ON(void)
{
/*	
Display Test Color Bar
	0b: Disable.
	1b: Enable.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_DPCR); // 0x12
	temp |= cSetb5;
	lcdRegDataWrite(RA8876_DPCR,temp); // 0x12
}

//************************************************/
/* Turn off color bar test screen                */
//************************************************/
void RA8876_8080::Color_Bar_OFF(void)
{
/*	
Display Test Color Bar
	0b: Disable.
	1b: Enable.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_DPCR); // 0x12
	temp &= cClrb5;
	lcdRegDataWrite(RA8876_DPCR,temp); // 0x12
}

//************************************************/
/* Drawing Functions                             */
//************************************************/

//**************************************************************//
/* Draw a line                                                  */
/* x0,y0: Line start coords                                     */
/* x1,y1: Line end coords                                       */
//**************************************************************//
void RA8876_8080::drawLine(ru16 x0, ru16 y0, ru16 x1, ru16 y1, ru16 color)
{
	x0 += _originx; x1 += _originx;
	y0 += _originy; y1 += _originy;
	
	if ((x0 == x1 && y0 == y1)) {//Thanks MrTOM
		drawPixel(x0,y0,color);
		return;
	}
	
  check2dBusy();
  Serial.println("drawLine");
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x1,y1); break;
		case 2: x0 = _width-x0; x1 = _width-x1;break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x1,y1); break;
  	}
Serial.printf("drawLine x0=%d y0=%d x1=%d y1=%d color=%x\n",x0,y0,x1,y1, color);
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x1, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x1>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y1, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y1>>8, false);//6fh        
  lcdRegDataWrite(RA8876_DCR0,RA8876_DRAW_LINE, true);//67h,0x80
}


//**************************************************************//
// Draw a filled rectangle:
// x0,y0 is upper left start
// x1,y1 is lower right end corner
//**************************************************************//
void RA8876_8080::drawSquareFill(ru16 x0, ru16 y0, ru16 x1, ru16 y1, ru16 color)
{
	x0 += _originx; x1 += _originx;
	y1 += _originy; y1 += _originy;
	int16_t x_end = x1;
	int16_t y_end = y1;
	if((x0 >= _displayclipx2)   || // Clip right
		 (y0 >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x0 < _displayclipx1) x0 = _displayclipx1;
	if (y0 < _displayclipy1) y0 = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;


  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
  switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x_end,y_end); break;
		case 2: x0 = _width-x0; x_end = _width-x_end; break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x_end,y_end); break;
  	}
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh     
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_SQUARE_FILL, true);//76h,0xE0  

}

//**************************************************************//
// Draw a round corner rectangle:
// x0,y0 is upper left start
// x1,y1 is lower right end corner
// xr is the major radius of corner (horizontal)
// yr is the minor radius of corner (vertical)
//**************************************************************//
void RA8876_8080::drawCircleSquare(ru16 x0, ru16 y0, ru16 x1, ru16 y1, ru16 xr, ru16 yr, ru16 color)
{
	x0 += _originx; x1 += _originx;
	y1 += _originy; y1 += _originy;
	int16_t x_end = x1;
	int16_t y_end = y1;
	if((x0 >= _displayclipx2)   || // Clip right
		 (y0 >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x0 < _displayclipx1) x0 = _displayclipx1;
	if (y0 < _displayclipy1) y0 = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;	

  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x_end,y_end); swapvals(xr, yr); break;
		case 2: x0 = _width-x0; x_end = _width-x_end;break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x_end,y_end);  swapvals(xr, yr);break;
  	}
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh    
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//79h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//7ah    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7bh
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE_SQUARE, true);//76h,0xb0
}

//**************************************************************//
// Draw a filled round corner rectangle:
// x0,y0 is upper left start
// x1,y1 is lower right end corner
// xr is the major radius of corner (horizontal)
// yr is the minor radius of corner (vertical)
//**************************************************************//
void RA8876_8080::drawCircleSquareFill(ru16 x0, ru16 y0, ru16 x1, ru16 y1, ru16 xr, ru16 yr, ru16 color)
{
	x0 += _originx; x1 += _originx;
	y1 += _originy; y1 += _originy;
	int16_t x_end = x1;
	int16_t y_end = y1;
	if((x0 >= _displayclipx2)   || // Clip right
		 (y0 >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x0 < _displayclipx1) x0 = _displayclipx1;
	if (y0 < _displayclipy1) y0 = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;
	
  check2dBusy();
  //graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x_end,y_end); swapvals(xr, yr); break;
		case 2: x0 = _width-x0; x_end = _width-x_end;break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x_end,y_end);  swapvals(xr, yr);break;
  	}
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh    
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7ah
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE_SQUARE_FILL, true);//76h,0xf0
}

//**************************************************************//
// Draw a triangle
// x0,y0 is triangle start
// x1,y1 is triangle second point
// x2,y2 is triangle end point
//**************************************************************//
void RA8876_8080::drawTriangle(ru16 x0,ru16 y0,ru16 x1,ru16 y1,ru16 x2,ru16 y2,ru16 color)
{
  x0 += _originx; x1 += _originx; x2 += _originx; 
  y0 += _originy; y1 += _originy; y2 += _originy;
	
	if (x0 >= _width || x1 >= _width || x2 >= _width) return;
	if (y0 >= _height || y1 >= _height || y2 >= _height) return;
	if (x0 == x1 && y0 == y1 && x0 == x2 && y0 == y2) {			// All points are same
		drawPixel(x0,y0, color);
		return;
	}

	switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x1,y1); swapvals(x2,y2);  break;
		case 2: x0 = _width-x0; x1 = _width - x1; x2 = _width - x2; break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x1,y1); rotateCCXY(x2,y2); break;
  	}

  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h point 0
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h point 0
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah point 0
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh point 0
  lcdRegDataWrite(RA8876_DLHER0,x1, false);//6ch point 1
  lcdRegDataWrite(RA8876_DLHER1,x1>>8, false);//6dh point 1
  lcdRegDataWrite(RA8876_DLVER0,y1, false);//6eh point 1
  lcdRegDataWrite(RA8876_DLVER1,y1>>8, false);//6fh point 1
  lcdRegDataWrite(RA8876_DTPH0,x2, false);//70h point 2
  lcdRegDataWrite(RA8876_DTPH1,x2>>8, false);//71h point 2
  lcdRegDataWrite(RA8876_DTPV0,y2, false);//72h point 2
  lcdRegDataWrite(RA8876_DTPV1,y2>>8, false);//73h  point 2
  lcdRegDataWrite(RA8876_DCR0,RA8876_DRAW_TRIANGLE, false);//67h,0x82
}

//**************************************************************//
// Draw a filled triangle
// x0,y0 is triangle start
// x1,y1 is triangle second point
// x2,y2 is triangle end point
//**************************************************************//
void RA8876_8080::drawTriangleFill(ru16 x0,ru16 y0,ru16 x1,ru16 y1,ru16 x2,ru16 y2,ru16 color)
{
  x0 += _originx; x1 += _originx; x2 += _originx; 
  y0 += _originy; y1 += _originy; y2 += _originy;
	
	if (x0 >= _width || x1 >= _width || x2 >= _width) return;
	if (y0 >= _height || y1 >= _height || y2 >= _height) return;
	if (x0 == x1 && y0 == y1 && x0 == x2 && y0 == y2) {			// All points are same
		drawPixel(x0,y0, color);
		return;
	}
	
	switch (_rotation) {
		case 1: swapvals(x0,y0); swapvals(x1,y1); swapvals(x2,y2);  break;
		case 2: x0 = _width-x0; x1 = _width - x1; x2 = _width - x2; break;
		case 3: rotateCCXY(x0,y0); rotateCCXY(x1,y1); rotateCCXY(x2,y2); break;
  	}

  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
  lcdRegDataWrite(RA8876_DLHSR0,x0, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x0>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y0, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y0>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x1, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x1>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y1, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y1>>8, false);//6fh  
  lcdRegDataWrite(RA8876_DTPH0,x2, false);//70h
  lcdRegDataWrite(RA8876_DTPH1,x2>>8, false);//71h
  lcdRegDataWrite(RA8876_DTPV0,y2, false);//72h
  lcdRegDataWrite(RA8876_DTPV1,y2>>8, false);//73h  
  lcdRegDataWrite(RA8876_DCR0,RA8876_DRAW_TRIANGLE_FILL, true);//67h,0xa2
}

//**************************************************************//
// Draw a circle
// x,y is center point of circle
// r is radius of circle
// See page 59 of RA8876.pdf for information on drawing arc's. (1 of 4 quadrants at a time only) 
//**************************************************************//
void RA8876_8080::drawCircle(ru16 x0,ru16 y0,ru16 r,ru16 color)
{
  x0 += _originx;
  y0 += _originy;
  
	if (r < 1) r = 1;
	if (r < 2) {//NEW
		drawPixel(x0,y0,color);
		return;
	}
	if (r > _height / 2) r = (_height / 2) - 1;//this is the (undocumented) hardware limit of RA8875
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0);  break;
		case 2: x0 = _width-x0;   break;
		case 3: rotateCCXY(x0,y0);  break;
  	}

  lcdRegDataWrite(RA8876_DEHR0,x0, false);//7bh
  lcdRegDataWrite(RA8876_DEHR1,x0>>8, false);//7ch
  lcdRegDataWrite(RA8876_DEVR0,y0, false);//7dh
  lcdRegDataWrite(RA8876_DEVR1,y0>>8, false);//7eh
  lcdRegDataWrite(RA8876_ELL_A0,r, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,r>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,r, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,r>>8, false);//7ah
//  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_BOTTOM_LEFT_CURVE);//76h,0x90 (arc test)
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE, true);//76h,0x80
}

//**************************************************************//
// Draw a filled circle
// x,y is center point of circle
// r is radius of circle
// See page 59 of RA8876.pdf for information on drawing arc's. (1 of 4 quadrants at a time only)
//**************************************************************//
void RA8876_8080::drawCircleFill(ru16 x0,ru16 y0,ru16 r,ru16 color)
{
  x0 += _originx;
  y0 += _originy;
  
	if (r < 1) r = 1;
	if (r < 2) {//NEW
		drawPixel(x0,y0,color);
		return;
	}
	if (r > _height / 2) r = (_height / 2) - 1;//this is the (undocumented) hardware limit of RA8875
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0);  break;
		case 2: x0 = _width-x0;   break;
		case 3: rotateCCXY(x0,y0);  break;
  	}
  lcdRegDataWrite(RA8876_DEHR0,x0, false);//7bh
  lcdRegDataWrite(RA8876_DEHR1,x0>>8, false);//7ch
  lcdRegDataWrite(RA8876_DEVR0,y0, false);//7dh
  lcdRegDataWrite(RA8876_DEVR1,y0>>8, false);//7eh
  lcdRegDataWrite(RA8876_ELL_A0,r, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,r>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,r, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,r>>8, false);//7ah
//  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_BOTTOM_LEFT_CURVE_FILL);//76h,0xd0 (arc test)
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE_FILL, false);//76h,0xc0
}

//**************************************************************//
// Draw an ellipse
// x0,y0 is ellipse start
// xr is ellipse x radius, major axis
// yr is ellipse y radius, minor axis
//**************************************************************//
void RA8876_8080::drawEllipse(ru16 x0,ru16 y0,ru16 xr,ru16 yr,ru16 color)
{
	
  x0 += _originx;
  y0 += _originy;
  
	//if (_portrait) {
	//	swapvals(x0,y0);
	//	swapvals(xr,yr);
	//	if (longAxis > _height/2) longAxis = (_height / 2) - 1;
	//	if (shortAxis > _width/2) shortAxis = (_width / 2) - 1;
	//} else {
		if (xr > _width/2) xr = (_width / 2) - 1;
		if (yr > _height/2) yr = (_height / 2) - 1;
	//}
	if (xr == 1 && yr == 1) {
		drawPixel(x0,y0,color);
		return;
	}
	
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0);  swapvals(xr,yr); break;
		case 2: x0 = _width-x0;   break;
		case 3: rotateCCXY(x0,y0); swapvals(xr,yr); break;
  	}
  lcdRegDataWrite(RA8876_DEHR0,x0, false);//7bh
  lcdRegDataWrite(RA8876_DEHR1,x0>>8, false);//7ch
  lcdRegDataWrite(RA8876_DEVR0,y0, false);//7dh
  lcdRegDataWrite(RA8876_DEVR1,y0>>8, false);//7eh
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h  
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7ah
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_ELLIPSE, true);//76h,0x80
}

//**************************************************************//
// Draw an filled ellipse
// x0,y0 is ellipse start
// x1,y1 is ellipse x radius
// x2,y2 is ellipse y radius
//**************************************************************//
void RA8876_8080::drawEllipseFill(ru16 x0,ru16 y0,ru16 xr,ru16 yr,ru16 color)
{
  x0 += _originx;
  y0 += _originy;
  
	//if (_portrait) {
	//	swapvals(xCenter,yCenter);
	//	swapvals(longAxis,shortAxis);
	//	if (longAxis > _height/2) longAxis = (_height / 2) - 1;
	//	if (shortAxis > _width/2) shortAxis = (_width / 2) - 1;
	//} else {
		if (xr > _width/2) xr = (_width / 2) - 1;
		if (yr > _height/2) yr = (_height / 2) - 1;
	//}
	if (xr == 1 && yr == 1) {
		drawPixel(x0,y0,color);
		return;
	}
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x0,y0);  swapvals(xr,yr); break;
		case 2: x0 = _width-x0;   break;
		case 3: rotateCCXY(x0,y0); swapvals(xr,yr); break;
  	}
  lcdRegDataWrite(RA8876_DEHR0,x0, false);//7bh
  lcdRegDataWrite(RA8876_DEHR1,x0>>8, false);//7ch
  lcdRegDataWrite(RA8876_DEVR0,y0, false);//7dh
  lcdRegDataWrite(RA8876_DEVR1,y0>>8, false);//7eh
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7ah
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_ELLIPSE_FILL, true);//76h,0xc0
}

//*************************************************************//
// Scroll Screen up
//*************************************************************//
void RA8876_8080::scroll(void) { // No arguments for now
	bteMemoryCopy(currentPage,SCREEN_WIDTH, _scrollXL, _scrollYT+(_FNTheight*_scaleY),	//Source
				  currentPage,SCREEN_WIDTH, _scrollXL, pageOffset+_scrollYT,	//Desination
				  _scrollXR-_scrollXL, _scrollYB-_scrollYT-(_FNTheight*_scaleY)); //Copy Width, Height
	// Clear bottom text line
	drawSquareFill(_scrollXL, _scrollYB-(_FNTheight*_scaleY), _scrollXR-1, _scrollYB-1, _TXTBackColor);
	textColor(_TXTForeColor,_TXTBackColor);
}

//*************************************************************//
// Scroll Screen down
//*************************************************************//
void RA8876_8080::scrollDown(void) { // No arguments for now
	bteMemoryCopy(currentPage,SCREEN_WIDTH, _scrollXL, _scrollYT,	//Source
				  PAGE10_START_ADDR,SCREEN_WIDTH, _scrollXL, _scrollYT,	//Desination
				  _scrollXR-_scrollXL, (_scrollYB-_scrollYT)-(_FNTheight*_scaleY)); //Copy Width, Height
	bteMemoryCopy(PAGE10_START_ADDR,SCREEN_WIDTH, _scrollXL, _scrollYT,	//Source
				  currentPage,SCREEN_WIDTH, _scrollXL, _scrollYT+(_FNTheight*_scaleY),	//Desination
				  _scrollXR-_scrollXL, (_scrollYB-_scrollYT)-_FNTheight); //Copy Width, Height
	// Clear top text line
	drawSquareFill(_scrollXL, _scrollYT, _scrollXR-1,_scrollYT+(_FNTheight*_scaleY), _TXTBackColor);
	textColor(_TXTForeColor,_TXTBackColor);

}

//*************************************************************//
// Put a section of current screen page to another screen page.
// x0,y0 is upper left start coordinates of section
// x1,y1 is lower right coordinates of section
// dx0,dy0 is start coordinates of destination screen page
//*************************************************************//
uint32_t RA8876_8080::boxPut(uint32_t vPageAddr, uint16_t x0, uint16_t y0,
							 uint16_t x1, uint16_t y1,
							 uint16_t dx0,uint16_t dy0) {

	bteMemoryCopy(currentPage, SCREEN_WIDTH, x0, y0,
    vPageAddr, SCREEN_WIDTH, dx0, dy0,
    x1-x0, y1-y0);
	return vPageAddr;
}

//*************************************************************//
// get a section of another screen page to current screen page.
// x0,y0 is upper left start coordinates of section
// x1,y1 is lower right coordinates of section
// dx0,dy0 is start coordinates of current screen page
//*************************************************************//
uint32_t RA8876_8080::boxGet(uint32_t vPageAddr, uint16_t x0, uint16_t y0,
							 uint16_t x1, uint16_t y1,
							 uint16_t dx0,uint16_t dy0) {

	bteMemoryCopy(vPageAddr, SCREEN_WIDTH, x0, y0,
	              currentPage, SCREEN_WIDTH, dx0, dy0,
	              x1-dx0, y1-dy0);
	return vPageAddr;
}

/*BTE function*/
//**************************************************************//
// Copy from place to place in memory (eg. from a non-displayed page to the current page)
//**************************************************************//
void RA8876_8080::bteMemoryCopy(ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,
								ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,
								ru16 copy_width,ru16 copy_height)
{
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);

//  bte_Source1_MemoryStartAddr(PAGE3_START_ADDR);
//  bte_Source1_ImageWidth(des_image_width);
//  bte_Source1_WindowStartXY(des_x,des_y);

  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(copy_width,copy_height); 

  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_CODE_12<<4|RA8876_BTE_MEMORY_COPY_WITH_ROP);//91h
//  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_CODE_12<<4|3);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,(RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP) & 0x7f);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
} 

//**************************************************************//
// Memory copy with Raster OPeration blend from two sources
// One source may be the destination, if blending with something already on the screen
//**************************************************************//
void RA8876_8080::bteMemoryCopyWithROP(
	ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,
	ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,
    ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,
    ru16 copy_width,ru16 copy_height,ru8 rop_code)
{
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);
  bte_Source1_MemoryStartAddr(s1_addr);
  bte_Source1_ImageWidth(s1_image_width);
  bte_Source1_WindowStartXY(s1_x,s1_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(copy_width,copy_height);
  lcdRegDataWrite(RA8876_BTE_CTRL1,rop_code<<4|RA8876_BTE_MEMORY_COPY_WITH_ROP);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
} 
//**************************************************************//
// Memory copy with one color set to transparent
//**************************************************************//
void RA8876_8080::bteMemoryCopyWithChromaKey(
		ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,
		ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,
		ru16 copy_width, ru16 copy_height, ru16 chromakey_color)
{
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(copy_width,copy_height);
  backGroundColor16bpp(chromakey_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_MEMORY_COPY_WITH_CHROMA);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
}
//**************************************************************//
// Blend two source images with a simple transparency 0-32
//**************************************************************//
void RA8876_8080::bteMemoryCopyWindowAlpha(
		ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,
		ru32 s1_addr,ru16 s1_image_width,ru16 s1_x,ru16 s1_y,
		ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,
		ru16 copy_width, ru16 copy_height, ru8 alpha)
{
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);
  bte_Source1_MemoryStartAddr(s1_addr);
  bte_Source1_ImageWidth(s1_image_width);
  bte_Source1_WindowStartXY(s1_x,s1_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(copy_width,copy_height);
  bte_WindowAlpha(alpha);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_MEMORY_COPY_WITH_OPACITY);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
}

//**************************************************************//
//write data after setting, using lcdDataWrite() or lcdDataWrite16bbp()
//**************************************************************//
void RA8876_8080::bteMpuWriteWithChromaKey(ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 chromakey_color)
{
  check2dBusy();
  graphicMode(true);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  backGroundColor16bpp(chromakey_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_MPU_WRITE_WITH_CHROMA);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();
}

//**************************************************************//
//**************************************************************//
void RA8876_8080::bteMpuWriteColorExpansionData(ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 foreground_color,ru16 background_color,const unsigned char *data)
{
  ru16 i,j;
  check2dBusy();
  graphicMode(true);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  foreGroundColor16bpp(foreground_color);
  backGroundColor16bpp(background_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_BUS_WIDTH8<<4|RA8876_BTE_MPU_WRITE_COLOR_EXPANSION);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();
  
  for(i=0;i< height;i++)
  {	
    for(j=0;j< ((width+7)/8);j++)
    {
      lcdDataWrite(*data, false);
      data++;
    }
  }
  lcdStatusRead();
}
//**************************************************************//
//write data after setting, using lcdDataWrite() or lcdDataWrite16bbp()
//**************************************************************//
void RA8876_8080::bteMpuWriteColorExpansion(ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 foreground_color,ru16 background_color)
{
  check2dBusy();
  graphicMode(true);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height); 
  foreGroundColor16bpp(foreground_color);
  backGroundColor16bpp(background_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_BUS_WIDTH8<<4|RA8876_BTE_MPU_WRITE_COLOR_EXPANSION);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();
}
//**************************************************************//
/*background_color do not set the same as foreground_color*/
//**************************************************************//
void RA8876_8080::bteMpuWriteColorExpansionWithChromaKeyData(ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 foreground_color,ru16 background_color, const unsigned char *data)
{
  ru16 i,j;
  check2dBusy();
  graphicMode(true);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  foreGroundColor16bpp(foreground_color);
  backGroundColor16bpp(background_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_BUS_WIDTH8<<4|RA8876_BTE_MPU_WRITE_COLOR_EXPANSION_WITH_CHROMA);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();
  
  for(i=0;i< height;i++)
  {	
    for(j=0;j< ((width+7)/8);j++)
    {
      lcdDataWrite(*data, false);
      data++;
    }
  }
  lcdStatusRead();
}
//**************************************************************//
/*background_color do not set the same as foreground_color*/
//write data after setting, using lcdDataWrite() or lcdDataWrite16bbp()
//**************************************************************//
void RA8876_8080::bteMpuWriteColorExpansionWithChromaKey(ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 foreground_color,ru16 background_color)
{
  check2dBusy();
  graphicMode(true);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  foreGroundColor16bpp(foreground_color);
  backGroundColor16bpp(background_color);
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_BUS_WIDTH8<<4|RA8876_BTE_MPU_WRITE_COLOR_EXPANSION_WITH_CHROMA);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);//90h
  ramAccessPrepare();
}
//**************************************************************//
//**************************************************************//
void  RA8876_8080::btePatternFill(ru8 p8x8or16x16, ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,
                                 ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height)
{ 
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height); 
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_CODE_12<<4|RA8876_BTE_PATTERN_FILL_WITH_ROP);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
  
  if(p8x8or16x16 == 0)
    lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4|RA8876_PATTERN_FORMAT8X8);//90h
  else
    lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4|RA8876_PATTERN_FORMAT16X16);//90h
   
}
//**************************************************************//
//**************************************************************//
void  RA8876_8080::btePatternFillWithChromaKey(ru8 p8x8or16x16, ru32 s0_addr,ru16 s0_image_width,ru16 s0_x,ru16 s0_y,ru32 des_addr,ru16 des_image_width, ru16 des_x,ru16 des_y,ru16 width,ru16 height,ru16 chromakey_color)
{
  check2dBusy();
  graphicMode(true);
  bte_Source0_MemoryStartAddr(s0_addr);
  bte_Source0_ImageWidth(s0_image_width);
  bte_Source0_WindowStartXY(s0_x,s0_y);
  bte_DestinationMemoryStartAddr(des_addr);
  bte_DestinationImageWidth(des_image_width);
  bte_DestinationWindowStartXY(des_x,des_y);
  bte_WindowSize(width,height);
  backGroundColor16bpp(chromakey_color); 
  lcdRegDataWrite(RA8876_BTE_CTRL1,RA8876_BTE_ROP_CODE_12<<4|RA8876_BTE_PATTERN_FILL_WITH_CHROMA);//91h
  lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h

  if(p8x8or16x16 == 0)
    lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4|RA8876_PATTERN_FORMAT8X8);//90h
  else
    lcdRegDataWrite(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4|RA8876_PATTERN_FORMAT16X16);//90h

}

 /*DMA Function*/
 //**************************************************************//
 /*If used 32bit address serial flash through ra8876, must be set command to serial flash to enter 4bytes mode first.
 only needs set one times after power on */
 //**************************************************************//
 void  RA8876_8080::setSerialFlash4BytesMode(ru8 scs_select)
 {
  if(scs_select==0)
  {
  lcdRegDataWrite( RA8876_SPIMCR2, RA8876_SPIM_NSS_SELECT_0<<5|RA8876_SPIM_MODE0);//b9h
  lcdRegDataWrite( RA8876_SPIMCR2, RA8876_SPIM_NSS_SELECT_0<<5|RA8876_SPIM_NSS_ACTIVE<<4|RA8876_SPIM_MODE0);//b9h 
  lcdRegWrite( RA8876_SPIDR);//b8h
  delay(1);
  lcdDataWrite(0xB7);//
  delay(1);
  lcdRegDataWrite( RA8876_SPIMCR2, RA8876_SPIM_NSS_SELECT_0<<5|RA8876_SPIM_NSS_INACTIVE<<4|RA8876_SPIM_MODE0);//b9h 
  }
  if(scs_select==1)
  {
  lcdRegDataWrite( RA8876_SPIMCR2 ,RA8876_SPIM_NSS_SELECT_1<<5|RA8876_SPIM_MODE0);//b9h
  lcdRegDataWrite( RA8876_SPIMCR2, RA8876_SPIM_NSS_SELECT_1<<5|RA8876_SPIM_NSS_ACTIVE<<4|RA8876_SPIM_MODE0);//b9h
  lcdRegWrite( RA8876_SPIDR);//b8h
  delay(1);
  lcdDataWrite(0xB7);//
  delay(1);
  lcdRegDataWrite( RA8876_SPIMCR2, RA8876_SPIM_NSS_SELECT_1<<5|RA8876_SPIM_NSS_INACTIVE<<4|RA8876_SPIM_MODE0);//b9h 
  } 
 }
//**************************************************************//
/* scs = 0 : select scs0, scs = 1 : select scs1, */
//**************************************************************//
 void  RA8876_8080::dma_24bitAddressBlockMode(ru8 scs_select,ru8 clk_div,ru16 x0,ru16 y0,ru16 width,ru16 height,ru16 picture_width,ru32 addr)
 {
   if(scs_select==0)
    lcdRegDataWrite(RA8876_SFL_CTRL,RA8876_SERIAL_FLASH_SELECT0<<7|RA8876_SERIAL_FLASH_DMA_MODE<<6|RA8876_SERIAL_FLASH_ADDR_24BIT<<5|RA8876_FOLLOW_RA8876_MODE<<4|RA8876_SPI_FAST_READ_8DUMMY);//b7h
   if(scs_select==1)
    lcdRegDataWrite(RA8876_SFL_CTRL,RA8876_SERIAL_FLASH_SELECT1<<7|RA8876_SERIAL_FLASH_DMA_MODE<<6|RA8876_SERIAL_FLASH_ADDR_24BIT<<5|RA8876_FOLLOW_RA8876_MODE<<4|RA8876_SPI_FAST_READ_8DUMMY);//b7h
  
  lcdRegDataWrite(RA8876_SPI_DIVSOR,clk_div);//bbh  
  lcdRegDataWrite(RA8876_DMA_DX0,x0);//c0h
  lcdRegDataWrite(RA8876_DMA_DX1,x0>>8);//c1h
  lcdRegDataWrite(RA8876_DMA_DY0,y0);//c2h
  lcdRegDataWrite(RA8876_DMA_DY1,y0>>8);//c3h 
  lcdRegDataWrite(RA8876_DMAW_WTH0,width);//c6h
  lcdRegDataWrite(RA8876_DMAW_WTH1,width>>8);//c7h
  lcdRegDataWrite(RA8876_DMAW_HIGH0,height);//c8h
  lcdRegDataWrite(RA8876_DMAW_HIGH1,height>>8);//c9h 
  lcdRegDataWrite(RA8876_DMA_SWTH0,picture_width);//cah
  lcdRegDataWrite(RA8876_DMA_SWTH1,picture_width>>8);//cbh 
  lcdRegDataWrite(RA8876_DMA_SSTR0,addr);//bch
  lcdRegDataWrite(RA8876_DMA_SSTR1,addr>>8);//bdh
  lcdRegDataWrite(RA8876_DMA_SSTR2,addr>>16);//beh
  lcdRegDataWrite(RA8876_DMA_SSTR3,addr>>24);//bfh 
  
  lcdRegDataWrite(RA8876_DMA_CTRL,RA8876_DMA_START);//b6h 
  check2dBusy(); 
 }
 //**************************************************************//
/* scs = 0 : select scs0, scs = 1 : select scs1, */
//**************************************************************//
 void  RA8876_8080::dma_32bitAddressBlockMode(ru8 scs_select,ru8 clk_div,ru16 x0,ru16 y0,ru16 width,ru16 height,ru16 picture_width,ru32 addr)
 {
   if(scs_select==0)
    lcdRegDataWrite(RA8876_SFL_CTRL,RA8876_SERIAL_FLASH_SELECT0<<7|RA8876_SERIAL_FLASH_DMA_MODE<<6|RA8876_SERIAL_FLASH_ADDR_32BIT<<5|RA8876_FOLLOW_RA8876_MODE<<4|RA8876_SPI_FAST_READ_8DUMMY);//b7h
   if(scs_select==1)
    lcdRegDataWrite(RA8876_SFL_CTRL,RA8876_SERIAL_FLASH_SELECT1<<7|RA8876_SERIAL_FLASH_DMA_MODE<<6|RA8876_SERIAL_FLASH_ADDR_32BIT<<5|RA8876_FOLLOW_RA8876_MODE<<4|RA8876_SPI_FAST_READ_8DUMMY);//b7h
  
  lcdRegDataWrite(RA8876_SPI_DIVSOR,clk_div);//bbh 
  
  lcdRegDataWrite(RA8876_DMA_DX0,x0);//c0h
  lcdRegDataWrite(RA8876_DMA_DX1,x0>>8);//c1h
  lcdRegDataWrite(RA8876_DMA_DY0,y0);//c2h
  lcdRegDataWrite(RA8876_DMA_DY1,y0>>8);//c3h 
  lcdRegDataWrite(RA8876_DMAW_WTH0,width);//c6h
  lcdRegDataWrite(RA8876_DMAW_WTH1,width>>8);//c7h
  lcdRegDataWrite(RA8876_DMAW_HIGH0,height);//c8h
  lcdRegDataWrite(RA8876_DMAW_HIGH1,height>>8);//c9h 
  lcdRegDataWrite(RA8876_DMA_SWTH0,picture_width);//cah
  lcdRegDataWrite(RA8876_DMA_SWTH1,picture_width>>8);//cbh 
  lcdRegDataWrite(RA8876_DMA_SSTR0,addr);//bch
  lcdRegDataWrite(RA8876_DMA_SSTR1,addr>>8);//bdh
  lcdRegDataWrite(RA8876_DMA_SSTR2,addr>>16);//beh
  lcdRegDataWrite(RA8876_DMA_SSTR3,addr>>24);//bfh  
  
  lcdRegDataWrite(RA8876_DMA_CTRL,RA8876_DMA_START);//b6h 
  check2dBusy(); 
 }
 
//**************************************************************//
// Setup PIP Windows ( 2 PIP Windows Avaiable)
//**************************************************************//
void RA8876_8080::PIP (
		 unsigned char On_Off // 0 : disable PIP, 1 : enable PIP, 2 : To maintain the original state
		,unsigned char Select_PIP // 1 : use PIP1 , 2 : use PIP2
		,unsigned long PAddr //start address of PIP
		,unsigned short XP //coordinate X of PIP Window, It must be divided by 4.
		,unsigned short YP //coordinate Y of PIP Window, It must be divided by 4.
		,unsigned long ImageWidth //Image Width of PIP (recommend = canvas image width)
		,unsigned short X_Dis //coordinate X of Display Window
		,unsigned short Y_Dis //coordinate Y of Display Window
		,unsigned short X_W //width of PIP and Display Window, It must be divided by 4.
		,unsigned short Y_H //height of PIP and Display Window , It must be divided by 4.
		)
{
	if(Select_PIP == 1 )  
	{
	Select_PIP1_Parameter();
	}
	if(Select_PIP == 2 )  
	{
	Select_PIP2_Parameter();
	}
	PIP_Display_Start_XY(X_Dis,Y_Dis);
	PIP_Image_Start_Address(PAddr);
	PIP_Image_Width(ImageWidth);
	PIP_Window_Image_Start_XY(XP,YP);
	PIP_Window_Width_Height(X_W,Y_H);

	if(On_Off == 0)
    {
  		if(Select_PIP == 1 )  
		{ 
		Disable_PIP1();	
		}
		if(Select_PIP == 2 )  
		{
		Disable_PIP2();
		}
    }

    if(On_Off == 1)
    {
  		if(Select_PIP == 1 )  
		{ 
		Enable_PIP1();	
		}
		if(Select_PIP == 2 )  
		{
		Enable_PIP2();
		}
    }
}

//[2Ah][2Bh][2Ch][2Dh]=========================================================================
void RA8876_8080::PIP_Display_Start_XY(unsigned short WX,unsigned short HY)	
{
/*
Reference Main Window coordination.
Unit: Pixel
It must be divisible by 4. PWDULX Bit [1:0] tie to ¡§0¡š internally.
X-axis coordination should less than horizontal display width.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters. 
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWDULX0, WX);    // [2Ah] PIP Window Display Upper-Left corner X-coordination [7:0]
	lcdRegDataWrite(RA8876_PWDULX1, WX>>8); // [2Bh] PIP Window Display Upper-Left corner X-coordination [12:8]


/*
Reference Main Window coordination.
Unit: Pixel
Y-axis coordination should less than vertical display height.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters.
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWDULY0, HY);    // [2Ch] PIP Window Display Upper-Left corner Y-coordination [7:0]
	lcdRegDataWrite(RA8876_PWDULY1, HY>>8); // [2Dh] PIP Window Display Upper-Left corner Y-coordination [12:8]
}

//[2Eh][2Fh][30h][31h]=========================================================================
void RA8876_8080::PIP_Image_Start_Address(unsigned long Addr)	
{
	lcdRegDataWrite(RA8876_PISA0, Addr); // [2Eh] PIP Image Start Address[7:2]
	lcdRegDataWrite(RA8876_PISA1, Addr>>8); // [2Fh] PIP Image Start Address[15:8]
	lcdRegDataWrite(RA8876_PISA2, Addr>>16); // [30h] PIP Image Start Address [23:16]
	lcdRegDataWrite(RA8876_PISA3, Addr>>24); // [31h] PIP Image Start Address [31:24]
}

//[32h][33h]=========================================================================
void RA8876_8080::PIP_Image_Width(unsigned short WX)	
{
/*
Unit: Pixel.
It must be divisible by 4. PIW Bit [1:0] tie to ¡§0¡š internally.
The value is physical pixel number.
This width should less than horizontal display width.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters.
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PIW0, WX);     // [32h] PIP Image Width [7:0]
	lcdRegDataWrite(RA8876_PIW1, WX>>8);  // [33h] PIP Image Width [12:8]
}

//[34h][35h][36h][37h]=========================================================================
void RA8876_8080::PIP_Window_Image_Start_XY(unsigned short WX,unsigned short HY)	
{
/*
Reference PIP Image coordination.
Unit: Pixel
It must be divisible by 4. PWIULX Bit [1:0] tie to ¡§0¡š internally.
X-axis coordination plus PIP image width cannot large than 8188.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters. 
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWIULX0, WX);    // [34h] PIP 1 or 2 Window Image Upper-Left corner X-coordination [7:0]
	lcdRegDataWrite(RA8876_PWIULX1, WX>>8); // [35h] PIP Window Image Upper-Left corner X-coordination [12:8]

/*
Reference PIP Image coordination.
Unit: Pixel
Y-axis coordination plus PIP window height should less than 8191.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters. 
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWIULY0, HY);    // [36h] PIP Windows Display Upper-Left corner Y-coordination [7:0]
	lcdRegDataWrite(RA8876_PWIULY1, HY>>8); // [37h] PIP Windows Image Upper-Left corner Y-coordination [12:8]

}

//[38h][39h][3Ah][3Bh]=========================================================================
void RA8876_8080::PIP_Window_Width_Height(unsigned short WX,unsigned short HY)	
{
/*

Unit: Pixel.
It must be divisible by 4. The value is physical pixel number.
Maximum value is 8188 pixels.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters. 
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWW0, WX);    // [38h] PIP Window Width [7:0]
	lcdRegDataWrite(RA8876_PWW1, WX>>8); // [39h] PIP Window Width [10:8]

/*
Unit: Pixel
The value is physical pixel number. Maximum value is 8191 pixels.
According to bit of Select Configure PIP 1 or 2 Window¡Šs parameters. 
Function bit will be configured for relative PIP window.
*/
	lcdRegDataWrite(RA8876_PWH0, HY);    // [3Ah] PIP Window Height [7:0]
	lcdRegDataWrite(RA8876_PWH1, HY>>8); // [3Bh] PIP Window Height [10:8]

}

//[10h]=========================================================================
// Turn on PIP window 1
void RA8876_8080::Enable_PIP1(void)
{
/*
PIP 1 window Enable/Disable
0 : PIP 1 window disable.
1 : PIP 1 window enable
PIP 1 window always on top of PIP 2 window.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
	temp |= cSetb7;
	lcdRegDataWrite(RA8876_MPWCTR,temp);

}

// Turn off PIP window 1
void RA8876_8080::Disable_PIP1(void)
{
/*
PIP 1 window Enable/Disable
0 : PIP 1 window disable.
1 : PIP 1 window enable
PIP 1 window always on top of PIP 2 window.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb7;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

// Turn on PIP window 2
void RA8876_8080::Enable_PIP2(void)
{
/*
PIP 2 window Enable/Disable
0 : PIP 2 window disable.
1 : PIP 2 window enable
PIP 1 window always on top of PIP 2 window.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
	temp |= cSetb6;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

// Turn off PIP window 1
void RA8876_8080::Disable_PIP2(void)
{
/*
PIP 2 window Enable/Disable
0 : PIP 2 window disable.
1 : PIP 2 window enable
PIP 1 window always on top of PIP 2 window.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb6;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

void RA8876_8080::Select_PIP1_Parameter(void)
{
/*
0: To configure PIP 1¡Šs parameters.
1: To configure PIP 2¡Šs parameters..
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb4;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

void RA8876_8080::Select_PIP2_Parameter(void)
{
/*
0: To configure PIP 1¡Šs parameters.
1: To configure PIP 2¡Šs parameters..
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
	temp |= cSetb4;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

/********************************************************/
// Select a screen page (Buffer) 1 to 9.
// ALT + (F1 to F9) using USBHost_t36 Keyboard Driver
// and my USBKeyboard host driver.
// Also, STBASIC Commands: screen 0 to screen 8
void RA8876_8080::selectScreen(unsigned long screenPage) {
	check2dBusy();
	tftSave_t *tempSave, *tempRestore;
	// Don't Select the current screen page
	if(screenPage == currentPage)
		return;
	switch(currentPage) {
		case PAGE1_START_ADDR:
			tempSave = screenPage1; 
			break;
		case PAGE2_START_ADDR:
			tempSave = screenPage2; 
			break;
		case PAGE3_START_ADDR:
			tempSave = screenPage3; 
			break;
		case PAGE4_START_ADDR:
			tempSave = screenPage4; 
			break;
		case PAGE5_START_ADDR:
			tempSave = screenPage5; 
			break;
		case PAGE6_START_ADDR:
			tempSave = screenPage6; 
			break;
		case PAGE7_START_ADDR:
			tempSave = screenPage7; 
			break;
		case PAGE8_START_ADDR:
			tempSave = screenPage8; 
			break;
		case PAGE9_START_ADDR:
			tempSave = screenPage9; 
			break;
//		case PAGE10_START_ADDR:
//			tempSave = screenPage10; 
//			break;
		default:
			tempSave = screenPage1; 
	}
	// Copy back selected screen page parameters
	switch(screenPage) {
		case PAGE1_START_ADDR:
			tempRestore = screenPage1; 
			break;
		case PAGE2_START_ADDR:
			tempRestore = screenPage2; 
			break;
		case PAGE3_START_ADDR:
			tempRestore = screenPage3; 
			break;
		case PAGE4_START_ADDR:
			tempRestore = screenPage4; 
			break;
		case PAGE5_START_ADDR:
			tempRestore = screenPage5; 
			break;
		case PAGE6_START_ADDR:
			tempRestore = screenPage6; 
			break;
		case PAGE7_START_ADDR:
			tempRestore = screenPage7; 
			break;
		case PAGE8_START_ADDR:
			tempRestore = screenPage8; 
			break;
		case PAGE9_START_ADDR:
			tempRestore = screenPage9; 
			break;
//		case PAGE10_START_ADDR:
//			tempRestore = screenPage10; 
//			break;
		default:
			tempRestore = screenPage1; 
	}
	// Save current screen page parameters 
	saveTFTParams(tempSave);
	// Restore selected screen page parameters
	restoreTFTParams(tempRestore);
	// Setup params to rebuild selected screen
	pageOffset = screenPage;
	currentPage = screenPage;
	displayImageStartAddress(currentPage);
	displayImageWidth(_width);
	displayWindowStartXY(0,0);
	canvasImageStartAddress(currentPage);
	canvasImageWidth(_width);
	activeWindowXY(0,0);
	activeWindowWH(_width,_height); 

	//setTextCursor(_cursorX, _cursorY);
	//textColor(_TXTForeColor,_TXTBackColor);
	// Rebuild the display
	//buildTextScreen();
	
}


void RA8876_8080::Select_Main_Window_8bpp(void)
{
/*
Main Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb3;
    temp &= cClrb2;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}
void RA8876_8080::Select_Main_Window_16bpp(void)
{
/*
Main Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb3;
    temp |= cSetb2;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}
void RA8876_8080::Select_Main_Window_24bpp(void)
{
/*
Main Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
	temp |= cSetb3;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

void RA8876_8080::Select_LCD_Sync_Mode(void)
{
/*
To Control panel's synchronous signals
0: Sync Mode: Enable XVSYNC, XHSYNC, XDE
1: DE Mode: Only XDE enable, XVSYNC & XHSYNC in idle state
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
    temp &= cClrb0;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}
void RA8876_8080::Select_LCD_DE_Mode(void)
{
/*
To Control panel's synchronous signals
0: Sync Mode: Enable XVSYNC, XHSYNC, XDE
1: DE Mode: Only XDE enable, XVSYNC & XHSYNC in idle state
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MPWCTR); // 0x10
	temp |= cSetb0;
	lcdRegDataWrite(RA8876_MPWCTR,temp);
}

//[11h]=========================================================================
void RA8876_8080::Select_PIP1_Window_8bpp(void)
{
/*
PIP 1 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp &= cClrb3;
    temp &= cClrb2;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

void RA8876_8080::Select_PIP1_Window_16bpp(void)
{
/*
PIP 1 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp &= cClrb3;
    temp |= cSetb2;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

void RA8876_8080::Select_PIP1_Window_24bpp(void)
{
/*
PIP 1 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp |= cSetb3;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

void RA8876_8080::Select_PIP2_Window_8bpp(void)
{
/*
PIP 2 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp &= cClrb1;
    temp &= cClrb0;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

void RA8876_8080::Select_PIP2_Window_16bpp(void)
{
/*
PIP 2 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp &= cClrb1;
    temp |= cSetb0;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

void RA8876_8080::Select_PIP2_Window_24bpp(void)
{
/*
PIP 2 Window Color Depth Setting
00b: 8-bpp generic TFT, i.e. 256 colors.
01b: 16-bpp generic TFT, i.e. 65K colors.
1xb: 24-bpp generic TFT, i.e. 1.67M colors.
*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_PIPCDEP); // 0x11
    temp |= cSetb1;
//    temp &= cClrb0;
	lcdRegDataWrite(RA8876_PIPCDEP,temp);
}

// Save current screen page parameters 
void RA8876_8080::saveTFTParams(tftSave_t *screenSave) {
	screenSave->width			= _width;
	screenSave->height			= _height;
	screenSave->cursorX			= _cursorX;
	screenSave->cursorY			= _cursorY;
	screenSave->scaleX			= _scaleX;
	screenSave->scaleY			= _scaleY;
	screenSave->FNTwidth		= _FNTwidth;
	screenSave->FNTheight		= _FNTheight;
	screenSave->fontheight		= _fontheight;
	screenSave->fontSource      = currentFont;
	screenSave->cursorXsize		= _cursorXsize;
	screenSave->cursorYsize		= _cursorYsize;
// Text Sreen Vars
	screenSave->prompt_size		= prompt_size; // prompt ">"
	screenSave->prompt_line		= prompt_line; // current text prompt row
	screenSave->vdata			= vdata;
	screenSave->leftmarg		= leftmarg;
	screenSave->topmarg			= topmarg;
	screenSave->rightmarg		= rightmarg;
	screenSave->bottommarg		= bottommarg;
	screenSave->tab_size		= tab_size;
	screenSave->CharPosX		= CharPosX;
	screenSave->CharPosY		= CharPosY;
	screenSave->UDFont  		= UDFont;

//scroll vars ----------------------------
	screenSave->scrollXL		= _scrollXL;
	screenSave->scrollXR		= _scrollXR;
	screenSave->scrollYT		= _scrollYT;
	screenSave->scrollYB		= _scrollYB;
	screenSave->TXTForeColor	= _TXTForeColor;
    screenSave->TXTBackColor	= _TXTBackColor;
}

// Restore selected screen page parameters
void RA8876_8080::restoreTFTParams(tftSave_t *screenSave) {
	 _width = screenSave->width;
	 _height = screenSave->height;
	 _cursorX = screenSave->cursorX;
	 _cursorY = screenSave->cursorY;
	 _scaleX = screenSave->scaleX;
	 _scaleY = screenSave->scaleY;
	 _FNTwidth = screenSave->FNTwidth;
	 _FNTheight = screenSave->FNTheight;
	 _fontheight = screenSave->fontheight;
	 currentFont = screenSave->fontSource;
	 _cursorXsize = screenSave->cursorXsize;		
	 _cursorYsize = screenSave->cursorYsize;		
	// Text Sreen Vars
	 prompt_size = screenSave->prompt_size; // prompt ">"
	 prompt_line = screenSave->prompt_line; // current text prompt row
	 vdata = screenSave->vdata;			
	 leftmarg = screenSave->leftmarg;		
	 topmarg = screenSave->topmarg;		
	 rightmarg = screenSave->rightmarg;
	 bottommarg = screenSave->bottommarg;		
	 tab_size =	screenSave->tab_size;		
	 CharPosX = screenSave->CharPosX;		
	 CharPosY = screenSave->CharPosY;		
	 UDFont = screenSave->UDFont;		
	//scroll vars -----------------
	 _scrollXL = screenSave->scrollXL;
	 _scrollXR = screenSave->scrollXR;
	 _scrollYT = screenSave->scrollYT;
	 _scrollYB = screenSave->scrollYB;
	 _TXTForeColor = screenSave->TXTForeColor;
	 _TXTBackColor = screenSave->TXTBackColor;
}

void RA8876_8080::useCanvas()
{
	displayImageStartAddress(PAGE1_START_ADDR);
	displayImageWidth(_width);
	displayWindowStartXY(0,0);
	
	canvasImageStartAddress(PAGE2_START_ADDR);
	canvasImageWidth(_width);
	activeWindowXY(0, 0);
	activeWindowWH(_width, _height);
	check2dBusy();
	ramAccessPrepare();
}

void RA8876_8080::useCanvas(uint8_t page) //LL
{
	canvasImageStartAddress(pageStartAddress(page));
	canvasImageWidth(_width);
	activeWindowXY(0, 0);
	activeWindowWH(_width, _height);
	check2dBusy();
	ramAccessPrepare();
}

void RA8876_8080::updateScreen() {
	bteMemoryCopy(PAGE2_START_ADDR,_width,0,0,
				  PAGE1_START_ADDR,_width, 0,0,
				 _width,_height);
}

void RA8876_8080::updateScreenWithROP(uint8_t src1, uint8_t src2, uint8_t dst, uint8_t ROP_CODE) { //LL
	bteMemoryCopyWithROP(pageStartAddress(src1),_width,0,0,
				  pageStartAddress(src2),_width, 0,0,
				  pageStartAddress(dst),_width, 0,0,
				 _width,_height,ROP_CODE);

}

uint32_t RA8876_8080::pageStartAddress(uint8_t pageNumber) {  //LL
	return (uint32_t)SCREEN_HEIGHT * SCREEN_WIDTH * 2 * pageNumber;
}

// Setup text cursor
void RA8876_8080::cursorInit(void)
{
	_cursorXsize = _FNTwidth;
	_cursorYsize = _FNTheight-1;
	Text_Cursor_H_V(_cursorXsize,_cursorYsize); 		// Block cusror
	Blinking_Time_Frames(20);		// Set blink rate
	Enable_Text_Cursor_Blinking();	// Turn blinking cursor on
}

//void RA8876_8080::setCursor(uint16_t x, uint16_t y)
//{
//	setTextCursor(x, y);
//}


/**************************************************************************/
/*!   
		Set the Text position for write Text only.
		Parameters:
		x:horizontal in pixels or CENTER(of the screen)
		y:vertical in pixels or CENTER(of the screen)
		autocenter:center text to choosed x,y regardless text lenght
		false: |ABCD
		true:  AB|CD
		NOTE: works with any font
*/
/**************************************************************************/
void RA8876_8080::setCursor(int16_t x, int16_t y, bool autocenter) 
{
	if(_use_default) {
		//setTextCursor(x, y);
		//return;
	}
	
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	
	_absoluteCenter = autocenter;
	
	if (_portrait) {//rotation 1,3
		//if (_use_default) swapvals(x,y);
		if (y == CENTER) {//swapped OK
			y = _width/2;
			if (!autocenter) {
				_relativeCenter = true;
				_TXTparameters |= (1 << 6);//set x flag
			}
		}
		if (x == CENTER) {//swapped
			x = _height/2;
			if (!autocenter) {
				_relativeCenter = true;
				_TXTparameters |= (1 << 5);//set y flag
			}
		}
	} else {//rotation 0,2
		if (x == CENTER) {
			x = _width/2;
			if (!autocenter) {
				_relativeCenter = true;
				_TXTparameters |= (1 << 5);
			}
		}
		if (y == CENTER) {
			y = _height/2;
			if (!autocenter) {
				_relativeCenter = true;
				_TXTparameters |= (1 << 6);
			}
		}
	}
	//TODO: This one? Useless?
	if (bitRead(_TXTparameters,2) == 0){//textWrap
		if (x >= _width) x = _width-1;
		if (y >= _height) y = _height-1;
	}
	
	_cursorX = x;
	_cursorY = y;

	//if _relativeCenter or _absoluteCenter do not apply to registers yet!
	// Have to go to _textWrite first to calculate the lenght of the entire string and recalculate the correct x,y
	if (_relativeCenter || _absoluteCenter) return;
	if (bitRead(_TXTparameters,7) == 0) _textPosition(x,y,false);
}

/**************************************************************************/
/*!   
		Give you back the current text cursor position by reading inside RA8875
		Parameters:
		x: horizontal pos in pixels
		y: vertical pos in pixels
		note: works also with rendered fonts
		USE: xxx.getCursor(myX,myY);
*/
/**************************************************************************/
void RA8876_8080::getCursor(int16_t &x, int16_t &y) 
{
		uint8_t t1,t2,t3,t4;
		t1 = lcdRegDataRead(RA8876_F_CURX0);
		t2 = lcdRegDataRead(RA8876_F_CURX1);
		t3 = lcdRegDataRead(RA8876_F_CURY0);
		t4 = lcdRegDataRead(RA8876_F_CURY1);
		x = (t2 << 8) | (t1 & 0xFF);
		y = (t4 << 8) | (t3 & 0xFF);
		//if (_portrait && _use_default) swapvals(x,y);
	
}

int16_t RA8876_8080::getCursorX(void)
{
	//if (_portrait && _use_default) return _cursorY;
	return _cursorX;
}

int16_t RA8876_8080::getCursorY(void)
{
	//if (_portrait && _use_default) return _cursorX;
	return _cursorY;
}

//Set Graphic Mode Margins (pixel based)
void RA8876_8080::setMargins(uint16_t xl, uint16_t yt, uint16_t xr, uint16_t yb) {
	_scrollXL = xl;
	_scrollYT = yt;
	_scrollXR = xr;
	_scrollYB = yb;
	buildTextScreen();	
	setTextCursor(    _scrollXL,    _scrollYT);
}


// Set text prompt size (font size based)
void RA8876_8080::setPromptSize(uint16_t ps) {
	prompt_size = ps*(    _FNTwidth *     _scaleX) +     _scrollXL; // Default prompt ">"
}

// Clear current screen to background 'color'
void RA8876_8080::fillScreen(uint16_t color) {
	drawSquareFill(_scrollXL, _scrollYT, _scrollXR, _scrollYB, color);
	check2dBusy();  //must wait for fill to finish before setting foreground color
}


//**************************************************************//
//**************************************************************//
void RA8876_8080::setBackGroundColor(uint16_t color)
{
	backGroundColor16bpp(color);
}



// Send a string to the status line
void RA8876_8080::printStatusLine(uint16_t x0,uint16_t fgColor,uint16_t bgColor, const char *text) {
	
	if(_use_gfx_font || _use_ili_font) {
		setTextColor(fgColor, bgColor);
		setTextCursor(x0, _height-STATUS_LINE_HEIGHT);
		printf("%s", text);
	} else {
		writeStatusLine(x0*(_FNTwidth*_scaleX), fgColor, bgColor, text);	
	}
}

// Load a user defined font from memory to RA8876 Character generator RAM
uint8_t RA8876_8080::fontLoadMEM(char *fontsrc) {
	CGRAM_initial(PATTERN1_RAM_START_ADDR, (uint8_t *)fontsrc, 16*256);
	return 0;
}

// Set USE_FF_FONTLOAD in h to 1 to use fontload function.
// Requires FatFS or Can be modified to use SDFat or SD.
#if USE_FF_FONTLOAD == 1
#include "ff.h"
// Load a user defined font from a file to RA8876 Character generator RAM
uint8_t RA8876_8080::fontLoad(char *fontfile) {
    FIL fsrc;      /* File object */
    FRESULT fresult = FR_OK;          /* FatFs function common result code */
    uint br = 0;
	uint8_t fontdata[4096];	// Buffer is setup to use 8x16 fonts.
							// Bigger fonts can be used.
    /* Open source file */
    fresult = f_open(&fsrc, fontfile, FA_READ);
    if (fresult)
		return 1; /* return any error */
    for (;;) {
        fresult = f_read(&fsrc, fontdata, sizeof fontdata, &br);  /* Read a chunk of source file */
        if (fresult || br == 0) break; /* error or eof */
    }
    /* Close open file */
    f_close(&fsrc);
	// Initialize CGRAM with font loaded into fontdata buffer.
	CGRAM_initial(PATTERN1_RAM_START_ADDR, fontdata, 16*256);
	return 0;
}
#endif

// Select internal fonts or user defined fonts (0 for internal or 1 for user defined)
void RA8876_8080::setFontSource(uint8_t source) {
	
	switch(source) {
	case 0:
		_scaleX = 1;
		_scaleY = 1;
		_setFNTdimensions(0);
		UDFont = false;
		break;
	case 1:
		if((_FNTheight != 16) && (_FNTwidth != 8)) {
			_FNTheight = 16;
			_FNTwidth  = 8;
		}
		UDFont = true;
		_setFNTdimensions(1);
		break;
	default:
		UDFont = false;	
	}
	//Ra8876_Lite::setFontSource(source);
	// Rebuild current screen page
	buildTextScreen();
}

// Set fontsize for fonts, currently 0 to 2 (Internal and User Defined)
boolean RA8876_8080::setFontSize(uint8_t scale, boolean runflag) {
	switch(scale) {
		
		case 0:
			if(UDFont) { // User Defined Fonts
				_scaleY = 1;
				_scaleX = 1;
			} else {
				_FNTheight = 16;
				_FNTwidth  = 8;
			}
			break;
		case 1:
			if(UDFont) {
				_FNTheight = 16;
				_FNTwidth  = 8;
				_scaleY = 2;
				_scaleX = 2;
			} else {
				_FNTheight = 24;
				_FNTwidth  = 12;
			}
			break;
		case 2:
			if(UDFont) {
				_FNTheight = 16;
				_FNTwidth  = 8;
				_scaleY = 3;
				_scaleX = 3;
			} else {
				_FNTheight = 32;
				_FNTwidth  = 16;
			}
			break;
		default:
			return true;
			break;
		setTextCursor(_scrollXL,_scrollYT);
	}
	// Rebuild current screen page
	buildTextScreen();
	cursorInit();
	if(runflag == false) {
		drawSquareFill(0, 0, _width-1, _height-1, _TXTBackColor);
		//have to wait for fill to finish before changing the foreground color...
		check2dBusy();
		textColor(_TXTForeColor,_TXTBackColor);
		setTextCursor(0,0);
	}
	rightmarg = (uint8_t)(_width / (_FNTwidth * _scaleX));
	bottommarg = (uint8_t)(_height / (_FNTheight * _scaleY));
	
	return false;
}

// Text cursor on or off
void RA8876_8080::setCursorMode(boolean mode) {

	if(mode == false)
		Disable_Text_Cursor();	// No cursor
	else
		Enable_Text_Cursor();	// Blinking block cursor
}

// Set text cursor type
void RA8876_8080::setCursorType(uint8_t type) {

	switch(type) {
		case 0:
			_cursorYsize = _FNTheight;
			Text_Cursor_H_V(_cursorXsize, _cursorYsize); 		// Block cursor
			break;
		case 1:
			_cursorYsize = 2;
			Text_Cursor_H_V(_cursorXsize, _cursorYsize); 		// Underline cursor
			break;
		case 2:
			_cursorYsize = _FNTheight;
			Text_Cursor_H_V(2, _cursorYsize); 						// I-beam cursor
			break;
		default:
			break;
	}
}

// Set text cursor blink mode (On or Off)
void RA8876_8080::setCursorBlink(boolean onOff) {
	if(onOff)
		Enable_Text_Cursor_Blinking();
	else
		Disable_Text_Cursor_Blinking();
}
	
// If you're looking to set text write position in characters within the current scroll window then use textxy()

// Get cursor X position
int16_t RA8876_8080::getTextX(void) {
	return (_cursorX / _FNTwidth); //getCursorX() / getFontWidth();
}	
	
// Get cursor Y position
int16_t RA8876_8080::getTextY(void) {
	return (_cursorY / _FNTheight);//getCursorY() / getFontHeight();
}	

// Get current text screen width (pixel * current font width)
int16_t RA8876_8080::getTwidth(void) {
	return (rightmarg - leftmarg)-1;
}

// Get current text screen height (pixel * current font height)
int16_t RA8876_8080::getTheight(void) {
	return (bottommarg - topmarg)-1;
}

// Get current screen foreground color
uint16_t RA8876_8080::getTextFGC() {
	return _TXTForeColor;
}

// Get current screen backround color
uint16_t RA8876_8080::getTextBGC(void) {
	return _TXTBackColor;
}


// Get font vertical size (in pixels)
uint8_t RA8876_8080::getFontHeight(void) {
	return _FNTheight * _scaleY;
}

// Get font width size (in pixels)
uint8_t RA8876_8080::getFontWidth(void) {
	return _FNTwidth * _scaleX;
}


// Draw a rectangle. Note: damages text color register
void RA8876_8080::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,uint16_t color) {
	x += _originx;
	y += _originy;
	int16_t x_end = x+w-1;
	int16_t y_end = y+h-1;
	if((x >= _displayclipx2)   || // Clip right
		 (y >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x < _displayclipx1) x = _displayclipx1;
	if (y < _displayclipy1) y = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
  switch (_rotation) {
  	case 1: swapvals(x,y); swapvals(x_end, y_end); break;
  	case 2: x = _width-x; x_end = _width - x_end;; break;
  	case 3: rotateCCXY(x,y); rotateCCXY(x_end, y_end); break;
  }

  lcdRegDataWrite(RA8876_DLHSR0,x, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh     
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_SQUARE, true);//76h,0xa0
}

// Draw a filled rectangle. Note: damages text color register
void RA8876_8080::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,uint16_t color) {
	x += _originx;
	y += _originy;
	int16_t x_end = x+w-1;
	int16_t y_end = y+h-1;
	if((x >= _displayclipx2)   || // Clip right
		 (y >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x < _displayclipx1) x = _displayclipx1;
	if (y < _displayclipy1) y = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
  switch (_rotation) {
  	case 1: swapvals(x,y); swapvals(x_end, y_end); break;
  	case 2: x = _width-x; x_end = _width - x_end;; break;
  	case 3: rotateCCXY(x,y); rotateCCXY(x_end, y_end); break;
  }

  lcdRegDataWrite(RA8876_DLHSR0,x, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh   
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_SQUARE_FILL, true);//76h,0xE0  

}

// Draw a round rectangle. 
void RA8876_8080::drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t xr, uint16_t yr, uint16_t color) {
	x += _originx;
	y += _originy;
	int16_t x_end = x+w-1;
	int16_t y_end = y+h-1;
	if((x >= _displayclipx2)   || // Clip right
		 (y >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x < _displayclipx1) x = _displayclipx1;
	if (y < _displayclipy1) y = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;
	//drawCircleSquare(x, y, x_end, y_end, xr, yr, color);
	
  check2dBusy();
  graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x,y); swapvals(x_end, y_end); swapvals(xr, yr); break;
		case 2:  x = _width-x; x_end = _width - x_end;   break;
		case 3: rotateCCXY(x,y); rotateCCXY(x_end, y_end); swapvals(xr, yr); break;
  	}
  lcdRegDataWrite(RA8876_DLHSR0,x, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh    
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//79h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//7ah    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7bh
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE_SQUARE, true);//76h,0xb0
	
}

// Draw a filed round rectangle.
void RA8876_8080::fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t xr, uint16_t yr, uint16_t color) {
		x += _originx;
	y += _originy;
	int16_t x_end = x+w-1;
	int16_t y_end = y+h-1;
	if((x >= _displayclipx2)   || // Clip right
		 (y >= _displayclipy2) || // Clip bottom
		 (x_end < _displayclipx1)    || // Clip left
		 (y_end < _displayclipy1))  	// Clip top 
	{
		// outside the clip rectangle
		return;
	}
	if (x < _displayclipx1) x = _displayclipx1;
	if (y < _displayclipy1) y = _displayclipy1;
	if (x_end > _displayclipx2) x_end = _displayclipx2;
	if (y_end > _displayclipy2) y_end = _displayclipy2;
	
  check2dBusy();
  //graphicMode(true);
  foreGroundColor16bpp(color);
	switch (_rotation) {
		case 1: swapvals(x,y); swapvals(x_end, y_end); swapvals(xr, yr); break;
		case 2:  x = _width-x; x_end = _width - x_end;   break;
		case 3: rotateCCXY(x,y); rotateCCXY(x_end, y_end); swapvals(xr, yr); break;
  	}

  lcdRegDataWrite(RA8876_DLHSR0,x, false);//68h
  lcdRegDataWrite(RA8876_DLHSR1,x>>8, false);//69h
  lcdRegDataWrite(RA8876_DLVSR0,y, false);//6ah
  lcdRegDataWrite(RA8876_DLVSR1,y>>8, false);//6bh
  lcdRegDataWrite(RA8876_DLHER0,x_end, false);//6ch
  lcdRegDataWrite(RA8876_DLHER1,x_end>>8, false);//6dh
  lcdRegDataWrite(RA8876_DLVER0,y_end, false);//6eh
  lcdRegDataWrite(RA8876_DLVER1,y_end>>8, false);//6fh    
  lcdRegDataWrite(RA8876_ELL_A0,xr, false);//77h    
  lcdRegDataWrite(RA8876_ELL_A1,xr>>8, false);//78h 
  lcdRegDataWrite(RA8876_ELL_B0,yr, false);//79h    
  lcdRegDataWrite(RA8876_ELL_B1,yr>>8, false);//7ah
  lcdRegDataWrite(RA8876_DCR1,RA8876_DRAW_CIRCLE_SQUARE_FILL, true);//76h,0xf0
}

// Draw a filled circle
void RA8876_8080::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	drawCircleFill(x0, y0, r, color);
}

// Draw a filled ellipse.
void RA8876_8080::fillEllipse(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint16_t color) {
	drawEllipseFill(xCenter, yCenter, longAxis, shortAxis, color);
}

// Draw a filled triangle
void RA8876_8080::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
	drawTriangleFill(x0, y0, x1, y1, x2, y2, color);
}

// Setup graphic mouse cursor
void RA8876_8080::gCursorSet(boolean gCursorEnable, uint8_t gcursortype, uint8_t gcursorcolor1, uint8_t gcursorcolor2) {
	if (!gCursorEnable)
		Disable_Graphic_Cursor();
	else
		Enable_Graphic_Cursor();
	switch(gcursortype) {
		case 1:
			Select_Graphic_Cursor_1(); // PEN
			break;
		case 2:
			Select_Graphic_Cursor_2(); // ARROW
			break;
		case 3:
			Select_Graphic_Cursor_3(); // HOUR GLASS
			break;
		case 4:
			Select_Graphic_Cursor_4(); // ERROR SYMBOL
			break;
		default:
			Select_Graphic_Cursor_2();
	}
	Set_Graphic_Cursor_Color_1(gcursorcolor1);
	Set_Graphic_Cursor_Color_2(gcursorcolor2);
}

// Set graphic cursor position on screen
void RA8876_8080::gcursorxy(uint16_t gcx, uint16_t gcy) {
	Graphic_Cursor_XY(gcx, gcy);
}



//=======================================================================
//= RA8876 BTE functions (Block Transfer Engine)                        =
//=======================================================================
// Copy box size part of the current screen page to another screen page
/*uint32_t RA8876_8080::tft_boxPut(uint32_t vPageAddr, uint16_t x0, uint16_t y0,
					uint16_t x1, uint16_t y1, uint16_t dx0, uint16_t dy0) {
	return boxPut(vPageAddr, x0, y0, x1, y1, dx0, dy0);
}

// Copy a box size part of another screen page to the current screen page
uint32_t RA8876_8080::tft_boxGet(uint32_t vPageAddr, uint16_t x0, uint16_t y0,
					uint16_t x1, uint16_t y1, uint16_t dx0, uint16_t dy0) {
	return boxGet(vPageAddr, x0, y0, x1, y1, dx0, dy0);
}
*/

//=======================================================================
//= PIP window function (Display one of two or both PIP windows)        =
//=======================================================================
/*void RA8876_8080::tft_PIP (
 unsigned char On_Off // 0 : disable PIP, 1 : enable PIP, 2 : To maintain the original state
,unsigned char Select_PIP // 1 : use PIP1 , 2 : use PIP2
,unsigned long PAddr //start address of PIP
,unsigned short XP //coordinate X of PIP Window, It must be divided by 4.
,unsigned short YP //coordinate Y of PIP Window, It must be divided by 4.
,unsigned long ImageWidth //Image Width of PIP (recommend = canvas image width)
,unsigned short X_Dis //coordinate X of Display Window
,unsigned short Y_Dis //coordinate Y of Display Window
,unsigned short X_W //width of PIP and Display Window, It must be divided by 4.
,unsigned short Y_H //height of PIP and Display Window , It must be divided by 4.
) {
	PIP (On_Off, Select_PIP, PAddr, XP, YP, ImageWidth, X_Dis, Y_Dis, X_W, Y_H);
}
*/

/**************************************************************************/
/*!		
		Set the Active Window
	    Parameters:
		XL: Horizontal Left
		XR: Horizontal Right
		YT: Vertical TOP
		YB: Vertical Bottom
*/
/**************************************************************************/
void RA8876_8080::setActiveWindow(int16_t XL,int16_t XR ,int16_t YT ,int16_t YB)
{
	//if (_portrait){ swapvals(XL,YT); swapvals(XR,YB);}

//	if (XR >= SCREEN_WIDTH) XR = SCREEN_WIDTH;
//	if (YB >= SCREEN_HEIGHT) YB = SCREEN_HEIGHT;
	
	_activeWindowXL = XL; _activeWindowXR = XR;
	_activeWindowYT = YT; _activeWindowYB = YB;
	_updateActiveWindow(false);
}

/**************************************************************************/
/*!		
		Set the Active Window as FULL SCREEN
*/
/**************************************************************************/
void RA8876_8080::setActiveWindow(void)
{
	_activeWindowXL = 0; _activeWindowXR = _width;
	_activeWindowYT = 0; _activeWindowYB = _height;
	//if (_portrait){swapvals(_activeWindowXL,_activeWindowYT); swapvals(_activeWindowXR,_activeWindowYB);}
	_updateActiveWindow(true);
}

/**************************************************************************/
/*!
		this update the RA8875 Active Window registers
		[private]
*/
/**************************************************************************/
void RA8876_8080::_updateActiveWindow(bool full)
{
	if (full){
		// X
		activeWindowXY(0, 0);
		activeWindowWH(_width, _height);;
	} else {
		activeWindowXY(_activeWindowXL, _activeWindowYT);
		activeWindowWH(_activeWindowXR-_activeWindowXL, _activeWindowYB-_activeWindowYT);		
	}
}

//**************************************************************//
/* JB ADD FOR ROTATING TEXT                                     */
/* Turn RA8876 text rotate mode ON/OFF (True = ON)              */
//**************************************************************//
void RA8876_8080::setRotation(uint8_t rotation) //rotate text and graphics
{
	_rotation = rotation & 0x3;
	uint8_t macr_settings;

	switch (_rotation) {
		case 0:
			_width = 	SCREEN_WIDTH;
			_height = 	SCREEN_HEIGHT;
			_portrait = false;
			VSCAN_T_to_B();
			macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1;
			break;
		case 1:
			_portrait = true;
			_width = 	SCREEN_HEIGHT;
			_height = 	SCREEN_WIDTH;
			VSCAN_B_to_T();
			macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_RLTB<<1;
			break;
		case 2: 
			_width = 	SCREEN_WIDTH;
			_height = 	SCREEN_HEIGHT;
			_portrait = false;
			VSCAN_B_to_T();
			macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_RLTB<<1;
			break;
		case 3: 
			_portrait = true;
			_width = 	SCREEN_HEIGHT;
			_height = 	SCREEN_WIDTH;
			VSCAN_T_to_B();
			macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1;
			//VSCAN_T_to_B();
			//macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_BTLR<<1;
			break;
	}
	lcdRegWrite(RA8876_MACR);//02h
	lcdDataWrite(macr_settings);

	_scrollXL = 0;
	_scrollXR = _width;
	_scrollYT = 0;
	_scrollYB = _height;
	
	_updateActiveWindow(false);

 	setClipRect();
	setOrigin();
	//Serial.println("Rotate: After Origins"); Serial.flush();

}

/**************************************************************************/
/*!
      Get rotation setting
*/
/**************************************************************************/
uint8_t RA8876_8080::getRotation()
{
	return _rotation;

}

//**************************************************************//
/* JB ADD FOR ROTATING TEXT                                     */
/* Turn RA8876 text rotate mode ON/OFF (True = ON)              */
//**************************************************************//
void RA8876_8080::textRotate(boolean on)
{
    if(on)
    {
        lcdRegDataWrite(RA8876_CCR1, RA8876_TEXT_ROTATION<<4);//cdh
    }
    else
    {
        lcdRegDataWrite(RA8876_CCR1, RA8876_TEXT_NO_ROTATION<<4);//cdh
    }
}



void RA8876_8080::MemWrite_Left_Right_Top_Down(void)
{
/* Host Write Memory Direction (Only for Graphic Mode)
00b: Left .. Right then Top ..Bottom.
Ignored if canvas in linear addressing mode.		*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
	temp &= cClrb2;
	temp &= cClrb1;
	Serial.println(temp, BIN);
	lcdRegDataWrite(RA8876_MACR, temp);
	
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);	
}

void RA8876_8080::MemWrite_Right_Left_Top_Down(void)
{
/* Host Write Memory Direction (Only for Graphic Mode)
01b: Right .. Left then Top .. Bottom.
Ignored if canvas in linear addressing mode.		*/
	unsigned char temp;

	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
	
	temp &= cClrb2;
	temp |= cSetb1;
	Serial.println(temp, BIN);
	lcdRegDataWrite(RA8876_MACR, temp);
	
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
	
	
}

void RA8876_8080::MemWrite_Top_Down_Left_Right(void)
{
/* Host Write Memory Direction (Only for Graphic Mode)
10b: Top .. Bottom then Left .. Right.
Ignored if canvas in linear addressing mode.		*/
	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
	//lcdRegWrite(RA8876_MACR);//02h
	//lcdDataWrite(RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1);

	//temp = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_BTLR<<4|RA8876_WRITE_MEMORY_BTLR<<1;
	temp = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_TBLR<<1;
	Serial.println(temp, BIN);
	lcdRegDataWrite(RA8876_MACR, temp);
	
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
/*
	unsigned char temp;
	lcdDataWrite(0x02);
	temp = lcdDataRead();
	temp |= cSetb2;
    temp &= cClrb1;
	lcdDataWrite(temp, true);
	*/

}

void RA8876_8080::MemWrite_Down_Top_Left_Right(void)
{
/* Host Write Memory Direction (Only for Graphic Mode)
11b: Bottom .. Top then Left .. Right.
Ignored if canvas in linear addressing mode.		*/

	unsigned char temp;
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);
	//lcdRegWrite(RA8876_MACR);//02h
	//lcdDataWrite(RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1);

	//temp = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_BTLR<<4|RA8876_WRITE_MEMORY_BTLR<<1;
	temp = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_BTLR<<1;
	Serial.println(temp, BIN);
	lcdRegDataWrite(RA8876_MACR, temp);
	
	temp = lcdRegDataRead(RA8876_MACR);
	Serial.println(temp, BIN);

}

void RA8876_8080::VSCAN_T_to_B(void)
{
/*	
Vertical Scan direction
0 : From Top to Bottom
1 : From bottom to Top
PIP window will be disabled when VDIR set as 1.
*/
	unsigned char temp;
	
	temp = lcdRegDataRead(RA8876_DPCR);
	temp &= cClrb3;
	lcdRegDataWrite(RA8876_DPCR, temp);

}

void RA8876_8080::VSCAN_B_to_T(void)
{
/*	
Vertical Scan direction
0 : From Top to Bottom
1 : From bottom to Top
PIP window will be disabled when VDIR set as 1.
*/
  
	unsigned char temp, temp_in;
	
	temp_in =  temp = lcdRegDataRead(RA8876_DPCR);
	temp |= cSetb3;
	lcdRegDataWrite(RA8876_DPCR, temp);
	Serial.printf("call vscan_b_to_t %x %x\n", temp_in, temp);

}