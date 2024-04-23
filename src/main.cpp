//**************************************************************//
/*
 * main.cpp
 *
 * AUTHOR: leutholl
 * DATE: FEB 2024
 * VERSION: 1.0
 *
 * This is a demo to drive the ER-TFT0784 7.84" 1280x400 display with the RA8876 and SSD2828 bridge using
 * - lvgl
 * - own-implemented software rotation to put the screen in landscape mode. No rotation on lvgl needed so Squareline projects in landscape are working.
 * - 8080 parallel 8-bit mode read and write.
 * - 16 bit color mode.
 * - using DMA. Lvgl can compute the next buffer while the current buffer is being shifted out. Nice!
 * - using MultiBeat on FlexIO2 which is only available on Teensy MicroMod or your custom board.
 * - with capacitive touch input from GT911 on I2C - rotated coordinates to map back to lvgl positions. It currently only logs touches but is prepared for lvgl UIs.
 *     -- The GT911 is typically already flashed out of factory with the correct resolution and config. You can reflash it with the GT911 library but first patch the Wire library for super large I2C buffers.
 * - with a little animation to show how to use lvgl buffes with the RA8876 BTE&ROP feature.
 * - uses RA8876 internal PWM for backlight control. Solder the jumpers on the ER-TFT0784 to enable internal PWM backlight control.
 *
 * - This works on a MicroMod Teensy with the ATP board. The ER-TFT0784 is connected to the ATP board with Dupont cables. Using 20 MHz is working well.
 *
 * with all the optimizations, the display is very responsive and fast. The lvgl animations are smooth and the touch input is very responsive.
 * The demo (a red ball sliding back and forth, which changes in size) and setting 40 FPS in lv_conf.h is running at 40 FPS with about 5 % CPU load.
 * Because the ball is changing size, lvgl buffer needs to sent in multiple junks when the ball is in the middle of the screen. Areas where the ball left must be updated as well.
 * The ball grows up to 200 pixels which is half of the screen height. This is a lot of pixels and a lot of things must be done right for a clean drawing.
 * Compared to SPI with DMA the same demo will run at around only 4 fps. Some lvgl ui components use animation and this is where this project shines.
 *
 * The whole thread of ups and downs is here: https://github.com/wwatson4506/Ra8876LiteTeensy/issues/16
 *
 * You can read more about the work in the library headers files.
 *
 */
//**************************************************************//

#include <Arduino.h>
#include <Metro.h>
#include <lvgl.h>

#include "ER-TFT0784_8080.hpp"
#include "GT911.h"
#include "Wire.h"

// Touch
#define GT911_INT_PIN 29
#define GT911_RST_PIN 0
GT911 touch = GT911();

// Additional Display Pins for ER-TFT0784. The Databus is connected to the Teensy MicroMod FlexIO2 like this
/*
    Pin 10 => FlexIO2:0 WR
    Pin 12 => FlexIO2:1 RD
    Pin 40 => FlexIO2:4 D0
    Pin 41 => FlexIO2:5 |
    Pin 42 => FlexIO2:6 |
    Pin 43 => FlexIO2:7 |
    Pin 44 => FlexIO2:8 |
    Pin 45 => FlexIO2:9 |
    Pin 6 => FlexIO2:10 |
    Pin 9 => FlexIO2:11 D7

    You must not change these pins as they are hardcoded in the library and FlexIO2 needs it that way.

    You can change the other additional pins as you like here:
*/

#define _2828_CS_PIN 2
#define _2828_RST_PIN 3
#define _2828_SDI_PIN 4
#define _2828_SCK_PIN 5
#define _8876_CS_PIN 16
#define _8876_DC_PIN 11
#define _8876_RST_PIN 17
#define _8876_WAIT_PIN 26  // The library uses this pin to wait for the RA8876 to be ready instead of polling the Status Register. Connect to WINT pin.

#define SCREENWIDTH 1280
#define SCREENHEIGHT 400

ER_TFT0784_8080 TFT = ER_TFT0784_8080(_2828_CS_PIN, _2828_RST_PIN, _2828_SDI_PIN, _2828_SCK_PIN, _8876_CS_PIN, _8876_DC_PIN, _8876_RST_PIN, _8876_WAIT_PIN);

#define BUFFER_SIZE (SCREENWIDTH * SCREENHEIGHT / 10)  // About 10th of the screen is large enough. If very small, the BTE&ROP overhead is too large.

#define LVGL_DRAW_8876_CANVAS 1      // Which RA8876 page or canvas lvgl shall draw to. Some projects use multiple canvases and combine them later.
#define VISIBLE_8876_CANVAS 1        // Which RA8876 page or canvas is visible. This is the canvas that is shown on the screen. It can be different from the drawing canvas.
#define NON_LVGL_DRAW_8876_CANVAS 2  // If you have multiple canvases, you can draw to one and then combine them to the visible canvas.

// lvgl stuff
static lv_disp_draw_buf_t disp_buf;
static DMAMEM lv_color_t buf_1[BUFFER_SIZE] __aligned(32);
static DMAMEM lv_color_t buf_2[BUFFER_SIZE] __aligned(32);
static lv_disp_drv_t disp_drv;
static lv_disp_t* disp;
static lv_indev_drv_t indev_drv;

DMAMEM uint16_t rotated_image[BUFFER_SIZE] __aligned(32);

volatile bool isLastBuffer = false;

Metro lvglTaskTimer = Metro(5);  // 5ms as recommended by lvgl library

static void anim_x_cb(lv_obj_t* var, int32_t v) {
    lv_obj_set_x(var, v);
}

static void anim_size_cb(lv_obj_t* var, int32_t v) {
    lv_obj_set_size(var, v, v);
}

FASTRUN void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint16_t width = (area->x2 - area->x1 + 1);
    uint16_t height = (area->y2 - area->y1 + 1);

    // rotate the buffer image
    for (uint16_t y = 1; y <= height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            rotated_image[x * height + (height - y)] = color_p++->full;
        }
    }

    // note that XY are swapped for the RA8876 and width and height are swapped in the arguments
    uint16_t newY = area->x1;                          // swap x to y
    uint16_t newX = SCREEN_WIDTH - area->y1 - height;  // swap y to x and reposition

    TFT.bteMpuWriteWithROPData16_MultiBeat_DMA(LVGL_DRAW_8876_CANVAS, SCREEN_WIDTH, newX, newY,                 // Source 1 is ignored for ROP 12
                                               LVGL_DRAW_8876_CANVAS, SCREEN_WIDTH, newX, newY, height, width,  // destination address, pagewidth, x/y, width/height
                                               RA8876_BTE_ROP_CODE_12,
                                               rotated_image);

    // while (TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete. This makes everything synchronous and slow. It nullifies the advantage of DMA. CPU load is 5 times higher.

    isLastBuffer = lv_disp_flush_is_last(&disp_drv);
}

FASTRUN void DMAfinished() {  // this is a callback of the LCD driver when DMA is finished. It is called from the ISR. LVGL needs to know when the DMA is done.
    lv_disp_flush_ready(&disp_drv);
    // if (isLastBuffer) TFT.updateScreenWithROP(LVGL_DRAW_8876_CANVAS, NON_LVGL_DRAW_8876_CANVAS, VISIBLE_8876_CANVAS, RA8876_BTE_ROP_CODE_14); // if application uses multiple canvases (e.g. non LVGL Drawings), we combine them here after lvgl is done.
}

void setupTouch() {
    Wire.setClock(400000);
    Wire.begin();
    if (touch.begin(GT911_INT_PIN, GT911_RST_PIN) != true) {
        Serial.println("Touch Module reset failed");
    } else {
        Serial.println("Touch Module reset OK");
    }
    Serial.print("Check Touch ACK on addr request on 0x");
    Serial.print(touch.i2cAddr, HEX);
    Wire.beginTransmission(touch.i2cAddr);
    int error = Wire.endTransmission();
    if (error != 0) {
        Serial.print(": ERROR #");
        Serial.println(error);
    } else {
        Serial.println(": SUCCESS");
        // do config
        /*
         * GT911 is already flashed once - do not flash again every time
        Serial.print("Setting resolution of TP");
        uint8_t err = touch.fwResolution(1280, 400); //1280 x 400
        if (err) {
          Serial.print(", error: "); Serial.println(err, HEX);
        } else Serial.println();


        GTConfig* cfg = touch.readConfig();
        Serial.print("config Ver: 0x");    Serial.println(cfg->configVersion, HEX);
        Serial.print("xResolution: ");   Serial.println(cfg->xResolution);
        Serial.print("yResolution: ");   Serial.println(cfg->yResolution);
        Serial.print("touchNumber: ");   Serial.println(cfg->touchNumber);
        Serial.print("moduleSwitch1: 0b"); Serial.println(cfg->moduleSwitch1, BIN);
        Serial.print("moduleSwitch2: 0b"); Serial.println(cfg->moduleSwitch2, BIN);
        Serial.print("shakeCount: ");    Serial.println(cfg->shakeCount);
        Serial.print("filter: 0b");        Serial.println(cfg->filter, BIN);
        Serial.print("largeTouch: 0x");    Serial.println(cfg->largeTouch, HEX);
        Serial.print("noiseReduction: 0x"); Serial.println(cfg->noiseReduction, HEX);
        Serial.print("screenLevel touch: 0x"); Serial.println(cfg->screenLevel.touch, HEX);
        Serial.print("screenLevel leave: 0x"); Serial.println(cfg->screenLevel.leave, HEX);
        Serial.print("lowPowerControl: "); Serial.println(cfg->lowPowerControl);
        Serial.print("refreshRate: "); Serial.println(cfg->refreshRate);
        Serial.print("xThreshold: "); Serial.println(cfg->xThreshold);
        Serial.print("yThreshold: "); Serial.println(cfg->yThreshold);
        Serial.print("xSpeedLimit: "); Serial.println(cfg->xSpeedLimit);
        Serial.print("ySpeedLimit: "); Serial.println(cfg->ySpeedLimit);
        Serial.print("vSpace: "); Serial.println(cfg->vSpace);
        Serial.print("hSpace: "); Serial.println(cfg->hSpace);
        cfg->lowPowerControl = 4;
        //cfg->moduleSwitch1 = 0x0D;
        //touch.writeConfig(cfg);
        */
    }
}

void my_touchpad_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint8_t contacts = touch.touched();
    data->state = contacts ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    if (contacts >= 1) {
        if (data->state == LV_INDEV_STATE_PR) {
            // FOR 270 degrees screen rotation
            data->point.y = SCREENHEIGHT - touch.getPoint(0).y;
            data->point.x = SCREENWIDTH - touch.getPoint(0).x;
            Serial.printf("touch at %u, %u\n", data->point.x, data->point.y);
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    TFT.onCompleteCB(DMAfinished);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    delay(1000);

    // while (!Serial && millis() < 5000) {}  //wait up to 2 seconds for Serial Monitor to connect
    Serial.println("RA8876_mm 8080");
    Serial.print("Compiled ");
    Serial.print(__DATE__);
    Serial.print(" at ");
    Serial.println(__TIME__);
    Serial.println("Graphics setup");

    bool success = TFT.begin(20);  // MHz  => select one of 1,2,4,8,12,20,24,30,40,60,120 MHz  (20 MHz is working well on the MM ATP board with dupont cables)
    if (success) {
        Serial.println("TFT.begin() success");
    } else {
        Serial.println("TFT.begin() failed");
    }
    setupTouch();

    TFT.backlight(true);  // uses internal PWM to control backlight brightness
    TFT.setBrightness(30);

    // TFT.Color_Bar_ON(); while (true); // this is a test pattern to check if the display is working. It should show a color bar. If not, check your wiring.

    TFT.setRotation(3);  // 270 degrees rotation for coordinates and scanning and memory write direction on the RA8876. Note that we will must rotate the lvgl buffer.

    for (uint8_t i = 1; i < 10; i++) {
        TFT.useCanvas(i);
        TFT.fillScreen(COLOR65K_BLACK);
    }

    // screen is set up. Now proceeding with lvgl setup.
    lv_init();
    lv_disp_draw_buf_init(&disp_buf, buf_1, buf_2, BUFFER_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREENWIDTH;
    disp_drv.ver_res = SCREENHEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.antialiasing = 1;
    disp_drv.direct_mode = 0;
    disp_drv.full_refresh = 0;
    disp_drv.sw_rotate = 0;
    disp_drv.rotated = LV_DISP_ROT_NONE;
    disp = lv_disp_drv_register(&disp_drv);

    // initialize the input device driver
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // a little demo using animations
    lv_obj_t* obj = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_pos(obj, 10, 10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 100, 200);
    lv_anim_set_time(&a, 4000);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_size_cb);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_x_cb);
    lv_anim_set_values(&a, 100, 1000);
    lv_anim_start(&a);

    Serial.println("Run Mode.");
}

void loop() {
    if (lvglTaskTimer.check()) {
        lv_task_handler(); /* let the GUI do its work */
        touch.loop();      // call often. LVGL has its own timer for touch handling that checks the driver if a touch has happend which is interrupt driven.
    }
}
