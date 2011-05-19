#include "pch.h"
#include <d3d9.h>
#include <dinput.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <mbctype.h>

#include <set>

#include "png.h"

#define USE_LIB_FREE_TYPE		1

#if USE_LIB_FREE_TYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include <Knownfolders.h>
#include <Shlobj.h>
#endif


#include "display.h"
#include "keyboard.h"
#include "gui.h"
#include "config.h"
#include "song.h"
#include "../res/resource.h"


using namespace display_resource;

// directX device handle.
static LPDIRECT3D9 d3d9 = NULL;
static LPDIRECT3DDEVICE9 device = NULL;
static LPDIRECT3DTEXTURE9 resource_texture = NULL;

// main window handle.
static HWND display_hwnd = NULL;

static int display_width = 0;
static int display_height = 0;

#define SCALE_DISPLAY 1

// vertex format
struct Vertex
{
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

static inline float round(float x) { return floor(x + 0.5f); }

// -----------------------------------------------------------------------------------------
// texture management
// -----------------------------------------------------------------------------------------
struct texture_node_t
{
	// child
	texture_node_t * parent;
	texture_node_t * child1;
	texture_node_t * child2;

	// rect in texture
	int min_u, min_v, max_u, max_v;

	// size
	int width, height;

	// used
	bool used;

	// constructor
	texture_node_t(texture_node_t * parent, int x1, int y1, int x2, int y2)
		: min_u(x1)
		, min_v(y1)
		, max_u(x2)
		, max_v(y2)
		, width(x2 - x1)
		, height(y2 - y1)
		, used(false)
		, parent(parent)
		, child1(NULL)
		, child2(NULL)
	{
	}

	// destructor
	~texture_node_t()
	{
		if (child1) delete child1;
		if (child2) delete child2;
	}

	// search
	texture_node_t * search(int width, int height)
	{
		if (width <= 0 || height <= 0)
			return NULL;

		int usize = max_u - min_u;
		int vsize = max_v - min_v;

		if (width > usize || height > vsize)
			return NULL;

		texture_node_t * result = NULL;

		int area = usize * vsize;

		// use this node
		if (!used && width <= this->width && height <= this->height)
		{
			result = this;
			area = this->width * this->height;
		}

		// try child
		if (child1)
		{
			texture_node_t * temp = child1->search(width, height);

			if (temp)
			{
				int temp_area = temp->width * temp->height;
				if (temp_area < area)
				{
					area = temp_area;
					result = temp;
				}
			}
		}

		// try child2
		if (child2)
		{
			texture_node_t * temp = child2->search(width, height);

			if (temp)
			{
				int temp_area = temp->width * temp->height;
				if (temp_area < area)
				{
					area = temp_area;
					result = temp;
				}
			}
		}

		return result;
	}

	// use
	bool use(int usize, int vsize)
	{
		if (used)
			return false;

		if (usize > width || vsize > height)
			return false;

		int uleft = width - usize;
		int vleft = height - vsize;

		used = true;
		width = usize;
		height = vsize;

		if (uleft >= vleft)
		{
			if (child1 == NULL && uleft > 0)
				child1 = new texture_node_t(this, min_u + usize, min_v, max_u, max_v);

			if (child2 == NULL && vleft > 0)
				child2 = new texture_node_t(this, min_u, min_v + vsize, min_u + usize, max_v);
		}
		else
		{
			if (child1 == NULL && uleft > 0)
				child1 = new texture_node_t(this, min_u + usize, min_v, max_u, min_v + vsize);

			if (child2 == NULL && vleft > 0)
				child2 = new texture_node_t(this, min_u, min_v + vsize, max_u, max_v);
		}

		return true;
	}

	// unuse this node
	void unuse()
	{
		used = false;

		if (child1)
		{
			if (child1->used ||
				child1->child1 ||
				child1->child2)
				return;
		}

		if (child2)
		{
			if (child2->used ||
				child2->child1 ||
				child2->child2)
				return;
		}

		width = max_u - min_u;
		height = max_v - min_v;
		SAFE_DELETE(child1);
		SAFE_DELETE(child2);

		if (parent)
		{
			if (!parent->used)
				parent->unuse();
		}
	}


	// insert
	texture_node_t * allocate(int width, int height)
	{
		texture_node_t * result = search(width, height);

		if (result)
		{
			result->use(width, height);
		}

		return result;
	}
};

// root texture node.
static texture_node_t * root_texture_node = NULL;

#if USE_LIB_FREE_TYPE

// free type library
static FT_Library font_library = NULL;

// default font face
static FT_Face font_face1 = NULL;
static FT_Face font_face2 = NULL;

// font character
struct font_character_t
{
	wchar_t ch;
	int size;
	int advance_x;
	int bmp_left;
	int bmp_top;
	int height;
	texture_node_t * texture;
};

struct font_character_less
{
	bool operator()(const font_character_t & l, const font_character_t & r) const
	{
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
static void preload_characters(const wchar_t * text, int len, int size)
{
	bool retry = true;

	if (len <= 0)
		len = wcslen(text);

	// set font size in pixels
	FT_Set_Pixel_Sizes(font_face1, 0, size);

	for (int i = 0; i < len; i++)
	{
		font_character_t cache;
		cache.ch = text[i];
		cache.size = size;
		cache.advance_x = 0;
		cache.bmp_left = 0;
		cache.bmp_top = 0;
		cache.texture = NULL;

		character_set_t::const_iterator it = font_character_set.find(cache);

		// character is not cached.
		if (it == font_character_set.end())
		{
			// load glyph image into the slot (erase previous one)
			int error = FT_Load_Char(font_face1, cache.ch, FT_LOAD_RENDER);
			if (error == 0)
			{
				FT_GlyphSlot slot = font_face1->glyph;

				// size
				int size_x = slot->bitmap.width;
				int size_y = slot->bitmap.rows;

				if (size_x > 0 && size_y > 0)
				{
					// allocates texture node
					texture_node_t * node = root_texture_node->allocate(size_x + 2, size_y + 2);

					if (node)
					{
						RECT data_rect;
						data_rect.left = (int)node->min_u;
						data_rect.right = (int)node->min_u + (int)node->width;
						data_rect.top = (int)node->min_v;
						data_rect.bottom = (int)node->min_v + (int)node->height;

						D3DLOCKED_RECT lock_rect;
						if (SUCCEEDED(resource_texture->LockRect(0, &lock_rect, &data_rect, D3DLOCK_DISCARD)))
						{
							for (int y = 0; y < slot->bitmap.rows; ++y)
							{
								uint * dst = (uint*)((byte*)lock_rect.pBits + lock_rect.Pitch * (y + 1) + 4);
								byte * src = slot->bitmap.buffer + slot->bitmap.pitch * y;

								for (int x = 0; x < slot->bitmap.width; x++)
								{
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
						cache.bmp_top = font_face1->size->metrics.ascender / 64 - slot->bitmap_top;
						cache.texture = node;
						cache.height = font_face1->size->metrics.height / 64;
					}
					else
					{
						if (retry)
						{
							SAFE_DELETE(root_texture_node->child1);
							SAFE_DELETE(root_texture_node->child2);
							root_texture_node->unuse();

							font_character_set.clear();
							i = 0;
							retry = false;
							continue;
						}
						else
						{
							return;
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
static int build_string_vertex(const wchar_t * text, int len, int size, float x, float y, uint color, Vertex * vertex_buffer, int vertex_count, int h_align, int v_align)
{
	if (len <= 0)
		len = wcslen(text);

	float width = 0;
	float height = 0;
	float pen_x = x;
	float pen_y = y;
	Vertex * v = vertex_buffer;

	for (int i = 0; i < len; i++)
	{
		font_character_t cache;
		cache.ch = text[i];
		cache.size = size;

		character_set_t::const_iterator it = font_character_set.find(cache);

		// character is not cached.
		if (it != font_character_set.end())
		{
			cache = *it;

			if (cache.texture && v + 6 < vertex_buffer + vertex_count)
			{
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

	if (h_align || v_align)
	{
		float x_offset = 0;
		float y_offset = 0;

		if (h_align == 1) x_offset = floor(-0.5f * (width - x));
		if (v_align == 1) y_offset = floor(-0.5f * (height - y));

		for (Vertex * v1 = vertex_buffer; v1 < v; v1++)
		{
			v1->x += x_offset;
			v1->y += y_offset;
		}
	}

	return v - vertex_buffer;
}

// create string texture
static texture_node_t * create_string_texture(const char * text, int height)
{
	int error;

	// set font size in pixels
	error = FT_Set_Pixel_Sizes(font_face1, 0, height);

	if (error)
		return NULL;


	int pen_x = 0;
	int pen_y = 0;
	int size_x = 0;
	int size_y = 0;
	int num_chars = strlen(text);

	// go over and calculate size
	for (int n = 0; n < num_chars; n++)
	{
		FT_GlyphSlot slot = font_face1->glyph;

		// load glyph image into the slot (erase previous one)
		error = FT_Load_Char(font_face1, text[n], FT_LOAD_RENDER);
		if ( error )
			continue; 

		// size
		int width = (slot->bitmap_left) + slot->bitmap.width;
		int height = font_face1->size->metrics.height / 64;

		// max size
		if (pen_x + width > size_x) size_x = pen_x + width;
		if (pen_y + height > size_y) size_y = pen_y + height;

		// increment pen position
		pen_x += slot->advance.x >> 6;
	}

	// allocates texture node
	texture_node_t * node = root_texture_node->allocate(size_x, size_y);

	// reset pen
	pen_x = 0;
	pen_y = 0;

	RECT data_rect;
	data_rect.left = (int)node->min_u;
	data_rect.right = (int)node->min_u + (int)node->width;
	data_rect.top = (int)node->min_v;
	data_rect.bottom = (int)node->min_v + (int)node->height;

	D3DLOCKED_RECT lock_rect;
	if (SUCCEEDED(resource_texture->LockRect(0, &lock_rect, &data_rect, D3DLOCK_DISCARD)))
	{
		// go over again and draw text
		for (int n = 0; n < num_chars; n++)
		{
			FT_GlyphSlot slot = font_face1->glyph;

			// load glyph image into the slot (erase previous one)
			error = FT_Load_Char(font_face1, text[n], FT_LOAD_RENDER);
			if ( error )
				continue;

			// darwing position
			int dx = pen_x + (slot->bitmap_left);
			int dy = pen_y + font_face1->size->metrics.ascender / 64 - (slot->bitmap_top);

			for (int y = 0; y < slot->bitmap.rows; ++y)
			{
				uint * dst = (uint*)((byte*)lock_rect.pBits + lock_rect.Pitch * (dy + y) + 4 * dx);
				byte * src = slot->bitmap.buffer + slot->bitmap.pitch * y;


				for (int x = 0; x < slot->bitmap.width; x++)
				{
					*dst = (*src << 24) | 0xffffff;
					dst++;
					src++;
				}
			}

			// increment pen position
			pen_x += slot->advance.x >> 6;
		}

		resource_texture->UnlockRect(0);
	}

	return node;
}

// initialize fonts
static int font_initialize()
{
	int error;

	// intialize free type library
	error = FT_Init_FreeType(&font_library);
	if (error)
	{
		fprintf(stderr, "an error occurred during font library initialization\n");
		return -1;
	}

	// get font path
	char font_path[MAX_PATH];
	char buffer[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT,  font_path);

	// load default fonts.
	_snprintf(buffer, sizeof(buffer), "%s\\arial.ttf", font_path);
	error = FT_New_Face(font_library, buffer, 0, &font_face1);
	if (error)
	{
		fprintf(stderr, "an error occurred during font library initialization\n");
		return -1;
	}

	return 0;
}

// shutdown font
static void font_shutdown()
{
	FT_Done_Face(font_face1);
	FT_Done_Face(font_face2);
	FT_Done_FreeType(font_library);
}

#else

// default windows font
static HFONT default_font = NULL;

// create string texture
static texture_node_t * create_string_texture(const char * str)
{
	HDC hdcWindow = GetDC(NULL);
	HDC hdcMemDC = CreateCompatibleDC(hdcWindow); 
	int len = strlen(str);

	// get text size
	SIZE size;
	SelectObject(hdcMemDC, default_font);
	GetTextExtentPoint32(hdcMemDC, str, len, &size);

	// set text and background color
	SetTextColor(hdcMemDC, RGB(255,255,255));
	SetBkColor(hdcMemDC, 0);

	// create bitmap
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize		= sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth		= size.cx;
	bmi.bmiHeader.biHeight		= -size.cy;
	bmi.bmiHeader.biPlanes		= 1;
	bmi.bmiHeader.biBitCount	= 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	uint * pBits;
	HBITMAP hBmp = CreateDIBSection(hdcMemDC, &bmi, DIB_RGB_COLORS, (void **) &pBits, NULL, 0);

	if (NULL == hBmp || NULL == pBits)
	{
		DeleteDC(hdcWindow);
		DeleteDC(hdcMemDC);
		return NULL;
	}

	SelectObject(hdcMemDC, hBmp);

	// draw text
	TextOut(hdcMemDC, 0, 0, str, len);

	// allocates texture node
	texture_node_t * node = root_texture_node->allocate(size.cx, size.cy);

	RECT data_rect;
	data_rect.left = (int)node->min_u;
	data_rect.right = (int)node->min_u + (int)node->width;
	data_rect.top = (int)node->min_v;
	data_rect.bottom = (int)node->min_v + (int)node->height;

	D3DLOCKED_RECT lock_rect;
	if (SUCCEEDED(resource_texture->LockRect(0, &lock_rect, &data_rect, D3DLOCK_DISCARD)))
	{
		for (int y = 0; y < size.cy; ++y)
		{
			uint * dst = (uint*)((byte*)lock_rect.pBits + lock_rect.Pitch * y);
			uint * src = (uint*)pBits + size.cx * y;

			for (int x = 0; x < size.cx; x++)
			{
				*dst = *src | 0xff000000;
				dst++;
				src++;
			}
		}

		resource_texture->UnlockRect(0);

	}

	DeleteObject(hBmp);
	DeleteDC(hdcMemDC);
	DeleteDC(hdcWindow);

	return node;
}

// initialize fonts
static int font_initialize()
{
	SystemParametersInfo(SPI_SETFONTSMOOTHING, TRUE, 0, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
	SystemParametersInfo(SPI_SETFONTSMOOTHINGTYPE, FE_FONTSMOOTHINGSTANDARD, 0, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE); 

	LOGFONT lfont;
	lfont.lfHeight = -12;
	lfont.lfWidth = 0;
	lfont.lfEscapement = 0;
	lfont.lfOrientation = 0;
	lfont.lfWeight = FW_MEDIUM;
	lfont.lfItalic = 0;
	lfont.lfUnderline = 0;
	lfont.lfStrikeOut = 0;
	lfont.lfCharSet = ANSI_CHARSET;
	lfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lfont.lfQuality = CLEARTYPE_QUALITY;
	lfont.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
	lstrcpy(lfont.lfFaceName,"Arial");
	default_font = CreateFontIndirect(&lfont);

	return 0;
}

// shutdown font
static void font_shutdown()
{
	DeleteObject(default_font);
}

#endif

// -----------------------------------------------------------------------------------------
// texture loading funcs
// -----------------------------------------------------------------------------------------

// create texture from png
static LPDIRECT3DTEXTURE9 create_texture_from_png(png_structp png_ptr, png_infop info_ptr)
{
	LPDIRECT3DTEXTURE9 texture = NULL;

	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);
	uint color_type = png_get_color_type(png_ptr, info_ptr);

	// only rgba png is supported.
	if (color_type == PNG_COLOR_TYPE_RGBA)
	{
		if (SUCCEEDED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)))
		{
			byte ** data = png_get_rows(png_ptr, info_ptr);
			uint rowbytes = png_get_rowbytes(png_ptr, info_ptr);

			D3DLOCKED_RECT lock_rect;
			if (SUCCEEDED(texture->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD)))
			{
				byte * line = (byte*)lock_rect.pBits;

				for (short y = 0; y < height; y++)
				{
					byte * src = data[y];
					byte * dst = line;
					for (short x = 0; x < width; x++)
					{
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
	}
	else if (color_type == PNG_COLOR_TYPE_RGB)
	{
		if (SUCCEEDED(device->CreateTexture(width, height, 1, 0, D3DFMT_R8G8B8, D3DPOOL_MANAGED, &texture, NULL)))
		{
			byte ** data = png_get_rows(png_ptr, info_ptr);
			uint rowbytes = png_get_rowbytes(png_ptr, info_ptr);

			D3DLOCKED_RECT lock_rect;
			if (SUCCEEDED(texture->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD)))
			{
				byte * line = (byte*)lock_rect.pBits;

				for (short y = 0; y < height; y++)
				{
					byte * src = data[y];
					byte * dst = line;
					for (short x = 0; x < width; x++)
					{
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[0];

						dst += 3;
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
static LPDIRECT3DTEXTURE9 load_png_from_file(const char * filename)
{
	png_structp png_ptr;
	png_infop info_ptr;
	LPDIRECT3DTEXTURE9 texture = NULL;
	FILE *fp;

	// open file
	if ((fp = fopen(filename, "rb")) == NULL)
		return (NULL);

	// create png read struct
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (png_ptr == NULL)
	{
		fclose(fp);
		return NULL;
	}

	// Allocate/initialize the memory for image information.
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	// Set error handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
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
static LPDIRECT3DTEXTURE9 load_png_from_resource(const char * name)
{
	struct resource_file
	{
		resource_file()
			: hrc(NULL)
			, data(NULL)
			, end(NULL)
		{
		}

		bool open(const char * name, const char * type)
		{
			// find resource
			HRSRC hrsrc = FindResource(0, name, "BINARY");
			if(hrsrc == 0)
				return false;

			// load resource
			hrc = LoadResource(0, hrsrc);
			if (hrc == 0)
				return false;

			data = (byte*)LockResource(hrc);
			end  = data + SizeofResource(NULL, hrsrc);

			return true;
		}

		~resource_file()
		{
			FreeResource(hrc);
		}

		static void read_data(png_structp png_ptr, png_bytep data, png_size_t length)
		{
			png_size_t check = 0;
			resource_file * file = (resource_file*)png_get_io_ptr(png_ptr);

			if (file != NULL)
			{
				if (file->data + length <= file->end)
				{
					memcpy(data, file->data, length);
					file->data += length;
					check = length;
				}
			}

			if (check != length)
			{
				png_error(png_ptr, "Read Error!");
			}
		}

		HGLOBAL hrc;
		byte * data;
		byte * end;
	};

	png_structp png_ptr;
	png_infop info_ptr;
	LPDIRECT3DTEXTURE9 texture = NULL;
	resource_file * file = new resource_file;

	if (!file->open(name, "BINARY"))
	{
		delete file;
		return NULL;
	}

	// create png read struct
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (png_ptr == NULL)
	{
		delete file;
		return NULL;
	}

	// Allocate/initialize the memory for image information.
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		delete file;
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	// Set error handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
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
static void d3d_reset_parameters(D3DPRESENT_PARAMETERS & params, HWND hwnd)
{
	ZeroMemory(&params, sizeof(D3DPRESENT_PARAMETERS));
	params.hDeviceWindow = hwnd;
	params.BackBufferFormat = D3DFMT_X8R8G8B8;
	params.BackBufferCount = 1;
	params.MultiSampleType = D3DMULTISAMPLE_NONE;
	params.MultiSampleQuality = 0;
	params.SwapEffect = D3DSWAPEFFECT_COPY;
	params.Windowed = TRUE;
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

#if SCALE_DISPLAY
	display_width = 760;
	display_height = 340;
#else
	display_width = params.BackBufferWidth;
	display_height = BackBufferHeight;
#endif
}


#pragma pack(push)
#pragma pack(1)
struct TgaHeader
{
	byte  identsize;          // size of ID field that follows 18 byte header (0 usually)
	byte  colourmaptype;      // type of colour map 0=none, 1=has palette
	byte  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	short colourmapstart;     // first colour map entry in palette
	short colourmaplength;    // number of colours in palette
	byte  colourmapbits;      // number of bits per palette entry 15,16,24,32

	short xstart;             // image x origin
	short ystart;             // image y origin
	short width;              // image width in pixels
	short height;             // image height in pixels
	byte  bits;               // image bits per pixel 8,16,24,32
	byte  descriptor;         // image descriptor bits (vh flip bits)
};
#pragma pack(pop)

// load texture
static int load_tga_from_resource(const char * name, LPDIRECT3DTEXTURE9 * texture)
{
	HRESULT hr;

	// find resource
	HRSRC hrsrc = FindResource(0, name, "BINARY");
	if(hrsrc == 0)
		return E_FAIL;

	// load resource
	HGLOBAL hrc = LoadResource(0, hrsrc);
	if (hrc == 0)
		return E_FAIL;


	// file header
	TgaHeader * header = (TgaHeader*)LockResource(hrc);

	// error
	hr = E_FAIL;

	// maybe a valid tga
	if (header->colourmaptype == 0 && header->imagetype == 2)
	{
		D3DFORMAT format = D3DFMT_UNKNOWN;

		switch (header->bits)
		{
		case 8:		format = D3DFMT_L8; break;
		case 16:	format = D3DFMT_A8L8; break;
		case 32:	format = D3DFMT_A8R8G8B8; break;
		}

		if (format)
		{
			if (SUCCEEDED(hr = device->CreateTexture(header->width, header->height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture, NULL)))
			{
				D3DLOCKED_RECT lock_rect;
				if (SUCCEEDED(hr = texture[0]->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD)))
				{
					uint pixelsize = header->bits / 8;
					byte * dst = (byte*)lock_rect.pBits;
					byte * src = (byte*)(header + 1) + (header->height - 1) * pixelsize * header->width;

					for (short y = 0; y < header->height; y++)
					{
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

// set texture
static void set_texture(LPDIRECT3DTEXTURE9 texture)
{
	D3DSURFACE_DESC desc;
	texture->GetLevelDesc(0, &desc);
	float w = 1.0f / desc.Width;
	float h = 1.0f / desc.Height;
	D3DMATRIX matrix = {
		w, 0, 0, 0,
		0, h, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
	device->SetTexture(0, texture);
	device->SetTransform(D3DTS_TEXTURE0, &matrix);
}
	

// terminate dx9
static void d3d_shutdown()
{
	d3d_device_lost();

	SAFE_DELETE(root_texture_node);
	SAFE_RELEASE(resource_texture);
	SAFE_RELEASE(device);
	SAFE_RELEASE(d3d9);
}

// initialize d3d9
static int d3d_initialize(HWND hwnd)
{
	HRESULT hr;

	// create direct3d 9 object
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d9 == NULL)
		return E_FAIL;

	// initialize params
	D3DPRESENT_PARAMETERS params;

	// reset params
	d3d_reset_parameters(params, hwnd);

	// create device
	if (FAILED(hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device)))
	{
		// try again with software vertex processing
		if (FAILED(hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
			goto error;
	}

	// create resource texture
	if (FAILED(hr = device->CreateTexture(512, 512, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &resource_texture, NULL)))
		goto error;

	// create root texture node
	root_texture_node = new texture_node_t(NULL, 0, 0, 512, 512);

	return S_OK;

error:
	d3d_shutdown();
	return hr;
}

// device lost
static void d3d_device_lost()
{
}

// reset 
static int d3d_device_reset()
{
	HRESULT hr;
	D3DPRESENT_PARAMETERS params;

	// test cooperative level.
	if (FAILED(hr = device->TestCooperativeLevel()))
	{
		if (D3DERR_DEVICELOST == hr)
			return hr;
	}

	// update parameters
	d3d_reset_parameters(params, display_hwnd);

	// reset device
	if (FAILED(hr = device->Reset(&params)))
	{
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

	// texture states
	float mipmap_bias = -1;
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS , *(DWORD*)&mipmap_bias);
	device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS,    D3DTTFF_COUNT2);

	// lighting
	device->SetRenderState(D3DRS_LIGHTING, FALSE);

	// set vertex declaration
	device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	return 0;
}

struct KeyboardState
{
	float x1;
	float y1;
	float x2;
	float y2;
	uint status;
};

struct MidiKeyState
{
	float x1;
	float y1;
	float x2;
	float y2;
	bool black;
	byte status;
};


static MidiKeyState midi_key_states[127] = {0};
static KeyboardState keyboard_states[256] = {0};

// init key display state
static void init_keyboard_states()
{
	struct Key
	{
		byte code;
		byte action;
		float x;
		float y;
	};

	static Key keys1[] =
	{
		{ 0,				3,	0.0f,	0.0f,	},
		{ DIK_ESCAPE,		0,	1.0f,	1.0f,	},
		{ 0,				0,	1.0f,	0.0f,	},
		{ DIK_F1,			0,	1.0f,	1.0f,	},
		{ DIK_F2,			0,	1.0f,	1.0f,	},
		{ DIK_F3,			0,	1.0f,	1.0f,	},
		{ DIK_F4,			0,	1.0f,	1.0f,	},
		{ 0,				0,	0.5f,	0.0f	},
		{ DIK_F5,			0,	1.0f,	1.0f,	},
		{ DIK_F6,			0,	1.0f,	1.0f,	},
		{ DIK_F7,			0,	1.0f,	1.0f,	},
		{ DIK_F8,			0,	1.0f,	1.0f,	},
		{ 0,				0,	0.5f,	0.0f,	},
		{ DIK_F9,			0,	1.0f,	1.0f,	},
		{ DIK_F10,			0,	1.0f,	1.0f,	},
		{ DIK_F11,			0,	1.0f,	1.0f,	},
		{ DIK_F12,			0,	1.0f,	1.0f,	},
		{ 0,				1,	0.0f,	1.2f,	},
		{ DIK_GRAVE,		0,	1.0f,	1.0f,	},
		{ DIK_1,			0,	1.0f,	1.0f,	},
		{ DIK_2,			0,	1.0f,	1.0f,	},
		{ DIK_3,			0,	1.0f,	1.0f,	},
		{ DIK_4,			0,	1.0f,	1.0f,	},
		{ DIK_5,			0,	1.0f,	1.0f,	},
		{ DIK_6,			0,	1.0f,	1.0f,	},
		{ DIK_7,			0,	1.0f,	1.0f,	},
		{ DIK_8,			0,	1.0f,	1.0f,	},
		{ DIK_9,			0,	1.0f,	1.0f,	},
		{ DIK_0,			0,	1.0f,	1.0f,	},
		{ DIK_MINUS,		0,	1.0f,	1.0f,	},
		{ DIK_EQUALS,		0,	1.0f,	1.0f,	},
		{ DIK_BACK,			0,	2.0f,	1.0f,	},
		{ 0,				1,	0.0f,	1.0f,	},
		{ DIK_TAB,			0,	1.5f,	1.0f,	},
		{ DIK_Q,			0,	1.0f,	1.0f,	},
		{ DIK_W,			0,	1.0f,	1.0f,	},
		{ DIK_E,			0,	1.0f,	1.0f,	},
		{ DIK_R,			0,	1.0f,	1.0f,	},
		{ DIK_T,			0,	1.0f,	1.0f,	},
		{ DIK_Y,			0,	1.0f,	1.0f,	},
		{ DIK_U,			0,	1.0f,	1.0f,	},
		{ DIK_I,			0,	1.0f,	1.0f,	},
		{ DIK_O,			0,	1.0f,	1.0f,	},
		{ DIK_P,			0,	1.0f,	1.0f,	},
		{ DIK_LBRACKET,		0,	1.0f,	1.0f,	},
		{ DIK_RBRACKET,		0,	1.0f,	1.0f,	},
		{ DIK_BACKSLASH,	0,	1.5f,	1.0f,	},
		{ 0,				1,	0.0f,	1.0f,	},
		{ DIK_CAPITAL,		0,	1.75f,	1.0f,	},
		{ DIK_A,			0,	1.0f,	1.0f,	},
		{ DIK_S,			0,	1.0f,	1.0f,	},
		{ DIK_D,			0,	1.0f,	1.0f,	},
		{ DIK_F,			0,	1.0f,	1.0f,	},
		{ DIK_G,			0,	1.0f,	1.0f,	},
		{ DIK_H,			0,	1.0f,	1.0f,	},
		{ DIK_J,			0,	1.0f,	1.0f,	},
		{ DIK_K,			0,	1.0f,	1.0f,	},
		{ DIK_L,			0,	1.0f,	1.0f,	},
		{ DIK_SEMICOLON,	0,	1.0f,	1.0f,	},
		{ DIK_APOSTROPHE,	0,	1.0f,	1.0f,	},
		{ DIK_RETURN,		0,	2.25f,	1.0f,	},
		{ 0,				1,	0.0f,	1.0f,	},
		{ DIK_LSHIFT,		0,	2.5f,	1.0f,	},
		{ DIK_Z,			0,	1.0f,	1.0f,	},
		{ DIK_X,			0,	1.0f,	1.0f,	},
		{ DIK_C,			0,	1.0f,	1.0f,	},
		{ DIK_V,			0,	1.0f,	1.0f,	},
		{ DIK_B,			0,	1.0f,	1.0f,	},
		{ DIK_N,			0,	1.0f,	1.0f,	},
		{ DIK_M,			0,	1.0f,	1.0f,	},
		{ DIK_COMMA,		0,	1.0f,	1.0f,	},
		{ DIK_PERIOD,		0,	1.0f,	1.0f,	},
		{ DIK_SLASH,		0,	1.0f,	1.0f,	},
		{ DIK_RSHIFT,		0,	2.5f,	1.0f,	},
		{ 0,				1,	0.0f,	1.0f,	},
		{ DIK_LCONTROL,		0,	1.25,	1.0f,	},
		{ DIK_LWIN,			0,	1.25,	1.0f,	},
		{ DIK_LMENU,		0,	1.25,	1.0f,	},
		{ DIK_SPACE,		0,	6.25,	1.0f,	},
		{ DIK_RMENU,		0,	1.25,	1.0f,	},
		{ DIK_RWIN,			0,	1.25,	1.0f,	},
		{ DIK_APPS,			0,	1.25,	1.0f,	},
		{ DIK_RCONTROL,		0,	1.25,	1.0f,	},

		{ 0,				3,	15.45f,	0.0f,	},
		{ DIK_SYSRQ,		0,	1.0f,	1.0f,	},
		{ DIK_SCROLL,		0,	1.0f,	1.0f,	},
		{ DIK_PAUSE,		0,	1.0f,	1.0f,	},
		{ 0,				0,	-3.0f,	1.2f,	},
		{ DIK_INSERT,		0,	1.0f,	1.0f,	},
		{ DIK_HOME,			0,	1.0f,	1.0f,	},
		{ DIK_PGUP,			0,	1.0f,	1.0f,	},
		{ 0,				0,	-3.0f,	1.0f,	},
		{ DIK_DELETE,		0,	1.0f,	1.0f,	},
		{ DIK_END,			0,	1.0f,	1.0f,	},
		{ DIK_PGDN,			0,	1.0f,	1.0f,	},
		{ 0,				0,	-2.0f,	2.0f,	},
		{ DIK_UP,			0,	1.0f,	1.0f,	},
		{ 0,				0,	-2.0f,	1.0f,	},
		{ DIK_LEFT,			0,	1.0f,	1.0f,	},
		{ DIK_DOWN,			0,	1.0f,	1.0f,	},
		{ DIK_RIGHT,		0,	1.0f,	1.0f,	},

		{ 0,				3,	18.9f,	1.2f,	},
		{ DIK_NUMLOCK,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPADSLASH,	0,	1.0f,	1.0f,	},
		{ DIK_NUMPADSTAR,	0,	1.0f,	1.0f,	},
		{ DIK_NUMPADMINUS,	0,	1.0f,	1.0f,	},
		{ 0,				0,	-4.0f,	1.0f,	},
		{ DIK_NUMPAD7,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD8,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD9,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPADPLUS,	0,	1.0f,	2.0f,	},
		{ 0,				0,	-4.0f,	1.0f,	},
		{ DIK_NUMPAD4,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD5,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD6,		0,	1.0f,	1.0f,	},
		{ 0,				0,	-3.0f,	1.0f,	},
		{ DIK_NUMPAD1,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD2,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPAD3,		0,	1.0f,	1.0f,	},
		{ DIK_NUMPADENTER,	0,	1.0f,	2.0f,	},
		{ 0,				0,	-4.0f,	1.0f,	},
		{ DIK_NUMPAD0,		0,	2.0f,	1.0f,	},
		{ DIK_NUMPADPERIOD,	0,	1.0f,	1.0f,	},
	};

	float x = 0;
	float y = 0;
	float key_width = 32;
	float key_height = 29;

	for (Key * key = keys1; key < keys1 + sizeof(keys1) / sizeof(Key); key++)
	{
		if (key->code)
		{
			KeyboardState & state = keyboard_states[key->code];
			state.x1 = round(14 + x * key_width);
			state.y1 = round(14 + y * key_height);
			state.x2 = round(14 + x * key_width + key->x * key_width);
			state.y2 = round(14 + y * key_height + key->y * key_height);
			x += key->x;
		}
		else
		{
			if (key->action & 1) x = key->x; else x += key->x;
			if (key->action & 2) y = key->y; else y += key->y;
		}
	}
}

// initialize midi key states
static void init_midi_keyboard_states()
{
	static byte key_flags[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };

	float x = 16;
	float y = 250;
	float w1 = 9;
	float h1 = 54;
	float w2 = 14;
	float h2 = 90;

	for (int i = 21; i < 21 + 88; i++)
	{
		MidiKeyState & key = midi_key_states[i];

		// black key
		if (key_flags[(i) % 12])
		{
			key.x1 = x - 4;
			key.y1 = y;
			key.x2 = key.x1 + w1;
			key.y2 = key.y1 + h1;
			key.black = true;
		}
		else
		{
			key.x1 = x;
			key.y1 = y;
			key.x2 = x + w2;
			key.y2 = y + h2;
			x = x + w2;
			key.black = false;
		}
	}
}


struct DisplayResource
{
	LPDIRECT3DTEXTURE9 texture;

	DisplayResource()
		: texture(NULL)
	{
	}
};

static DisplayResource resources[resource_count];

// set display image
void display_set_image(uint type, const char * name)
{
	if (type < resource_count)
	{
		if (resources[type].texture)
			resources[type].texture->Release();

		resources[type].texture = load_png_from_file(name);
	}
}

// set display default skin
void display_default_skin()
{

	for (int i = 0; i < resource_count; i++)
	{
		if (resources[i].texture)
			resources[i].texture->Release();

#ifdef _DEBUG
		const char * default_skins[] = {
			"background.png",
			"notes.png",
			"keyboard_unmapped_down.png",
			"keyboard_unmapped_up.png",
			"keyboard_note_down.png",
			"keyboard_note_up.png",
			"midi_black_down.png",
			"midi_black_up.png",
			"midi_white_down.png",
			"midi_white_up.png",
			"check_button_down.png",
			"check_button_up.png",
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

// darw string
static void draw_string(float x, float y, uint color, const char * text, int size, int h_align, int v_align)
{
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
static void draw_sprite(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, uint color)
{
	Vertex v[4] =
	{
		{ x1, y1, 0, color, u1, v1 },
		{ x2, y1, 0, color, u2, v1 },
		{ x1, y2, 0, color, u1, v2 },
		{ x2, y2, 0, color, u2, v2 },
	};

	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
}

// draw image
static void draw_image(uint resource_id, float x1, float x2, float y1, float y2, uint color)
{
	if (resource_id < resource_count)
	{
		LPDIRECT3DTEXTURE9 texture = resources[resource_id].texture;
		if (texture)
		{
			D3DSURFACE_DESC desc;
			texture->GetLevelDesc(0, &desc);
			float w = (float)desc.Width;
			float h = (float)desc.Height;
			D3DMATRIX matrix = {
				1.0f / w, 0, 0, 0,
				0, 1.0f / h, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1,
			};
			device->SetTexture(0, texture);
			device->SetTransform(D3DTS_TEXTURE0, &matrix);
			draw_sprite(x1, x2, y1, y2, 0, 0, w, h, color);
		}
	}
}

// draw texture node
static void draw_node_unscaled(float x, float y, texture_node_t * node, uint color)
{
	if (node)
	{
		if (resource_texture)
		{
			D3DSURFACE_DESC desc;
			resource_texture->GetLevelDesc(0, &desc);
			float w = (float)desc.Width;
			float h = (float)desc.Height;
			D3DMATRIX matrix = {
				1.0f / w, 0, 0, 0,
				0, 1.0f / h, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1,
			};
			device->SetTexture(0, resource_texture);
			device->SetTransform(D3DTS_TEXTURE0, &matrix);

			draw_sprite(x, y, x + node->width, y + node->height, (float)node->min_u, (float)node->min_v, (float)node->min_u + (float)node->width, (float)node->min_v + (float)node->height, color);
		}
	}
}

// draw scaleable sprite
static void draw_sprite_border(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, float lm, float rm, float tm, float bm, uint color)
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

	v[0].color = v[1].color = v[2].color = v[3].color = color;
	v[4].color = v[5].color = v[6].color = v[7].color = color;
	v[8].color = v[9].color = v[10].color = v[11].color = color;
	v[12].color = v[13].color = v[14].color = v[15].color = color;

	// darw primitive
	device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 16, 18, index, D3DFMT_INDEX16, v, sizeof(Vertex));

}

// draw image
static void draw_image_border(uint resource_id, float x1, float x2, float y1, float y2, float lm, float rm, float tm, float bm, uint color)
{
		if (resource_id < resource_count)
	{
		LPDIRECT3DTEXTURE9 texture = resources[resource_id].texture;
		if (texture)
		{
			D3DSURFACE_DESC desc;
			texture->GetLevelDesc(0, &desc);
			float w = (float)desc.Width;
			float h = (float)desc.Height;
			D3DMATRIX matrix = {
				1.0f / w, 0, 0, 0,
				0, 1.0f / h, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1,
			};
			device->SetTexture(0, texture);
			device->SetTransform(D3DTS_TEXTURE0, &matrix);
			draw_sprite_border(x1, x2, y1, y2, 0, 0, w, h, lm, rm, tm, bm, color);
		}
	}
}


// draw keyboard
static void draw_keyboard()
{
	for (KeyboardState * key = keyboard_states; key < keyboard_states + 256; key++)
	{
		if (key->x2 > key->x1)
		{
			KeyboardEvent map;

			// get keyboard map
			keyboard_get_map(key - keyboard_states, &map, NULL);

			// modify keyboard map
			midi_modify_event(map.action, map.arg1, map.arg2, map.arg3, keyboard_get_octshift(map.action & 0xf) * 12, keyboard_get_velocity(map.action & 0xf));

			float x1 = key->x1;
			float y1 = key->y1;
			float x2 = key->x2;
			float y2 = key->y2;

			uint img = map.action ? keyboard_note_down : keyboard_unmapped_down;

			// draw key button
			if (!key->status) img ++;
			draw_image_border(img, x1, y1, x2, y2, 6, 6, 6, 6, 0xffffffff);

			// key label
			const char * label = keyboard_get_label(key - keyboard_states);
			if (label[0])
			{
				draw_string(floor((x1 + x2 - 1) * 0.5f), floor((y1 + y2 - 1) * 0.5f), 0xff6e6e6e, label, 10, 1, 1);
			}

			// draw note
			else if ((map.action & 0xF0) == 0x90)
			{
				byte note = map.arg1;

				if (note > 12)
				{
					if (resources[notes].texture)
					{
						float x3 = round(x1 + (x2 - x1 - 25.f) * 0.5f);
						float x4 = x3 + 25.f;
						float y3 = round(y1 + (y2 - y1 - 25.f) * 0.5f);
						float y4 = y3 + 25.f;
						float u3 = ((note - 12) % 12) * 25.f;
						float v3 = ((note - 12) / 12) * 25.f;
						float u4 = u3 + 25.f;
						float v4 = v3 + 25.f;

						set_texture(resources[notes].texture);
						draw_sprite(x3, y3, x4, y4, u3, v3, u4, v4, 0xffffffff);
					}

				}
			}
		}
	}

}

// draw midi keyboard
static void draw_midi_keyboard()
{
	for (MidiKeyState * key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++)
	{
		if (!key->black)
		{
			if (key->x2 > key->x1) 
			{
				uint img = key->status ? midi_white_down : midi_white_up;
				draw_image(img, key->x1, key->y1, key->x2, key->y2, 0xffffffff);
			}
		}
	}

	for (MidiKeyState * key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++)
	{
		if (key->black)
		{
			if (key->x2 > key->x1) 
			{
				uint img = key->status ? midi_black_down : midi_black_up;
				draw_image(img, key->x1, key->y1, key->x2, key->y2, 0xffffffff);
			}
		}
	}
}

// darw keyboard controls
static void draw_keyboard_controls()
{
	char buff[256];

	static struct midi_key_t
	{
		const char * name;
		int shift;
	}
	midi_key_items[] = {
		{ "C(0)",	0 },
		{ "bD(+1)",	1 },
		{ "D(+2)",	2 },
		{ "bE(+3)",	3 },
		{ "E(+4)",	4 },
		{ "F(+5)",	5 },
		{ "#F(+6)",	6 },
		{ "G(+7)",	7 },
		{ "bA(-4)",	-4 },
		{ "A(-3)",	-3 },
		{ "bB(-2)",	-2 },
		{ "B(-1)",	-1 },
	};

	// sustain pedal
	_snprintf(buff, sizeof(buff), "%d", midi_get_controller_value(0x40));
	draw_string(92, 222, 0xff6e6e6e, buff, 11, 1, 0);

	// midi key
	switch (midi_get_key_signature())
	{
	case 0:		strcpy(buff, "C(0)"); break;
	case 1:		strcpy(buff, "bD(+1)"); break;
	case 2:		strcpy(buff, "D(+2)"); break;
	case 3:		strcpy(buff, "bE(+3)"); break;
	case 4:		strcpy(buff, "E(+4)"); break;
	case 5:		strcpy(buff, "F(+5)"); break;
	case 6:		strcpy(buff, "#F(+6)"); break;
	case 7:		strcpy(buff, "G(+7)"); break;
	case -4:	strcpy(buff, "bA(-4)"); break;
	case -3:	strcpy(buff, "A(-3)"); break;
	case -2:	strcpy(buff, "bB(-2)"); break;
	case -1:	strcpy(buff, "B(-1)"); break;
	default:	_snprintf(buff, sizeof(buff), "%d", midi_get_key_signature()); break;
	}

	draw_string(202, 222, 0xff6e6e6e, buff, 11, 1, 0);


	// record
	if (song_is_recording())
		draw_image(check_button_down, 244, 220, 244 + 18, 220 + 18, 0xffd23b36);
	else
		draw_image(check_button_up, 244, 220, 244 + 18, 220 + 18, 0xff4d3b36);

	// timer
	_snprintf(buff, sizeof(buff), "%d:%02d", song_get_time() / 1000 / 60, song_get_time() / 1000 % 60);
	draw_string(332, 222, 0xff6e6e6e, buff, 11, 1, 0);

	// play
	if (song_is_playing())
		draw_image(check_button_down, 359, 220, 359 + 18, 220 + 18, 0xff53ce36);
	else
		draw_image(check_button_up, 359, 220, 359 + 18, 220 + 18, 0xff4d5c37);

	// velocity
	_snprintf(buff, sizeof(buff), "%d", keyboard_get_velocity(0));
	draw_string(512, 222, 0xff6e6e6e, buff, 11, 1, 0);
	_snprintf(buff, sizeof(buff), "%d", keyboard_get_velocity(1));
	draw_string(554, 222, 0xff6e6e6e, buff, 11, 1, 0);

	// octshift
	{
		int s1 = keyboard_get_octshift(0);
		int s2 = keyboard_get_octshift(1);

		_snprintf(buff, sizeof(buff), "%s%d", s1 > 0 ? "+" : s1 < 0 ? "-" : "", abs(s1));
		draw_string(672, 222, 0xff6e6e6e, buff, 11, 1, 0);

		_snprintf(buff, sizeof(buff), "%s%d", s2 > 0 ? "+" : s2 < 0 ? "-" : "", abs(s2));
		draw_string(716, 222, 0xff6e6e6e, buff, 11, 1, 0);
	}
}

// draw main window
 void display_render()
{
	// begin scene
	if (FAILED(device->BeginScene()))
	{
		d3d_device_lost();
		d3d_device_reset();
		device->Present(NULL, NULL, NULL, NULL);
		return;
	}

	{
		float sx = 2.0f / (float)display_width;
		float sy = -2.0f / (float)display_height;
		float tx = -1 + sx * 0.5f;
		float ty = 1 + sy * 0.5f;
		tx = -1 - sx * 0.5f;
		ty = 1 - sy * 0.5f;

		D3DMATRIX projection = {
			sx, 0, 0, 0,
			0, sy, 0, 0,
			0, 0, 1, 0,
			tx, ty, 0, 1,
		};

		device->SetTransform(D3DTS_PROJECTION, &projection);
		device->SetTransform(D3DTS_VIEW, &matrix_identity);
	}

#ifdef _DEBUG
	if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_F1))
	{
		// clear backbuffer
		device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff00ff00, 0, 0);

		// set white texture
		set_texture(resource_texture);

		// draw resource texture
		draw_sprite(0, 0, 512, 512, 0, 0, 512, 512, 0xffffffff);
	}
	else
#endif
	{
		draw_image(background, 0, 0, (float)display_width, (float)display_height, 0xffffffff);
		draw_keyboard();
		draw_midi_keyboard();
		draw_keyboard_controls();
	}

	device->EndScene();
	device->Present(NULL, NULL, NULL, NULL);
}

// init display
int display_init(HWND hwnd)
{
	display_hwnd = hwnd;

	// initialize font
	if (font_initialize())
	{
		fprintf(stderr, "failed to initialize fonts");
		return -1;
	}

	// initialize directx 9
	if (d3d_initialize(display_hwnd))
	{
		fprintf(stderr, "failed to initialize d3d");
		return -1;
	}

	// reset device
	d3d_device_reset();

	// load default skin
	display_default_skin();

	// initialize keyboard display
	init_keyboard_states();
	init_midi_keyboard_states();

	return 0;
}

// shutdown display
int display_shutdown()
{
	for (int i = 0; i < resource_count; i++)
	{
		if (resources[i].texture)
			resources[i].texture->Release();
	}

	d3d_shutdown();
	display_hwnd = NULL;

	return 0;
}

// get display window handle
HWND display_get_hwnd()
{
	return display_hwnd;
}

// display set key down
void display_keyboard_event(byte code, uint status)
{
	keyboard_states[code].status = status;
	PostMessage(display_hwnd, WM_TIMER, 0, 0);
}

// display update midi
void display_midi_event(byte data1, byte data2, byte data3, byte data4)
{
	switch(data1 & 0xf0)
	{
	case 0x90:	midi_key_states[data2].status = data3; break;
	case 0x80:	midi_key_states[data2].status = 0; break;
	}

	PostMessage(display_hwnd, WM_TIMER, 0, 0);
}

// find keyboard key
static int find_keyboard_key(int x, int y)
{
	// find pointed key
	for (KeyboardState * key = keyboard_states; key < keyboard_states + 256; key++)
	{
		if (key->x2 > key->x1)
		{
			if (x >= key->x1 && x < key->x2 && y >= key->y1 && y < key->y2)
			{
				return key - keyboard_states;
			}
		}
	}
	return -1;
}

// find midi note
static int find_midi_note(int x, int y, int * velocity = NULL)
{
	for (int black = 0; black < 2; black ++)
	{
		for (MidiKeyState * key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++)
		{
			if ((int)key->black != black)
			{
				if (key->x2 > key->x1) 
				{
					if (x >= key->x1 && x < key->x2 && y >= key->y1 && y < key->y2)
					{
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

// mouse commands
enum MouseCommands
{
	CMD_NONE,
	CMD_SUSTAIN,
	CMD_MIDI_KEY,
	CMD_RECORD,
	CMD_PLAY,
	CMD_VELOCITY_LEFT,
	CMD_VELOCITY_RIGHT,
	CMD_OCTSHIFT_LEFT,
	CMD_OCTSHIFT_RIGHT,
};

// find buttons
static int find_control_button(int x, int y)
{
	static struct control_button_t
	{
		float x;
		float y;
		float w;
		float h;
		int command;
	}
	buttons[] =
	{
		{ 73, 221, 36, 15, CMD_SUSTAIN },
		{ 185, 221, 36, 15, CMD_MIDI_KEY },
		{ 245, 221, 65, 15, CMD_RECORD },
		{ 361, 221, 53, 15, CMD_PLAY },
		{ 494, 221, 36, 15, CMD_VELOCITY_LEFT },
		{ 538, 221, 36, 15, CMD_VELOCITY_RIGHT },
		{ 655, 221, 36, 15, CMD_OCTSHIFT_LEFT },
		{ 699, 221, 36, 15, CMD_OCTSHIFT_RIGHT },
	};

	for (int i = 0; i < ARRAY_COUNT(buttons); i++)
	{
		control_button_t & btn = buttons[i];

		if (x >= btn.x && x <= btn.x + btn.w &&
			y >= btn.y && y <= btn.y + btn.h)
		{
			return btn.command;
		}
	}

	return CMD_NONE;
}

// dispatch command
void dispatch_command(int command, int action)
{
	switch (command)
	{
	case CMD_SUSTAIN:
		if (midi_get_controller_value(0x40))
			song_send_event(0xb0, 0x40, 0, 0, true);
		else
			song_send_event(0xb0, 0x40, 127, 0, true);
		break;

	case CMD_MIDI_KEY:
		if (song_allow_input())
			song_send_event(1, action, 1, 0, true);
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
		song_send_event(3, 0, action, 10, true);
		break;

	case CMD_VELOCITY_RIGHT:
		song_send_event(3, 1, action, 10, true);
		break;

	case CMD_OCTSHIFT_LEFT:
		song_send_event(2, 0, action, 1, true);
		break;

	case CMD_OCTSHIFT_RIGHT:
		song_send_event(2, 1, action, 1, true);
		break;
	}
}

// adjust mouse position
static void adjust_mouse_position(int & x, int & y)
{
	RECT rect;
	GetClientRect(display_hwnd, &rect);

	if (rect.right > rect.left &&
		rect.bottom > rect.top)
	{
		x = x * display_width / (rect.right - rect.left);
		y = y * display_height / (rect.bottom - rect.top);
	}
}

// mouse control
static int mouse_control(HWND window, uint msg, int x, int y, int z)
{
	static int previous_keycode = -1;
	static int previous_midi_note = -1;

	int keycode = -1;
	int midinote = -1;
	int velocity = 127;
	int handled = false; 

	// raw mouse position
	POINT point = {x, y};

	// adjust mouse position
	adjust_mouse_position(x, y);


	switch (msg)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			int button = find_control_button(x, y);
			if (button != CMD_NONE)
			{
				dispatch_command(button, 1);
				return 0;
			}
			else
			{
				SetCapture(window);
			}
		}
		break;
		
	case WM_LBUTTONUP:
		handled = true;
		ReleaseCapture();
		break;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		{
			int button = find_control_button(x, y);
			if (button != CMD_NONE)
			{
				dispatch_command(button, 2);
				return 0;
			}
		}
		handled = true;
		break;

	case WM_RBUTTONUP:
		keycode = find_keyboard_key(x, y);

		if (keycode != -1)
		{
			ClientToScreen(window, &point);
			gui_popup_keymenu(keycode, point.x, point.y);
			return 0;
		}
		break;

	case WM_MOUSEMOVE:
		if (GetCapture() != window)
			handled = true;
		break;


	case WM_MOUSEWHEEL:
		{
			int button = find_control_button(x, y);
			if (button != CMD_NONE && 
				button != CMD_RECORD &&
				button != CMD_PLAY)
				dispatch_command(button, z > 0 ? 1 : 2);

			handled = true;
		}
		break;
	}

	if (song_allow_input() && !handled)
	{
		keycode = find_keyboard_key(x, y);
		midinote = find_midi_note(x, y, &velocity);
	}

	// update keyboard button
	if (keycode != previous_keycode)
	{
		if (previous_keycode != -1)
			song_send_event(0, 0, previous_keycode, 0, true);

		if (keycode != -1)
			song_send_event(0, 0, keycode, 1, true);

		previous_keycode = keycode;
	}

	// update midi button
	if (midinote != previous_midi_note)
	{
		if (previous_midi_note != -1)
			song_send_event(0x80, previous_midi_note, velocity, 0, true);

		if (midinote != -1)
			song_send_event(0x90, midinote, velocity, 0, true);

		previous_midi_note = midinote;
	}

	return 0;
}

// mouse menu
static void mouse_menu(int x, int y)
{
	POINT point = {x, y};

	adjust_mouse_position(x, y);

	int keycode = find_keyboard_key(x, y);

	if (keycode != -1)
	{
		ClientToScreen(display_hwnd, &point);
		gui_popup_keymenu(keycode, point.x, point.y);
	}
}

// display process emssage
int display_process_message(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ACTIVATE:
		if (WA_INACTIVE == LOWORD(wParam))
			mouse_control(hWnd, WM_LBUTTONUP, -1, -1, 0);
		break;

	case WM_SIZE:
		d3d_device_lost();
		d3d_device_reset();
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

	case WM_MOUSEWHEEL:
		{
			POINT cursor_pos;
			GetCursorPos(&cursor_pos);
			ScreenToClient(hWnd, &cursor_pos);
			mouse_control(hWnd, uMsg, cursor_pos.x, cursor_pos.y, (short)HIWORD(wParam) / 120);
		}
		break;

	}

	return 0;
}