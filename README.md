This is a library and demo sketch to drive the ER-TFT0784 7.84" 1280x400 display with the RA8876 and SSD2828 bridge using

 - lvgl
 - own-implemented software rotation to put the screen in landscape mode. No rotation on lvgl needed so Squareline projects in landscape are working.
 - 8080 parallel 8-bit mode read and write.
 - 16 bit color mode.
 - using DMA. Lvgl can compute the next buffer while the current buffer is being shifted out. Nice!
 - using MultiBeat on FlexIO2 which is only available on Teensy MicroMod or your custom board.
 - with capacitive touch input from GT911 on I2C - rotated coordinates to map back to lvgl positions. It currently only logs touches but is prepared for lvgl UIs.
     -- The GT911 is typically already flashed out of factory with the correct resolution and config. You can reflash it with the GT911 library but first patch the Wire library for super large I2C buffers.
 - with a little animation to show how to use lvgl buffes with the RA8876 BTE&ROP feature.
 - uses RA8876 internal PWM for backlight control. Solder the jumpers on the ER-TFT0784 to enable internal PWM backlight control.
 - This works on a MicroMod Teensy with the ATP board. The ER-TFT0784 is connected to the ATP board with Dupont cables. Using 20 MHz is working well.

this is based on the works of:
 - https://github.com/wwatson4506/Ra8876LiteTeensy/tree/Ra8876Lite-8080-MicroMod
 
with all the optimizations, the display is very responsive and fast. The lvgl animations are smooth and the touch input is very responsive.
The demo (a red ball sliding back and forth, which changes in size) and setting 40 FPS in lv_conf.h is running at 40 FPS with about 5 % CPU load.
Because the ball is changing size, lvgl buffer needs to sent in multiple junks when the ball is in the middle of the screen. Areas where the ball left must be updated as well.
The ball grows up to 200 pixels which is half of the screen height. This is a lot of pixels and a lot of things must be done right for a clean drawing.
Compared to SPI with DMA the same demo will run at around only 4 fps. Some lvgl ui components use animation and this is where this project shines.

The whole thread of ups and downs is here: https://github.com/wwatson4506/Ra8876LiteTeensy/issues/16

The screen can be ordered here: https://www.buydisplay.com/spi-1280x400-7-84-inch-ips-tft-lcd-module-optl-capacitive-touch
choose 5V DC, 8080 interface on pin header, add the touch panel

You can read more about the work in the library headers files.

A hookup guide is provided to show a fully working demo using the Micro Mod ATP carrier board. <img width="612" alt="image" src="https://github.com/leutholl/TFT0784_mm/assets/5789698/fba82003-5c52-41c7-8c99-f73fc9fd8190">
