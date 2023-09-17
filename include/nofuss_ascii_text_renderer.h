#ifndef NFATR_H_
#define NFATR_H_

// Dependencies
#include <stdint.h>

// Defines & Constants
#define NFATR_FIRST_PRINTABLE_ASCII_CODE (33) // Starting at '!'
#define NFATR_LAST_PRINTABLE_ASCII_CODE (126) // Ending at '~'
#define NFATR_PRINTABLE_CHARACTER_COUNT (NFATR_LAST_PRINTABLE_ASCII_CODE - NFATR_FIRST_PRINTABLE_ASCII_CODE + 1)

// Data types
enum nfatr_bool {
   NFATR_BOOL_FALSE = 0,
   NFATR_BOOL_TRUE = 1
};

struct nfatr_texture {
   int width;
   int height;
   uint8_t * p_rgba;
   size_t bytes;
   int glyphs_horizontal;
   int glyphs_vertical;
   int glyphs_total;
};

struct nfatr_region_i {
   int min_x;
   int min_y;
   int max_x;
   int max_y;
};

struct nfatr_region_f {
   float min_x;
   float min_y;
   float max_x;
   float max_y;
};

struct nfatr_glyph_render_info {
   struct nfatr_region_i region_surface;
   struct nfatr_region_f region_tex_coords;
};

struct nfatr_render_info {
   int capacity;
   int count;
   struct nfatr_glyph_render_info * p_glyph_infos;
   struct nfatr_region_i text_region;
};

struct nfatr_ascii_glyph_design {
   int texture_cell_x;
   int texture_cell_y;
   enum nfatr_bool configured;
};

struct nfatr_settings {
   int position_x;
   int position_y;
   int font_scale;
   int padding_horizontal;
   int padding_vertical;
   int cutoff_horizontal;
   int cutoff_vertical;
};

struct nfatr_instance {
   enum nfatr_bool usable;
   struct nfatr_settings settings;
   struct nfatr_texture texture;
   struct nfatr_ascii_glyph_design ascii_glyph_designs[NFATR_PRINTABLE_CHARACTER_COUNT];
   struct nfatr_render_info render_info;
};

// Interface - Life cycle
enum nfatr_bool nfatr_instantiate
(
   struct nfatr_instance * p_out_instance,
   int max_render_info_glyphs,
   int desired_texture_width,
   int desired_texture_height
);
void nfatr_destroy(struct nfatr_instance * p_instance);

// Interface - Settings
enum nfatr_bool nfatr_set_position(struct nfatr_instance * p_instance, int x, int y);
enum nfatr_bool nfatr_set_padding(struct nfatr_instance * p_instance, int horizontal, int vertical);
enum nfatr_bool nfatr_set_cutoff(struct nfatr_instance * p_instance, int horizontal, int vertical);
enum nfatr_bool nfatr_set_font_height(struct nfatr_instance * p_instance, int desired_font_height);

// Interface - Rendering
void nfatr_render_text(struct nfatr_instance * p_instance, const char * p_text);

// Interface - Queries
const struct nfatr_texture * nfatr_get_glyph_texture(struct nfatr_instance * p_instance);
enum nfatr_bool nfatr_get_cutoff_region(struct nfatr_instance * p_instance, struct nfatr_region_i * p_out_cutoff_region);
int nfatr_get_render_info_count(struct nfatr_instance * p_instance);
const struct nfatr_glyph_render_info * nfatr_get_render_info_glyph(struct nfatr_instance * p_instance, int info_index);
enum nfatr_bool nfatr_get_render_info_text_region(struct nfatr_instance * p_instance, struct nfatr_region_i * p_out_text_region);

#endif