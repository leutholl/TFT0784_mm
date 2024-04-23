//**************************************************************//
/*
 * RA8876_8080.cpp
 *
 * AUTHOR: leutholl
 * DATE: FEB 2024
 * VERSION: 1.0
 *
 * Library for the RA8876 using Teensy MicroMod and FlexIO2 in
 * MultiBeat and DMA mode. LVGL can compute the next buffer while
 * DMA is shifting the current buffer to the display. This plus the
 * four SHIFTERS of FlexIO2 allows for a very fast refresh rate.
 *
 * The library is minimized to work with lvgl. Primitive drawing
 * and text, cursor, pip etc... was removed. Instead of 8000 lines
 * it is now 2000 lines long. Using lvgl, you wouldn't need the text
 * features and other things anyway and instead work with buffers.
 *
 * lvgl knows which area of the screen is dirty and will send the buffer to
 * update the screen just for that area. Using BTE&ROP feature of the RA8876
 * we can then place a picture of the buffer at the right location so that we
 * can only update the dirty part of the screen which lvgl will tell us.
 * This greatly reduces the overhead to otherwise full send a full screen
 * refresh.
 *
 * How fast is it you ask? Well on this 1280x400 screen it can do up to 40 fps
 * when doing large area updates. When doing small area updates it can do up to
 * 60 fps. In comparison the RA8876 with SPI and DMA can only do around 4 fps
 * for larger areas. This is around 10 times faster. Larger UI components like
 * the roller control which uses animation are finally buttery smooth.
 *
 * Should you need additional functionality, please take it from the original library
 *
 * ORIGINAL:
 *
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

RA8876_8080 *RA8876_8080::dmaCallback = nullptr;
DMAChannel RA8876_8080::flexDma;

RA8876_8080::RA8876_8080(uint8_t DCp, uint8_t CSp, uint8_t RSTp, uint8_t WAITp) {
    _cs = CSp;
    _rst = RSTp;
    _dc = DCp;
    _wait = WAITp;
}

FLASHMEM boolean RA8876_8080::begin(uint8_t baud_div) {
    WR_DMATransferDone = true;

    switch (baud_div) {
        case 1:
            _baud = 240;
            break;
        case 2:
            _baud = 120;
            break;
        case 4:
            _baud = 60;
            break;
        case 8:
            _baud = 30;
            break;
        case 12:
            _baud = 20;
            break;
        case 20:
            _baud = 12;
            break;
        case 24:
            _baud = 10;
            break;
        case 30:
            _baud = 8;
            break;
        case 40:
            _baud = 6;
            break;
        case 60:
            _baud = 4;
            break;
        case 120:
            _baud = 2;
            break;
        default:
            _baud = 20;  // 12Mhz
            break;
    }

    pinMode(_cs, OUTPUT);  // CS

    pinMode(_dc, OUTPUT);            // DC
    pinMode(_rst, OUTPUT);           // RST
    pinMode(_wait, INPUT_PULLDOWN);  // WAIT

    *(portControlRegister(_cs)) = 0xFF;
    *(portControlRegister(_dc)) = 0xFF;
    *(portControlRegister(_rst)) = 0xFF;

    digitalWriteFast(_cs, LOW);  // select screen always
    // digitalWriteFast(_cs, HIGH);
    digitalWriteFast(_dc, HIGH);
    digitalWriteFast(_rst, HIGH);

    delay(15);
    digitalWrite(_rst, LOW);
    delay(15);
    digitalWriteFast(_rst, HIGH);
    delay(100);

    FlexIO_Init();

    if (!checkIcReady())
        return false;

    // read ID code must disable pll, 01h bit7 set 0
    if (BUS_WIDTH == 16) {
        lcdRegDataWrite(0x01, 0x09);
    } else {
        lcdRegDataWrite(0x01, 0x08);
    }
    delay(1);
    if ((lcdRegDataRead(0xff) != 0x76) && (lcdRegDataRead(0xff) != 0x77)) {
        return false;
    }

    // Initialize RA8876 to default settings
    if (!ra8876Initialize()) {
        return false;
    }

    return true;
}

FASTRUN void RA8876_8080::CSLow() {
    digitalWriteFast(_cs, LOW);  // Select TFT
}

FASTRUN void RA8876_8080::CSHigh() {
    digitalWriteFast(_cs, HIGH);  // Deselect TFT
}

FASTRUN void RA8876_8080::DCLow() {
    digitalWriteFast(_dc, LOW);  // Writing command to TFT
}

FASTRUN void RA8876_8080::DCHigh() {
    digitalWriteFast(_dc, HIGH);  // Writing data to TFT
}

FASTRUN void RA8876_8080::WAIT() {
    while (digitalReadFast(_wait) == LOW) {
        __asm__("nop\n\t");
        Serial.print('.');
    };
}

/*
FASTRUN void RA8876_8080::microSecondDelay() {
  for (uint32_t i=0; i<99; i++) __asm__("nop\n\t");
}
*/

FASTRUN void RA8876_8080::gpioWrite() {
#if (FLEXIO2_MM_DMA)
    pFlex->setIOPinToFlexMode(10);
    pinMode(12, OUTPUT);
    digitalWriteFast(12, HIGH);
#else
    pFlex->setIOPinToFlexMode(36);  // ReConfig /WR pin
    pinMode(37, OUTPUT);            // Set /RD pin to output
    digitalWriteFast(37, HIGH);     // Set /RD pin High
#endif
}

FASTRUN void RA8876_8080::gpioRead() {
#if (FLEXIO2_MM_DMA)
    pFlex->setIOPinToFlexMode(12);
    pinMode(10, OUTPUT);
    digitalWriteFast(10, HIGH);
#else
    pFlex->setIOPinToFlexMode(37);  // ReConfig /RD pin
    pinMode(36, OUTPUT);            // Set /WR pin to output
    digitalWriteFast(36, HIGH);     // Set /WR pin High
#endif
}

FASTRUN void RA8876_8080::FlexIO_Init() {
    /* Get a FlexIO channel */
#if (FLEXIO2_MM_DMA)
    pFlex = FlexIOHandler::flexIOHandler_list[1];  // use FlexIO2
#else
    pFlex = FlexIOHandler::flexIOHandler_list[2];  // use FlexIO3
#endif

    /* Pointer to the port structure in the FlexIO channel */
    p = &pFlex->port();
    /* Pointer to the hardware structure in the FlexIO channel */
    hw = &pFlex->hardware();

#if (FLEXIO2_MM_DMA)
    /* Basic pin setup */
    pinMode(10, OUTPUT);  // FlexIO2:0 WR
    pinMode(12, OUTPUT);  // FlexIO2:1 RD
    pinMode(40, OUTPUT);  // FlexIO2:4 D0
    pinMode(41, OUTPUT);  // FlexIO2:5 |
    pinMode(42, OUTPUT);  // FlexIO2:6 |
    pinMode(43, OUTPUT);  // FlexIO2:7 |
    pinMode(44, OUTPUT);  // FlexIO2:8 |
    pinMode(45, OUTPUT);  // FlexIO2:9 |
    pinMode(6, OUTPUT);   // FlexIO2:10 |
    pinMode(9, OUTPUT);   // FlexIO2:11 D7
    digitalWriteFast(10, HIGH);

#else
    /* Basic pin setup */
    pinMode(19, OUTPUT);  // FlexIO3:0 D0
    pinMode(18, OUTPUT);  // FlexIO3:1 |
    pinMode(14, OUTPUT);  // FlexIO3:2 |
    pinMode(15, OUTPUT);  // FlexIO3:3 |
    pinMode(40, OUTPUT);  // FlexIO3:4 |
    pinMode(41, OUTPUT);  // FlexIO3:5 |
    pinMode(17, OUTPUT);  // FlexIO3:6 |
    pinMode(16, OUTPUT);  // FlexIO3:7 D7
#if (BUS_WIDTH == 16)
    pinMode(22, OUTPUT);  // FlexIO3:8 D8
    pinMode(23, OUTPUT);  // FlexIO3:9  |
    pinMode(20, OUTPUT);  // FlexIO3:10 |
    pinMode(21, OUTPUT);  // FlexIO3:11 |
    pinMode(38, OUTPUT);  // FlexIO3:12 |
    pinMode(39, OUTPUT);  // FlexIO3:13 |
    pinMode(26, OUTPUT);  // FlexIO3:14 |
    pinMode(27, OUTPUT);  // FlexIO3:15 D15
#endif

    pinMode(36, OUTPUT);
    digitalWriteFast(36, HIGH);
    pinMode(37, OUTPUT);
    digitalWriteFast(37, HIGH);
#endif

    /* High speed and drive strength configuration */

#if (FLEXIO2_MM_DMA)

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
    // LL  pFlex->setClockSettings(3, 1, 0); // (480 MHz source, 1+1, 1+0) >> 480/2/1 >> 240Mhz
    pFlex->setClockSettings(3, 1, 0);  // (480 MHz source, 1+1, 1+0) >> 480/2/1 >> 240Mhz

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

    hw->clock_gate_register |= hw->clock_gate_mask;
    /* Enable the FlexIO with fast access */
#if (FLEXIO2_MM_DMA)
    p->CTRL = FLEXIO_CTRL_FLEXEN;
#else
    p->CTRL = FLEXIO_CTRL_FLEXEN;
#endif
}

FASTRUN void RA8876_8080::FlexIO_Config_SnglBeat_Read() {
    // Serial.println("FlexIO_Config_SnglBeat_Read");

    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    gpioRead();

#if (FLEXIO2_MM_DMA)
    /* Configure the shifters */
    p->SHIFTCFG[3] =
        // FLEXIO_SHIFTCFG_INSRC                                                  /* Shifter input */
        FLEXIO_SHIFTCFG_SSTOP(0)     /* Shifter stop bit disabled */
        | FLEXIO_SHIFTCFG_SSTART(0)  /* Shifter start bit disabled and loading data on enabled */
        | FLEXIO_SHIFTCFG_PWIDTH(7); /* Bus width */

    p->SHIFTCTL[3] =
        FLEXIO_SHIFTCTL_TIMSEL(0)      /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (1) /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(0)    /* Shifter's pin configured as input */
        | FLEXIO_SHIFTCTL_PINSEL(4)    // was 4                             /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(1);     /* Shifter mode as recieve */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        (((1 * 2) - 1) << 8)    /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | (((_baud) / 2) - 1);  // was 30                         /* TIMCMP[7:0] = baud rate divider / 2 – 1 ::: 30 = 8Mhz with current controller speed */

    p->TIMCFG[0] =
        FLEXIO_TIMCFG_TIMOUT(0)       /* Timer output logic one when enabled and not affected by reset */
        | FLEXIO_TIMCFG_TIMDEC(0)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
        | FLEXIO_TIMCFG_TIMRST(0)     /* Timer never reset */
        | FLEXIO_TIMCFG_TIMDIS(2)     /* Timer disabled on timer compare */
        | FLEXIO_TIMCFG_TIMENA(2)     /* Timer enabled on trigger high */
        | FLEXIO_TIMCFG_TSTOP(1)      /* Timer stop bit disabled */
        | FLEXIO_TIMCFG_TSTART * (0); /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL((((3) << 2) | 1)) /* Timer trigger selected as shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1)           /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1)           /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3)              /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(1)              // was 1                             /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1)           /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1);              /* Timer mode as dual 8-bit counters baud/bit */

    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;
#else
    /* Configure the shifters */
    p->SHIFTCFG[3] =
        FLEXIO_SHIFTCFG_SSTOP(0)                 /* Shifter stop bit disabled */
        | FLEXIO_SHIFTCFG_SSTART(0)              /* Shifter start bit disabled and loading data on enabled */
        | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH - 1); /* Bus width */

    p->SHIFTCTL[3] =
        FLEXIO_SHIFTCTL_TIMSEL(0)      /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (1) /* Shift on negative edge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(1)    /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(0)    /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(1);     /* Shifter mode as receive */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        (((1 * 2) - 1) << 8) /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | ((60 / 2) - 1);    /* TIMCMP[7:0] = baud rate divider / 2 – 1 */

    p->TIMCFG[0] =
        FLEXIO_TIMCFG_TIMOUT(0)       /* Timer output logic one when enabled and not affected by reset */
        | FLEXIO_TIMCFG_TIMDEC(0)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
        | FLEXIO_TIMCFG_TIMRST(0)     /* Timer never reset */
        | FLEXIO_TIMCFG_TIMDIS(2)     /* Timer disabled on timer compare */
        | FLEXIO_TIMCFG_TIMENA(2)     /* Timer enabled on trigger high */
        | FLEXIO_TIMCFG_TSTOP(1)      /* Timer stop bit enabled */
        | FLEXIO_TIMCFG_TSTART * (0); /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL((((3) << 2) | 1)) /* Timer trigger selected as shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1)           /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1)           /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3)              /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(19)             /* Timer' pin index: RD pin */
        | FLEXIO_TIMCTL_PINPOL * (1)           /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1);              /* Timer mode as dual 8-bit counters baud/bit */

    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;
#endif
}

FASTRUN void RA8876_8080::FlexIO_Config_SnglBeat() {
    //	Serial.println("FlexIO_Config_SnglBeat");

    gpioWrite();

    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

#if (FLEXIO2_MM_DMA)

    /* Configure the shifters */
    p->SHIFTCFG[0] =
        FLEXIO_SHIFTCFG_INSRC * (1)  /* Shifter input */
        | FLEXIO_SHIFTCFG_SSTOP(0)   /* Shifter stop bit disabled */
        | FLEXIO_SHIFTCFG_SSTART(0)  /* Shifter start bit disabled and loading data on enabled */
        | FLEXIO_SHIFTCFG_PWIDTH(7); /* Bus width */

    p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)      /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0) /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3)    /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(4)    // was 4                                      /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2);     /* Shifter mode as transmit */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        (((1 * 2) - 1) << 8) /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | ((_baud / 2) - 1); /* TIMCMP[7:0] = baud rate divider / 2 – 1 */

    p->TIMCFG[0] =
        FLEXIO_TIMCFG_TIMOUT(0)       /* Timer output logic one when enabled and not affected by reset */
        | FLEXIO_TIMCFG_TIMDEC(0)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
        | FLEXIO_TIMCFG_TIMRST(0)     /* Timer never reset */
        | FLEXIO_TIMCFG_TIMDIS(2)     /* Timer disabled on timer compare */
        | FLEXIO_TIMCFG_TIMENA(2)     /* Timer enabled on trigger high */
        | FLEXIO_TIMCFG_TSTOP(0)      /* Timer stop bit disabled */
        | FLEXIO_TIMCFG_TSTART * (0); /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL((((0) << 2) | 1)) /* Timer trigger selected as shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1)           /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1)           /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3)              /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(0)              // was 0                             /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1)           /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1);              /* Timer mode as dual 8-bit counters baud/bit */

    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;

#else

    /* Configure the shifters */
    p->SHIFTCFG[0] =
        FLEXIO_SHIFTCFG_INSRC * (1)               /* Shifter input */
        | FLEXIO_SHIFTCFG_SSTOP(0)                /* Shifter stop bit disabled */
        | FLEXIO_SHIFTCFG_SSTART(0)               /* Shifter start bit disabled and loading data on enabled */
        | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH - 1);  // Was 7 for 8 bit bus         /* Bus width */

    p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)      /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0) /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3)    /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(0)    /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2);     /* Shifter mode as transmit */

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        (((1 * 2) - 1) << 8) /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | ((_baud / 2) - 1); /* TIMCMP[7:0] = baud rate divider / 2 – 1 */

    p->TIMCFG[0] =
        FLEXIO_TIMCFG_TIMOUT(0)       /* Timer output logic one when enabled and not affected by reset */
        | FLEXIO_TIMCFG_TIMDEC(0)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
        | FLEXIO_TIMCFG_TIMRST(0)     /* Timer never reset */
        | FLEXIO_TIMCFG_TIMDIS(2)     /* Timer disabled on timer compare */
        | FLEXIO_TIMCFG_TIMENA(2)     /* Timer enabled on trigger high */
        | FLEXIO_TIMCFG_TSTOP(0)      /* Timer stop bit disabled */
        | FLEXIO_TIMCFG_TSTART * (0); /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL((((0) << 2) | 1)) /* Timer trigger selected as shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1)           /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1)           /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3)              /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(18)             /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1)           /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1);              /* Timer mode as dual 8-bit counters baud/bit */

    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;
#endif
}

FASTRUN void RA8876_8080::FlexIO_Clear_Config_SnglBeat() {
    // Serial.println("FlexIO_Clear_Config_SnglBeat");
    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    // p->TIMCMP[0] = 0;

    p->TIMCTL[0] = 0U;
    p->TIMCFG[0] = 0U;
    p->TIMSTAT = (1U << 0); /* Timer start bit disabled */
    p->SHIFTCTL[0] = 0U;
    p->SHIFTCFG[0] = 0U;
    p->SHIFTSTAT = (1 << 0);
    /* Enable FlexIO */
    p->CTRL |= FLEXIO_CTRL_FLEXEN;
}

FASTRUN void RA8876_8080::FlexIO_Config_MultiBeat() {
    // FlexIO_Clear_Config_SnglBeat();
    // Serial.println("FlexIO_Config_MultiBeat");
    uint8_t beats = SHIFTNUM * BEATS_PER_SHIFTER;  // Number of beats = number of shifters * beats per shifter
    /* Disable and reset FlexIO */
    p->CTRL &= ~FLEXIO_CTRL_FLEXEN;
    p->CTRL |= FLEXIO_CTRL_SWRST;
    p->CTRL &= ~FLEXIO_CTRL_SWRST;

    gpioWrite();

#if (FLEXIO2_MM_DMA)
    uint32_t i;

    p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)       /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0U) /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3U)    /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(4)     /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0U) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2U);     /* shifter mode transmit */

    for (i = 0; i <= SHIFTNUM - 1; i++) {
        p->SHIFTCFG[i] =
            FLEXIO_SHIFTCFG_INSRC * (1U)       /* Shifter input from next shifter's output */
            | FLEXIO_SHIFTCFG_SSTOP(0U)        /* Shifter stop bit disabled */
            | FLEXIO_SHIFTCFG_SSTART(0U)       /* Shifter start bit disabled and loading data on enabled */
            | FLEXIO_SHIFTCFG_PWIDTH(8U - 1U); /* 8 bit shift width */
    }

    for (i = 1; i <= SHIFTNUM - 1; i++) {
        p->SHIFTCTL[i] =
            FLEXIO_SHIFTCTL_TIMSEL(0U)      /* Shifter's assigned timer index */
            | FLEXIO_SHIFTCTL_TIMPOL * (0U) /* Shift on posedge of shift clock */
            | FLEXIO_SHIFTCTL_PINCFG(0U)    /* Shifter's pin configured as output disabled */
            | FLEXIO_SHIFTCTL_PINSEL(0U)    /* Shifter's pin start index */
            | FLEXIO_SHIFTCTL_PINPOL * (0U) /* Shifter's pin active high */
            | FLEXIO_SHIFTCTL_SMOD(2U);     /* shifter mode transmit */
    }

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        ((beats * 2U - 1) << 8) /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | (_baud / 2U - 1U);
    /* TIMCMP[7:0] = shift clock divide ratio / 2 - 1 */  // try 4 was _baud

    p->TIMCFG[0] = FLEXIO_TIMCFG_TIMOUT(0U)       /* Timer output logic one when enabled and not affected by reset */
                   | FLEXIO_TIMCFG_TIMDEC(0U)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
                   | FLEXIO_TIMCFG_TIMRST(0U)     /* Timer never reset */
                   | FLEXIO_TIMCFG_TIMDIS(2U)     /* Timer disabled on timer compare */
                   | FLEXIO_TIMCFG_TIMENA(2U)     /* Timer enabled on trigger high */
                   | FLEXIO_TIMCFG_TSTOP(0U)      /* Timer stop bit disabled */
                   | FLEXIO_TIMCFG_TSTART * (0U); /* Timer start bit disabled */

    p->TIMCTL[0] =  // equals 0x0DC30081;

        FLEXIO_TIMCTL_TRGSEL(((SHIFTNUM - 1) << 2) | 1U) /* Timer trigger selected as highest shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1U)                    /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1U)                    /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3U)                       /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(0)                        /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1U)                    /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1U);                       /* Timer mode 8-bit baud counter */

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
    p->SHIFTSDEN |= 1U << (SHIFTER_DMA_REQUEST);  // enable DMA trigger when shifter status flag is set on shifter SHIFTER_DMA_REQUEST

#else

    /* Configure the shifters */
    for (int i = 0; i <= SHIFTNUM - 1; i++) {
        p->SHIFTCFG[i] =
            FLEXIO_SHIFTCFG_INSRC * (1U)             /* Shifter input from next shifter's output */
            | FLEXIO_SHIFTCFG_SSTOP(0U)              /* Shifter stop bit disabled */
            | FLEXIO_SHIFTCFG_SSTART(0U)             /* Shifter start bit disabled and loading data on enabled */
            | FLEXIO_SHIFTCFG_PWIDTH(BUS_WIDTH - 1); /* 8 bit shift width */
    }

    p->SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_TIMSEL(0)       /* Shifter's assigned timer index */
        | FLEXIO_SHIFTCTL_TIMPOL * (0U) /* Shift on posedge of shift clock */
        | FLEXIO_SHIFTCTL_PINCFG(3U)    /* Shifter's pin configured as output */
        | FLEXIO_SHIFTCTL_PINSEL(0)     /* Shifter's pin start index */
        | FLEXIO_SHIFTCTL_PINPOL * (0U) /* Shifter's pin active high */
        | FLEXIO_SHIFTCTL_SMOD(2U);     /* shifter mode transmit */

    for (int i = 1; i <= SHIFTNUM - 1; i++) {
        p->SHIFTCTL[i] =
            FLEXIO_SHIFTCTL_TIMSEL(0)       /* Shifter's assigned timer index */
            | FLEXIO_SHIFTCTL_TIMPOL * (0U) /* Shift on posedge of shift clock */
            | FLEXIO_SHIFTCTL_PINCFG(0U)    /* Shifter's pin configured as output disabled */
            | FLEXIO_SHIFTCTL_PINSEL(0)     /* Shifter's pin start index */
            | FLEXIO_SHIFTCTL_PINPOL * (0U) /* Shifter's pin active high */
            | FLEXIO_SHIFTCTL_SMOD(2U);
    }

    /* Configure the timer for shift clock */
    p->TIMCMP[0] =
        ((beats * 2U - 1) << 8) /* TIMCMP[15:8] = number of beats x 2 – 1 */
        | (_baud / 2U - 1U);    /* TIMCMP[7:0] = shift clock divide ratio / 2 - 1 */

    p->TIMCFG[0] = FLEXIO_TIMCFG_TIMOUT(0U)       /* Timer output logic one when enabled and not affected by reset */
                   | FLEXIO_TIMCFG_TIMDEC(0U)     /* Timer decrement on FlexIO clock, shift clock equals timer output */
                   | FLEXIO_TIMCFG_TIMRST(0U)     /* Timer never reset */
                   | FLEXIO_TIMCFG_TIMDIS(2U)     /* Timer disabled on timer compare */
                   | FLEXIO_TIMCFG_TIMENA(2U)     /* Timer enabled on trigger high */
                   | FLEXIO_TIMCFG_TSTOP(0U)      /* Timer stop bit disabled */
                   | FLEXIO_TIMCFG_TSTART * (0U); /* Timer start bit disabled */

    p->TIMCTL[0] =
        FLEXIO_TIMCTL_TRGSEL(((SHIFTNUM - 1) << 2) | 1U) /* Timer trigger selected as highest shifter's status flag */
        | FLEXIO_TIMCTL_TRGPOL * (1U)                    /* Timer trigger polarity as active low */
        | FLEXIO_TIMCTL_TRGSRC * (1U)                    /* Timer trigger source as internal */
        | FLEXIO_TIMCTL_PINCFG(3U)                       /* Timer' pin configured as output */
        | FLEXIO_TIMCTL_PINSEL(18)                       /* Timer' pin index: WR pin */
        | FLEXIO_TIMCTL_PINPOL * (1U)                    /* Timer' pin active low */
        | FLEXIO_TIMCTL_TIMOD(1U);                       /* Timer mode 8-bit baud counter */
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

FASTRUN void RA8876_8080::MultiBeat_ROP_DMA(volatile void *value, uint32_t const length) {
    WR_DMATransferDone = false;

    uint32_t BeatsPerMinLoop = SHIFTNUM * sizeof(uint32_t) / sizeof(uint8_t);  // Number of shifters * number of 8 bit values per shifter
    volatile uint16_t majorLoopCount, minorLoopBytes;
    uint32_t destinationModulo = 31 - (__builtin_clz(SHIFTNUM * sizeof(uint32_t)));  // defines address range for circular DMA destination buffer

    // delayMicroseconds(1);

    if (length < 8) {
        // Serial.println("In DMA but to Short to multibeat");
        const uint16_t *newValue = (uint16_t *)value;
        uint16_t buf;
        for (uint32_t i = 0; i < length; i++) {
            buf = *newValue++;
            while (0 == (p->SHIFTSTAT & (1U << 0))) {
            }
            p->SHIFTBUF[0] = buf >> 8;

            while (0 == (p->SHIFTSTAT & (1U << 0))) {
            }
            p->SHIFTBUF[0] = buf & 0xFF;
        }
        // Wait for transfer to be completed
        while (0 == (p->TIMSTAT & (1U << 0))) {
        }

    }

    else {
        FlexIO_Config_MultiBeat();

        MulBeatCountRemain = length % BeatsPerMinLoop;
        MulBeatDataRemain = (uint16_t *)value + ((length - MulBeatCountRemain));  // pointer to the next unused byte (overflow if MulBeatCountRemain = 0)
        TotalSize = ((length)-MulBeatCountRemain) * 2;                            /* in bytes */
        minorLoopBytes = SHIFTNUM * sizeof(uint32_t);
        majorLoopCount = TotalSize / minorLoopBytes;
        // Serial.printf("Length(16bit): %d, Count remain(16bit): %d, Data remain: %d, TotalSize(8bit): %d, majorLoopCount: %d \n",length, MulBeatCountRemain, MulBeatDataRemain, TotalSize, majorLoopCount );

        /* Setup DMA transfer with on-the-fly swapping of MSB and LSB in 16-bit data:
         *  Within each minor loop, read 16-bit values from buf in reverse order, then write 32bit values to SHIFTBUFBYS[i] in reverse order.
         *  Result is that every pair of bytes are swapped, while half-words are unswapped.
         *  After each minor loop, advance source address using minor loop offset. */
        int destinationAddressOffset, destinationAddressLastOffset, sourceAddressOffset, sourceAddressLastOffset, minorLoopOffset;
        volatile void *sourceAddress;
        volatile void *destinationAddress;

        DMA_CR |= DMA_CR_EMLM;  // enable minor loop mapping

        CSLow();

        arm_dcache_flush_delete((void *)value, length * 2);  // important to flush cache before DMA. Otherwise, DMA will read from cache instead of memory and screen shows "snow" effects.

        /* My most time-consumed lines of code to get a perfect image is here. I still don't fully understand why this is needed. But here is my clue:
         * The DMA setup further down uses the transfers in reverse mode. That's another thing that I don't understand as this is the only way I get it working.
         * It seems that because of the reverse transfer logic of DMA the first (in forward thinking) bytes which equals the first 12 clocks are zero.
         * This lead to 6 black pixels for every new buffer transfered. Together with my Logic Analyzer and countless hours I found out that I can set the data
         * for the first 12 clocks if I set the first three SHIFTERS with the first 12 bytes of the image. This is done here.
         * Why 12 and not 16 and why SHIFTER 4 is not having this problem - I don't know.....
         * Somebody more celver than me can maybe explain this.
         */

        p->SHIFTBUFHWS[0] = *(uint32_t *)value;
        uint32_t *value32 = (uint32_t *)value;
        value32++;
        p->SHIFTBUFHWS[1] = *(uint32_t *)value32;
        value32++;
        p->SHIFTBUFHWS[2] = *(uint32_t *)value32;

        /*
         * this is a regular "forward" way and high-level way of doing DMA transfers which should work in our use-case. But it doesn't.
         * for whatever reason, the buffer must be filled in reverse order. I don't know why. But it works. It could be that finer control
         * of the minor Loop behavior is needed which is not available through this DMA API therefore this is setup manually below.
                flexDma.begin();
                flexDma.sourceBuffer((volatile uint16_t*)value, majorLoopCount*2);
                flexDma.destinationCircular((volatile uint32_t*)&p->SHIFTBUF[SHIFTNUM-1], SHIFTNUM);
                flexDma.transferCount(majorLoopCount);
                flexDma.transferSize(minorLoopBytes*2);
                flexDma.triggerAtHardwareEvent(hw->shifters_dma_channel[SHIFTER_DMA_REQUEST]);
                flexDma.disableOnCompletion();
                flexDma.interruptAtCompletion();
                flexDma.clearComplete();
                flexDma.attachInterrupt(dmaISR);

                flexDma.enable();
                dmaCallback = this;

                return;
                */

        /* From now on, the SHIFTERS in MultiBeat mode are working correctly. Begin DMA transfer */
        flexDma.begin();
        sourceAddress = (volatile uint16_t *)value + (minorLoopBytes) / sizeof(uint16_t) - 1;  // last 16bit address within current minor loop
        sourceAddressOffset = -sizeof(uint16_t);                                               // read values in reverse order
        minorLoopOffset = 2 * minorLoopBytes;                                                  // source address offset at end of minor loop to advance to next minor loop
        sourceAddressLastOffset = minorLoopOffset - TotalSize;                                 // source address offset at completion to reset to beginning
        destinationAddress = (void *)&p->SHIFTBUFHWS[SHIFTNUM - 1];                            // last 32bit shifter address (with reverse byte order)
        destinationAddressOffset = -sizeof(uint32_t);                                          // write words in reverse order
        destinationAddressLastOffset = 0;

        flexDma.TCD->SADDR = (volatile void *)sourceAddress;
        flexDma.TCD->SOFF = (int16_t)sourceAddressOffset;
        flexDma.TCD->SLAST = (int32_t)sourceAddressLastOffset;
        flexDma.TCD->DADDR = (volatile void *)destinationAddress;
        flexDma.TCD->DOFF = (int16_t)destinationAddressOffset;
        flexDma.TCD->DLASTSGA = (int32_t)destinationAddressLastOffset;
        flexDma.TCD->ATTR =
            DMA_TCD_ATTR_SMOD(0U) | DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_16BIT)                    // 16bit reads
            | DMA_TCD_ATTR_DMOD(destinationModulo) | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT);  // 32bit writes
        flexDma.TCD->NBYTES_MLOFFYES =
            DMA_TCD_NBYTES_SMLOE | DMA_TCD_NBYTES_MLOFFYES_MLOFF(minorLoopOffset) | DMA_TCD_NBYTES_MLOFFYES_NBYTES(minorLoopBytes);
        flexDma.TCD->CITER = majorLoopCount;  // Current major iteration count
        flexDma.TCD->BITER = majorLoopCount;  // Starting major iteration count

        flexDma.triggerAtHardwareEvent(hw->shifters_dma_channel[SHIFTER_DMA_REQUEST]);
        flexDma.disableOnCompletion();
        flexDma.interruptAtCompletion();
        flexDma.clearComplete();

        flexDma.attachInterrupt(dmaISR);

        flexDma.enable();
        dmaCallback = this;
    }
}

FLASHMEM void RA8876_8080::onCompleteCB(CBF callback) {
    _callback = callback;
    isCB = true;
}

FASTRUN void RA8876_8080::_onCompleteCB() {
    if (_callback) {
        _callback();
    }
    return;
}

// WIP: Offer an alternative to DMA using IRQs. Not working yet.
/*
FASTRUN void RA8876_8080::flexIRQ_Callback() {

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
            p->TIMCMP[0] = ((beats * 2U - 1) << 8) | (_baud / 2U - 1); // takes effect on final burst
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
*/
FASTRUN void RA8876_8080::dmaISR() {
    flexDma.clearInterrupt();
    asm volatile("dsb");  // prevent interrupt from re-entering
    dmaCallback->flexDma_Callback();
}

FASTRUN void RA8876_8080::flexDma_Callback() {
    // Serial.printf("DMA callback start triggred \n");

    /* the interrupt is called when the final DMA transfer completes writing to the shifter buffers, which would generally happen while
    data is still in the process of being shifted out from the second-to-last major iteration. In this state, all the status flags are cleared.
    when the second-to-last major iteration is fully shifted out, the final data is transfered from the buffers into the shifters which sets all the status flags.
    if you have only one major iteration, the status flags will be immediately set before the interrupt is called, so the while loop will be skipped. */

    while (0 == (p->SHIFTSTAT & (1U << (SHIFTNUM - 1)))) {
    }

    /* Wait the last multi-beat transfer to be completed. Clear the timer flag
    before the completing of the last beat. The last beat may has been completed
    at this point, then code would be dead in the while() below. So mask the
    while() statement and use the software delay .*/
    p->TIMSTAT |= (1U << 0U);

    // Wait timer flag to be set to ensure the completing of the last beat.

    delayMicroseconds(200);

    if (MulBeatCountRemain) {
        // Serial.printf("MulBeatCountRemain in DMA callback: %d, MulBeatDataRemain %x \n", MulBeatCountRemain,MulBeatDataRemain);
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
        for (uint32_t i = 0; i < (MulBeatCountRemain); i++) {
            value = *MulBeatDataRemain++;

            while (0 == (p->SHIFTSTAT & (1U << 0))) {
            }
            p->SHIFTBUF[0] = value & 0xFF;
            while (0 == (p->SHIFTSTAT & (1U << 0))) {
            }
            p->SHIFTBUF[0] = value >> 8;
        }
        p->TIMSTAT |= (1U << 0);
        /*Wait for transfer to be completed */
        while (0 == (p->TIMSTAT |= (1U << 0))) {
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
    if (isCB) {
        // Serial.printf("custom callback triggred \n");
        _onCompleteCB();
    }
    //  Serial.printf("DMA callback end triggred \n");
}

FASTRUN void RA8876_8080::bteMpuWriteWithROPData16_MultiBeat_DMA(ru8 s1, ru16 s1_image_width, ru16 s1_x, ru16 s1_y, ru8 des, ru16 des_image_width,
                                                                 ru16 des_x, ru16 des_y, ru16 width, ru16 height, ru8 rop_code, volatile void *data) {
    // log all params with print
    // Serial.printf("bteMpuWriteWithROPData16_MultiBeat_DMA s1_x: %u, s1_y: %u, width: %u, height: %u\n", s1_x, s1_y, width, height);

    bteMpuWriteWithROP(PAGE_TO_ADDR(s1), s1_image_width, s1_x, s1_y, PAGE_TO_ADDR(des), des_image_width, des_x, des_y, width, height, rop_code);
    CSLow();
    DCHigh();
    // Serial.printf("bteMpuWriteWithROPData16: %d pixels a 16bit\n", width*height);
    MultiBeat_ROP_DMA(data, width * height);
    // dma Callback will be called when the transfer is done and will set DC and CS
}

FASTRUN void RA8876_8080::bteMpuWriteWithROP(ru32 s1_addr, ru16 s1_image_width, ru16 s1_x, ru16 s1_y, ru32 des_addr, ru16 des_image_width, ru16 des_x, ru16 des_y, ru16 width, ru16 height, ru8 rop_code) {
    // Serial.printf("bteMpuWriteWithROP: s1_addr %d, s1_image_width %d, s1_x %d, s1_y %d, des_addr %d, des_image_width %d, des_x %d, des_y %d, width %d, height %d, rop %d\n", s1_addr, s1_image_width, s1_x, s1_y, des_addr, des_image_width, des_x, des_y, width, height, rop_code);

    check2dBusy();
    graphicMode(true);
    bte_Source1_MemoryStartAddr(s1_addr);
    bte_Source1_ImageWidth(s1_image_width);
    bte_Source1_WindowStartXY(s1_x, s1_y);
    bte_DestinationMemoryStartAddr(des_addr);
    bte_DestinationImageWidth(des_image_width);
    bte_DestinationWindowStartXY(des_x, des_y);
    bte_WindowSize(width, height);

    lcdRegDataWrite(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | RA8876_S1_COLOR_DEPTH_16BPP << 2 | RA8876_DESTINATION_COLOR_DEPTH_16BPP);  // 92h
    // lcdRegDataWrite(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_8BPP<<5|RA8876_S1_COLOR_DEPTH_8BPP<<2|RA8876_DESTINATION_COLOR_DEPTH_16BPP);//92h
    lcdRegDataWrite(RA8876_BTE_CTRL1, rop_code << 4 | RA8876_BTE_MPU_WRITE_WITH_ROP);  // 91h
    lcdRegDataWrite(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4);                         // 90h
    ramAccessPrepare();
}

void RA8876_8080::updateScreenWithROP(uint8_t src1, uint8_t src2, uint8_t dst, uint8_t ROP_CODE) {
    bteMemoryCopyWithROP(PAGE_TO_ADDR(src1), _width, 0, 0,
                         PAGE_TO_ADDR(src2), _width, 0, 0,
                         PAGE_TO_ADDR(dst), _width, 0, 0,
                         _width, _height, ROP_CODE);
}

//**************************************************************//
// Memory copy with Raster OPeration blend from two sources
// One source may be the destination, if blending with something already on the screen
//**************************************************************//
void RA8876_8080::bteMemoryCopyWithROP(ru32 s0_addr, ru16 s0_image_width, ru16 s0_x, ru16 s0_y, ru32 s1_addr, ru16 s1_image_width, ru16 s1_x, ru16 s1_y,
                                       ru32 des_addr, ru16 des_image_width, ru16 des_x, ru16 des_y, ru16 copy_width, ru16 copy_height, ru8 rop_code) {
    check2dBusy();
    // graphicMode(true);
    bte_Source0_MemoryStartAddr(s0_addr);
    bte_Source0_ImageWidth(s0_image_width);
    bte_Source0_WindowStartXY(s0_x, s0_y);
    bte_Source1_MemoryStartAddr(s1_addr);
    bte_Source1_ImageWidth(s1_image_width);
    bte_Source1_WindowStartXY(s1_x, s1_y);
    bte_DestinationMemoryStartAddr(des_addr);
    bte_DestinationImageWidth(des_image_width);
    bte_DestinationWindowStartXY(des_x, des_y);
    bte_WindowSize(copy_width, copy_height);

    lcdRegDataWrite(RA8876_BTE_CTRL1, rop_code << 4 | RA8876_BTE_MEMORY_COPY_WITH_ROP);                                                            // 91h
    lcdRegDataWrite(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | RA8876_S1_COLOR_DEPTH_16BPP << 2 | RA8876_DESTINATION_COLOR_DEPTH_16BPP);  // 92h
    lcdRegDataWrite(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4);                                                                                     // 90h
}

void RA8876_8080::bte_Source0_MemoryStartAddr(ru32 addr) {
    lcdRegDataWrite(RA8876_S0_STR0, addr);        // 93h
    lcdRegDataWrite(RA8876_S0_STR1, addr >> 8);   // 94h
    lcdRegDataWrite(RA8876_S0_STR2, addr >> 16);  // 95h
    lcdRegDataWrite(RA8876_S0_STR3, addr >> 24);  ////96h
}

void RA8876_8080::bte_Source0_ImageWidth(ru16 width) {
    lcdRegDataWrite(RA8876_S0_WTH0, width);       // 97h
    lcdRegDataWrite(RA8876_S0_WTH1, width >> 8);  // 98h
}

void RA8876_8080::bte_Source0_WindowStartXY(ru16 x0, ru16 y0) {
    lcdRegDataWrite(RA8876_S0_X0, x0);       // 99h
    lcdRegDataWrite(RA8876_S0_X1, x0 >> 8);  // 9ah
    lcdRegDataWrite(RA8876_S0_Y0, y0);       // 9bh
    lcdRegDataWrite(RA8876_S0_Y1, y0 >> 8);  // 9ch
}

void RA8876_8080::bte_Source1_MemoryStartAddr(ru32 addr) {
    lcdRegDataWrite(RA8876_S1_STR0, addr);        // 9dh
    lcdRegDataWrite(RA8876_S1_STR1, addr >> 8);   // 9eh
    lcdRegDataWrite(RA8876_S1_STR2, addr >> 16);  // 9fh
    lcdRegDataWrite(RA8876_S1_STR3, addr >> 24);  // a0h
}

void RA8876_8080::bte_Source1_ImageWidth(ru16 width) {
    lcdRegDataWrite(RA8876_S1_WTH0, width);       // a1h
    lcdRegDataWrite(RA8876_S1_WTH1, width >> 8);  // a2h
}

void RA8876_8080::bte_Source1_WindowStartXY(ru16 x0, ru16 y0) {
    lcdRegDataWrite(RA8876_S1_X0, x0);       // a3h
    lcdRegDataWrite(RA8876_S1_X1, x0 >> 8);  // a4h
    lcdRegDataWrite(RA8876_S1_Y0, y0);       // a5h
    lcdRegDataWrite(RA8876_S1_Y1, y0 >> 8);  // a6h
}

void RA8876_8080::bte_DestinationMemoryStartAddr(ru32 addr) {
    lcdRegDataWrite(RA8876_DT_STR0, addr);        // a7h
    lcdRegDataWrite(RA8876_DT_STR1, addr >> 8);   // a8h
    lcdRegDataWrite(RA8876_DT_STR2, addr >> 16);  // a9h
    lcdRegDataWrite(RA8876_DT_STR3, addr >> 24);  // aah
}

void RA8876_8080::bte_DestinationImageWidth(ru16 width) {
    lcdRegDataWrite(RA8876_DT_WTH0, width);       // abh
    lcdRegDataWrite(RA8876_DT_WTH1, width >> 8);  // ach
}

void RA8876_8080::bte_DestinationWindowStartXY(ru16 x0, ru16 y0) {
    lcdRegDataWrite(RA8876_DT_X0, x0);       // adh
    lcdRegDataWrite(RA8876_DT_X1, x0 >> 8);  // aeh
    lcdRegDataWrite(RA8876_DT_Y0, y0);       // afh
    lcdRegDataWrite(RA8876_DT_Y1, y0 >> 8);  // b0h
}

void RA8876_8080::bte_WindowSize(ru16 width, ru16 height) {
    lcdRegDataWrite(RA8876_BTE_WTH0, width);        // b1h
    lcdRegDataWrite(RA8876_BTE_WTH1, width >> 8);   // b2h
    lcdRegDataWrite(RA8876_BTE_HIG0, height);       // b3h
    lcdRegDataWrite(RA8876_BTE_HIG1, height >> 8);  // b4h
}

boolean RA8876_8080::ra8876Initialize() {
    // Init PLL
    if (!ra8876PllInitial()) {
        return false;
    }
    // Init SDRAM
    if (!ra8876SdramInitial()) {
        return false;
    }

    lcdRegWrite(RA8876_CCR);  // 01h
    if (BUS_WIDTH == 16) {
        lcdDataWrite(RA8876_PLL_ENABLE << 7 | RA8876_WAIT_NO_MASK << 6 | RA8876_KEY_SCAN_DISABLE << 5 | RA8876_TFT_OUTPUT24 << 3 | RA8876_I2C_MASTER_DISABLE << 2 | RA8876_SERIAL_IF_DISABLE << 1 | RA8876_HOST_DATA_BUS_16BIT);
    } else {
        lcdDataWrite(RA8876_PLL_ENABLE << 7 | RA8876_WAIT_NO_MASK << 6 | RA8876_KEY_SCAN_DISABLE << 5 | RA8876_TFT_OUTPUT24 << 3 | RA8876_I2C_MASTER_DISABLE << 2 | RA8876_SERIAL_IF_DISABLE << 1 | RA8876_HOST_DATA_BUS_8BIT);
    }
    lcdRegWrite(RA8876_MACR);  // 02h
    lcdDataWrite(RA8876_DIRECT_WRITE << 6 | RA8876_READ_MEMORY_LRTB << 4 | RA8876_WRITE_MEMORY_LRTB << 1);

    lcdRegWrite(RA8876_ICR);  // 03h
    lcdDataWrite(RA8877_LVDS_FORMAT << 3 | RA8876_GRAPHIC_MODE << 2 | RA8876_MEMORY_SELECT_IMAGE);

    lcdRegWrite(RA8876_MPWCTR);  // 10h
    lcdDataWrite(RA8876_PIP1_WINDOW_DISABLE << 7 | RA8876_PIP2_WINDOW_DISABLE << 6 | RA8876_SELECT_CONFIG_PIP1 << 4 | RA8876_IMAGE_COLOCR_DEPTH_16BPP << 2 | TFT_MODE);

    lcdRegWrite(RA8876_PIPCDEP);  // 11h
    lcdDataWrite(RA8876_PIP1_COLOR_DEPTH_16BPP << 2 | RA8876_PIP2_COLOR_DEPTH_16BPP);

    lcdRegWrite(RA8876_AW_COLOR);  // 5Eh
    lcdDataWrite(RA8876_CANVAS_BLOCK_MODE << 2 | RA8876_CANVAS_COLOR_DEPTH_16BPP);

    lcdRegDataWrite(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | RA8876_S1_COLOR_DEPTH_16BPP << 2 | RA8876_S0_COLOR_DEPTH_16BPP);  // 92h

    /*TFT timing configure*/
    lcdRegWrite(RA8876_DPCR);  // 12h
    lcdDataWrite(XPCLK_INV << 7 | RA8876_DISPLAY_OFF << 6 | RA8876_OUTPUT_RGB);

    /* TFT timing configure */
    lcdRegWrite(RA8876_DPCR);  // 12h
    lcdDataWrite(XPCLK_INV << 7 | RA8876_DISPLAY_OFF << 6 | RA8876_OUTPUT_RGB);

    lcdRegWrite(RA8876_PCSR);  // 13h
    lcdDataWrite(XHSYNC_INV << 7 | XVSYNC_INV << 6 | XDE_INV << 5);

    lcdHorizontalWidthVerticalHeight(HDW, VDH);
    // Serial.printf("HDW: %u, VDH: %u\n", HDW, VDH);
    lcdHorizontalNonDisplay(HND);
    lcdHsyncStartPosition(HST);
    lcdHsyncPulseWidth(HPW);
    lcdVerticalNonDisplay(VND);
    lcdVsyncStartPosition(VST);
    lcdVsyncPulseWidth(VPW);

    displayOn(true);                                                                                               // Turn on TFT display
    lcdRegDataWrite(RA8876_ICR, RA8877_LVDS_FORMAT << 3 | RA8876_GRAPHIC_MODE << 2 | RA8876_MEMORY_SELECT_IMAGE);  // 03h  //switch to graphic mode - always

    // Init Global Variables
    _width    = SCREEN_WIDTH;
    _height   = SCREEN_HEIGHT;
    _rotation = 0;

    selectScreen(PAGE_TO_ADDR(1));  // back to page 1 screen

    Serial.println("RA8876 Initialized");

    return true;
}

void RA8876_8080::lcdRegWrite(ru8 reg) {
    while (WR_DMATransferDone == false) {
    }  // Wait for any DMA transfers to complete
    FlexIO_Config_SnglBeat();
    CSLow();
    DCLow();
    /* Write command index */
    p->SHIFTBUF[0] = reg;
    /*Wait for transfer to be completed */
    while (0 == (p->SHIFTSTAT & (1 << 0))) {
    }
    while (0 == (p->TIMSTAT & (1 << 0))) {
    }
    /* De-assert RS pin */
    DCHigh();
    CSHigh();
}

void RA8876_8080::lcdDataWrite(ru8 data) {
    while (WR_DMATransferDone == false) {
    }  // Wait for any DMA transfers to complete
    FlexIO_Config_SnglBeat();
    CSLow();
    DCHigh();
    /* Write command index */
    p->SHIFTBUF[0] = data;
    // Serial.printf("Data 0x%x SHIFTBUF 0x%x\n", data, p->SHIFTBUF[0]);
    /*Wait for transfer to be completed */
    while (0 == (p->SHIFTSTAT & (1 << 0))) {
    }
    while (0 == (p->TIMSTAT & (1 << 0))) {
    }
    /* De-assert /CS pin */
    CSHigh();
}

ru8 RA8876_8080::lcdStatusRead() {
    while (WR_DMATransferDone == false) {
    }  // Wait for any DMA transfers to complete
    CSLow();
    /* De-assert RS pin */
    DCLow();
    FlexIO_Config_SnglBeat_Read();
    uint16_t dummy;
    uint16_t data = 0;
    while (0 == (p->SHIFTSTAT & (1 << 3))) {
    }
    dummy = p->SHIFTBUFBYS[3];
    while (0 == (p->SHIFTSTAT & (1 << 3))) {
    }
    data = p->SHIFTBUFBYS[3];
    DCHigh();
    CSHigh();
    //  Serial.printf("Dummy 0x%x, data 0x%x\n", dummy, data);
    // Set FlexIO back to Write mode
    FlexIO_Config_SnglBeat();
    if (BUS_WIDTH == 16)
        return data >> 8;
    else
        return data;
}

ru8 RA8876_8080::lcdDataRead() {
    while (WR_DMATransferDone == false) {
    }  // Wait for any DMA transfers to complete
    CSLow();
    /* De-assert RS pin */
    DCHigh();
    FlexIO_Config_SnglBeat_Read();
    uint16_t dummy;
    uint16_t data = 0;
    while (0 == (p->SHIFTSTAT & (1 << 3))) {
    }
    dummy = p->SHIFTBUFBYS[3];
    while (0 == (p->SHIFTSTAT & (1 << 3))) {
    }
    data = p->SHIFTBUFBYS[3];
    // Set FlexIO back to Write mode
    FlexIO_Config_SnglBeat();
    CSHigh();
    // Serial.printf("1st read(dummy) 0x%4.4x, 2nd read(data) 0x%4.4x\n", dummy, data);

    if (BUS_WIDTH == 8) {
        return data;
    } else {
        return dummy = (data >> 8) | (data & 0xff);  // High byte to low byte and mask.
    }
}

ru8 RA8876_8080::lcdRegDataRead(ru8 reg) {
    lcdRegWrite(reg);
    return lcdDataRead();
}

void RA8876_8080::lcdRegDataWrite(ru8 reg, ru8 data) {
    lcdRegWrite(reg);
    (BUS_WIDTH == 8) ? lcdDataWrite(data) : lcdDataWrite16(data);
}

void RA8876_8080::lcdDataWrite16(uint16_t data) {
    while (WR_DMATransferDone == false) {
    }  // Wait for any DMA transfers to complete
    FlexIO_Config_SnglBeat();
    CSLow();
    DCHigh();
    p->SHIFTBUF[0] = data;
    /*Wait for transfer to be completed */
    while (0 == (p->SHIFTSTAT & (1 << 0))) {
    }
    while (0 == (p->TIMSTAT & (1 << 0))) {
    }
    /* De-assert /CS pin */
    CSHigh();
}

void RA8876_8080::check2dBusy(void) {
    WAIT();
}

boolean RA8876_8080::checkIcReady(void) {
    ru32 i;
    for (i = 0; i < 1000000; i++)  // Please according to your usage to modify i value.
    {
        delayMicroseconds(1);
        if ((lcdStatusRead() & 0x02) == 0x00) {
            return true;
        }
    }
    return false;
}

boolean RA8876_8080::checkSdramReady(void) {
    ru32 i;
    for (i = 0; i < 1000000; i++)  // Please according to your usage to modify i value.
    {
        delayMicroseconds(1);
        if ((lcdStatusRead() & 0x04) == 0x04) {
            return true;
        }
    }
    return false;
}

boolean RA8876_8080::ra8876PllInitial(void) {
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
    // ru16 x_Divide,PLLC1,PLLC2;
    // ru16 pll_m_lo, pll_m_hi;
    // ru8 temp;

    // Set tft output pixel clock
    if (SCAN_FREQ >= 79)  //&&(SCAN_FREQ<=100))
    {
        lcdRegDataWrite(0x05, 0x04);  // PLL Divided by 4
        lcdRegDataWrite(0x06, (SCAN_FREQ * 4 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ >= 63) && (SCAN_FREQ <= 78)) {
        lcdRegDataWrite(0x05, 0x05);  // PLL Divided by 4
        lcdRegDataWrite(0x06, (SCAN_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ >= 40) && (SCAN_FREQ <= 62)) {
        lcdRegDataWrite(0x05, 0x06);  // PLL Divided by 8
        lcdRegDataWrite(0x06, (SCAN_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ >= 32) && (SCAN_FREQ <= 39)) {
        lcdRegDataWrite(0x05, 0x07);  // PLL Divided by 8
        lcdRegDataWrite(0x06, (SCAN_FREQ * 16 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ >= 16) && (SCAN_FREQ <= 31)) {
        lcdRegDataWrite(0x05, 0x16);  // PLL Divided by 16
        lcdRegDataWrite(0x06, (SCAN_FREQ * 16 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ >= 8) && (SCAN_FREQ <= 15)) {
        lcdRegDataWrite(0x05, 0x26);  // PLL Divided by 32
        lcdRegDataWrite(0x06, (SCAN_FREQ * 32 / OSC_FREQ) - 1);
    } else if ((SCAN_FREQ > 0) && (SCAN_FREQ <= 7)) {
        lcdRegDataWrite(0x05, 0x36);  // PLL Divided by 64
        //			lcdRegDataWrite(0x06,(SCAN_FREQ*64/OSC_FREQ)-1);
        lcdRegDataWrite(0x06, ((SCAN_FREQ * 64 / OSC_FREQ) - 1) & 0x3f);
    }
    // Set internal Buffer Ram clock
    if (DRAM_FREQ >= 158)  //
    {
        lcdRegDataWrite(0x07, 0x02);  // PLL Divided by 4
        lcdRegDataWrite(0x08, (DRAM_FREQ * 2 / OSC_FREQ) - 1);
    } else if ((DRAM_FREQ >= 125) && (DRAM_FREQ <= 157)) {
        lcdRegDataWrite(0x07, 0x03);  // PLL Divided by 4
        lcdRegDataWrite(0x08, (DRAM_FREQ * 4 / OSC_FREQ) - 1);
    } else if ((DRAM_FREQ >= 79) && (DRAM_FREQ <= 124)) {
        lcdRegDataWrite(0x07, 0x04);  // PLL Divided by 4
        lcdRegDataWrite(0x08, (DRAM_FREQ * 4 / OSC_FREQ) - 1);
    } else if ((DRAM_FREQ >= 63) && (DRAM_FREQ <= 78)) {
        lcdRegDataWrite(0x07, 0x05);  // PLL Divided by 4
        lcdRegDataWrite(0x08, (DRAM_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((DRAM_FREQ >= 40) && (DRAM_FREQ <= 62)) {
        lcdRegDataWrite(0x07, 0x06);  // PLL Divided by 8
        lcdRegDataWrite(0x08, (DRAM_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((DRAM_FREQ >= 32) && (DRAM_FREQ <= 39)) {
        lcdRegDataWrite(0x07, 0x07);  // PLL Divided by 16
        lcdRegDataWrite(0x08, (DRAM_FREQ * 16 / OSC_FREQ) - 1);
    } else if (DRAM_FREQ <= 31) {
        lcdRegDataWrite(0x07, 0x06);                     // PLL Divided by 8
        lcdRegDataWrite(0x08, (30 * 8 / OSC_FREQ) - 1);  // set to 30MHz if out off range
    }
    // Set Core clock
    if (CORE_FREQ >= 158) {
        lcdRegDataWrite(0x09, 0x02);  // PLL Divided by 2
        lcdRegDataWrite(0x0A, (CORE_FREQ * 2 / OSC_FREQ) - 1);
    } else if ((CORE_FREQ >= 125) && (CORE_FREQ <= 157)) {
        lcdRegDataWrite(0x09, 0x03);  // PLL Divided by 4
        lcdRegDataWrite(0x0A, (CORE_FREQ * 4 / OSC_FREQ) - 1);
    } else if ((CORE_FREQ >= 79) && (CORE_FREQ <= 124)) {
        lcdRegDataWrite(0x09, 0x04);  // PLL Divided by 4
        lcdRegDataWrite(0x0A, (CORE_FREQ * 4 / OSC_FREQ) - 1);
    } else if ((CORE_FREQ >= 63) && (CORE_FREQ <= 78)) {
        lcdRegDataWrite(0x09, 0x05);  // PLL Divided by 8
        lcdRegDataWrite(0x0A, (CORE_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((CORE_FREQ >= 40) && (CORE_FREQ <= 62)) {
        lcdRegDataWrite(0x09, 0x06);  // PLL Divided by 8
        lcdRegDataWrite(0x0A, (CORE_FREQ * 8 / OSC_FREQ) - 1);
    } else if ((CORE_FREQ >= 32) && (CORE_FREQ <= 39)) {
        lcdRegDataWrite(0x09, 0x06);  // PLL Divided by 8
        lcdRegDataWrite(0x0A, (CORE_FREQ * 8 / OSC_FREQ) - 1);
    } else if (CORE_FREQ <= 31) {
        lcdRegDataWrite(0x09, 0x06);                     // PLL Divided by 8
        lcdRegDataWrite(0x0A, (30 * 8 / OSC_FREQ) - 1);  // set to 30MHz if out off range
    }

    delay(1);
    lcdRegWrite(0x01);
    delay(2);
    lcdDataWrite(0x80);
    delay(2);  // wait for pll stable
    if ((lcdDataRead() & 0x80) == 0x80)
        return true;
    else
        return false;
}

boolean RA8876_8080::ra8876SdramInitial(void) {
    ru8 CAS_Latency;
    ru16 Auto_Refresh;

#ifdef IS42SM16160D
    if (DRAM_FREQ <= 133)
        CAS_Latency = 2;
    else
        CAS_Latency = 3;

    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (8192);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0xf9);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x09);
#endif

#ifdef IS42S16320B
    if (DRAM_FREQ <= 133)
        CAS_Latency = 2;
    else
        CAS_Latency = 3;

    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (8192);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x32);
    lcdRegDataWrite(0xe1, CAS_Latency);
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x09);
#endif

#ifdef IS42S16400F
    if (DRAM_FREQ < 143)
        CAS_Latency = 2;
    else
        CAS_Latency = 3;

    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x28);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef M12L32162A
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x08);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x09);
#endif

#ifdef M12L2561616A
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (8192);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x31);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef M12L64164A
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x28);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x09);
#endif

#ifdef W9825G6JH
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x31);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef W9812G6JH
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x29);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef MT48LC4M16A
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x28);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef K4S641632N
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x28);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef K4S281632K
    CAS_Latency = 3;
    Auto_Refresh = (64 * DRAM_FREQ * 1000) / (4096);
    Auto_Refresh = Auto_Refresh - 2;
    lcdRegDataWrite(0xe0, 0x29);
    lcdRegDataWrite(0xe1, CAS_Latency);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, Auto_Refresh);
    lcdRegDataWrite(0xe3, Auto_Refresh >> 8);
    lcdRegDataWrite(0xe4, 0x01);
#endif

#ifdef INTERNAL_LT7683
    lcdRegDataWrite(0xe0, 0x29);
    lcdRegDataWrite(0xe1, 0x03);  // CAS:2=0x02，CAS:3=0x03
    lcdRegDataWrite(0xe2, 0x1A);
    lcdRegDataWrite(0xe3, 0x06);
    lcdRegDataWrite(0xe4, 0x01);
#endif
    return checkSdramReady();
}

void RA8876_8080::displayOn(boolean on) {
    unsigned char temp;

    temp = lcdRegDataRead(RA8876_DPCR) & (1 << 3);

    if (on)
        lcdRegDataWrite(RA8876_DPCR, XPCLK_INV << 7 | RA8876_DISPLAY_ON << 6 | RA8876_OUTPUT_RGB | temp);
    else
        lcdRegDataWrite(RA8876_DPCR, XPCLK_INV << 7 | RA8876_DISPLAY_OFF << 6 | RA8876_OUTPUT_RGB | temp);

    delay(20);
}

void RA8876_8080::backlight(boolean on) {
    if (on) {
        pwm_Prescaler(RA8876_PRESCALER);  // if core_freq = 120MHz, pwm base clock = 120/(3+1) = 30MHz
        pwm_ClockMuxReg(RA8876_PWM_TIMER_DIV4, RA8876_PWM_TIMER_DIV4, RA8876_XPWM1_OUTPUT_PWM_TIMER1, RA8876_XPWM0_OUTPUT_PWM_TIMER0);
        // pwm timer clock = 30/4 = 7.5MHz
        pwm0_ClocksPerPeriod(256);  // pwm0 = 7.5MHz/1024 = 7.3KHz
        pwm0_Duty(10);              // pwm0 set 10/1024 duty

        pwm1_ClocksPerPeriod(256);  // pwm1 = 7.5MHz/256 = 29.2KHz
        pwm1_Duty(5);               // pwm1 set 5/256 duty

        pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_ON, RA8876_PWM_TIMER1_AUTO_RELOAD, RA8876_PWM_TIMER1_START, RA8876_PWM_TIMER0_DEAD_ZONE_DISABLE, RA8876_PWM_TIMER0_INVERTER_ON, RA8876_PWM_TIMER0_AUTO_RELOAD, RA8876_PWM_TIMER0_START);

    } else {
        pwm_Configuration(RA8876_PWM_TIMER1_INVERTER_OFF, RA8876_PWM_TIMER1_AUTO_RELOAD, RA8876_PWM_TIMER1_STOP,
                          RA8876_PWM_TIMER0_DEAD_ZONE_ENABLE, RA8876_PWM_TIMER1_INVERTER_OFF,
                          RA8876_PWM_TIMER0_AUTO_RELOAD, RA8876_PWM_TIMER0_STOP);
    }
}

void RA8876_8080::setBrightness(ru8 brightness) {
    pwm0_Duty(0x00FF - brightness);
}

void RA8876_8080::pwm_Prescaler(ru8 prescaler) {
    lcdRegDataWrite(RA8876_PSCLR, prescaler);  // 84h
}

void RA8876_8080::pwm0_Duty(ru16 duty) {
    lcdRegDataWrite(RA8876_TCMPB0L, duty);       // 88h
    lcdRegDataWrite(RA8876_TCMPB0H, duty >> 8);  // 89h
}

void RA8876_8080::pwm1_Duty(ru16 duty) {
    lcdRegDataWrite(RA8876_TCMPB1L, duty);       // 8ch
    lcdRegDataWrite(RA8876_TCMPB1H, duty >> 8);  // 8dh
}

void RA8876_8080::pwm0_ClocksPerPeriod(ru16 clocks_per_period) {
    lcdRegDataWrite(RA8876_TCNTB0L, clocks_per_period);       // 8ah
    lcdRegDataWrite(RA8876_TCNTB0H, clocks_per_period >> 8);  // 8bh
}

void RA8876_8080::pwm1_ClocksPerPeriod(ru16 clocks_per_period) {
    lcdRegDataWrite(RA8876_TCNTB1L, clocks_per_period);       // 8eh
    lcdRegDataWrite(RA8876_TCNTB1F, clocks_per_period >> 8);  // 8fh
}

void RA8876_8080::pwm_ClockMuxReg(ru8 pwm1_clk_div, ru8 pwm0_clk_div, ru8 xpwm1_ctrl, ru8 xpwm0_ctrl) {
    lcdRegDataWrite(RA8876_PMUXR, pwm1_clk_div << 6 | pwm0_clk_div << 4 | xpwm1_ctrl << 2 | xpwm0_ctrl);  // 85h
}

void RA8876_8080::pwm_Configuration(ru8 pwm1_inverter, ru8 pwm1_auto_reload, ru8 pwm1_start, ru8 pwm0_dead_zone, ru8 pwm0_inverter, ru8 pwm0_auto_reload, ru8 pwm0_start) {
    lcdRegDataWrite(RA8876_PCFGR, pwm1_inverter << 6 | pwm1_auto_reload << 5 | pwm1_start << 4 | pwm0_dead_zone << 3 |
                                      pwm0_inverter << 2 | pwm0_auto_reload << 1 | pwm0_start);  // 86h
}

void RA8876_8080::lcdHorizontalWidthVerticalHeight(ru16 width, ru16 height) {
    unsigned char temp;
    temp = (width / 8) - 1;
    lcdRegDataWrite(RA8876_HDWR, temp);
    temp = width % 8;
    lcdRegDataWrite(RA8876_HDWFTR, temp);
    temp = height - 1;
    lcdRegDataWrite(RA8876_VDHR0, temp);
    temp = (height - 1) >> 8;
    lcdRegDataWrite(RA8876_VDHR1, temp);
}

void RA8876_8080::lcdHorizontalNonDisplay(ru16 numbers) {
    ru8 temp;
    if (numbers < 8) {
        lcdRegDataWrite(RA8876_HNDR, 0x00);
        lcdRegDataWrite(RA8876_HNDFTR, numbers);
    } else {
        temp = (numbers / 8) - 1;
        lcdRegDataWrite(RA8876_HNDR, temp);
        temp = numbers % 8;
        lcdRegDataWrite(RA8876_HNDFTR, temp);
    }
}

void RA8876_8080::lcdHsyncStartPosition(ru16 numbers) {
    ru8 temp;
    if (numbers < 8) {
        lcdRegDataWrite(RA8876_HSTR, 0x00);
    } else {
        temp = (numbers / 8) - 1;
        lcdRegDataWrite(RA8876_HSTR, temp);
    }
}

void RA8876_8080::lcdHsyncPulseWidth(ru16 numbers) {
    ru8 temp;
    if (numbers < 8) {
        lcdRegDataWrite(RA8876_HPWR, 0x00);
    } else {
        temp = (numbers / 8) - 1;
        lcdRegDataWrite(RA8876_HPWR, temp);
    }
}

void RA8876_8080::lcdVerticalNonDisplay(ru16 numbers) {
    ru8 temp;
    temp = numbers - 1;
    lcdRegDataWrite(RA8876_VNDR0, temp);
    lcdRegDataWrite(RA8876_VNDR1, temp >> 8);
}

void RA8876_8080::lcdVsyncStartPosition(ru16 numbers) {
    ru8 temp;
    temp = numbers - 1;
    lcdRegDataWrite(RA8876_VSTR, temp);
}

void RA8876_8080::lcdVsyncPulseWidth(ru16 numbers) {
    ru8 temp;
    temp = numbers - 1;
    lcdRegDataWrite(RA8876_VPWR, temp);
}

void RA8876_8080::displayImageStartAddress(ru32 addr) {
    lcdRegDataWrite(RA8876_MISA0, addr);        // 20h
    lcdRegDataWrite(RA8876_MISA1, addr >> 8);   // 21h
    lcdRegDataWrite(RA8876_MISA2, addr >> 16);  // 22h
    lcdRegDataWrite(RA8876_MISA3, addr >> 24);  // 23h
}

void RA8876_8080::displayImageWidth(ru16 width) {
    lcdRegDataWrite(RA8876_MIW0, width);       // 24h
    lcdRegDataWrite(RA8876_MIW1, width >> 8);  // 25h
}

void RA8876_8080::displayWindowStartXY(ru16 x0, ru16 y0) {
    lcdRegDataWrite(RA8876_MWULX0, x0);       // 26h
    lcdRegDataWrite(RA8876_MWULX1, x0 >> 8);  // 27h
    lcdRegDataWrite(RA8876_MWULY0, y0);       // 28h
    lcdRegDataWrite(RA8876_MWULY1, y0 >> 8);  // 29h
}

void RA8876_8080::canvasImageStartAddress(ru32 addr) {
    lcdRegDataWrite(RA8876_CVSSA0, addr);        // 50h
    lcdRegDataWrite(RA8876_CVSSA1, addr >> 8);   // 51h
    lcdRegDataWrite(RA8876_CVSSA2, addr >> 16);  // 52h
    lcdRegDataWrite(RA8876_CVSSA3, addr >> 24);  // 53h
}

void RA8876_8080::canvasImageWidth(ru16 width) {
    lcdRegDataWrite(RA8876_CVS_IMWTH0, width);       // 54h
    lcdRegDataWrite(RA8876_CVS_IMWTH1, width >> 8);  // 55h
}

void RA8876_8080::activeWindowXY(ru16 x0, ru16 y0) {
    lcdRegDataWrite(RA8876_AWUL_X0, x0);       // 56h
    lcdRegDataWrite(RA8876_AWUL_X1, x0 >> 8);  // 57h
    lcdRegDataWrite(RA8876_AWUL_Y0, y0);       // 58h
    lcdRegDataWrite(RA8876_AWUL_Y1, y0 >> 8);  // 59h
}

void RA8876_8080::activeWindowWH(ru16 width, ru16 height) {
    lcdRegDataWrite(RA8876_AW_WTH0, width);       // 5ah
    lcdRegDataWrite(RA8876_AW_WTH1, width >> 8);  // 5bh
    lcdRegDataWrite(RA8876_AW_HT0, height);       // 5ch
    lcdRegDataWrite(RA8876_AW_HT1, height >> 8);  // 5dh
}

void RA8876_8080::ramAccessPrepare(void) {
    // check2dBusy();
    // checkWriteFifoNotFull();
    // checkWriteFifoEmpty();
    lcdRegWrite(RA8876_MRWDP);  // 04h
    // checkWriteFifoNotFull();
}

void RA8876_8080::VSCAN_T_to_B(void) {
    unsigned char temp;
    temp = lcdRegDataRead(RA8876_DPCR);
    temp &= cClrb3;
    lcdRegDataWrite(RA8876_DPCR, temp);
}

void RA8876_8080::VSCAN_B_to_T(void) {
    unsigned char temp, temp_in;
    temp_in = temp = lcdRegDataRead(RA8876_DPCR);
    temp |= cSetb3;
    lcdRegDataWrite(RA8876_DPCR, temp);
}

void RA8876_8080::graphicMode(boolean on) {
    // in our case we don't need to switch between graphic and text mode. We are always in graphic mode. If you need text - use lvgl
    return;  

    // here's the code should you still need it.
    if (on)
        lcdRegDataWrite(RA8876_ICR, RA8877_LVDS_FORMAT << 3 | RA8876_GRAPHIC_MODE << 2 | RA8876_MEMORY_SELECT_IMAGE);  // 03h  //switch to graphic mode
    else
        lcdRegDataWrite(RA8876_ICR, RA8877_LVDS_FORMAT << 3 | RA8876_TEXT_MODE << 2 | RA8876_MEMORY_SELECT_IMAGE);  // 03h  //switch back to text mode
}

void RA8876_8080::Color_Bar_ON(void) {
    unsigned char temp;
    temp = lcdRegDataRead(RA8876_DPCR);  // 0x12
    temp |= cSetb5;
    lcdRegDataWrite(RA8876_DPCR, temp);  // 0x12
}

void RA8876_8080::Color_Bar_OFF(void) {
    unsigned char temp;
    temp = lcdRegDataRead(RA8876_DPCR);  // 0x12
    temp &= cClrb5;
    lcdRegDataWrite(RA8876_DPCR, temp);  // 0x12
}

void RA8876_8080::setRotation(uint8_t rotation) {
    _rotation = rotation & 0x3;
    uint8_t macr_settings;

    switch (_rotation) {
        case 0:
            _width = SCREEN_WIDTH;
            _height = SCREEN_HEIGHT;
            VSCAN_T_to_B();
            macr_settings = RA8876_DIRECT_WRITE << 6 | RA8876_READ_MEMORY_LRTB << 4 | RA8876_WRITE_MEMORY_LRTB << 1;
            break;
        case 1:
            _width = SCREEN_HEIGHT;
            _height = SCREEN_WIDTH;
            VSCAN_B_to_T();
            macr_settings = RA8876_DIRECT_WRITE << 6 | RA8876_READ_MEMORY_LRTB << 4 | RA8876_WRITE_MEMORY_RLTB << 1;
            break;
        case 2:
            _width = SCREEN_WIDTH;
            _height = SCREEN_HEIGHT;
            VSCAN_B_to_T();
            macr_settings = RA8876_DIRECT_WRITE << 6 | RA8876_READ_MEMORY_LRTB << 4 | RA8876_WRITE_MEMORY_RLTB << 1;
            break;
        case 3:
            _width = SCREEN_HEIGHT;
            _height = SCREEN_WIDTH;
            VSCAN_T_to_B();
            macr_settings = RA8876_DIRECT_WRITE << 6 | RA8876_READ_MEMORY_LRTB << 4 | RA8876_WRITE_MEMORY_LRTB << 1;
            // VSCAN_T_to_B();
            // macr_settings = RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_BTLR<<1;
            break;
    }
    lcdRegWrite(RA8876_MACR);  // 02h
    lcdDataWrite(macr_settings);

    canvasImageWidth(_width);
    activeWindowXY(0, 0);
    activeWindowWH(_width, _height);
}

void RA8876_8080::selectScreen(unsigned long screenPage) {
    check2dBusy();
    displayImageStartAddress(screenPage);
    displayImageWidth(_width);
    displayWindowStartXY(0, 0);
    canvasImageStartAddress(screenPage);
    canvasImageWidth(_width);
    activeWindowXY(0, 0);
    activeWindowWH(_width, _height);
}

void RA8876_8080::useCanvas(uint8_t page) {
    canvasImageStartAddress(PAGE_TO_ADDR(page));
    canvasImageWidth(_width);
    activeWindowXY(0, 0);
    activeWindowWH(_width, _height);
    check2dBusy();
    ramAccessPrepare();
}

// SOME PRIMITIVE DRAWING FUNCTIONS OF THE RA8876 ARE STILL IN
void RA8876_8080::fillScreen(uint16_t color) {
    fillRect(0, 0, _width - 1, _height - 1, color);
    check2dBusy();  // must wait for fill to finish before setting foreground color
}

void RA8876_8080::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    int16_t x_end = x + w - 1;
    int16_t y_end = y + h - 1;

    check2dBusy();
    graphicMode(true);
    foreGroundColor16bpp(color);

    switch (_rotation) {
        case 1:
            swapvals(x, y);
            swapvals(x_end, y_end);
            break;
        case 2:
            x = _width - x;
            x_end = _width - x_end;
            ;
            break;
        case 3:
            rotateCCXY(x, y);
            rotateCCXY(x_end, y_end);
            break;
    }

    lcdRegDataWrite(RA8876_DLHSR0, x);                      // 68h
    lcdRegDataWrite(RA8876_DLHSR1, x >> 8);                 // 69h
    lcdRegDataWrite(RA8876_DLVSR0, y);                      // 6ah
    lcdRegDataWrite(RA8876_DLVSR1, y >> 8);                 // 6bh
    lcdRegDataWrite(RA8876_DLHER0, x_end);                  // 6ch
    lcdRegDataWrite(RA8876_DLHER1, x_end >> 8);             // 6dh
    lcdRegDataWrite(RA8876_DLVER0, y_end);                  // 6eh
    lcdRegDataWrite(RA8876_DLVER1, y_end >> 8);             // 6fh
    lcdRegDataWrite(RA8876_DCR1, RA8876_DRAW_SQUARE_FILL);  // 76h,0xE0
}

void RA8876_8080::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    int16_t x_end = x + w - 1;
    int16_t y_end = y + h - 1;

    check2dBusy();
    graphicMode(true);
    foreGroundColor16bpp(color);
    switch (_rotation) {
        case 1:
            swapvals(x, y);
            swapvals(x_end, y_end);
            break;
        case 2:
            x = _width - x;
            x_end = _width - x_end;
            ;
            break;
        case 3:
            rotateCCXY(x, y);
            rotateCCXY(x_end, y_end);
            break;
    }

    lcdRegDataWrite(RA8876_DLHSR0, x);                 // 68h
    lcdRegDataWrite(RA8876_DLHSR1, x >> 8);            // 69h
    lcdRegDataWrite(RA8876_DLVSR0, y);                 // 6ah
    lcdRegDataWrite(RA8876_DLVSR1, y >> 8);            // 6bh
    lcdRegDataWrite(RA8876_DLHER0, x_end);             // 6ch
    lcdRegDataWrite(RA8876_DLHER1, x_end >> 8);        // 6dh
    lcdRegDataWrite(RA8876_DLVER0, y_end);             // 6eh
    lcdRegDataWrite(RA8876_DLVER1, y_end >> 8);        // 6fh
    lcdRegDataWrite(RA8876_DCR1, RA8876_DRAW_SQUARE);  // 76h,0xa0
}

void RA8876_8080::foreGroundColor16bpp(ru16 color) {
    lcdRegDataWrite(RA8876_FGCR, color >> 8);  // d2h
    lcdRegDataWrite(RA8876_FGCG, color >> 3);  // d3h
    lcdRegDataWrite(RA8876_FGCB, color << 3);  // d4h
}
