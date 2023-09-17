#include "..\include\nofuss_ascii_text_renderer.h"
#include "..\include\nofuss_ascii_text_renderer_glyphs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Defines & Constants
#define NFATR_GLYPH_WIDTH (5)
#define NFATR_GLYPH_HEIGHT (9)
#define NFATR_GLYPH_RESOLUTION (NFATR_GLYPH_WIDTH * NFATR_GLYPH_HEIGHT)

// Private helper functions
static enum nfatr_bool nfatr_ascii_printable(int ascii_code)
{
   return (
      ascii_code >= NFATR_FIRST_PRINTABLE_ASCII_CODE &&
      ascii_code <= NFATR_LAST_PRINTABLE_ASCII_CODE
   );
}

static int nfatr_relative_ascii_code(int ascii_code)
{
   return ascii_code - NFATR_FIRST_PRINTABLE_ASCII_CODE;
}

static void nfatr_associate_glyph_design
(
   struct nfatr_instance * p_instance,
   int ascii_code,
   const char * p_glyph_design
)
{
   // Plot design into the texture and keep track of the glyph texture cell
   if (!p_instance || !p_glyph_design) return;

   // Allow only printable characters to be associated to a design
   if (!nfatr_ascii_printable(ascii_code)) return;

   // The glyph design string must have the exact resolution
   if (strlen(p_glyph_design) != NFATR_GLYPH_RESOLUTION) return;

   /*
      Every ascii character glyph is assigned to a specific, unique cell
      within the texture. The desired texture might thus not be enough to
      house all the printable ascii character glyphs.
   */
   struct nfatr_texture * const P_TEXTURE = &p_instance->texture;
   const int ASCII_TEXTURE_INDEX = nfatr_relative_ascii_code(ascii_code);
   if (ASCII_TEXTURE_INDEX >= P_TEXTURE->glyphs_total)
   {
      // No room for the glyph in terms of the texture size
      return;
   }

   // Ready to plot the glyph design into the texture
   struct nfatr_ascii_glyph_design * p_glyph_info = p_instance->ascii_glyph_designs + ASCII_TEXTURE_INDEX;
   p_glyph_info->texture_cell_x = ASCII_TEXTURE_INDEX % P_TEXTURE->glyphs_horizontal;
   p_glyph_info->texture_cell_y = ASCII_TEXTURE_INDEX / P_TEXTURE->glyphs_horizontal;

   // Plot design into the corresponding texture cell
   const int GLYPH_TEXTURE_MIN_X = p_glyph_info->texture_cell_x * NFATR_GLYPH_WIDTH;
   const int GLYPH_TEXTURE_MAX_X = GLYPH_TEXTURE_MIN_X + NFATR_GLYPH_WIDTH;
   const int GLYPH_TEXTURE_MIN_Y = p_glyph_info->texture_cell_y * NFATR_GLYPH_HEIGHT;
   const int GLYPH_TEXTURE_MAX_Y = GLYPH_TEXTURE_MIN_Y + NFATR_GLYPH_HEIGHT;

   int design_texel_index = 0;
   for (int texel_y = GLYPH_TEXTURE_MIN_Y; texel_y < GLYPH_TEXTURE_MAX_Y; texel_y++)
   {
      for (int texel_x = GLYPH_TEXTURE_MIN_X; texel_x < GLYPH_TEXTURE_MAX_X; texel_x++)
      {
         // Consider only design texels
         const char DESIGN_CHAR = p_glyph_design[design_texel_index++];
         if (DESIGN_CHAR != '#') continue;

         // Plot into the texture
         const int TEXEL_INDEX = texel_y * P_TEXTURE->width + texel_x;
         uint32_t * const p_texel_rgba = (uint32_t *)P_TEXTURE->p_rgba + TEXEL_INDEX;
         *p_texel_rgba = 0xFFFFFFFF; // ABGR
      }
   }

   // Mark glyph design as configured
   p_glyph_info->configured = NFATR_BOOL_TRUE;
}

static void nfatr_initialize_settings(struct nfatr_instance * p_instance)
{
   if (!p_instance) return;

   struct nfatr_settings * const p_settings = &p_instance->settings;
   p_settings->position_x = 0;
   p_settings->position_y = 0;
   p_settings->font_scale = 1;
   p_settings->padding_horizontal = 1;
   p_settings->padding_vertical = 1;
   p_settings->cutoff_horizontal = 0;
   p_settings->cutoff_vertical = 0;
}

static enum nfatr_bool nfatr_initialize_texture
(
   struct nfatr_instance * p_instance,
   int desired_width,
   int desired_height
)
{
   if (!p_instance) return NFATR_BOOL_FALSE;

   // Attempt to allocate the texture - RGBA components per texel
   struct nfatr_texture new_texture;
   new_texture.width = desired_width;
   new_texture.height = desired_height;
   new_texture.glyphs_horizontal = new_texture.width / NFATR_GLYPH_WIDTH;
   new_texture.glyphs_vertical = new_texture.height / NFATR_GLYPH_HEIGHT;
   new_texture.glyphs_total = new_texture.glyphs_horizontal * new_texture.glyphs_vertical;

   new_texture.bytes = sizeof(uint32_t) * new_texture.width * new_texture.height;
   new_texture.p_rgba = malloc(new_texture.bytes);

   // Make sure texture allocation was successful
   if (!new_texture.p_rgba) return NFATR_BOOL_FALSE;

   // Texture allocation successful - Initialize every texel to transparent black
   const int TEXTURE_TEXEL_COUNT = new_texture.width * new_texture.height;
   for (int i_texel = 0; i_texel < TEXTURE_TEXEL_COUNT; i_texel++)
   {
      uint32_t * const p_texel_rgba = (uint32_t *)new_texture.p_rgba + i_texel;
      // Uint literal notation texel component order ABGR
      *p_texel_rgba = 0x00000000;
   }

   // Initialize instance texture
   p_instance->texture = new_texture;

   // Success
   return NFATR_BOOL_TRUE;
}

static enum nfatr_bool nfatr_designs_initialized_successfully
(
   struct nfatr_instance * p_instance
)
{
   if (!p_instance) return NFATR_BOOL_FALSE;

   // The designs for all the glyph designs must be configured for success
   for (int i_design = 0; i_design < NFATR_PRINTABLE_CHARACTER_COUNT; i_design++)
   {
      if (!p_instance->ascii_glyph_designs[i_design].configured)
      {
         // Failure - At least one design is not configured
         return NFATR_BOOL_FALSE;
      }
   }

   // Success - All designs are configured
   return NFATR_BOOL_TRUE;
}

static void nfatr_associate_all_glyph_designs
(
   struct nfatr_instance * p_instance
)
{
   if (!p_instance) return;

   nfatr_associate_glyph_design(p_instance, '!', NFATR_DESIGN_EXCLAMATION_MARK);
   nfatr_associate_glyph_design(p_instance, '"', NFATR_DESIGN_DOUBLE_QUOTES);
   nfatr_associate_glyph_design(p_instance, '#', NFATR_DESIGN_HASHTAG);
   nfatr_associate_glyph_design(p_instance, '$', NFATR_DESIGN_DOLLAR_SIGN);
   nfatr_associate_glyph_design(p_instance, '%', NFATR_DESIGN_PERCENTAGE_SIGN);

   nfatr_associate_glyph_design(p_instance, '&', NFATR_DESIGN_AMPERSAND);
   nfatr_associate_glyph_design(p_instance, '\'', NFATR_DESIGN_SINGLE_QUOTE);
   nfatr_associate_glyph_design(p_instance, '(', NFATR_DESIGN_PARENTHESIS_OPEN);
   nfatr_associate_glyph_design(p_instance, ')', NFATR_DESIGN_PARENTHESIS_CLOSED);
   nfatr_associate_glyph_design(p_instance, '*', NFATR_DESIGN_ASTERISK);
   nfatr_associate_glyph_design(p_instance, '+', NFATR_DESIGN_PLUS_SIGN);
   nfatr_associate_glyph_design(p_instance, ',', NFATR_DESIGN_COMMA);
   nfatr_associate_glyph_design(p_instance, '-', NFATR_DESIGN_MINUS_SIGN);
   nfatr_associate_glyph_design(p_instance, '.', NFATR_DESIGN_PERIOD);
   nfatr_associate_glyph_design(p_instance, '/', NFATR_DESIGN_FORWARD_SLASH);

   nfatr_associate_glyph_design(p_instance, '0', NFATR_DESIGN_DIGIT_ZERO);
   nfatr_associate_glyph_design(p_instance, '1', NFATR_DESIGN_DIGIT_ONE);
   nfatr_associate_glyph_design(p_instance, '2', NFATR_DESIGN_DIGIT_TWO);
   nfatr_associate_glyph_design(p_instance, '3', NFATR_DESIGN_DIGIT_THREE);
   nfatr_associate_glyph_design(p_instance, '4', NFATR_DESIGN_DIGIT_FOUR);
   nfatr_associate_glyph_design(p_instance, '5', NFATR_DESIGN_DIGIT_FIVE);
   nfatr_associate_glyph_design(p_instance, '6', NFATR_DESIGN_DIGIT_SIX);
   nfatr_associate_glyph_design(p_instance, '7', NFATR_DESIGN_DIGIT_SEVEN);
   nfatr_associate_glyph_design(p_instance, '8', NFATR_DESIGN_DIGIT_EIGHT);
   nfatr_associate_glyph_design(p_instance, '9', NFATR_DESIGN_DIGIT_NINE);

   nfatr_associate_glyph_design(p_instance, ':', NFATR_DESIGN_COLON);
   nfatr_associate_glyph_design(p_instance, ';', NFATR_DESIGN_SEMI_COLON);
   nfatr_associate_glyph_design(p_instance, '<', NFATR_DESIGN_SMALLER_THAN_SIGN);
   nfatr_associate_glyph_design(p_instance, '=', NFATR_DESIGN_EQUAL_SIGN);
   nfatr_associate_glyph_design(p_instance, '>', NFATR_DESIGN_LARGER_THAN_SIGN);
   nfatr_associate_glyph_design(p_instance, '?', NFATR_DESIGN_QUESTION_MARK);
   nfatr_associate_glyph_design(p_instance, '@', NFATR_DESIGN_AT_SIGN);

   nfatr_associate_glyph_design(p_instance, 'A', NFATR_DESIGN_A);
   nfatr_associate_glyph_design(p_instance, 'B', NFATR_DESIGN_B);
   nfatr_associate_glyph_design(p_instance, 'C', NFATR_DESIGN_C);
   nfatr_associate_glyph_design(p_instance, 'D', NFATR_DESIGN_D);
   nfatr_associate_glyph_design(p_instance, 'E', NFATR_DESIGN_E);
   nfatr_associate_glyph_design(p_instance, 'F', NFATR_DESIGN_F);
   nfatr_associate_glyph_design(p_instance, 'G', NFATR_DESIGN_G);
   nfatr_associate_glyph_design(p_instance, 'H', NFATR_DESIGN_H);
   nfatr_associate_glyph_design(p_instance, 'I', NFATR_DESIGN_I);
   nfatr_associate_glyph_design(p_instance, 'J', NFATR_DESIGN_J);
   nfatr_associate_glyph_design(p_instance, 'K', NFATR_DESIGN_K);
   nfatr_associate_glyph_design(p_instance, 'L', NFATR_DESIGN_L);
   nfatr_associate_glyph_design(p_instance, 'M', NFATR_DESIGN_M);
   nfatr_associate_glyph_design(p_instance, 'N', NFATR_DESIGN_N);
   nfatr_associate_glyph_design(p_instance, 'O', NFATR_DESIGN_O);
   nfatr_associate_glyph_design(p_instance, 'P', NFATR_DESIGN_P);
   nfatr_associate_glyph_design(p_instance, 'Q', NFATR_DESIGN_Q);
   nfatr_associate_glyph_design(p_instance, 'R', NFATR_DESIGN_R);
   nfatr_associate_glyph_design(p_instance, 'S', NFATR_DESIGN_S);
   nfatr_associate_glyph_design(p_instance, 'T', NFATR_DESIGN_T);
   nfatr_associate_glyph_design(p_instance, 'U', NFATR_DESIGN_U);
   nfatr_associate_glyph_design(p_instance, 'V', NFATR_DESIGN_V);
   nfatr_associate_glyph_design(p_instance, 'W', NFATR_DESIGN_W);
   nfatr_associate_glyph_design(p_instance, 'X', NFATR_DESIGN_X);
   nfatr_associate_glyph_design(p_instance, 'Y', NFATR_DESIGN_Y);
   nfatr_associate_glyph_design(p_instance, 'Z', NFATR_DESIGN_Z);

   nfatr_associate_glyph_design(p_instance, '[', NFATR_DESIGN_SQUARE_BRACKET_OPEN);
   nfatr_associate_glyph_design(p_instance, '\\', NFATR_DESIGN_BACKSPACE);
   nfatr_associate_glyph_design(p_instance, ']', NFATR_DESIGN_SQUARE_BRACKET_CLOSED);
   nfatr_associate_glyph_design(p_instance, '^', NFATR_DESIGN_HAT);
   nfatr_associate_glyph_design(p_instance, '_', NFATR_DESIGN_UNDERSCORE);
   nfatr_associate_glyph_design(p_instance, '`', NFATR_DESIGN_TICK_MARK);

   nfatr_associate_glyph_design(p_instance, 'a', NFATR_DESIGN_a);
   nfatr_associate_glyph_design(p_instance, 'b', NFATR_DESIGN_b);
   nfatr_associate_glyph_design(p_instance, 'c', NFATR_DESIGN_c);
   nfatr_associate_glyph_design(p_instance, 'd', NFATR_DESIGN_d);
   nfatr_associate_glyph_design(p_instance, 'e', NFATR_DESIGN_e);
   nfatr_associate_glyph_design(p_instance, 'f', NFATR_DESIGN_f);
   nfatr_associate_glyph_design(p_instance, 'g', NFATR_DESIGN_g);
   nfatr_associate_glyph_design(p_instance, 'h', NFATR_DESIGN_h);
   nfatr_associate_glyph_design(p_instance, 'i', NFATR_DESIGN_i);
   nfatr_associate_glyph_design(p_instance, 'j', NFATR_DESIGN_j);
   nfatr_associate_glyph_design(p_instance, 'k', NFATR_DESIGN_k);
   nfatr_associate_glyph_design(p_instance, 'l', NFATR_DESIGN_l);
   nfatr_associate_glyph_design(p_instance, 'm', NFATR_DESIGN_m);
   nfatr_associate_glyph_design(p_instance, 'n', NFATR_DESIGN_n);
   nfatr_associate_glyph_design(p_instance, 'o', NFATR_DESIGN_o);
   nfatr_associate_glyph_design(p_instance, 'p', NFATR_DESIGN_p);
   nfatr_associate_glyph_design(p_instance, 'q', NFATR_DESIGN_q);
   nfatr_associate_glyph_design(p_instance, 'r', NFATR_DESIGN_r);
   nfatr_associate_glyph_design(p_instance, 's', NFATR_DESIGN_s);
   nfatr_associate_glyph_design(p_instance, 't', NFATR_DESIGN_t);
   nfatr_associate_glyph_design(p_instance, 'u', NFATR_DESIGN_u);
   nfatr_associate_glyph_design(p_instance, 'v', NFATR_DESIGN_v);
   nfatr_associate_glyph_design(p_instance, 'w', NFATR_DESIGN_w);
   nfatr_associate_glyph_design(p_instance, 'x', NFATR_DESIGN_x);
   nfatr_associate_glyph_design(p_instance, 'y', NFATR_DESIGN_y);
   nfatr_associate_glyph_design(p_instance, 'z', NFATR_DESIGN_z);

   nfatr_associate_glyph_design(p_instance, '{', NFATR_DESIGN_CURLY_BRACKET_OPEN);
   nfatr_associate_glyph_design(p_instance, '|', NFATR_DESIGN_PIPE);
   nfatr_associate_glyph_design(p_instance, '}', NFATR_DESIGN_CURLY_BRACKET_CLOSED);
   nfatr_associate_glyph_design(p_instance, '~', NFATR_DESIGN_TILDE);
}

static enum nfatr_bool nfatr_instance_usable
(
   struct nfatr_instance * p_instance
)
{
   return (p_instance && p_instance->usable);
}

static enum nfatr_bool nfatr_instance_un_usable
(
   struct nfatr_instance * p_instance
)
{
   return !nfatr_instance_usable(p_instance);
}

static int nfatr_font_scale(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return p_instance->settings.font_scale;
}

static int nfatr_horizontal_padding_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return p_instance->settings.padding_horizontal * nfatr_font_scale(p_instance);
}

static int nfatr_vertical_padding_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return p_instance->settings.padding_vertical * nfatr_font_scale(p_instance);
}

static int nfatr_glyph_width_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return NFATR_GLYPH_WIDTH * nfatr_font_scale(p_instance);
}

static int nfatr_glyph_height_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return NFATR_GLYPH_HEIGHT * nfatr_font_scale(p_instance);
}

static int nfatr_glyph_offset_horizontal_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return nfatr_glyph_width_scaled(p_instance) + nfatr_horizontal_padding_scaled(p_instance);
}

static int nfatr_glyph_offset_vertical_scaled(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return 0;

   return nfatr_glyph_height_scaled(p_instance) + nfatr_vertical_padding_scaled(p_instance);
}

static enum nfatr_bool nfatr_initialize_glyph_designs
(
   struct nfatr_instance * p_instance
)
{
   if (!p_instance) return NFATR_BOOL_FALSE;

   // Initialize glyph designs to be un-configured
   for (int i_glyph = 0; i_glyph < NFATR_PRINTABLE_CHARACTER_COUNT; i_glyph++)
   {
      struct nfatr_ascii_glyph_design * const p_glyph_design = p_instance->ascii_glyph_designs + i_glyph;
      p_glyph_design->configured = NFATR_BOOL_FALSE;
   }

   // Associate ass printable ascii codes to a glyph design
   nfatr_associate_all_glyph_designs(p_instance);

   // Report success based on whether all glyphs designs were configured or not
   return nfatr_designs_initialized_successfully(p_instance);
}

static void nfatr_set_render_info_text_region
(
   struct nfatr_instance * p_instance,
   int min_x,
   int min_y,
   int max_x,
   int max_y
)
{
   if (nfatr_instance_un_usable(p_instance)) return;

   p_instance->render_info.text_region.min_x = min_x;
   p_instance->render_info.text_region.min_y = min_y;
   p_instance->render_info.text_region.max_x = max_x;
   p_instance->render_info.text_region.max_y = max_y;
}

static void nfatr_zero_render_info_text_region(struct nfatr_instance * p_instance)
{
   nfatr_set_render_info_text_region(p_instance, 0, 0, 0, 0);
}

static enum nfatr_bool nfatr_initialize_render_info
(
   struct nfatr_instance * p_instance,
   int max_render_info_glyphs
)
{
   if (!p_instance) return NFATR_BOOL_FALSE;

   // Allocate render info buffer
   struct nfatr_render_info new_render_info;
   new_render_info.p_glyph_infos = malloc(sizeof(struct nfatr_glyph_render_info) * max_render_info_glyphs);
   if (!new_render_info.p_glyph_infos)
   {
      // Failure - Render info buffer could not be allocated
      return NFATR_BOOL_FALSE;
   }

   // Success allocating the render info buffer - Now initialize the rest of the render info
   new_render_info.capacity = max_render_info_glyphs;
   new_render_info.count = 0;
   nfatr_zero_render_info_text_region(p_instance);

   // Initialize instance texture
   p_instance->render_info = new_render_info;

   // Success
   return NFATR_BOOL_TRUE;
}

static struct nfatr_region_i nfatr_make_region_i(int min_x, int min_y, int max_x, int max_y)
{
   struct nfatr_region_i region;

   region.min_x = min_x;
   region.min_y = min_y;
   region.max_x = max_x;
   region.max_y = max_y;

   return region;
}

static struct nfatr_region_f nfatr_make_region_f(float min_x, float min_y, float max_x, float max_y)
{
   struct nfatr_region_f region;

   region.min_x = min_x;
   region.min_y = min_y;
   region.max_x = max_x;
   region.max_y = max_y;

   return region;
}

static int nfatr_min_i(int a, int b)
{
   return (a < b) ? a : b;
}

static int nfatr_max_i(int a, int b)
{
   return (a > b) ? a : b;
}

// Interface - Life cycle
enum nfatr_bool nfatr_instantiate
(
   struct nfatr_instance * p_out_instance,
   int max_render_info_glyphs,
   int desired_texture_width,
   int desired_texture_height
)
{
   // Failure when no instance given or that instance is already instanciated
   if (!p_out_instance) return NFATR_BOOL_FALSE;

   // Initialize instance as un-usable in case initialization fails
   p_out_instance->usable = NFATR_BOOL_FALSE;

   // Silly parameters
   if (
      max_render_info_glyphs <= 0                 ||
      desired_texture_width  < NFATR_GLYPH_WIDTH  ||
      desired_texture_height < NFATR_GLYPH_HEIGHT
   ) return NFATR_BOOL_FALSE;
   
   // New internal instance
   struct nfatr_instance new_instance;

   // Initialize settings
   nfatr_initialize_settings(&new_instance);

   // The texture; glyph designs and render info must be initialized for the instance to be usable
   if (
      !nfatr_initialize_texture(&new_instance, desired_texture_width, desired_texture_height) ||
      !nfatr_initialize_glyph_designs(&new_instance)                                          ||
      !nfatr_initialize_render_info(&new_instance, max_render_info_glyphs)
   )
   {
      // Failure - Cleanup and mark the instance as un-usable
      nfatr_destroy(&new_instance);
      return NFATR_BOOL_FALSE;
   }

   // Success - Mark instance as usable - Initialize external instance
   new_instance.usable = NFATR_BOOL_TRUE;
   *p_out_instance = new_instance;
   return NFATR_BOOL_TRUE;
}

void nfatr_destroy(struct nfatr_instance * p_instance)
{
   // Cleanup only instanciated instances
   if (nfatr_instance_un_usable(p_instance)) return;

   // Release allocated resources and mark instance as un-usable
   free(p_instance->texture.p_rgba);
   free(p_instance->render_info.p_glyph_infos);

   // Null out dynamic memory references
   p_instance->texture.p_rgba = NULL;
   p_instance->render_info.p_glyph_infos = NULL;

   // Mark instance as unusable
   p_instance->usable = NFATR_BOOL_FALSE;
}

// Interface - Settings
enum nfatr_bool nfatr_set_position(struct nfatr_instance * p_instance, int x, int y)
{
   if (nfatr_instance_un_usable(p_instance)) return NFATR_BOOL_FALSE;

   p_instance->settings.position_x = x;
   p_instance->settings.position_y = y;

   return NFATR_BOOL_TRUE;
}

enum nfatr_bool nfatr_set_padding(struct nfatr_instance * p_instance, int horizontal, int vertical)
{
   if (nfatr_instance_un_usable(p_instance)) return NFATR_BOOL_FALSE;

   p_instance->settings.padding_horizontal = horizontal;
   p_instance->settings.padding_vertical = vertical;

   return NFATR_BOOL_TRUE;
}

enum nfatr_bool nfatr_set_cutoff(struct nfatr_instance * p_instance, int horizontal, int vertical)
{
   if (nfatr_instance_un_usable(p_instance)) return NFATR_BOOL_FALSE;

   p_instance->settings.cutoff_horizontal = horizontal;
   p_instance->settings.cutoff_vertical = vertical;

   return NFATR_BOOL_TRUE;
}

enum nfatr_bool nfatr_set_font_height(struct nfatr_instance * p_instance, int desired_font_height)
{
   if (nfatr_instance_un_usable(p_instance)) return NFATR_BOOL_FALSE;

   // Glyph height is the minimum font height
   const int CORRECTED_FONT_HEIGHT = (
      desired_font_height < NFATR_GLYPH_HEIGHT
   ) ? NFATR_GLYPH_HEIGHT : desired_font_height;

   // Determine the closest font scale
   const int FONT_HEIGHT_FLOOR = (CORRECTED_FONT_HEIGHT / NFATR_GLYPH_HEIGHT) * NFATR_GLYPH_HEIGHT;
   const int FONT_HEIGHT_CEILING = FONT_HEIGHT_FLOOR + NFATR_GLYPH_HEIGHT;
   const int FLOOR_DISTANCE = abs(CORRECTED_FONT_HEIGHT - FONT_HEIGHT_FLOOR);
   const int CEILING_DISTANCE = abs(CORRECTED_FONT_HEIGHT - FONT_HEIGHT_CEILING);
   const int CLOSEST_FONT_HEIGHT = (
      FLOOR_DISTANCE < CEILING_DISTANCE
   ) ? FONT_HEIGHT_FLOOR : FONT_HEIGHT_CEILING;

   printf("\nDesired: %d Final: %d", desired_font_height, CLOSEST_FONT_HEIGHT);

   // Update settings
   p_instance->settings.font_scale = CORRECTED_FONT_HEIGHT / NFATR_GLYPH_HEIGHT;
   
   return NFATR_BOOL_TRUE;
}

// Interface - Rendering
void nfatr_render_text(struct nfatr_instance * p_instance, const char * p_text)
{
   if (nfatr_instance_un_usable(p_instance)) return;

   // Reset render info count
   p_instance->render_info.count = 0;

   // Don't render anything when text empty
   if (!p_text) return;

   // Text rendering state per chunk of text
   int cursor_x = p_instance->settings.position_x;
   int cursor_y = p_instance->settings.position_y;

   // Reset text render region
   nfatr_set_render_info_text_region(p_instance, cursor_x, cursor_y, cursor_x, cursor_y);

   // Pre-compute texture mapping factors
   const float TEX_COORD_FRAC_X = 1.0f / (float)p_instance->texture.width;
   const float TEX_COORD_FRAC_Y = 1.0f / (float)p_instance->texture.height;

   // Pre-compute cutoff region to render into
   struct nfatr_region_i region_cutoff;
   nfatr_get_cutoff_region(p_instance, &region_cutoff);

   // Process every single character
   for (const char * p_character = p_text; *p_character; p_character++)
   {
      // Make sure there is room for another glyph
      if (p_instance->render_info.count >= p_instance->render_info.capacity)
      {
         // No more room to render glyphs
         break;
      }

      // Handle control characters
      const char character = *p_character;

      if ('\n' == character)
      {
         // Newline - Set cursor to the start of the next line
         cursor_x = p_instance->settings.position_x;
         cursor_y += nfatr_glyph_offset_vertical_scaled(p_instance);
         continue;
      }
      else if (' ' == character)
      {
         // Space - Skip single character horizontally with padding on the left
         cursor_x += nfatr_glyph_offset_horizontal_scaled(p_instance);
         continue;
      }

      // Character is not a control character - Handle printable character
      if (!nfatr_ascii_printable(character)) continue;

      // Get character glyph design
      const int RELATIVE_ASCII_CODE = nfatr_relative_ascii_code(character);
      const struct nfatr_ascii_glyph_design * P_GLYPH_DESIGN = p_instance->ascii_glyph_designs + RELATIVE_ASCII_CODE;

      // Render glyph to get things going
      struct nfatr_glyph_render_info new_glyph_info;
      new_glyph_info.region_surface = nfatr_make_region_i(
         cursor_x,
         cursor_y,
         cursor_x + nfatr_glyph_width_scaled(p_instance),
         cursor_y + nfatr_glyph_height_scaled(p_instance)
      );

      // Move to the next character on the same line
      cursor_x += nfatr_glyph_offset_horizontal_scaled(p_instance);

      // Handle cutoff per axis, if active
      const int CUTOFF_HORIZONTAL_ACTIVE = (p_instance->settings.cutoff_horizontal > 0);
      const int CUTOFF_VERTICAL_ACTIVE = (p_instance->settings.cutoff_vertical > 0);

      // Continue to next glyph if the left edge of the glyph is out of cutoff bounds
      if (CUTOFF_HORIZONTAL_ACTIVE && (new_glyph_info.region_surface.min_x > region_cutoff.max_x))
         continue;

      // Done if vertical cutoff specified and glyph vertically out of cutoff bounds
      if (CUTOFF_VERTICAL_ACTIVE && (new_glyph_info.region_surface.min_y > region_cutoff.max_y))
      {
         break;
      }

      // Glyph is in cutoff bounds - Determine final glyph dimensions by clipping against cutoff region - If enabled
      new_glyph_info.region_surface.max_x = CUTOFF_HORIZONTAL_ACTIVE
      ? nfatr_min_i(new_glyph_info.region_surface.max_x, region_cutoff.max_x)
      : new_glyph_info.region_surface.max_x;

      new_glyph_info.region_surface.max_y = CUTOFF_VERTICAL_ACTIVE
      ? nfatr_min_i(new_glyph_info.region_surface.max_y, region_cutoff.max_y)
      : new_glyph_info.region_surface.max_y;

      // Determine glyph region delta size to base texture coordinates from
      const float FONT_SCALE_F = (float)p_instance->settings.font_scale;
      const int GLYPH_TEXTURE_BASE_X = P_GLYPH_DESIGN->texture_cell_x * NFATR_GLYPH_WIDTH;
      const int GLYPH_TEXTURE_BASE_Y = P_GLYPH_DESIGN->texture_cell_y * NFATR_GLYPH_HEIGHT;
      const float GLYPH_RENDER_WIDTH = (
         (float)(new_glyph_info.region_surface.max_x - new_glyph_info.region_surface.min_x) / FONT_SCALE_F
      );
      const float GLYPH_RENDER_HEIGHT = (
         (float)(new_glyph_info.region_surface.max_y - new_glyph_info.region_surface.min_y) / FONT_SCALE_F
      );

      // Determine texture coordinated based on final rendering dimensions
      new_glyph_info.region_tex_coords = nfatr_make_region_f(
         GLYPH_TEXTURE_BASE_X * TEX_COORD_FRAC_X,
         GLYPH_TEXTURE_BASE_Y * TEX_COORD_FRAC_Y,
         (GLYPH_TEXTURE_BASE_X + GLYPH_RENDER_WIDTH) * TEX_COORD_FRAC_X,
         (GLYPH_TEXTURE_BASE_Y + GLYPH_RENDER_HEIGHT) * TEX_COORD_FRAC_Y
      );

      // Keep track of rendered text region as it grows based on the final glyph render dimensions
      if (new_glyph_info.region_surface.max_x > p_instance->render_info.text_region.max_x)
      {
         // Extends horizontal region
         p_instance->render_info.text_region.max_x = new_glyph_info.region_surface.max_x;
      }
      if (new_glyph_info.region_surface.max_y > p_instance->render_info.text_region.max_y)
      {
         // Extends vertical region
         p_instance->render_info.text_region.max_y = new_glyph_info.region_surface.max_y;
      }

      // Add info the rendering list
      p_instance->render_info.p_glyph_infos[p_instance->render_info.count++] = new_glyph_info;
   }
}

// Interface - Queries
const struct nfatr_texture * nfatr_get_glyph_texture(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) return NULL;

   return &p_instance->texture;
}

enum nfatr_bool nfatr_get_cutoff_region(struct nfatr_instance * p_instance, struct nfatr_region_i * p_out_cutoff_region)
{
   if (
      !p_out_cutoff_region ||
      nfatr_instance_un_usable(p_instance)
   ) return NFATR_BOOL_FALSE;

   // Initialize external region
   *p_out_cutoff_region = nfatr_make_region_i(
      p_instance->settings.position_x,
      p_instance->settings.position_y,
      p_instance->settings.position_x + p_instance->settings.cutoff_horizontal,
      p_instance->settings.position_y + p_instance->settings.cutoff_vertical
   );

   // Success
   return NFATR_BOOL_TRUE;
}

int nfatr_get_render_info_count(struct nfatr_instance * p_instance)
{
   if (nfatr_instance_un_usable(p_instance)) 0;

   return p_instance->render_info.count;
}

const struct nfatr_glyph_render_info * nfatr_get_render_info_glyph(struct nfatr_instance * p_instance, int info_index)
{
   if (nfatr_instance_un_usable(p_instance)) return NULL;

   // Make sure the index is in bounds
   if (info_index < 0 || info_index >= p_instance->render_info.count)
   {
      return NULL;
   }

   // Index is in bounds
   return p_instance->render_info.p_glyph_infos + info_index;
}

enum nfatr_bool nfatr_get_render_info_text_region(struct nfatr_instance * p_instance, struct nfatr_region_i * p_out_text_region)
{
   if (
      !p_out_text_region ||
      nfatr_instance_un_usable(p_instance)
   ) return NFATR_BOOL_FALSE;

   // Initialize external region
   *p_out_text_region = p_instance->render_info.text_region;

   // Success
   return NFATR_BOOL_TRUE;
}
