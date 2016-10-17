#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "enamel.h"
#include <pebble-events/pebble-events.h>

static Layer *background_layer;
static Window *window;
static FFont* filled_font;
static FFont* outlined_font;
static char minute_buffer[3];
static char hour_buffer[3];

// used to pass bimap info to get/set pixel accurately
typedef struct {
  uint8_t *bitmap_data;
  int bytes_per_row;
  GBitmapFormat bitmap_format;
}  BitmapInfo;

#ifndef PBL_COLOR
static bool byte_get_bit(uint8_t *byte, uint8_t bit) {
  return ((*byte) >> bit) & 1;
}

static void byte_set_bit(uint8_t *byte, uint8_t bit, uint8_t value) {
  *byte ^= (-value ^ *byte) & (1 << bit);
}
#endif

static GColor get_pixel_color(GBitmapDataRowInfo info, GPoint point) {
#if defined(PBL_COLOR)
  // Read the single byte color pixel
  return (GColor){ .argb = info.data[point.x] };
#elif defined(PBL_BW)
  // Read the single bit of the correct byte
  uint8_t byte = point.x / 8;
  uint8_t bit = point.x % 8;
  return byte_get_bit(&info.data[byte], bit) ? GColorWhite : GColorBlack;
#endif
}

static void set_pixel_color(GBitmapDataRowInfo info, GPoint point,
                                                                GColor color) {
#if defined(PBL_COLOR)
  // Write the pixel's byte color
  memset(&info.data[point.x], color.argb, 1);
#elif defined(PBL_BW)
  // Find the correct byte, then set the appropriate bit
  uint8_t byte = point.x / 8;
  uint8_t bit = point.x % 8;
  byte_set_bit(&info.data[byte], bit, gcolor_equal(color, GColorWhite) ? 1 : 0);
#endif
}

static int min(int val1, int val2) {
  return val1<val2? val1:val2;
}

static void enamel_settings_received_handler(void *context){
  layer_mark_dirty(background_layer);
}

static void background_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  uint8_t offset_from_bottom = 0;
#if PBL_API_EXISTS(layer_get_unobstructed_bounds)
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(layer);
  // move everything up by half the obstruction
  offset_from_bottom = (bounds.size.h - unobstructed_bounds.size.h) / 2;
  bounds.size.h-=offset_from_bottom;
#endif
  FContext fctx;
  fctx_init_context(&fctx, ctx);
  graphics_context_set_fill_color(ctx,enamel_get_background());
  graphics_fill_rect(ctx,bounds,0,GCornerNone);
  //hour
  fctx_begin_fill(&fctx);
  fctx_set_text_em_height(&fctx, filled_font, 100);
  fctx_set_fill_color(&fctx, enamel_get_hourFill());
  fctx_set_pivot(&fctx, FPointZero);
  fctx_set_offset(&fctx, FPointI(bounds.size.w/2-10,bounds.size.h/3));
  fctx_draw_string(&fctx, hour_buffer, filled_font, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);
  fctx_begin_fill(&fctx);
  fctx_set_text_em_height(&fctx, outlined_font, 100);
  fctx_set_fill_color(&fctx, enamel_get_hourOutline());
  fctx_set_pivot(&fctx, FPointZero);
  fctx_set_offset(&fctx, FPointI(bounds.size.w/2-10,bounds.size.h/3));
  fctx_draw_string(&fctx, hour_buffer, outlined_font, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);

  // copy framebuffer to bitmap
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  GBitmap *copy = gbitmap_create_blank(gbitmap_get_bounds(fb).size,gbitmap_get_format(fb));
  // Iterate over all rows
  for(int y = 0; y < bounds.size.h/2+10; y++) {
    // Get this row's range and data
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    GBitmapDataRowInfo copy_info = gbitmap_get_data_row_info(copy, y);

    // Iterate over all visible columns
    for(int x = info.min_x; x <= info.max_x; x++) {
      set_pixel_color(copy_info, GPoint(x, y), get_pixel_color(info, GPoint(x,y)));
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  //black background
  graphics_context_set_fill_color(ctx,enamel_get_background());
  graphics_fill_rect(ctx,bounds,0,GCornerNone);
  //minute
  fctx_begin_fill(&fctx);
  fctx_set_text_em_height(&fctx, filled_font, 100);
  fctx_set_fill_color(&fctx, enamel_get_minuteFill());
  fctx_set_pivot(&fctx, FPointZero);
  fctx_set_offset(&fctx, FPointI(bounds.size.w/2+10,bounds.size.h*2/3));
  fctx_draw_string(&fctx, minute_buffer, filled_font, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);
  fctx_begin_fill(&fctx);
  fctx_set_text_em_height(&fctx, outlined_font, 100);
  fctx_set_fill_color(&fctx, enamel_get_minuteOutline());
  fctx_set_pivot(&fctx, FPointZero);
  fctx_set_offset(&fctx, FPointI(bounds.size.w/2+10,bounds.size.h*2/3));
  fctx_draw_string(&fctx, minute_buffer, outlined_font, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);

  // copy bitmap to framebuffer
  fb = graphics_capture_frame_buffer(ctx);
  // Iterate over all rows
  for(int y = 0; y < bounds.size.h/2-10; y++) {
    // Get this row's range and data
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    GBitmapDataRowInfo copy_info = gbitmap_get_data_row_info(copy, y);

    // Iterate over all visible columns
    for(int x = info.min_x; x <= info.max_x; x++) {
      set_pixel_color(info, GPoint(x, y), get_pixel_color(copy_info, GPoint(x,y)));
    }
  }
  int max_x = bounds.size.w;
  for(int y = bounds.size.h/2-10; y < bounds.size.h/2+10; y++) {
    // Get this row's range and data
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    GBitmapDataRowInfo copy_info = gbitmap_get_data_row_info(copy, y);

    // Iterate over some pixels
    for(int x = info.min_x; x <= min(info.max_x,max_x); x++) {
      set_pixel_color(info, GPoint(x, y), get_pixel_color(copy_info, GPoint(x,y)));
    }
    max_x -= bounds.size.w/20;
  }
  graphics_release_frame_buffer(ctx, fb);
  gbitmap_destroy(copy);

  fctx_deinit_context(&fctx);

  // draw line
  graphics_context_set_stroke_color(ctx,enamel_get_line());
  graphics_context_set_stroke_width(ctx,3);
  graphics_draw_line(ctx,GPoint(0,bounds.size.h/2+10),GPoint(bounds.size.w,bounds.size.h/2-10));
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  strftime(minute_buffer, sizeof(minute_buffer),"%M", tick_time);
  strftime(hour_buffer, sizeof(hour_buffer),clock_is_24h_style() ? "%H" : "%I", tick_time);
  layer_mark_dirty(background_layer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // init background
  background_layer = layer_create(bounds);
  layer_set_update_proc(background_layer, background_update_proc);
  layer_add_child(window_layer, background_layer);

  outlined_font = ffont_create_from_resource(RESOURCE_ID_PEACE_OUTLINE_FFONT);
  filled_font = ffont_create_from_resource(RESOURCE_ID_PEACE_FFONT);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_tick(t, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
}

static void window_unload(Window *window) {
  layer_destroy(background_layer);
  ffont_destroy(filled_font);
  ffont_destroy(outlined_font);
  tick_timer_service_unsubscribe();
}

static void init(void) {
  enamel_init();
  enamel_settings_received_subscribe(enamel_settings_received_handler,NULL);
  events_app_message_open();
  window = window_create();
  window_set_background_color(window, enamel_get_background());
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);

}

static void deinit(void) {
  enamel_deinit();
  window_destroy(window);
}


int main(void) {
  init();
  app_event_loop();
  deinit();
}
