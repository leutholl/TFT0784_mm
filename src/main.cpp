#include <Arduino.h>
#include <Metro.h>
#include "ER-TFT0784_8080.h" //TFT Driver
#include <lvgl.h>       //UI Library

//Metro blinkTimer(500);
#define SCREENWIDTH 400
#define SCREENHEIGHT 1280
#define BUFFER_SIZE (SCREENWIDTH * 128)  //recommended is *10 but this sketch has nothing else useful to do with the memory available on Teensy 4, so go large!

        static lv_disp_draw_buf_t disp_buf;
        static DMAMEM lv_color_t buf_1[BUFFER_SIZE] __aligned(32);
        static DMAMEM lv_color_t buf_2[BUFFER_SIZE] __aligned(32);
        static lv_disp_drv_t  disp_drv;
        static lv_disp_t*     disp;


ER_TFT0784_8080 TFT = ER_TFT0784_8080(2,3,4,5); //SSD pins
                  
              
//volatile uint32_t DMAMEM *rotated_image __attribute__((aligned(32))) = nullptr;

unsigned long start;
unsigned long end;

#define myTRIG 36

Metro     lvglTaskTimer       = Metro(5);        //5ms as recommended by lvgl library

static void anim_x_cb(void * var, int32_t v)
{
   lv_obj_set_y(var, v);
}

static void anim_size_cb(void * var, int32_t v)
{
   lv_obj_set_size(var, v, v);
}


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {

  
  
  uint16_t width = (area->x2 - area->x1 + 1);
  uint16_t height = (area->y2 - area->y1 + 1);
  
  /*
  TFT.bteMpuWriteWithROPData16(TFT.pageStartAddress(0), TFT.width(), area->x1, area->y1,  //Source 1 is ignored for ROP12
                         TFT.pageStartAddress(0), TFT.width(), area->x1, area->y1, width, height,     //destination address, pagewidth, x/y, width/height
                         RA8876_BTE_ROP_CODE_12, (const unsigned short)color_p);  //code 12 means overwrite
  */

  //uint16_t rotated_image[width*height];

  //rotated_image = TFT.rotateImageRect(width, height, (uint16_t*)color_p, 3);
  //TFT.writeRotatedRect(area->x1, 400-area->y1, width, height, rotated_image);
  
  
  ////// TFT.lvglRotateAndROP(area->x1, 400-area->y1-(height), width, height, (uint16_t*)color_p);
  



  
  uint16_t rotated_image[width*height];
  //uint32_t *rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 31) & ~((uintptr_t)(31)));
  for (uint32_t i = 0; i < width*height; i++) {
      rotated_image[i] = color_p->full; //color_p->full;
      color_p++;
  }

  
  

  digitalWriteFast(myTRIG, LOW);

  for (int i = 0; i < 6; i++) {
    delayMicroseconds(1);
    digitalToggleFast(myTRIG);
  }

  Serial.printf(" ==> Flush: %d,%d to %d,%d width=%d, height=%d pixels=%d at %d\n", area->x1, area->y1, area->x2, area->y2, width, height, width*height, millis());
  TFT.writeRotatedRect(area->x1, area->y1, width, height, rotated_image);


  //digitalWriteFast(myTRIG, HIGH);
  while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  //digitalWriteFast(myTRIG, LOW);
  
  
  //arm_dcache_flush_delete(rotated_colors_aligned, width*height*sizeof(uint32_t));

  /*
	for(int i = 0; i < height*width; i++) {
    //uint16_t pixel1 = *((uint16_t*)color_p);             // First 16-bit pixel
    //uint16_t pixel2 = *((uint16_t*)color_p++);       // Second 16-bit pixel
    rotated_image[i] = color_p->full;
    color_p++;
    //color_p[i] = (lv_color_to)COLOR65K_GREEN;
      //color_p++;
	}
  */

  //uint32_t *rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 32) & ~((uintptr_t)(31)));
	
  //TFT.writeRotatedRect(area->x1, area->y1, width, height, rotated_colors_aligned);
  //while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  //free(buffer);
  
  
  //Serial.printf("Flush: %d,%d to %d,%d width=%d, height=%d at %d\n", area->x1, area->y1, area->x2, area->y2, width, height, millis());

  //TFT.writeRect2(area->x1, area->y1, width, height, (uint16_t*)color_p);
  //Serial.printf("Flush: %d,%d to %d,%d at %d\n", area->x1, area->y1, area->x2, area->y2, millis());

  /*
  TFT.bteMpuWriteWithROP(TFT.pageStartAddress(9), TFT.width(), area->x1, area->y1,  //Source 1 is ignored for ROP12
                         TFT.pageStartAddress(9), TFT.width(), area->x1, area->y1, width, height,     //destination address, pagewidth, x/y, width/height
                         RA8876_BTE_ROP_CODE_12);  //code 12 means overwrite
  
  TFT.pushPixels16bitAsync(0xAA, area->x1, area->y1, area->x2, area->y2); // 16 bit color
  */
  /*
  TFT.startSend();
  SPI1.transfer(RA8876_SPI_DATAWRITE);
  TFT.activeDMA = true;
  spiDMAEvent.setContext(disp);  // Set the contxt to us
  //bool x  = SPI1.transfer(color_p, NULL, width * height, spiDMAEvent);  // 8bit color
  bool x  = SPI1.transfer(color_p, NULL, width * height * 2, spiDMAEvent);  // * 2 for 16 bit. also uncomment line in bteMpuWriteWithROP in cpp file
  if (!x) Serial.println("SPI busy");
  //check return value of transfer
  */
 
 lv_disp_flush_ready(&disp_drv);
  

}

void drawTest() {
  //TFT.Color_Bar_ON();
  //TFT.drawLine(0, 0, 2, 3, COLOR65K_RED);

  //TFT.setRotation(3); //x and y will swap as well

  //TFT.Color_Bar_ON();
  //uint16_t w = 240; //240
  //uint16_t h = 320;  //320
 
  //rotated_image = TFT.rotateImageRect(240, 320, (uint16_t*)teensy40_pinout2, 0);
  
  
  uint16_t w = 190;
  uint16_t h = 600;
  //uint16_t *rotated_image = (uint16_t *)malloc(w*h*sizeof(uint16_t));
  uint16_t rotated_image[w*h];

  for (uint32_t i = 0; i < w*h; i+=4) {
      //rotated_image[j] = (teensy40_pinout2[i] << 16) | teensy40_pinout2[i+1];
      //rotated_image[i] = teensy40_pinout2[i];
      rotated_image[i] = COLOR65K_BLACK;
      rotated_image[i+1] = COLOR65K_RED;
      rotated_image[i+2] = COLOR65K_GREEN;
      rotated_image[i+3] = COLOR65K_BLUE;
      //rotated_image[i+1] = teensy40_pinout2[i+1];
      //j = i%2 == 0 ? j+1 : j;
  }


  uint32_t *rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 32) & ~((uintptr_t)(31)));
  start = micros();
  // copy teensy40_pinout2 to rotated_image by memcopy so that the address is aligned to 32 bytes and is DMA
  //memcpy((volatile uint32_t*)rotated_image, (uint16_t*)teensy40_pinout2, w*h*sizeof(uint16_t));
  
  /*
  for (uint32_t i = 0; i < w*h; i++) {
      //rotated_image[j] = (teensy40_pinout2[i] << 16) | teensy40_pinout2[i+1];
      rotated_colors_aligned[i] = (COLOR65K_GREEN << 16) | COLOR65K_GREEN;
      //j = i%2 == 0 ? j+1 : j;
  }
  */
  

  /*
  rotated_image[w*h-1] = 0x07E0; //mark ending
  rotated_image[w*h-2] = 0x07E0; //mark ending
  rotated_image[w*h-3] = 0x07E0; //mark ending
  rotated_image[w*h-4] = 0x07E0; //mark ending

  rotated_image[0] = 0x07E0; //mark beginning
  rotated_image[1] = 0x07E0; //mark beginning
  rotated_image[2] = 0x07E0; //mark beginning
  rotated_image[3] = 0x07E0; //mark beginning
  */
  

  TFT.writeRotatedRect(10, 10, w, h, rotated_colors_aligned);
  while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  end = micros();
  Serial.printf("\n\nDMA transfer time: %lu us\n", end-start);

  

  for (uint32_t i = 0; i < w*h; i+=4) {
      //rotated_image[j] = (teensy40_pinout2[i] << 16) | teensy40_pinout2[i+1];
      //rotated_image[i] = teensy40_pinout2[i];
      rotated_image[i] = COLOR65K_BLACK;
      rotated_image[i+1] = COLOR65K_BLACK;
      rotated_image[i+2] = COLOR65K_BLACK;
      rotated_image[i+3] = COLOR65K_BLACK;
      //rotated_image[i+1] = teensy40_pinout2[i+1];
      //j = i%2 == 0 ? j+1 : j;
  }
  rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 32) & ~((uintptr_t)(31)));
  start = micros();
  TFT.writeRotatedRect(10+w, 10, w, h, rotated_colors_aligned);
  while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  end = micros();
  Serial.printf("\n\nDMA transfer time: %lu us\n", end-start);

  start = micros();
  for (uint32_t i = 0; i < w*h; i++) {
      //rotated_image[j] = (teensy40_pinout2[i] << 16) | teensy40_pinout2[i+1];
      //rotated_image[i] = teensy40_pinout2[i];
      rotated_image[i] = COLOR65K_BLUE;
      //rotated_image[i+1] = teensy40_pinout2[i+1];
      //j = i%2 == 0 ? j+1 : j;
  }
  
  rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 32) & ~((uintptr_t)(31)));
  TFT.writeRotatedRect(10, h+10, w, h, rotated_colors_aligned);
  while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  end = micros();
  Serial.printf("\n\nDMA transfer time: %lu us\n", end-start);

  start = micros();
  for (uint32_t i = 0; i < w*h; i++) {
      //rotated_image[j] = (teensy40_pinout2[i] << 16) | teensy40_pinout2[i+1];
      //rotated_image[i] = teensy40_pinout2[i];
      rotated_image[i] = COLOR65K_YELLOW;
      //rotated_image[i+1] = teensy40_pinout2[i+1];
      //j = i%2 == 0 ? j+1 : j;
  }
  
  rotated_colors_aligned = (uint32_t *)(((uintptr_t)rotated_image + 32) & ~((uintptr_t)(31)));
  TFT.writeRotatedRect(w+10, h+10, w, h, rotated_colors_aligned);
  while(TFT.WR_DMATransferDone == false) {} //Wait for any DMA transfers to complete
  end = micros();
  Serial.printf("\n\nDMA transfer time: %lu us\n", end-start);




  //for (uint32_t i = 0; i < w*h; i++) Serial.printf("img[%u] = 0x%x\n", i, rotated_image[i]);
	//TFT.lvglRotateAndROP(100,100,w,h,rotated_image);
  //TFT.pushPixels16bit(rotated_image, 100, 100, 100+w, 100+h);
  //delay(5000);
  
  //TFT.writeRotatedRect(10, 10, w, h, (volatile uint16_t*)teensy40_pinout2);
  
  //Serial.println("Image written");

  //TFT.Color_Bar_ON();
  while (true) {};

}

void DMAfinished() {
  //lv_disp_flush_ready(&disp_drv);
}

void setup() {

  TFT.onCompleteCB(DMAfinished);
  //delay(500);

  pinMode(13, OUTPUT);
  digitalWrite(13,!digitalRead(13));

  digitalWrite(myTRIG, HIGH);
  pinMode(myTRIG, OUTPUT);
 
  

  delay(1000);
 
  //while (!Serial && millis() < 5000) {}  //wait up to 2 seconds for Serial Monitor to connect
  Serial.println("r7studio GPCF-LCD 8080");
  Serial.print("Compiled ");
  Serial.print(__DATE__);
  Serial.print(" at ");
  Serial.println(__TIME__);

  Serial.println("Graphics setup");


  //TFT Lowlevel
  //delay(1000);
  //pinMode(BACKLIGHT, OUTPUT);
  //analogWriteFrequency(BACKLIGHT, 50000); //50kHz - make sure the add a series resistor to the PWM output pin to limit slew rate
  //analogWrite(BACKLIGHT, toBeBrightness);  //256 = 818mA@5V
  //analogWrite(BACKLIGHT, 32);  //256 = 818mA@5V
  //delay(500);
  /*
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
              */
  bool success = TFT.begin(20); //MHz  => 4 Mhz on Breadboard
  if (success) {
    Serial.println("TFT.begin() success");
  } else {
    Serial.println("TFT.begin() failed");
  }
  TFT.setRotation(0);

  TFT.backlight(true);
  TFT.setBrightness(50);

  //TFT.lcdHorizontalWidthVerticalHeight(400,1280);

  //TFT.Color_Bar_ON();
  /*
  TFT.useCanvas(0);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(1);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(2);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(3);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(4);
  TFT.fillScreen(COLOR65K_WHITE);
  TFT.useCanvas(5);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(6);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(7);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(8);
  TFT.fillScreen(COLOR65K_BLACK);
  TFT.useCanvas(9);
  TFT.fillScreen(COLOR65K_BLACK);
  */
  TFT.useCanvas(0);

   
  
  //TFT.drawLine(0, 0, 150, 150, COLOR65K_GREEN);


  lv_init();

  

  lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, BUFFER_SIZE); //or with buf_2
 
  //Initialize the display driver - basically just the buffers
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREENWIDTH;
  disp_drv.ver_res = SCREENHEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.antialiasing = 1;
  disp_drv.direct_mode = 0;
  disp_drv.full_refresh = 0; //new
  disp_drv.sw_rotate = 0;
  disp_drv.rotated = LV_DISP_ROT_NONE;      //LV_DISP_ROT_270;
  disp = lv_disp_drv_register(&disp_drv);

//  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);
  //set foreground color to white
//  lv_obj_set_style_text_color(lv_scr_act(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  
  
  //do the same for lines
  lv_obj_set_style_line_color(lv_scr_act(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  
  /*
  static lv_point_t line_points[] = { {2, 2}, {395, 1270}}; //max coordinates 0,0 to 399,1279

  static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 8);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));

    lv_obj_t * obj;
    obj = lv_line_create(lv_scr_act());
    lv_line_set_points(obj, line_points, 2);     

    
    lv_obj_add_style(obj, &style_line, 0);
    */

    

   
    lv_obj_t * obj = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

    //lv_obj_align(obj, LV_ALIGN_, 000, -600);
    lv_obj_set_pos(obj, 10, 10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 100, 200);
    lv_anim_set_time(&a, 4000);
    //lv_anim_set_duration(&a, 1000);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_playback_time(&a, 4000);
    //lv_anim_set_playback_duration(&a, 300);
    lv_anim_set_repeat_delay(&a, 4100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    lv_anim_set_exec_cb(&a, anim_size_cb);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, 10, 1000);
    lv_anim_start(&a);
    

    for (int i = 0; i < 10; i++) {
    delayMicroseconds(1);
    digitalToggleFast(myTRIG);
  }


}

void loop() {
  
  //Graphics::loopGraphics();
  /*
  if (blinkTimer.check()) {
      digitalToggle(LED_BUILTIN);
      if (digitalRead(LED_BUILTIN)) {
        //TFT.fillScreen(COLOR65K_GREEN);
          //TFT.backlight(true);
          //TFT.setBrightness(64);
      } else {
        //TFT.fillScreen(COLOR65K_BLACK);
        //TFT.drawSquareFill(1, 1, 10, 10, COLOR65K_RED);
          //TFT.drawLine(0x80, 0x80, 0x88, 0x88, 0xEEEE);
          //TFT.backlight(false);
          //TFT.setBrightness(0);
      }
    }
    */

   //  TFT.updateScreenWithROP(9, 2, 0, 14); //MERGE (Logical OR) Canvas 9 (UI) and 2 (Meters) to Canvas 0 (visible)
  if (lvglTaskTimer.check()) {
    lv_task_handler(); /* let the GUI do its work */
    //touch.loop();
  }
}

