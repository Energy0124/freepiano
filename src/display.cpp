#include "pch.h"
#include <d3d9.h>
#include <dinput.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <windowsx.h>
#include <mbctype.h>
#include <Shlwapi.h>

#include <set>

#include "png.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <Knownfolders.h>
#include <Shlobj.h>

#include "display.h"
#include "keyboard.h"
#include "gui.h"
#include "config.h"
#include "song.h"
#include "export.h"
#include "../res/resource.h"
#include "language.h"
#include "utilities.h"


// directX device handle.
static LPDIRECT3D9 d3d9 = NULL;
static LPDIRECT3DDEVICE9 device = NULL;
static LPDIRECT3DTEXTURE9 resource_texture = NULL;
static IDirect3DTexture9 *render_texture = NULL;
static IDirect3DSurface9 *back_buffer = NULL;

// main window handle.
static HWND display_hwnd = NULL;

static int display_width = 0;
static int display_height = 0;
static bool display_dirty = true;

// get display width
int display_get_width() { return 752; }
int display_get_height() { return 336; }

// vertex format
struct Vertex {
  float x, y, z;
  DWORD color;
  float u, v;
};

// draw entire main window
// identity matrix
static D3DMATRIX matrix_identity = {
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1,
};

enum resource_type {
  background,
  notes,
  keyboard_note_down,
  keyboard_note_up,
  keyboard_note_empty,
  midi_black_down,
  midi_black_up,
  midi_white_down,
  midi_white_up,
  check_button_down,
  check_button_up,

  edit_box,
  round_corner,
  logo,

  resource_count
};

// -----------------------------------------------------------------------------------------
// texture management
// -----------------------------------------------------------------------------------------
struct texture_node_t {
  // child
  texture_node_t *parent;
  texture_node_t *child1;
  texture_node_t *child2;

  // rect in texture
  int min_u, min_v, max_u, max_v;

  // size
  int width, height;

  // used
  bool used;

  // constructor
  texture_node_t(texture_node_t *parent, int x1, int y1, int x2, int y2)
    : min_u(x1)
    , min_v(y1)
    , max_u(x2)
    , max_v(y2)
    , width(x2 - x1)
    , height(y2 - y1)
    , used(false)
    , parent(parent)
    , child1(NULL)
    , child2(NULL) {
  }

  // destructor
  ~texture_node_t() {
    if (child1) delete child1;
    if (child2) delete child2;
  }

  // search
  texture_node_t* search(int width, int height) {
    if (width <= 0 || height <= 0)
      return NULL;

    int usize = max_u - min_u;
    int vsize = max_v - min_v;

    if (width > usize || height > vsize)
      return NULL;

    texture_node_t *result = NULL;

    int area = usize * vsize;

    // use this node
    if (!used && width <= this->width && height <= this->height) {
      result = this;
      area = this->width * this->height;
    }

    // try child
    if (child1) {
      texture_node_t *temp = child1->search(width, height);

      if (temp) {
        int temp_area = temp->width * temp->height;
        if (temp_area < area) {
          area = temp_area;
          result = temp;
        }
      }
    }

    // try child2
    if (child2) {
      texture_node_t *temp = child2->search(width, height);

      if (temp) {
        int temp_area = temp->width * temp->height;
        if (temp_area < area) {
          area = temp_area;
          result = temp;
        }
      }
    }

    return result;
  }

  // use
  bool use(int usize, int vsize) {
    if (used)
      return false;

    if (usize > width || vsize > height)
      return false;

    int uleft = width - usize;
    int vleft = height - vsize;

    used = true;
    width = usize;
    height = vsize;

    if (uleft >= vleft) {
      if (child1 == NULL && uleft > 0)
        child1 = new texture_node_t(this, min_u + usize, min_v, max_u, max_v);

      if (child2 == NULL && vleft > 0)
        child2 = new texture_node_t(this, min_u, min_v + vsize, min_u + usize, max_v);
    } else {
      if (child1 == NULL && uleft > 0)
        child1 = new texture_node_t(this, min_u + usize, min_v, max_u, min_v + vsize);

      if (child2 == NULL && vleft > 0)
        child2 = new texture_node_t(this, min_u, min_v + vsize, max_u, max_v);
    }

    return true;
  }

  // unuse this node
  void unuse() {
    used = false;

    if (child1) {
      if (child1->used ||
          child1->child1 ||
          child1->child2)
        return;
    }

    if (child2) {
      if (child2->used ||
          child2->child1 ||
          child2->child2)
        return;
    }

    width = max_u - min_u;
    height = max_v - min_v;
    SAFE_DELETE(child1);
    SAFE_DELETE(child2);

    if (parent) {
      if (!parent->used)
        parent->unuse();
    }
  }


  // insert
  texture_node_t* allocate(int width, int height) {
    texture_node_t *result = search(width, height);

    if (result) {
      result->use(width, height);
    }

    return result;
  }
};

// root texture node.
static texture_node_t *root_texture_node = NULL;

// free type library
static FT_Library font_library = NULL;

// default font face
static FT_Face font_list[4] = {0};

// font character
struct font_character_t {
  wchar_t ch;
  int size;
  int advance_x;
  int bmp_left;
  int bmp_top;
  int height;
  texture_node_t *texture;
};

struct font_character_less {
  bool operator () (const font_character_t &l, const font_character_t &r) const {
    if (l.ch < r.ch) return true;
    if (l.ch > r.ch) return false;
    if (l.size < r.size) return true;
    if (l.size > r.size) return false;
    return false;
  }
};

// font character map
typedef std::set<font_character_t, font_character_less> character_set_t;
static character_set_t font_character_set;

// preload character
static void preload_characters(const wchar_t *text, int len, int size) {
  bool retry = true;

  if (len <= 0)
    len = wcslen(text);

  for (int i = 0; i < len; i++) {
    font_character_t cache;
    cache.ch = text[i];
    cache.size = size;
    cache.advance_x = 0;
    cache.bmp_left = 0;
    cache.bmp_top = 0;
    cache.texture = NULL;

    character_set_t::const_iterator it = font_character_set.find(cache);

    // character is not cached.
    if (it == font_character_set.end()) {
      FT_Face face = NULL;

      // find a font that can render this character
      for (int font_id = 0; font_id < ARRAY_COUNT(font_list); font_id++) {
        if (font_list[font_id] && FT_Get_Char_Index(font_list[font_id], cache.ch)) {
          face = font_list[font_id];
          break;
        }
      }

      if (face) {
        // set font size in pixels
        FT_Set_Pixel_Sizes(face, 0, size);

        // load glyph image into the slot (erase previous one)
        int error = FT_Load_Char(face, cache.ch, FT_LOAD_RENDER);
        if (error == 0) {
          FT_GlyphSlot slot = face->glyph;

          // size
          int size_x = slot->bitmap.width;
          int size_y = slot->bitmap.rows;

          if (size_x >= 0 && size_y >= 0) {
            // allocates texture node
            texture_node_t *node = root_texture_node->allocate(size_x + 2, size_y + 2);

            if (node) {
              RECT data_rect;
              data_rect.left = (int)node->min_u;
              data_rect.right = (int)node->min_u + (int)node->width;
              data_rect.top = (int)node->min_v;
              data_rect.bottom = (int)node->min_v + (int)node->height;

              D3DLOCKED_RECT lock_rect;
              if (SUCCEEDED(resource_texture->LockRect(0, &lock_rect, &data_rect, D3DLOCK_DISCARD))) {
                for (int y = 0; y < slot->bitmap.rows; ++y) {
                  uint *dst = (uint *)((byte *)lock_rect.pBits + lock_rect.Pitch * (y + 1) + 4);
                  byte *src = slot->bitmap.buffer + slot->bitmap.pitch * y;

                  for (int x = 0; x < slot->bitmap.width; x++) {
                    *dst = (*src << 24) | 0x00ffffff;
                    dst++;
                    src++;
                  }
                }

                resource_texture->UnlockRect(0);
              }

              // set cache attributes
              cache.advance_x = slot->advance.x >> 6;
              cache.bmp_left = slot->bitmap_left;
              cache.bmp_top = face->size->metrics.ascender / 64 - slot->bitmap_top;
              cache.texture = node;
              cache.height = face->size->metrics.height / 64;
            } else {
              if (retry) {
                SAFE_DELETE(root_texture_node->child1);
                SAFE_DELETE(root_texture_node->child2);
                root_texture_node->unuse();

                font_character_set.clear();
                i = 0;
                retry = false;
                continue;
              } else {
                return;
              }
            }
          }
        }
      }

      // insert cache to character map
      font_character_set.insert(cache);
    }
  }
}

// build string vertex
static int build_string_vertex(const wchar_t *text, int len, int size, float x, float y, uint color, Vertex *vertex_buffer, int vertex_count, int h_align, int v_align) {
  if (len <= 0)
    len = wcslen(text);

  float width = 0;
  float height = 0;
  float pen_x = x;
  float pen_y = y;
  Vertex *v = vertex_buffer;

  for (int i = 0; i < len; i++) {
    font_character_t cache;
    cache.ch = text[i];
    cache.size = size;

    character_set_t::const_iterator it = font_character_set.find(cache);

    // character is not cached.
    if (it != font_character_set.end()) {
      cache = *it;

      if (cache.texture && v + 6 < vertex_buffer + vertex_count) {
        v[0].x = pen_x + cache.bmp_left;
        v[0].y = pen_y + cache.bmp_top;
        v[0].z = 0;
        v[0].u = (float)cache.texture->min_u;
        v[0].v = (float)cache.texture->min_v;
        v[0].color = color;

        v[1].x = v[0].x + cache.texture->width;
        v[1].y = v[0].y;
        v[1].z = 0;
        v[1].u = v[0].u + cache.texture->width;
        v[1].v = v[0].v;
        v[1].color = color;

        v[2].x = v[0].x;
        v[2].y = v[0].y + cache.texture->height;
        v[2].z = 0;
        v[2].u = v[0].u;
        v[2].v = v[0].v + cache.texture->height;
        v[2].color = color;

        v[3] = v[1];
        v[4] = v[2];

        v[5].x = v[1].x;
        v[5].y = v[2].y;
        v[5].z = 0;
        v[5].u = v[1].u;
        v[5].v = v[2].v;
        v[5].color = color;

        v += 6;
        pen_x += cache.advance_x;

        if (width < pen_x)
          width = pen_x;

        if (height < pen_y + cache.height)
          height = pen_y + cache.height;
      }
    }
  }

  if (h_align || v_align) {
    float x_offset = 0;
    float y_offset = 0;

    if (h_align == 1) x_offset = floor(-0.5f * (width - x));
    if (v_align == 1) y_offset = floor(-0.5f * (height - y));

    for (Vertex *v1 = vertex_buffer; v1 < v; v1++) {
      v1->x += x_offset;
      v1->y += y_offset;
    }
  }

  return v - vertex_buffer;
}

// create string texture
static texture_node_t* create_string_texture(const char *text, int height) {
  int error;

  int pen_x = 0;
  int pen_y = 0;
  int size_x = 0;
  int size_y = 0;
  int num_chars = strlen(text);

  // go over and calculate size
  for (int n = 0; n < num_chars; n++) {
    FT_Face face = NULL;

    // find a font that can render this character
    for (int font_id = 0; font_id < ARRAY_COUNT(font_list); font_id++) {
      if (font_list[font_id] && FT_Get_Char_Index(font_list[font_id], text[n])) {
        face = font_list[font_id];
        break;
      }
    }

    if (face) {
      FT_GlyphSlot slot = face->glyph;

      // set font size in pixels
      error = FT_Set_Pixel_Sizes(face, 0, height);

      if (error)
        continue;

      // load glyph image into the slot (erase previous one)
      error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
      if (error)
        continue;

      // size
      int width = (slot->bitmap_left) + slot->bitmap.width;
      int height = face->size->metrics.height / 64;

      // max size
      if (pen_x + width > size_x) size_x = pen_x + width;
      if (pen_y + height > size_y) size_y = pen_y + height;

      // increment pen position
      pen_x += slot->advance.x >> 6;
    }
  }

  // allocates texture node
  texture_node_t *node = root_texture_node->allocate(size_x, size_y);

  // reset pen
  pen_x = 0;
  pen_y = 0;

  RECT data_rect;
  data_rect.left = (int)node->min_u;
  data_rect.right = (int)node->min_u + (int)node->width;
  data_rect.top = (int)node->min_v;
  data_rect.bottom = (int)node->min_v + (int)node->height;

  D3DLOCKED_RECT lock_rect;
  if (SUCCEEDED(resource_texture->LockRect(0, &lock_rect, &data_rect, D3DLOCK_DISCARD))) {
    // go over again and draw text
    for (int n = 0; n < num_chars; n++) {
      FT_Face face = NULL;

      // find a font that can render this character
      for (int font_id = 0; font_id < ARRAY_COUNT(font_list); font_id++) {
        if (font_list[font_id] && FT_Get_Char_Index(font_list[font_id], text[n])) {
          face = font_list[font_id];
          break;
        }
      }

      if (face) {
        FT_GlyphSlot slot = face->glyph;

        // set font size in pixels
        error = FT_Set_Pixel_Sizes(face, 0, height);

        if (error)
          continue;

        // load glyph image into the slot (erase previous one)
        error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
        if (error)
          continue;

        // darwing position
        int dx = pen_x + (slot->bitmap_left);
        int dy = pen_y + face->size->metrics.ascender / 64 - (slot->bitmap_top);

        for (int y = 0; y < slot->bitmap.rows; ++y) {
          uint *dst = (uint *)((byte *)lock_rect.pBits + lock_rect.Pitch * (dy + y) + 4 * dx);
          byte *src = slot->bitmap.buffer + slot->bitmap.pitch * y;


          for (int x = 0; x < slot->bitmap.width; x++) {
            *dst = (*src << 24) | 0xffffff;
            dst++;
            src++;
          }
        }

        // increment pen position
        pen_x += slot->advance.x >> 6;
      }
    }

    resource_texture->UnlockRect(0);
  }

  return node;
}

// initialize fonts
static int font_initialize() {
  int error;

  // intialize free type library
  error = FT_Init_FreeType(&font_library);
  if (error) {
    lang_set_last_error(IDS_ERR_INIT_FONT, "FT_Init_FreeType");
    return -1;
  }

  // get font path
  char font_path[MAX_PATH];
  char buffer[MAX_PATH];
  SHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT,  font_path);

  // default fonts
  static const char *fonts[] = { "arial.ttf", "simhei.ttf" };

  int font_count = 0;
  for (int i = 0; i < ARRAY_COUNT(fonts); i++) {
    // load default fonts.
    _snprintf(buffer, sizeof(buffer), "%s\\%s", font_path, fonts[i]);

    if (PathFileExists(buffer)) {
      if (0 == FT_New_Face(font_library, buffer, 0, &font_list[font_count])) {
        if (++font_count == ARRAY_COUNT(font_list))
          break;
      }
      else {
        fprintf(stderr, "font load failed: %s\n", buffer);
      }
    }
    else {
      fprintf(stderr, "font not found: %s\n", buffer);
    }
  }

  if (font_count == 0) {
    lang_set_last_error(IDS_ERR_INIT_FONT, "NO FONT FOUND");
    return -1;
  }

  return 0;
}

// shutdown font
static void font_shutdown() {
  for (int i = 0; i < ARRAY_COUNT(font_list); i++) {
    if (font_list[i]) {
      FT_Done_Face(font_list[i]);
      font_list[i] = NULL;
    }
  }

  FT_Done_FreeType(font_library);
}

// -----------------------------------------------------------------------------------------
// texture loading funcs
// -----------------------------------------------------------------------------------------

// create texture from png
static LPDIRECT3DTEXTURE9 create_texture_from_png(png_structp png_ptr, png_infop info_ptr) {
  LPDIRECT3DTEXTURE9 texture = NULL;

  int width = png_get_image_width(png_ptr, info_ptr);
  int height = png_get_image_height(png_ptr, info_ptr);
  uint color_type = png_get_color_type(png_ptr, info_ptr);

  // only rgba png is supported.
  if (color_type == PNG_COLOR_TYPE_RGBA) {
    if (SUCCEEDED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL))) {
      byte * *data = png_get_rows(png_ptr, info_ptr);
      uint rowbytes = png_get_rowbytes(png_ptr, info_ptr);

      D3DLOCKED_RECT lock_rect;
      if (SUCCEEDED(texture->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD))) {
        byte *line = (byte *)lock_rect.pBits;

        for (short y = 0; y < height; y++) {
          byte *src = data[y];
          byte *dst = line;
          for (short x = 0; x < width; x++) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];

            dst += 4;
            src += 4;
          }
          line += lock_rect.Pitch;
        }

        texture->UnlockRect(0);
      }
    }
  } else if (color_type == PNG_COLOR_TYPE_RGB) {
    if (SUCCEEDED(device->CreateTexture(width, height, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, NULL))) {
      byte * *data = png_get_rows(png_ptr, info_ptr);
      uint rowbytes = png_get_rowbytes(png_ptr, info_ptr);

      D3DLOCKED_RECT lock_rect;
      if (SUCCEEDED(texture->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD))) {
        byte *line = (byte *)lock_rect.pBits;

        for (short y = 0; y < height; y++) {
          byte *src = data[y];
          byte *dst = line;
          for (short x = 0; x < width; x++) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 0xff;

            dst += 4;
            src += 3;
          }
          line += lock_rect.Pitch;
        }

        texture->UnlockRect(0);
      }
    }
  }

  return texture;
}

// load png texture
static LPDIRECT3DTEXTURE9 load_png_from_file(const char *filename) {
  png_structp png_ptr;
  png_infop info_ptr;
  LPDIRECT3DTEXTURE9 texture = NULL;
  FILE *fp;

  // open file
  if ((fp = fopen(filename, "rb")) == NULL)
    return (NULL);

  // create png read struct
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL) {
    fclose(fp);
    return NULL;
  }

  // Allocate/initialize the memory for image information.
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    return NULL;
  }

  // Set error handling
  if (setjmp(png_jmpbuf(png_ptr))) {
    // Free all of the memory associated with the png_ptr and info_ptr
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    // If we get here, we had a problem reading the file
    return NULL;
  }

  // initialize io
  png_init_io(png_ptr, fp);

  // read png
  png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  // create texture node from png image
  texture = create_texture_from_png(png_ptr, info_ptr);

  // Clean up after the read, and free any memory allocated
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  // Close the file
  fclose(fp);

  return texture;
}


// load png texture
static LPDIRECT3DTEXTURE9 load_png_from_resource(const char *name) {
  struct resource_file {
    resource_file()
      : hrc(NULL)
      , data(NULL)
      , end(NULL) {
    }

    bool open(const char *name, const char *type) {
      // find resource
      HRSRC hrsrc = FindResource(0, name, "BINARY");
      if (hrsrc == 0)
        return false;

      // load resource
      hrc = LoadResource(0, hrsrc);
      if (hrc == 0)
        return false;

      data = (byte *)LockResource(hrc);
      end  = data + SizeofResource(NULL, hrsrc);

      return true;
    }

    ~resource_file() {
      FreeResource(hrc);
    }

    static void read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
      png_size_t check = 0;
      resource_file *file = (resource_file *)png_get_io_ptr(png_ptr);

      if (file != NULL) {
        if (file->data + length <= file->end) {
          memcpy(data, file->data, length);
          file->data += length;
          check = length;
        }
      }

      if (check != length) {
        png_error(png_ptr, "Read Error!");
      }
    }

    HGLOBAL hrc;
    byte *data;
    byte *end;
  };

  png_structp png_ptr;
  png_infop info_ptr;
  LPDIRECT3DTEXTURE9 texture = NULL;
  resource_file *file = new resource_file;

  if (!file->open(name, "BINARY")) {
    delete file;
    return NULL;
  }

  // create png read struct
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL) {
    delete file;
    return NULL;
  }

  // Allocate/initialize the memory for image information.
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    delete file;
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    return NULL;
  }

  // Set error handling
  if (setjmp(png_jmpbuf(png_ptr))) {
    // Free all of the memory associated with the png_ptr and info_ptr
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    delete file;

    // If we get here, we had a problem reading the file
    return NULL;
  }

  // set read function
  png_set_read_fn(png_ptr, (png_voidp)file, &resource_file::read_data);


  // read png
  png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  // create texture node from png image
  texture = create_texture_from_png(png_ptr, info_ptr);

  // Clean up after the read, and free any memory allocated
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  // Close the file
  delete file;

  return texture;

}

// -----------------------------------------------------------------------------------------
// directx functions
// -----------------------------------------------------------------------------------------
static void d3d_device_lost();

// get parameters
static void d3d_reset_parameters(D3DPRESENT_PARAMETERS &params, HWND hwnd) {
  ZeroMemory(&params, sizeof(D3DPRESENT_PARAMETERS));
  params.hDeviceWindow = hwnd;
  params.BackBufferFormat = D3DFMT_X8R8G8B8;
  params.BackBufferCount = 1;
  params.MultiSampleType = D3DMULTISAMPLE_NONE;
  params.MultiSampleQuality = 0;
  params.SwapEffect = D3DSWAPEFFECT_DISCARD;
#if FULLSCREEN
  params.Windowed = FALSE;
#else
  params.Windowed = TRUE;
#endif
  params.FullScreen_RefreshRateInHz = 0;
  params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
  params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  params.EnableAutoDepthStencil = FALSE;
  params.AutoDepthStencilFormat = D3DFMT_D24S8;

  RECT rect1, rect2;
  GetWindowRect(hwnd, &rect1);
  SetRect(&rect2, 0, 0, 0, 0);
  AdjustWindowRect(&rect2, GetWindowLong(hwnd, GWL_STYLE), GetMenu(hwnd) != NULL);

  params.BackBufferWidth = (rect1.right - rect1.left) - (rect2.right - rect2.left);
  params.BackBufferHeight = (rect1.bottom - rect1.top) - (rect2.bottom - rect2.top);

  char buff[256];
  sprintf(buff, "%d x %d\n", params.BackBufferWidth, params.BackBufferHeight);
  OutputDebugString(buff);

#if SCALE_DISPLAY
  display_width = display_get_width();
  display_height = display_get_height();
#else
  display_width = params.BackBufferWidth;
  display_height = params.BackBufferHeight;
#endif
}


#pragma pack(push)
#pragma pack(1)
struct TgaHeader {
  byte identsize;             // size of ID field that follows 18 byte header (0 usually)
  byte colourmaptype;         // type of colour map 0=none, 1=has palette
  byte imagetype;             // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

  short colourmapstart;       // first colour map entry in palette
  short colourmaplength;      // number of colours in palette
  byte colourmapbits;         // number of bits per palette entry 15,16,24,32

  short xstart;               // image x origin
  short ystart;               // image y origin
  short width;                // image width in pixels
  short height;               // image height in pixels
  byte bits;                  // image bits per pixel 8,16,24,32
  byte descriptor;            // image descriptor bits (vh flip bits)
};
#pragma pack(pop)

// load texture
static int load_tga_from_resource(const char *name, LPDIRECT3DTEXTURE9 *texture) {
  HRESULT hr;

  // find resource
  HRSRC hrsrc = FindResource(0, name, "BINARY");
  if (hrsrc == 0)
    return E_FAIL;

  // load resource
  HGLOBAL hrc = LoadResource(0, hrsrc);
  if (hrc == 0)
    return E_FAIL;


  // file header
  TgaHeader *header = (TgaHeader *)LockResource(hrc);

  // error
  hr = E_FAIL;

  // maybe a valid tga
  if (header->colourmaptype == 0 && header->imagetype == 2) {
    D3DFORMAT format = D3DFMT_UNKNOWN;

    switch (header->bits) {
     case 8:     format = D3DFMT_L8; break;
     case 16:    format = D3DFMT_A8L8; break;
     case 32:    format = D3DFMT_A8R8G8B8; break;
    }

    if (format) {
      if (SUCCEEDED(hr = device->CreateTexture(header->width, header->height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture, NULL))) {
        D3DLOCKED_RECT lock_rect;
        if (SUCCEEDED(hr = texture[0]->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD))) {
          uint pixelsize = header->bits / 8;
          byte *dst = (byte *)lock_rect.pBits;
          byte *src = (byte *)(header + 1) + (header->height - 1) * pixelsize * header->width;

          for (short y = 0; y < header->height; y++) {
            memcpy(dst, src, pixelsize * header->width);
            src -= pixelsize * header->width;
            dst += lock_rect.Pitch;
          }

          texture[0]->UnlockRect(0);
        }
      }
    }
  }

  // free resource
  FreeResource(hrc);
  return hr;
}

// terminate dx9
static void d3d_shutdown() {
  d3d_device_lost();

  SAFE_DELETE(root_texture_node);
  SAFE_RELEASE(resource_texture);
  SAFE_RELEASE(device);
  SAFE_RELEASE(d3d9);
}

// initialize d3d9
static int d3d_initialize(HWND hwnd) {
  HRESULT hr;

  // create direct3d 9 object
  d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d9 == NULL) {
    lang_set_last_error(IDS_ERR_INIT_DISPLAY, "Direct3DCreate9", -1);
    return E_FAIL;
  }

  // initialize params
  D3DPRESENT_PARAMETERS params;

  // reset params
  d3d_reset_parameters(params, hwnd);

  // create device
  if (FAILED(hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device))) {
    // try mixed vertex processing
    if (FAILED(hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_MIXED_VERTEXPROCESSING, &params, &device))) {
      // try software vertex processing
      if (FAILED(hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device))) {
        lang_set_last_error(IDS_ERR_INIT_DISPLAY, "CreateDevice", hr);
        goto error;
      }
    }
  }

  // create resource texture
  if (FAILED(hr = device->CreateTexture(512, 512, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &resource_texture, NULL))) {
    lang_set_last_error(IDS_ERR_INIT_DISPLAY, "CreateTexture", hr);
    goto error;
  }

  // create root texture node
  root_texture_node = new texture_node_t(NULL, 0, 0, 512, 512);

  return S_OK;

error:
  d3d_shutdown();
  return hr;
}

// device lost
static void d3d_device_lost() {
  SAFE_RELEASE(render_texture);
  SAFE_RELEASE(back_buffer);
}

// reset
static int d3d_device_reset() {
  if (!device)
    return 0;

  HRESULT hr;
  D3DPRESENT_PARAMETERS params;

  // test cooperative level.
  if (FAILED(hr = device->TestCooperativeLevel())) {
    if (D3DERR_DEVICELOST == hr)
      return hr;
  }

  // update parameters
  d3d_reset_parameters(params, display_hwnd);

  // reset device
  if (FAILED(hr = device->Reset(&params))) {
    lang_set_last_error(IDS_ERR_INIT_DISPLAY, "DeviceReset", hr);
    return hr;
  }

  // create render texture
  if (FAILED(hr = device->CreateTexture(display_get_width(), display_get_height(), 1, D3DUSAGE_RENDERTARGET,
                                        D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &render_texture, NULL))) {
    lang_set_last_error(IDS_ERR_INIT_DISPLAY, "CreateRenderTExture", hr);
    return hr;
  }

  if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer))) {
    lang_set_last_error(IDS_ERR_INIT_DISPLAY, "GetBackBuffer", hr);
    return hr;
  }

  // reset render states
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->SetRenderState(D3DRS_ZENABLE, FALSE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
  device->SetRenderState(D3DRS_ALPHAREF, 0);
  device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
  device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

  // alpha blend
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

  // color sources
  device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
  device->SetTextureStageState(0, D3DTSS_COLORARG1,  D3DTA_DIFFUSE);
  device->SetTextureStageState(0, D3DTSS_COLORARG2,  D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_COLOROP,    D3DTOP_MODULATE);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG1,  D3DTA_DIFFUSE);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG2,  D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_ALPHAOP,    D3DTOP_MODULATE);

  // texture states
  float mipmap_bias = -1;
  device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
  device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, *(DWORD *)&mipmap_bias);
  device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS,    D3DTTFF_COUNT2);

  // lighting
  device->SetRenderState(D3DRS_LIGHTING, FALSE);

  // set vertex declaration
  device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

  // refresh display
  display_force_refresh();
  return 0;
}

struct KeyboardState {
  float x1;
  float y1;
  float x2;
  float y2;
  float fade;

  // display options
  uint bg_image;
  uint color1;
  uint color2;
  int  note;
  int  notename;
  char label[18];
  char label1[18];
  key_bind_t map;
};

struct MidiKeyState {
  float x1;
  float y1;
  float x2;
  float y2;
  bool black;
  float fade;

  uint  color1;
  uint  color2;
};


static MidiKeyState midi_key_states[127] = {0};
static KeyboardState keyboard_states[256] = {0};

// init key display state
static void init_keyboard_states() {
  struct Key {
    byte code;
    byte action;
    float x;
    float y;
  };

  static Key keys1[] = {
    { 0,                3,  0.0f,   0.0f,   },
    { DIK_ESCAPE,       0,  1.0f,   1.0f,   },
    { 0,                0,  1.0f,   0.0f,   },
    { DIK_F1,           0,  1.0f,   1.0f,   },
    { DIK_F2,           0,  1.0f,   1.0f,   },
    { DIK_F3,           0,  1.0f,   1.0f,   },
    { DIK_F4,           0,  1.0f,   1.0f,   },
    { 0,                0,  0.5f,   0.0f    },
    { DIK_F5,           0,  1.0f,   1.0f,   },
    { DIK_F6,           0,  1.0f,   1.0f,   },
    { DIK_F7,           0,  1.0f,   1.0f,   },
    { DIK_F8,           0,  1.0f,   1.0f,   },
    { 0,                0,  0.5f,   0.0f,   },
    { DIK_F9,           0,  1.0f,   1.0f,   },
    { DIK_F10,          0,  1.0f,   1.0f,   },
    { DIK_F11,          0,  1.0f,   1.0f,   },
    { DIK_F12,          0,  1.0f,   1.0f,   },
    { 0,                1,  0.0f,   1.2f,   },
    { DIK_GRAVE,        0,  1.0f,   1.0f,   },
    { DIK_1,            0,  1.0f,   1.0f,   },
    { DIK_2,            0,  1.0f,   1.0f,   },
    { DIK_3,            0,  1.0f,   1.0f,   },
    { DIK_4,            0,  1.0f,   1.0f,   },
    { DIK_5,            0,  1.0f,   1.0f,   },
    { DIK_6,            0,  1.0f,   1.0f,   },
    { DIK_7,            0,  1.0f,   1.0f,   },
    { DIK_8,            0,  1.0f,   1.0f,   },
    { DIK_9,            0,  1.0f,   1.0f,   },
    { DIK_0,            0,  1.0f,   1.0f,   },
    { DIK_MINUS,        0,  1.0f,   1.0f,   },
    { DIK_EQUALS,       0,  1.0f,   1.0f,   },
    { DIK_BACK,         0,  2.0f,   1.0f,   },
    { 0,                1,  0.0f,   1.0f,   },
    { DIK_TAB,          0,  1.5f,   1.0f,   },
    { DIK_Q,            0,  1.0f,   1.0f,   },
    { DIK_W,            0,  1.0f,   1.0f,   },
    { DIK_E,            0,  1.0f,   1.0f,   },
    { DIK_R,            0,  1.0f,   1.0f,   },
    { DIK_T,            0,  1.0f,   1.0f,   },
    { DIK_Y,            0,  1.0f,   1.0f,   },
    { DIK_U,            0,  1.0f,   1.0f,   },
    { DIK_I,            0,  1.0f,   1.0f,   },
    { DIK_O,            0,  1.0f,   1.0f,   },
    { DIK_P,            0,  1.0f,   1.0f,   },
    { DIK_LBRACKET,     0,  1.0f,   1.0f,   },
    { DIK_RBRACKET,     0,  1.0f,   1.0f,   },
    { DIK_BACKSLASH,    0,  1.5f,   1.0f,   },
    { 0,                1,  0.0f,   1.0f,   },
    { DIK_CAPITAL,      0,  1.75f,  1.0f,   },
    { DIK_A,            0,  1.0f,   1.0f,   },
    { DIK_S,            0,  1.0f,   1.0f,   },
    { DIK_D,            0,  1.0f,   1.0f,   },
    { DIK_F,            0,  1.0f,   1.0f,   },
    { DIK_G,            0,  1.0f,   1.0f,   },
    { DIK_H,            0,  1.0f,   1.0f,   },
    { DIK_J,            0,  1.0f,   1.0f,   },
    { DIK_K,            0,  1.0f,   1.0f,   },
    { DIK_L,            0,  1.0f,   1.0f,   },
    { DIK_SEMICOLON,    0,  1.0f,   1.0f,   },
    { DIK_APOSTROPHE,   0,  1.0f,   1.0f,   },
    { DIK_RETURN,       0,  2.25f,  1.0f,   },
    { 0,                1,  0.0f,   1.0f,   },
    { DIK_LSHIFT,       0,  2.25,   1.0f,   },
    { DIK_Z,            0,  1.0f,   1.0f,   },
    { DIK_X,            0,  1.0f,   1.0f,   },
    { DIK_C,            0,  1.0f,   1.0f,   },
    { DIK_V,            0,  1.0f,   1.0f,   },
    { DIK_B,            0,  1.0f,   1.0f,   },
    { DIK_N,            0,  1.0f,   1.0f,   },
    { DIK_M,            0,  1.0f,   1.0f,   },
    { DIK_COMMA,        0,  1.0f,   1.0f,   },
    { DIK_PERIOD,       0,  1.0f,   1.0f,   },
    { DIK_SLASH,        0,  1.0f,   1.0f,   },
    { DIK_RSHIFT,       0,  2.75f,  1.0f,   },
    { 0,                1,  0.0f,   1.0f,   },
    { DIK_LCONTROL,     0,  1.25,   1.0f,   },
    { DIK_LWIN,         0,  1.25,   1.0f,   },
    { DIK_LMENU,        0,  1.25,   1.0f,   },
    { DIK_SPACE,        0,  6.00,   1.0f,   },
    { DIK_RMENU,        0,  1.25,   1.0f,   },
    { DIK_RWIN,         0,  1.25,   1.0f,   },
    { DIK_APPS,         0,  1.25,   1.0f,   },
    { DIK_RCONTROL,     0,  1.50,   1.0f,   },

    { 0,                3,  15.45f, 0.0f,   },
    { DIK_SYSRQ,        0,  1.0f,   1.0f,   },
    { DIK_SCROLL,       0,  1.0f,   1.0f,   },
    { DIK_PAUSE,        0,  1.0f,   1.0f,   },
    { 0,                0,  -3.0f,  1.2f,   },
    { DIK_INSERT,       0,  1.0f,   1.0f,   },
    { DIK_HOME,         0,  1.0f,   1.0f,   },
    { DIK_PGUP,         0,  1.0f,   1.0f,   },
    { 0,                0,  -3.0f,  1.0f,   },
    { DIK_DELETE,       0,  1.0f,   1.0f,   },
    { DIK_END,          0,  1.0f,   1.0f,   },
    { DIK_PGDN,         0,  1.0f,   1.0f,   },
    { 0,                0,  -2.0f,  2.0f,   },
    { DIK_UP,           0,  1.0f,   1.0f,   },
    { 0,                0,  -2.0f,  1.0f,   },
    { DIK_LEFT,         0,  1.0f,   1.0f,   },
    { DIK_DOWN,         0,  1.0f,   1.0f,   },
    { DIK_RIGHT,        0,  1.0f,   1.0f,   },

    { 0,                3,  18.9f,  1.2f,   },
    { DIK_NUMLOCK,      0,  1.0f,   1.0f,   },
    { DIK_NUMPADSLASH,  0,  1.0f,   1.0f,   },
    { DIK_NUMPADSTAR,   0,  1.0f,   1.0f,   },
    { DIK_NUMPADMINUS,  0,  1.0f,   1.0f,   },
    { 0,                0,  -4.0f,  1.0f,   },
    { DIK_NUMPAD7,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD8,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD9,      0,  1.0f,   1.0f,   },
    { DIK_NUMPADPLUS,   0,  1.0f,   2.0f,   },
    { 0,                0,  -4.0f,  1.0f,   },
    { DIK_NUMPAD4,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD5,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD6,      0,  1.0f,   1.0f,   },
    { 0,                0,  -3.0f,  1.0f,   },
    { DIK_NUMPAD1,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD2,      0,  1.0f,   1.0f,   },
    { DIK_NUMPAD3,      0,  1.0f,   1.0f,   },
    { DIK_NUMPADENTER,  0,  1.0f,   2.0f,   },
    { 0,                0,  -4.0f,  1.0f,   },
    { DIK_NUMPAD0,      0,  2.0f,   1.0f,   },
    { DIK_NUMPADPERIOD, 0,  1.0f,   1.0f,   },
  };

  float x = 0;
  float y = 0;
  float key_width = 32;
  float key_height = 29;

  for (Key *key = keys1; key < keys1 + sizeof(keys1) / sizeof(Key); key++) {
    if (key->code) {
      KeyboardState &state = keyboard_states[key->code];
      state.x1 = round(10 + x * key_width);
      state.y1 = round(13 + y * key_height);
      state.x2 = round(10 + x * key_width + key->x * key_width);
      state.y2 = round(13 + y * key_height + key->y * key_height);
      x += key->x;
    } else {
      if (key->action & 1) x = key->x; else x += key->x;
      if (key->action & 2) y = key->y; else y += key->y;
    }
  }
}

// initialize midi key states
static void init_midi_keyboard_states() {
  static byte key_flags[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };

  float x = 12;
  float y = 248;
  float w1 = 9;
  float h1 = 54;
  float w2 = 14;
  float h2 = 90;

  for (int i = 21; i < 21 + 88; i++) {
    MidiKeyState &key = midi_key_states[i];

    // black key
    if (key_flags[(i) % 12]) {
      key.x1 = x - 4;
      key.y1 = y;
      key.x2 = key.x1 + w1;
      key.y2 = key.y1 + h1;
      key.black = true;
    } else {
      key.x1 = x;
      key.y1 = y;
      key.x2 = x + w2;
      key.y2 = y + h2;
      x = x + w2;
      key.black = false;
    }
  }
}


struct DisplayResource {
  LPDIRECT3DTEXTURE9 texture;

  DisplayResource()
    : texture(NULL) {
  }
};

static DisplayResource resources[resource_count];

// set display image
void display_set_image(uint type, const char *name) {
  if (type < resource_count) {
    if (resources[type].texture)
      resources[type].texture->Release();

    resources[type].texture = load_png_from_file(name);
  }
}

// set display default skin
void display_default_skin() {

  for (int i = 0; i < resource_count; i++) {
    if (resources[i].texture)
      resources[i].texture->Release();

#ifdef _DEBUG
    const char *default_skins[] = {
      "background.png",
      "notes.png",
      "keyboard_note_down.png",
      "keyboard_note_up.png",
      "keyboard_note_empty.png",
      "midi_black_down.png",
      "midi_black_up.png",
      "midi_white_down.png",
      "midi_white_up.png",
      "check_button_down.png",
      "check_button_up.png",
      "edit_box.png",
      "round_corner.png",
      "logo.png",
    };

    // try to load texture from develop path
    char temp[256];
    _snprintf(temp, sizeof(temp), "..\\res\\%s", default_skins[i]);
    resources[i].texture = load_png_from_file(temp);
#endif

    if (resources[i].texture == NULL)
      resources[i].texture = load_png_from_resource(MAKEINTRESOURCE(IDR_SKIN_RES0) + i);
  }

}

// get texture size
static void get_texture_size(LPDIRECT3DTEXTURE9 texture, int *width, int *height) {
  D3DSURFACE_DESC desc;
  texture->GetLevelDesc(0, &desc);
  *width = desc.Width;
  *height = desc.Height;
}

// set texture
static void set_texture(LPDIRECT3DTEXTURE9 texture, int width = 0, int height = 0) {
  D3DSURFACE_DESC desc;
  texture->GetLevelDesc(0, &desc);
  float w = width;
  float h = height;
  if (w == 0) w = desc.Width;
  if (h == 0) h = desc.Height;
  D3DMATRIX matrix = {
    1.0f / w, 0, 0, 0,
    0, 1.0f / h, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
  };
  device->SetTexture(0, texture);
  device->SetTransform(D3DTS_TEXTURE0, &matrix);
}

// darw string
static void draw_string(float x, float y, uint color, const char *text, int size, int h_align, int v_align) {
  wchar_t buff[1024];
  static Vertex vertex_buff[1024 * 6];

  // convert text to unicode
  int len = MultiByteToWideChar(_getmbcp(), 0, text, -1, buff, 1024);

  if (len == 0)
    return;

  // preload characters
  preload_characters(buff, len - 1, size);

  // build string vertex
  int count = build_string_vertex(buff, len - 1, size, x, y, color, vertex_buff, ARRAY_COUNT(vertex_buff), h_align, v_align);

  // draw primitives
  set_texture(resource_texture);
  device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, vertex_buff, sizeof(Vertex));
}

// draw sprite
static void draw_sprite(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, uint color1, uint color2) {
  Vertex v[4] = {
    { x1, y1, 0, color1, u1, v1 },
    { x2, y1, 0, color1, u2, v1 },
    { x1, y2, 0, color2, u1, v2 },
    { x2, y2, 0, color2, u2, v2 },
  };

  device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
}

static void draw_sprite(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, uint color) {
  draw_sprite(x1, y1, x2, y2, u1, v1, u2, v2, color, color);
}

// draw image
static void draw_image(uint resource_id, float x1, float x2, float y1, float y2, uint color) {
  // transparent
  if ((color & 0xff000000) == 0)
    return;

  if (resource_id < resource_count) {
    set_texture(resources[resource_id].texture, 1, 1);
    draw_sprite(x1, x2, y1, y2, 0, 0, 1, 1, color);
  }
}

// draw image2
static void draw_image(uint resource_id, float x1, float x2, float y1, float y2, uint color1, uint color2) {
  if ((color1 & 0xff000000) == 0 && (color2 & 0xff000000) == 0)
      return;
    
  if (resource_id < resource_count) {
    set_texture(resources[resource_id].texture, 1, 1);
    draw_sprite(x1, x2, y1, y2, 0, 0, 1, 1, color1, color2);
  }
}

// draw scaleabe sprite
static void draw_sprite_border(
  float x1, float y1, float x2, float y2,
  float u1, float v1, float u2, float v2,
  float lm, float rm, float tm, float bm,
  uint color1, uint color2)
{
  ushort index[18 * 3] = {
    0, 1, 4, 1, 4, 5,
    1, 2, 5, 2, 5, 6,
    2, 3, 6, 3, 6, 7,
    4, 5, 8, 5, 8, 9,
    5, 6, 9, 6, 9, 10,
    6, 7, 10, 7, 10, 11,
    8, 9, 12, 9, 12, 13,
    9, 10, 13, 10, 13, 14,
    10, 11, 14, 11, 14, 15,
  };

  Vertex v[16];

  v[0].x = v[4].x = v[8].x = v[12].x = x1;
  v[1].x = v[5].x = v[9].x = v[13].x = x1 + lm;
  v[2].x = v[6].x = v[10].x = v[14].x = x2 - rm;
  v[3].x = v[7].x = v[11].x = v[15].x = x2;

  v[0].y = v[1].y = v[2].y = v[3].y = y1;
  v[4].y = v[5].y = v[6].y = v[7].y = y1 + tm;
  v[8].y = v[9].y = v[10].y = v[11].y = y2 - bm;
  v[12].y = v[13].y = v[14].y = v[15].y = y2;

  v[0].z = v[1].z = v[2].z = v[3].z = 0;
  v[4].z = v[5].z = v[6].z = v[7].z = 0;
  v[8].z = v[9].z = v[10].z = v[11].z = 0;
  v[12].z = v[13].z = v[14].z = v[15].z = 0;

  v[0].u = v[4].u = v[8].u = v[12].u = u1;
  v[1].u = v[5].u = v[9].u = v[13].u = u1 + lm;
  v[2].u = v[6].u = v[10].u = v[14].u = u2 - rm;
  v[3].u = v[7].u = v[11].u = v[15].u = u2;

  v[0].v = v[1].v = v[2].v = v[3].v = v1;
  v[4].v = v[5].v = v[6].v = v[7].v = v1 + tm;
  v[8].v = v[9].v = v[10].v = v[11].v = v2 - bm;
  v[12].v = v[13].v = v[14].v = v[15].v = v2;

  v[0].color = v[1].color = v[2].color = v[3].color = color1;
  v[4].color = v[5].color = v[6].color = v[7].color = color1;
  v[8].color = v[9].color = v[10].color = v[11].color = color2;
  v[12].color = v[13].color = v[14].color = v[15].color = color2;

  // darw primitive
  device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 16, 18, index, D3DFMT_INDEX16, v, sizeof(Vertex));
}

// draw scaleable sprite
static void draw_sprite_border(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, float lm, float rm, float tm, float bm, uint color) {
  draw_sprite_border(x1, y1, x2, y2, u1, v1, u2, v2, lm, rm , tm, bm, color, color);
}

// draw image
static void draw_image_border(uint resource_id, float x1, float y1, float x2, float y2, float lm, float rm, float tm, float bm, uint color) {
  if (resource_id < resource_count) {
    int w, h;
    get_texture_size(resources[resource_id].texture, &w, &h);
    set_texture(resources[resource_id].texture, w, h);
    draw_sprite_border(x1, y1, x2, y2, 0, 0, w, h, lm, rm, tm, bm, color, color);
  }
}

static void draw_image_border(uint resource_id, float x1, float y1, float x2, float y2, float lm, float rm, float tm, float bm, uint color1, uint color2) {
  if (resource_id < resource_count) {
    int w, h;
    get_texture_size(resources[resource_id].texture, &w, &h);
    set_texture(resources[resource_id].texture, w, h);
    draw_sprite_border(x1, y1, x2, y2, 0, 0, w, h, lm, rm, tm, bm, color1, color2);
  }
}

static void hsv2rgb(float &r, float &g, float &b, float h, float s, float v) {
  int in;
  float fl;
  float m,n;
  h = clamp_value<float>(h, 0, 360);
  s = clamp_value<float>(s, 0, 1);
  v = clamp_value<float>(v, 0, 1);
  in = (int)floor(h / 60);
  fl = (h / 60) - in;

  if( !(in & 1) ) fl = 1 - fl;
  m = v * (1 - s);
  n = v * (1 - s * fl);
  switch(in) {
  case 0: r = v; g = n; b = m; break;
  case 1: r = n; g = v; b = m; break;
  case 2: r = m; g = v; b = n; break;
  case 3: r = m; g = n; b = v; break;
  case 4: r = n; g = m; b = v; break;
  case 5: r = v; g = m; b = n; break; 
  }
}

static void rgb2hsv( float &H, float &S, float &V, float R, float G, float B )
{
  float Z;
  float r,g,b;
  V = std::max(R, std::max(G, B));
  Z = std::min(R, std::min(G, B));

  if( V != 0.0f )
    S = (V - Z) / V;
  else
    S = 0.0f;

  if((V - Z) != 0) {
    r = (V - R) / (V - Z);
    g = (V - G) / (V - Z);
    b = (V - B) / (V - Z);
  } else {
    r = g = b = 0;
  }

  if( V == R )
    H = 60 * ( b - g );
  else if( V == G )
    H = 60 * (2 + r - b);
  else
    H = 60 * (4 + g - r);

  if( H < 0.0f )
    H = H + 360;
}

static inline void rgba2uint(float r, float g, float b, float a, uint &rgba) {
  rgba =
    clamp_value<uint>(a * 255, 0, 255) << 24 |
    clamp_value<uint>(r * 255, 0, 255) << 16 |
    clamp_value<uint>(g * 255, 0, 255) << 8 |
    clamp_value<uint>(b * 255, 0, 255) << 0;
}

static inline void uint2rgba(uint value, float &r, float &g, float &b, float &a) {
  a = ((value >> 24) & 0xff) / 255.f;
  r = ((value >> 16) & 0xff) / 255.f;
  g = ((value >> 8) & 0xff) / 256.f;
  b = ((value >> 0) & 0xff) / 255.f;
}

static inline uint hsv2color(float h, float s, float v, float alpha) {
  uint color;
  float r, g, b;
  hsv2rgb(r, g, b, h, s, v);
  rgba2uint(r, g, b, alpha, color);
  return color;
}

static void update_keyboard(double fade) {
  for (KeyboardState *key = keyboard_states; key < keyboard_states + 256; key++) {
    if (key->x2 > key->x1) {
      key_bind_t map;

      // get keyboard map
      config_bind_get_keydown(key - keyboard_states, &map, 1);

      // bg image
      uint bg_image = map.a ? keyboard_note_up : keyboard_note_empty;
      if (key->bg_image != bg_image) {
        key->bg_image = bg_image;
        display_dirty = true;
      }

      // key note
      int note = -1;
      int notename = -1;
      if (map.a == SM_NOTE_ON || map.a == SM_NOTE_OFF) {
        byte ch = map.b;
        note = map.c + config_get_key_octshift(ch) * 12 + config_get_key_transpose(ch);

        switch (config_get_note_display()) {
        case NOTE_DISPLAY_DOH:
          note = clamp_value(note, 0, 127);
          break;

        case NOTE_DIAPLAY_FIXED_DOH:
          note += config_get_key_signature();
          note = clamp_value(note, 0, 127);
          break;

        case NOTE_DISPLAY_NAME:
          note += config_get_key_signature();
          notename = note;
          note = -1;
          break;
        }
      }
      // key note changed
      if (note != key->note) {
        key->note = note;
        display_dirty = true;
      }
      if (notename != key->notename) {
        key->notename = notename;
        key->note = note;
      }

      // key label changed
      const char *label = config_bind_get_label(key - keyboard_states);
      if (strcmp(label, key->label)) {
        strcpy_s(key->label, label);
        display_dirty = true;
      }

      // keymap changed
      if (map.a != key->map.a || map.b != key->map.b || map.c != key->map.c || map.d != key->map.d) {
        int size = config_default_keylabel(key->label1, sizeof(key->label1), map);
        key->label1[size] = 0;
        key->map = map;
        display_dirty = true;
      }

      // fade
      if (keyboard_get_status(key - keyboard_states) || (keyboard_states + gui_get_selected_key() == key)) {
        key->fade = 1.0f;
      }
      else {
        float to = config_get_preview_color() / 100.f;
        key->fade = to + fade * (key->fade - to);
      }

      // color
      float h = 30;
      float s = 0;
      float v = 0.9f;

      uint key_color = config_bind_get_color(key - keyboard_states);

      if (key_color != 0) {
        float r, g, b, a;
        uint2rgba(key_color, r, g, b, a);
        rgb2hsv(h, s, v, r, g, b);
      }
      else {
        if (config_get_auto_color() == AUTO_COLOR_VELOCITY) {
          if (map.a) {
            switch (map.a) {
            case SM_NOTE_ON:
            case SM_NOTE_OFF:
            case SM_NOTE_PRESSURE:
              {
                int vel = map.d * config_get_key_velocity(map.b) / 256;
                vel = clamp_value<int>(vel, 0, 64);
                h = 64 - vel;
                s = 1;
                v = 0.9f;
              }
              break;

            default:
              h = 120;
              s = 1;
              v = 0.9f;
              break;
            }
          }
        }
        else if (config_get_auto_color() == AUTO_COLOR_CHANNEL) {
          if (map.a) {
            static float channel_colors[16] = {
              30, 10, 350, 330, 310, 290, 270, 210, 190, 170, 150, 130, 110, 90, 70, 50, 
            };
            switch (map.a) {
            case SM_OCTAVE:
            case SM_VELOCITY:
            case SM_CHANNEL:
            case SM_TRANSPOSE:
            case SM_NOTE_OFF:
            case SM_NOTE_ON:
            case SM_NOTE_PRESSURE:
            case SM_PRESSURE:
            case SM_PITCH:
            case SM_PROGRAM:
            case SM_BANK_MSB:
            case SM_BANK_LSB:
            case SM_SUSTAIN:
            case SM_MODULATION:
            case SM_FOLLOW_KEY:
              h = channel_colors[map.b & 0x0f];
              s = 1;
              v = 0.9f;
              break;

            default:
              h = 120;
              s = 1;
              v = 0.9f;
              break;
            }
          }
        }
        else {
          h = 30;
          s = 1;
          v = 0.9f;
        }
      }

      // active color
      uint color1 = hsv2color(h, s - 0.3f, v + 0.1f, key->fade);
      uint color2 = hsv2color(h, s - 0.0f, v + 0.0f, key->fade);

      // check if color is changed
      if (color1 != key->color1 || color2 != key->color2) {
        key->color1 = color1;
        key->color2 = color2;
        display_dirty = true;
      }
    }
  }
}

// draw keyboard
static void draw_keyboard() {
  for (KeyboardState *key = keyboard_states; key < keyboard_states + 256; key++) {
    if (key->x2 > key->x1) {
      float x1 = key->x1;
      float y1 = key->y1;
      float x2 = key->x2;
      float y2 = key->y2;

      draw_image_border(key->bg_image, x1, y1, x2, y2, 6, 6, 6, 6, 0xffffffff);
      draw_image_border(keyboard_note_down, x1, y1, x2, y2, 6, 6, 6, 6, key->color1, key->color2);

      // key label
      if (key->label[0]) {
        draw_string(floor((x1 + x2 - 1) * 0.5f), floor((y1 + y2 - 1) * 0.5f), 0xff6e6e6e, key->label, 10, 1, 1);
      }
      // draw note
      else if (key->note != -1) {
        if (key->note >= 12 && key->note <= 120) {
          if (resources[notes].texture) {
            float x3 = round(x1 + (x2 - x1 - 25.f) * 0.5f);
            float x4 = x3 + 25.f;
            float y3 = round(y1 + (y2 - y1 - 25.f) * 0.5f);
            float y4 = y3 + 25.f;
            float u3 = ((key->note - 12) % 12) * 25.f;
            float v3 = ((key->note - 12) / 12) * 25.f;
            float u4 = u3 + 25.f;
            float v4 = v3 + 25.f;

            set_texture(resources[notes].texture);
            draw_sprite(x3, y3, x4, y4, u3, v3, u4, v4, 0xffffffff);
          }
        }
        else {
          const char* note_name = config_get_note_name(key->note);
          draw_string(floor((x1 + x2 - 1) * 0.5f), floor((y1 + y2 - 1) * 0.5f), 0xff6e6e6e, note_name, 10, 1, 1);
        }
      }
      else if (key->notename != -1) {
        const char* note_name = config_get_note_name(key->notename);
        draw_string(floor((x1 + x2 - 1) * 0.5f), floor((y1 + y2 - 1) * 0.5f), 0xff6e6e6e, note_name, 10, 1, 1);
      }
      else if (key->label1[0]) {
        draw_string(floor((x1 + x2 - 1) * 0.5f), floor((y1 + y2 - 1) * 0.5f), 0xff6e6e6e, key->label1, 10, 1, 1);
      }
    }
  }
}

static void update_midi_keyboard(double fade) {
  for (MidiKeyState *key = midi_key_states; key < midi_key_states + 127; key++) {
    if (key->x2 > key->x1) {
      byte note = key - midi_key_states;

      if (config_get_midi_transpose()) {
        note += config_get_key_signature();
      }

      if (midi_get_note_status(config_get_output_channel(SM_INPUT_0), note) ||
          midi_get_note_status(config_get_output_channel(SM_INPUT_1), note))
        key->fade = 1.0f;
      else
        key->fade = 0 + fade * (key->fade - 0);

      uint color1;
      uint color2;

      if (config_get_auto_color() == AUTO_COLOR_CLASSIC) {
        color1 = hsv2color(27, 0.3f, 1.0f, key->fade);
        color2 = hsv2color(30, 1.0f, 1.0f, key->fade);
      }
      else {
        int vel1 = midi_get_note_pressure(config_get_output_channel(SM_INPUT_0), note);
        int vel2 = midi_get_note_pressure(config_get_output_channel(SM_INPUT_1), note);
        int vel = std::max(vel1, vel2);

        vel = 64 - clamp_value<int>(vel / 2, 0, 64);
        color1 = hsv2color(vel, 0.3f, 1.0f, key->fade);
        color2 = hsv2color(vel, 1.0f, 1.0f, key->fade);
      }

      if (color1 != key->color1 || color2 != key->color2) {
        key->color1 = color1;
        key->color2 = color2;
        display_dirty = true;
      }
    }
  }
}

// draw midi keyboard
static void draw_midi_keyboard() {
  for (MidiKeyState *key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++) {
    if (!key->black) {
      if (key->x2 > key->x1) {
        draw_image(midi_white_up, key->x1, key->y1, key->x2, key->y2, 0xffffffff);
        draw_image(midi_white_down, key->x1, key->y1, key->x2, key->y2, key->color1, key->color2);
      }
    }
  }

  for (MidiKeyState *key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++) {
    if (key->black) {
      if (key->x2 > key->x1) {
        draw_image(midi_black_up, key->x1, key->y1, key->x2, key->y2, 0xffffffff);
        draw_image(midi_black_down, key->x1, key->y1, key->x2, key->y2, key->color1, key->color2);
      }
    }
  }
}

// control commands
enum ControlCommands {
  CMD_NONE,
  CMD_SUSTAIN,
  CMD_MIDI_KEY,
  CMD_RECORD,
  CMD_PLAY,
  CMD_VELOCITY_LEFT,
  CMD_VELOCITY_RIGHT,
  CMD_OCTSHIFT_LEFT,
  CMD_OCTSHIFT_RIGHT,
  CMD_GROUP,
  CMD_TIME,
};

// control type
enum ControlType {
  CTL_TEXTBOX,
  CTL_LABEL,
  CTL_GROUP,
  CTL_BUTTON,
};

static struct gui_control_t {
  uint type;
  uint command;
  int x, y, w, h;
  uint label;
  uint image;
  uint color;
  char text[256];
}
controls[] = {
  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_LABEL,    CMD_NONE,            0, 219, 44, 15, IDS_DISPLAY_GROUP },
  { CTL_TEXTBOX,  CMD_GROUP,           0, 219, 38, 15  },

  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_LABEL,    CMD_NONE,            0, 219, 50, 15, IDS_DISPLAY_SUSTAIN },
  { CTL_TEXTBOX,  CMD_SUSTAIN,         0, 219, 38, 15  },

  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_LABEL,    CMD_NONE,            0, 219, 34, 15, IDS_DISPLAY_KEY },
  { CTL_TEXTBOX,  CMD_MIDI_KEY,        0, 219, 46, 15 },

  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_BUTTON,   CMD_RECORD,          0, 218, 18, 18, },
  { CTL_LABEL,    CMD_RECORD,          0, 219, 34, 15, IDS_DISPLAY_REC },
  { CTL_TEXTBOX,  CMD_TIME,            0, 219, 44, 15 },
  { CTL_BUTTON,   CMD_PLAY,            0, 218, 18, 18, },
  { CTL_LABEL,    CMD_PLAY,            0, 219, 38, 15, IDS_DISPLAY_PLAY },

  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_LABEL,    CMD_NONE,            0, 219, 60, 15, IDS_DISPLAY_VELOCITY },
  { CTL_TEXTBOX,  CMD_VELOCITY_LEFT,   0, 219, 38, 15 },
  { CTL_TEXTBOX,  CMD_VELOCITY_RIGHT,  0, 219, 38, 15 },

  { CTL_GROUP,    CMD_NONE,            0, 214, 0, 25  },
  { CTL_LABEL,    CMD_NONE,            0, 219, 60, 15, IDS_DISPLAY_OCTSHIFT },
  { CTL_TEXTBOX,  CMD_OCTSHIFT_LEFT,   0, 219, 38, 15 },
  { CTL_TEXTBOX,  CMD_OCTSHIFT_RIGHT,  0, 219, 38, 15 },
};

static gui_control_t* find_control(uint type, uint command) {
  for (uint i = 0; i < ARRAYSIZE(controls); i++) {
    if (controls[i].command == command && controls[i].type == type) {
      return &controls[i];
    }
  }
  return NULL;
}

static void control_set_text(gui_control_t *ctl, const char *format, ...) {
  char buff[sizeof(ctl->text)];

  va_list args;
  va_start(args, format);
  vsprintf_s(buff, format, args);
  va_end(args);

  if (strcmp(buff, ctl->text)) {
    strcpy_s(ctl->text, buff);
    display_dirty = true;
  }
}

static void control_set_image(gui_control_t *ctl, uint image, uint color) {
  if (ctl->image != image) {
    ctl->image = image;
    display_dirty = true;
  }

  if (ctl->color != color) {
    ctl->color = color;
    display_dirty = true;
  }
}

static void init_keyboard_controls() {
  int i, j;
  int x = 10;

  for (i = 0; i < ARRAYSIZE(controls); ++i) {
    if (controls[i].type != CTL_GROUP)
      continue;

    for (j = i + 1; j < ARRAYSIZE(controls); ++j) {
      if (controls[j].type == CTL_GROUP)
        break;
    }

    controls[i].x = x;
    x += 4;

    for (int k = i + 1; k < j; k++) {
      controls[k].x = x;
      x += controls[k].w;
    }

    x += 4;
    controls[i].w = x - controls[i].x;

    x += 2;
  }
}

// update keyboard controls
static void update_keyboard_controls() {
  gui_control_t *ctl;

  // sustain pedal
  {
    byte sustain = config_get_controller(SM_INPUT_0, 0x40);
    ctl = find_control(CTL_TEXTBOX, CMD_SUSTAIN);

    if (sustain < 128)
      control_set_text(ctl, "%d", config_get_controller(SM_INPUT_0, 0x40));
    else
      control_set_text(ctl, "-");
  }

  // group
  ctl = find_control(CTL_TEXTBOX, CMD_GROUP);
  control_set_text(ctl, "%d", config_get_setting_group());

  // midi key
  ctl = find_control(CTL_TEXTBOX, CMD_MIDI_KEY);
  switch (config_get_key_signature()) {
   case 0:     control_set_text(ctl, "C(0)"); break;
   case 1:     control_set_text(ctl, "bD(+1)"); break;
   case 2:     control_set_text(ctl, "D(+2)"); break;
   case 3:     control_set_text(ctl, "bE(+3)"); break;
   case 4:     control_set_text(ctl, "E(+4)"); break;
   case 5:     control_set_text(ctl, "F(+5)"); break;
   case 6:     control_set_text(ctl, "#F(+6)"); break;
   case 7:     control_set_text(ctl, "G(+7)"); break;
   case -4:    control_set_text(ctl, "bA(-4)"); break;
   case -3:    control_set_text(ctl, "A(-3)"); break;
   case -2:    control_set_text(ctl, "bB(-2)"); break;
   case -1:    control_set_text(ctl, "B(-1)"); break;
   default:    control_set_text(ctl, "%d", config_get_key_signature()); break;
  }

  // record
  ctl = find_control(CTL_BUTTON, CMD_RECORD);
  if (song_is_recording()) {
    control_set_image(ctl, check_button_down, 0xffd23b36);
  } else {
    control_set_image(ctl, check_button_up, 0xff4d3b36);
  }

  // timer
  ctl = find_control(CTL_TEXTBOX, CMD_TIME);
  control_set_text(ctl, "%d:%02d", song_get_time() / 1000 / 60, song_get_time() / 1000 % 60);

  // play
  ctl = find_control(CTL_BUTTON, CMD_PLAY);
  if (song_is_playing()) {
    control_set_image(ctl, check_button_down, 0xff53ce36);
  } else {
    control_set_image(ctl, check_button_up, 0xff4d5c37);
  }

  // velocity
  ctl = find_control(CTL_TEXTBOX, CMD_VELOCITY_LEFT);
  control_set_text(ctl, "%d", config_get_key_velocity(0));
  ctl = find_control(CTL_TEXTBOX, CMD_VELOCITY_RIGHT);
  control_set_text(ctl, "%d", config_get_key_velocity(1));

  // octshift
  {
    int s1 = config_get_key_octshift(0);
    int s2 = config_get_key_octshift(1);

    ctl = find_control(CTL_TEXTBOX, CMD_OCTSHIFT_LEFT);
    control_set_text(ctl, "%s%d", s1 > 0 ? "+" : s1 < 0 ? "-" : "", abs(s1));

    ctl = find_control(CTL_TEXTBOX, CMD_OCTSHIFT_RIGHT);
    control_set_text(ctl, "%s%d", s2 > 0 ? "+" : s2 < 0 ? "-" : "", abs(s2));
  }
}

// darw keyboard controls
static void draw_keyboard_controls() {
  // draw controls
  for (int i = 0; i < ARRAY_COUNT(controls); i++) {
    gui_control_t &ctl = controls[i];

    switch (ctl.type) {
      case CTL_GROUP:
        draw_image_border(round_corner, ctl.x, ctl.y, ctl.x + ctl.w, ctl.y + ctl.h, 4, 4, 4, 4, 0xffffffff);
        break;

      case CTL_LABEL:
        draw_string(ctl.x + ctl.w / 2, ctl.y + ctl.h / 2 + 1, 0xffbdb8b7, lang_load_string(ctl.label), 11, 1, 1);
        break;

      case CTL_TEXTBOX:
        draw_image_border(edit_box, ctl.x, ctl.y, ctl.x + ctl.w, ctl.y + ctl.h, 7, 7, 3, 3, 0xffffffff);
        draw_string(ctl.x + ctl.w / 2, ctl.y + ctl.h / 2 + 1, 0xff6e6e6e, ctl.text, 11, 1, 1);
        break;

      case CTL_BUTTON:
        draw_image(ctl.image, ctl.x, ctl.y, ctl.x + ctl.w, ctl.y + ctl.h, ctl.color);
        break;
    }
  }
}

static void setup_matrix(float x, float y, float width, float height) {
  float sx = 2.0f / width;
  float sy = -2.0f / height;
  float tx = -1 - sx * 0.5f;
  float ty = 1 - sy * 0.5f;

  D3DMATRIX projection = {
    sx, 0, 0, 0,
    0, sy, 0, 0,
    0, 0,  1, 0,
    tx, ty, 0, 1,
  };

  D3DMATRIX view = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    x, y, 0, 1,
  };

  device->SetTransform(D3DTS_PROJECTION, &projection);
  device->SetTransform(D3DTS_VIEW, &view);
}

static int fps_frames = 0;

static void display_update_fps() {
}

// draw
static void display_draw() {
  static DWORD frames = 0;

  // begin scene
  if (SUCCEEDED(device->BeginScene())) {
    float width = (float)display_get_width();
    float height = (float)display_get_height();

    IDirect3DSurface9 *surface;
    if (SUCCEEDED(render_texture->GetSurfaceLevel(0, &surface))) {
      device->SetRenderTarget(0, surface);
      setup_matrix(0, 0, width, height);
      draw_image(background, 0, 0, (float)display_get_width(), (float)display_get_height(), 0xffffffff);
      draw_image(logo, 624, 14, 624 + 115, 14 + 27, 0xffffffff);
      draw_keyboard();
      draw_midi_keyboard();
      draw_keyboard_controls();
      surface->Release();
    }

    device->SetRenderTarget(0, back_buffer);

#if !SCALE_DISPLAY
    device->Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 0, 0);
    setup_matrix((display_width - width) / 2, (display_height - height) / 2, display_width, display_height);
#endif
    set_texture(render_texture);
    draw_sprite(0, 0, width, height, 0, 0, width, height, 0xffffffff);


#ifdef _DEBUG
    if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_F1)) {
      // clear backbuffer
      device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff00ff00, 0, 0);

      // set white texture
      set_texture(resource_texture);

      // draw resource texture
      draw_sprite(0, 0, 512, 512, 0, 0, 512, 512, 0xffffffff);
    }
#endif
    device->EndScene();
    ++fps_frames;
  }
}

// update display
static void display_update() {
  // update timer
  static double old_time = 0;
  double new_time = song_get_clock();
  if (old_time > new_time)
    old_time = new_time;

  // calculate fade
  double fade = 0;
  if (config_get_key_fade()) {
    double fade_speed = 10 * (1 - config_get_key_fade() / 100.0);

    if (fade_speed < 0.1)
      fade_speed = 0.1;

    fade = pow(0.001, (new_time - old_time) / 1000 * fade_speed);
  }

  // update keyboard
  update_keyboard(fade);

  // update midi keyboard
  update_midi_keyboard(fade);

  // update keyboard controls
  update_keyboard_controls();

  // remember time
  old_time = new_time;

  // update fps
  if (1) {
    static DWORD last_time = 0;
    static DWORD FPS = 0;

    DWORD time = GetTickCount();
    if (time - last_time > 1000) {
      FPS = fps_frames;
      fps_frames = 0;
      last_time = time;

#ifdef _DEBUG
      char buff[256];
      sprintf_s(buff, APP_NAME" FPS: %d\n", FPS);
      SetWindowText(gui_get_window(), buff);
#endif
    }
  }
}

// draw main window
void display_render() {
  display_update();

  if (device == NULL)
    return;

  if (display_dirty) {
    display_dirty = false;
    display_draw();
    display_present();
  }
}

// present
void display_present() {
  HRESULT hr = device->Present(NULL, NULL, NULL, NULL);
  if (hr == D3DERR_DEVICELOST) {
    hr = device->TestCooperativeLevel();

    if (D3DERR_DEVICENOTRESET == hr) {
      d3d_device_lost();
      d3d_device_reset();
    }
  }
}

// refresh display
void display_force_refresh() {
  // make all map dirty
  for (KeyboardState *key = keyboard_states; key < keyboard_states + 256; key++) {
    key->map.a = 0;
  }
  display_dirty = true;
}

// init display
int display_init(HWND hwnd) {
  display_hwnd = hwnd;

  // initialize font
  if (font_initialize()) {
    return -1;
  }

  // initialize directx 9
  if (d3d_initialize(display_hwnd)) {
    return -1;
  }

  // reset device
  if (d3d_device_reset()) {
    return -1;
      }

  // load default skin
  display_default_skin();

  // initialize keyboard display
  init_keyboard_states();
  init_midi_keyboard_states();
  init_keyboard_controls();

  return 0;
}

// shutdown display
int display_shutdown() {
  for (int i = 0; i < resource_count; i++) {
    if (resources[i].texture)
      resources[i].texture->Release();
  }

  d3d_shutdown();
  display_hwnd = NULL;

  return 0;
}

// get display window handle
HWND display_get_hwnd() {
  return display_hwnd;
}

// find keyboard key
static int find_keyboard_key(int x, int y) {
  // find pointed key
  for (KeyboardState *key = keyboard_states; key < keyboard_states + 256; key++) {
    if (key->x2 > key->x1) {
      if (x >= key->x1 && x < key->x2 && y >= key->y1 && y < key->y2) {
        return key - keyboard_states;
      }
    }
  }
  return -1;
}

// find midi note
static int find_midi_note(int x, int y, int *velocity = NULL) {
  for (int black = 0; black < 2; black++) {
    for (MidiKeyState *key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++) {
      if ((int)key->black != black) {
        if (key->x2 > key->x1) {
          if (x >= key->x1 && x < key->x2 && y >= key->y1 && y < key->y2) {
            if (velocity)
              *velocity = (int)(127 * (y - key->y1) / (key->y2 - key->y1));

            return key - midi_key_states;
          }
        }
      }
    }
  }

  return -1;
}

// find buttons
static int find_control_button(int x, int y) {
  for (int i = 0; i < ARRAY_COUNT(controls); i++) {
    gui_control_t &ctl = controls[i];

    if (ctl.command != CMD_NONE) {
      if (x >= ctl.x && x <= ctl.x + ctl.w &&
        y >= ctl.y && y <= ctl.y + ctl.h) {
          return ctl.command;
      }
    }
  }

  return CMD_NONE;
}

// dispatch command
void dispatch_command(int command, int action) {
  switch (command) {
   case CMD_SUSTAIN:
     song_send_event(SM_SUSTAIN, 0, SM_VALUE_FLIP, 127, true);
     break;

   case CMD_MIDI_KEY:
     song_send_event(SM_KEY_SIGNATURE, action, 1, 0, true);
     break;

   case CMD_RECORD:
     if (song_is_recording())
       song_stop_record();
     else
       song_start_record();
     break;

   case CMD_PLAY:
     if (song_is_playing())
       song_stop_playback();
     else
       song_start_playback();
     break;

   case CMD_VELOCITY_LEFT:
     song_send_event(SM_VELOCITY, 0, action, 10, true);
     break;

   case CMD_VELOCITY_RIGHT:
     song_send_event(SM_VELOCITY, 1, action, 10, true);
     break;

   case CMD_OCTSHIFT_LEFT:
     song_send_event(SM_OCTAVE, 0, action, 1, true);
     break;

   case CMD_OCTSHIFT_RIGHT:
     song_send_event(SM_OCTAVE, 1, action, 1, true);
     break;

   case CMD_GROUP:
     song_send_event(SM_SETTING_GROUP, action, 1, 0, true);
     break;
  }
}

// convert canvas position to client position
static void client_to_canvas(int &x, int &y) {
  RECT rect;
  GetClientRect(display_hwnd, &rect);

  if (rect.right > rect.left &&
      rect.bottom > rect.top) {
#if SCALE_DISPLAY
    x = x * display_width / (rect.right - rect.left);
    y = y * display_height / (rect.bottom - rect.top);
#else
    x -= (display_width - display_get_width()) / 2;
    y -= (display_height - display_get_height()) / 2;
#endif
  }
}

// convert client position to canvas position
static void canvas_to_client(int &x, int &y) {
  RECT rect;
  GetClientRect(display_hwnd, &rect);

  if (rect.right > rect.left &&
      rect.bottom > rect.top) {
#if SCALE_DISPLAY
    x = x * (rect.right - rect.left) / display_width;
    y = y * (rect.bottom - rect.top) / display_height;
#else
    x += (display_width - display_get_width()) / 2;
    y += (display_height - display_get_height()) / 2;
#endif
  }
}

// get key rect
void display_get_key_rect(int keycode, int &x1, int &y1, int &x2, int &y2) {
  x1 = keyboard_states[keycode].x1;
  y1 = keyboard_states[keycode].y1;
  x2 = keyboard_states[keycode].x2;
  y2 = keyboard_states[keycode].y2;

  canvas_to_client(x1, y1);
  canvas_to_client(x2, y2);
}

// mouse control
static int mouse_control(HWND window, uint msg, int x, int y, int z) {
  if (export_rendering())
    return 0;

  static int previous_keycode = -1;
  static int previous_midi_note = -1;

  int keycode = -1;
  int midinote = -1;
  int velocity = 127;
  int handled = false;

  // raw mouse position
  POINT point = {x, y};

  // adjust mouse position
  client_to_canvas(x, y);


  switch (msg) {
   case WM_LBUTTONDOWN:
   case WM_LBUTTONDBLCLK: {
     int button = find_control_button(x, y);
     if (button != CMD_NONE) {
       dispatch_command(button, 1);
       return 0;
     } else {
       SetCapture(window);
     }
   }
   break;

   case WM_LBUTTONUP:
     handled = true;
     ReleaseCapture();
     break;

   case WM_RBUTTONDOWN:
   case WM_RBUTTONDBLCLK: {
     int button = find_control_button(x, y);
     if (button != CMD_NONE) {
       dispatch_command(button, 2);
       return 0;
     }
   }
   handled = true;
   break;

   case WM_RBUTTONUP:
     keycode = find_keyboard_key(x, y);

     if (keycode != -1) {
       ClientToScreen(window, &point);
       gui_popup_keymenu(keycode, point.x, point.y);
       return 0;
     }

     handled = true;
     break;

   case WM_MOUSEMOVE:
     if (GetCapture() != window)
       handled = true;
     break;


   case WM_MOUSEWHEEL: {
     int button = find_control_button(x, y);
     if (button != CMD_NONE &&
         button != CMD_RECORD &&
         button != CMD_PLAY)
       dispatch_command(button, z > 0 ? 1 : 2);

     handled = true;
   }
   break;
  }

  if (!handled) {
    keycode = find_keyboard_key(x, y);
    midinote = find_midi_note(x, y, &velocity);

    if (midinote != -1) {
      midinote -= config_get_key_octshift(0) * 12;

      if (!config_get_midi_transpose()) {

        if (config_get_follow_key(0))
          midinote = midinote - config_get_key_signature();
      }
      else {
        if (!config_get_follow_key(0))
          midinote = midinote + config_get_key_signature();
      }
      midinote = clamp_value(midinote, 0, 127);
    }
  }

  // update keyboard button
  if (keycode != previous_keycode) {
    if (previous_keycode != -1)
      song_send_event(SM_SYSTEM, SMS_KEY_EVENT, previous_keycode, 0, true);

    if (keycode != -1)
      song_send_event(SM_SYSTEM, SMS_KEY_EVENT, keycode, 1, true);

    previous_keycode = keycode;
  }

  // update midi button
  if (midinote != previous_midi_note) {
    if (previous_midi_note != -1)
      song_send_event(SM_NOTE_OFF, 0, previous_midi_note, 0, true);

    if (midinote != -1)
      song_send_event(SM_NOTE_ON, 0, midinote, velocity, true);

    previous_midi_note = midinote;
  }

  return 0;
}

// mouse menu
static void mouse_menu(int x, int y) {
  POINT point = {x, y};

  client_to_canvas(x, y);

  int keycode = find_keyboard_key(x, y);

  if (keycode != -1) {
    ClientToScreen(display_hwnd, &point);
    gui_popup_keymenu(keycode, point.x, point.y);
  }
}

#define rgbtoy(b, g, r, y) \
  y = (unsigned char)(((int)(30 * r) + (int)(59 * g) + (int)(11 * b)) / 100)

#define rgbtouv(b, g, r, u, v) \
  u = (unsigned char)(((int)(-17 * r) - (int)(33 * g) + (int)(50 * b) + 12800) / 100); \
  v = (unsigned char)(((int)(50 * r) - (int)(42 * g) - (int)(8 * b) + 12800) / 100)

// capture bitmap
static int capture_bitmap_I420(unsigned char * *planes, int *strikes) {
  HRESULT hr;
  D3DLOCKED_RECT rect;
  IDirect3DTexture9 *texture = NULL;
  IDirect3DSurface9 *src_surface = NULL;
  IDirect3DSurface9 *dst_surface = NULL;
  if (FAILED(hr = device->CreateTexture(display_get_width(), display_get_height(), 1,  0,
                                        D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &texture, NULL)))
    goto cleanup;

  if (FAILED(hr = texture->GetSurfaceLevel(0, &dst_surface)))
    goto cleanup;

  if (FAILED(hr = render_texture->GetSurfaceLevel(0, &src_surface)))
    goto cleanup;

  if (FAILED(hr = device->GetRenderTargetData(src_surface, dst_surface)))
    goto cleanup;

  if (SUCCEEDED(hr = texture->LockRect(0, &rect, NULL, D3DLOCK_READONLY))) {
    int height = display_get_height();
    int width = display_get_width();

    for (int y = 0; y < height / 2; y++) {
      BYTE *yline = planes[0] + y * 2 * strikes[0];
      BYTE *uline = planes[1] + y * strikes[1];
      BYTE *vline = planes[2] + y * strikes[2];
      BYTE *rgb = (BYTE *)rect.pBits + rect.Pitch * y * 2;

      for (int x = 0; x < width / 2; x++) {
        uint sr = 0, sg = 0, sb = 0;
        rgbtoy(rgb[0], rgb[1], rgb[2], yline[0]); sr += rgb[0]; sg += rgb[1]; sb += rgb[2];
        rgb += rect.Pitch; yline += strikes[0];
        rgbtoy(rgb[0], rgb[1], rgb[2], yline[0]); sr += rgb[0]; sg += rgb[1]; sb += rgb[2];
        rgb += 4; yline++;
        rgbtoy(rgb[0], rgb[1], rgb[2], yline[0]); sr += rgb[0]; sg += rgb[1]; sb += rgb[2];
        rgb -= rect.Pitch; yline -= strikes[0];
        rgbtoy(rgb[0], rgb[1], rgb[2], yline[0]); sr += rgb[0]; sg += rgb[1]; sb += rgb[2];

        rgb += 4; yline++;
        sr /= 4; sg /= 4; sb /= 4;
        rgbtouv(sr, sg, sb, *uline, *vline);
        uline++;
        vline++;
      }
    }
    texture->UnlockRect(0);
    hr = S_OK;
  }

cleanup:
  SAFE_RELEASE(src_surface);
  SAFE_RELEASE(dst_surface);
  SAFE_RELEASE(texture);
  return hr == S_OK;
}

// display process emssage
int display_process_message(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_ACTIVATE:
     if (WA_INACTIVE == LOWORD(wParam))
       mouse_control(hWnd, WM_LBUTTONUP, -1, -1, 0);
     break;

   case WM_SIZE:
     d3d_device_lost();
     d3d_device_reset();
     display_render();
     break;

   case WM_LBUTTONDOWN:
   case WM_LBUTTONDBLCLK:
   case WM_LBUTTONUP:
   case WM_MOUSEMOVE:
   case WM_RBUTTONDOWN:
   case WM_RBUTTONDBLCLK:
   case WM_RBUTTONUP:
     mouse_control(hWnd, uMsg, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0);
     break;

   case WM_MOUSEWHEEL: {
     POINT cursor_pos;
     GetCursorPos(&cursor_pos);
     ScreenToClient(hWnd, &cursor_pos);
     mouse_control(hWnd, uMsg, cursor_pos.x, cursor_pos.y, (short)HIWORD(wParam) / 120);
   }
   break;

   case WM_USER + 10: {
     unsigned char * *planes = (unsigned char * *)wParam;
     int *strikes = (int *)lParam;
     display_update();
     display_draw();
     return capture_bitmap_I420(planes, strikes);
   }
   break;
  }

  return 0;
}

// capture bitmap
int display_capture_bitmap_I420(unsigned char * *planes, int *strikes) {
  return SendMessage(gui_get_window(), WM_USER + 10, (WPARAM)planes, (LPARAM)strikes);
}