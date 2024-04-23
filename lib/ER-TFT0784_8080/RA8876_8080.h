//**************************************************************//
/*
 * RA8876_8080.h
 *
 * AUTHOR: Lukas Leuthold
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
 * Modified Version of: File Name : RA8876_8080.h
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
/*File Name : tft.h
 *          : For Teensy 3.x and T4
 *          : By Warren Watson
 *          : 06/07/2018 - 11/31/2019
 *          : Copyright (c) 2017-2019 Warren Watson.
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
/***************************************************************
 *  Modified by mjs513 and KurtE and MorganS
 *  Combined libraries and added functions to be compatible with
 *  other display libraries
 *  See PJRC Forum Thread: https://forum.pjrc.com/threads/58565-RA8876LiteTeensy-For-Teensy-T36-and-T40/page5
 *
 ***************************************************************/

#include "Arduino.h"
#include "RA8876Registers_8080.h"

#ifndef _RA8876_MM
#define _RA8876_MM

#include "DMAChannel.h"
#include "FlexIO_t4.h"

#define FLEXIO2_MM_DMA 1

#define BUS_WIDTH 8 /*Available options are 8 or 16 */
#define SHIFTNUM 4  // number of shifters used (up to 8) => Multibeat only works with 4 ?!?!
#define BYTES_PER_BEAT (sizeof(uint8_t))
#define BEATS_PER_SHIFTER (sizeof(uint32_t) / BYTES_PER_BEAT)
#define BYTES_PER_BURST (sizeof(uint32_t) * SHIFTNUM)

// when using IRQ instead of DMA
#define SHIFTER_IRQ (SHIFTNUM - 1)
#define TIMER_IRQ 0
#define FLEXIO_ISR_PRIORITY 64  // interrupt is timing sensitive, so use relatively high priority (supersedes USB) //was 64

// when using DMA
#define SHIFTER_DMA_REQUEST 3  // only 0, 1, 2, 3 expected to work

#if !defined(swapvals)
#define swapvals(a, b)   \
    {                    \
        typeof(a) t = a; \
        a = b;           \
        b = t;           \
    }
#endif

class RA8876_8080 {
   public:
    typedef void (*CBF)();
    CBF _callback;
    void onCompleteCB(CBF callback);

    volatile bool WR_DMATransferDone;

    RA8876_8080(uint8_t DCp, uint8_t CSp, uint8_t RSTp, uint8_t WAITp);

    boolean begin(uint8_t baud_div);

    /* Test */
    void Color_Bar_ON(void);
    void Color_Bar_OFF(void);

    void setRotation(uint8_t rotation);

    void bteMpuWriteWithROPData16_MultiBeat_DMA(ru8 s1, ru16 s1_image_width, ru16 s1_x, ru16 s1_y, ru8 des, ru16 des_image_width,
                                                ru16 des_x, ru16 des_y, ru16 width, ru16 height, ru8 rop_code, volatile void *data);

    void updateScreenWithROP(uint8_t src1, uint8_t src2, uint8_t dst, uint8_t ROP_CODE);

    void displayOn(boolean on);
    void backlight(boolean on);
    void setBrightness(ru8 brightness);

    // SOME PRIMITIVE DRAWING FUNCTIONS OF THE RA8876 ARE STILL IN
    void fillScreen(uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    void useCanvas(uint8_t page);

   private:
    void CSLow();
    void CSHigh();
    void DCLow();
    void DCHigh();
    void WAIT();

    void gpioWrite();
    void gpioRead();

    void FlexIO_Init();
    void FlexIO_Config_SnglBeat();
    void FlexIO_Config_MultiBeat();
    void FlexIO_Config_SnglBeat_Read();
    void FlexIO_Clear_Config_SnglBeat();

    void bteMpuWriteWithROP(ru32 s1_addr, ru16 s1_image_width, ru16 s1_x, ru16 s1_y, ru32 des_addr, ru16 des_image_width, ru16 des_x, ru16 des_y, ru16 width, ru16 height, ru8 rop_code);
    void bteMemoryCopyWithROP(ru32 s0_addr, ru16 s0_image_width, ru16 s0_x, ru16 s0_y, ru32 s1_addr, ru16 s1_image_width, ru16 s1_x, ru16 s1_y,
                              ru32 des_addr, ru16 des_image_width, ru16 des_x, ru16 des_y, ru16 copy_width, ru16 copy_height, ru8 rop_code);

    void bte_Source0_MemoryStartAddr(ru32 addr);
    void bte_Source0_ImageWidth(ru16 width);
    void bte_Source0_WindowStartXY(ru16 x0, ru16 y0);

    void bte_Source1_MemoryStartAddr(ru32 addr);
    void bte_Source1_ImageWidth(ru16 width);
    void bte_Source1_WindowStartXY(ru16 x0, ru16 y0);

    void bte_DestinationMemoryStartAddr(ru32 addr);
    void bte_DestinationImageWidth(ru16 width);
    void bte_DestinationWindowStartXY(ru16 x0, ru16 y0);
    void bte_WindowSize(ru16 width, ru16 height);

    void MultiBeat_ROP_DMA(volatile void *value, uint32_t const length);

    static void dmaISR();
    void flexDma_Callback();

    void lcdRegWrite(ru8 reg);
    void lcdDataWrite(ru8 data);
    ru8 lcdStatusRead();
    ru8 lcdDataRead();
    ru8 lcdRegDataRead(ru8 reg);
    void lcdRegDataWrite(ru8 reg, ru8 data);
    void lcdDataWrite16(uint16_t data);
    void check2dBusy();
    boolean checkIcReady();
    boolean checkSdramReady();
    boolean ra8876Initialize();
    boolean ra8876PllInitial(void);
    boolean ra8876SdramInitial(void);

    void pwm_Prescaler(ru8 prescaler);
    void pwm0_Duty(ru16 duty);
    void pwm1_Duty(ru16 duty);
    void pwm_ClockMuxReg(ru8 pwm1_clk_div, ru8 pwm0_clk_div, ru8 xpwm1_ctrl, ru8 xpwm0_ctrl);
    void pwm_Configuration(ru8 pwm1_inverter, ru8 pwm1_auto_reload, ru8 pwm1_start, ru8 pwm0_dead_zone, ru8 pwm0_inverter, ru8 pwm0_auto_reload, ru8 pwm0_start);
    void pwm0_ClocksPerPeriod(ru16 clocks_per_period);
    void pwm1_ClocksPerPeriod(ru16 clocks_per_period);
    void lcdHorizontalWidthVerticalHeight(ru16 width, ru16 height);
    void lcdHorizontalNonDisplay(ru16 numbers);
    void lcdHsyncStartPosition(ru16 numbers);
    void lcdHsyncPulseWidth(ru16 numbers);
    void lcdVerticalNonDisplay(ru16 numbers);
    void lcdVsyncStartPosition(ru16 numbers);
    void lcdVsyncPulseWidth(ru16 numbers);
    void displayImageStartAddress(ru32 addr);
    void displayImageWidth(ru16 width);
    void displayWindowStartXY(ru16 x0, ru16 y0);

    void canvasImageStartAddress(ru32 addr);
    void canvasImageWidth(ru16 width);
    void activeWindowXY(ru16 x0, ru16 y0);
    void activeWindowWH(ru16 width, ru16 height);

    void ramAccessPrepare(void);
    void graphicMode(boolean on);

    void selectScreen(unsigned long screenPage);

    // rotation functions
    void MemWrite_Left_Right_Top_Down(void);
    void MemWrite_Right_Left_Top_Down(void);
    void MemWrite_Top_Down_Left_Right(void);
    void MemWrite_Down_Top_Left_Right(void);
    void VSCAN_T_to_B(void);
    void VSCAN_B_to_T(void);

    // GRAPHIC FUNCTIONS OF RA8876
    void drawSquareFill(ru16 x0, ru16 y0, ru16 x1, ru16 y1, ru16 color);
    void foreGroundColor16bpp(ru16 color);

    uint8_t _baud;
    uint8_t _dc;
    uint8_t _cs;
    uint8_t _wait;
    uint8_t _rst;

    uint32_t MulBeatCountRemain;
    uint16_t *MulBeatDataRemain;
    uint32_t TotalSize;

    uint8_t _rotation;
    int16_t _width, _height;

    bool isCB = false;
    void _onCompleteCB();
    static RA8876_8080 *dmaCallback;
    static DMAChannel flexDma;

    FlexIOHandler *pFlex;
    IMXRT_FLEXIO_t *p;
    const FlexIOHandler::FLEXIO_Hardware_t *hw;

    // BUGBUG:: two different versions as some places used signed others use unsigned...
    inline void rotateCCXY(int16_t &x, int16_t &y) {
        int16_t yt;
        yt = y;
        y = x;
        x = _height - yt;
    }

    inline void rotateCCXY(ru16 &x, ru16 &y) {
        ru16 yt;
        yt = y;
        y = x;
        x = _height - yt;
    }
};

#endif