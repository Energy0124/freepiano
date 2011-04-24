#include "pch.h"
#include <d3d9.h>
#include <dinput.h>
#include <math.h>

#include "display.h"
#include "keyboard.h"

#define WM_KEYBOARD_EVENT	WM_USER + 1
#define WM_MIDI_EVENT		WM_USER + 2

// main window handle.
static HWND mainhwnd = NULL;

// directX device handle.
static LPDIRECT3D9 d3d9 = NULL;
static LPDIRECT3DDEVICE9 device = NULL;
static LPDIRECT3DTEXTURE9 white_texture = NULL;
static LPDIRECT3DTEXTURE9 resource_texture = NULL;

// vertex format
struct Vertex
{
	float x, y, z;
	DWORD color;
	float u, v;
};

// draw entire main window
static void draw_mainwindow();

// identity matrix
static D3DMATRIX matrix_identity = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
};

static inline float round(float x) { return floor(x + 0.5f); }

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
	params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	params.EnableAutoDepthStencil = FALSE;
	params.AutoDepthStencilFormat = D3DFMT_D24S8;

	RECT rect1, rect2;
	GetWindowRect(hwnd, &rect1);
	SetRect(&rect2, 0, 0, 0, 0);
	AdjustWindowRect(&rect2, GetWindowLong(hwnd, GWL_STYLE), FALSE);

	params.BackBufferWidth = (rect1.right - rect1.left) - (rect2.right - rect2.left);
	params.BackBufferHeight = (rect1.bottom - rect1.top) - (rect2.bottom - rect2.top);
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
static int load_texture_from_resource(const char * name, LPDIRECT3DTEXTURE9 * texture)
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

	SAFE_RELEASE(resource_texture);
	SAFE_RELEASE(white_texture);
	SAFE_RELEASE(device);
	SAFE_RELEASE(d3d9);
}

// initialize d3d9
static int d3d_initialize(HWND hwnd)
{
	// create direct3d 9 object
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d9 == NULL)
		return -1;

	// initialize params
	D3DPRESENT_PARAMETERS params;

	// reset params
	d3d_reset_parameters(params, hwnd);

	// create device
	if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device)))
	{
		// try again with software vertex processing
		if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device)))
		{
			d3d_shutdown();
			return -1;
		}
	}

	// create white texture
	if (FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &white_texture, NULL)))
	{
		d3d_shutdown();
		return -1;
	}

	D3DLOCKED_RECT lock_rect;
	if (SUCCEEDED(white_texture->LockRect(0, &lock_rect, NULL, D3DLOCK_DISCARD)))
	{
		uint * colors = (uint*)lock_rect.pBits;
		colors[0] = 0xffffffff;
		white_texture->UnlockRect(0);
	}

	if (FAILED(load_texture_from_resource("RESOURCE", &resource_texture)))
	{
		d3d_shutdown();
		return -1;
	}
	return 0;
}

// device lost
static void d3d_device_lost()
{
}

// reset 
static int d3d_reset_device()
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
	d3d_reset_parameters(params, mainhwnd);

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

	// set white texture
	set_texture(resource_texture);

	// draw main window
	draw_mainwindow();
	return 0;
}

struct KeyboardState
{
	byte action;
	float x1;
	float y1;
	float x2;
	float y2;
	uint status;
};

struct MidiKeyState
{
	float x;
	float y;
	float width;
	float height;
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

		{ 0,				3,	15.5,	0.0f,	},
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

		{ 0,				3,	19.0f,	1.2f,	},
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

	for (Key * key = keys1; key < keys1 + sizeof(keys1) / sizeof(Key); key++)
	{
		if (key->code)
		{
			KeyboardState & state = keyboard_states[key->code];
			state.x1 = round(12 + x * 25);
			state.y1 = round(12 + y * 25);
			state.x2 = round(12 + x * 25 + key->x * 25);
			state.y2 = round(12 + y * 25 + key->y * 25);
			state.action = key->action;
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

	float x = 10;
	float y = 340;

	for (int i = 21; i < 21 + 88; i++)
	{
		MidiKeyState & key = midi_key_states[i];

		// black key
		if (key_flags[(i) % 12])
		{
			key.x = x - 4;
			key.y = y;
			key.width = 6;
			key.height  = 30;
			key.black = true;
		}
		else
		{
			key.x = x;
			key.y = y;
			key.width = 11;
			key.height = 50;
			x = x + 11;
			key.black = false;
		}
	}
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

// draw menu
static void draw_menu()
{
}

// draw keyboard
static void draw_keyboard()
{
	for (KeyboardState * key = keyboard_states; key < keyboard_states + 256; key++)
	{
		if (key->x2 > key->x1)
		{
			MidiEvent map;

			// get keyboard map
			keyboard_get_map(key - keyboard_states, &map, NULL);
			
			// modify keyboard map
			midi_modify_event(map.action, map.arg1, map.arg2, map.arg3, keyboard_get_octshift(map.action & 0xf), keyboard_get_velocity(map.action & 0xf));

			float x1 = key->x1;
			float y1 = key->y1;
			float x2 = key->x2;
			float y2 = key->y2;
			float u1 = 0.f;
			float v1 = 0.f;

			switch (map.action & 0xf0)
			{
			case 0x00:	u1 = 0; break;
			case 0x80:	u1 = 25; break;
			case 0x90:	u1 = 25; break;
			case 0xb0:	u1 = 75; break;
			default:	u1 = 75; break;
			}

			float u2 = u1 + 25;
			float v2 = v1 + 25;

			if (key->status) { v1 += 25.f; v2 += 25.f; }
			draw_sprite_border(x1, y1, x2, y2, u1, v1, u2, v2, 6, 6, 6, 6, 0xffffffff);

			if ((map.action & 0xF0) == 0x90)
			{
				byte note = map.arg1;

				if (note > 12)
				{
					float x3 = round(x1 + (x2 - x1 - 25.f) * 0.5f);
					float x4 = x3 + 25.f;
					float y3 = round(y1 + (y2 - y1 - 25.f) * 0.5f);
					float y4 = y3 + 25.f;
					float u3 = (note % 12) * 25.f;
					float v3 = (note / 12) * 25.f + 25.f;
					float u4 = u3 + 25.f;
					float v4 = v3 + 25.f;
					draw_sprite(x3, y3, x4, y4, u3, v3, u4, v4, 0xff000000);
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
			if (key->width && key->height)
			{
				float l = key->x;
				float t = key->y;
				float r = key->x + key->width;
				float b = key->y + key->height;
				float u1 = 100;
				float v1 = 0;
				float u2 = 111;
				float v2 = 50;
				if (key->status) { u1 += 11; u2 += 11; }
				draw_sprite(l, t, r, b, u1, v1, u2, v2, 0xffffffff);
			}
		}
	}

	for (MidiKeyState * key = midi_key_states; key < midi_key_states + sizeof(midi_key_states) / sizeof(midi_key_states[0]); key++)
	{
		if (key->black)
		{
			if (key->width && key->height)
			{
				float l = key->x;
				float t = key->y;
				float r = key->x + key->width;
				float b = key->y + key->height;
				float u1 = 125;
				float v1 = 0;
				float u2 = 131;
				float v2 = 30;
				if (key->status) { u1 += 6; u2 += 6; }
				draw_sprite(l, t, r, b, u1, v1, u2, v2, 0xffffffff);
			}
		}
	}
}

// draw entire main window
static void draw_mainwindow()
{
	// begin scene
	if (FAILED(device->BeginScene()))
	{
		device->Present(NULL, NULL, NULL, NULL);
		return;
	}

	// clear backbuffer
	device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 0, 0);

	{
		D3DVIEWPORT9 vp;
		device->GetViewport(&vp);
		float sx = 2.0f / (float)vp.Width;
		float sy = -2.0f / (float)vp.Height;
		float tx = -1 - sx * 0.5f;
		float ty = 1 + sy * 0.5f;

		D3DMATRIX projection = {
			sx, 0, 0, 0,
			0, sy, 0, 0,
			0, 0, 1, 0,
			tx, ty, 0, 1,
		};

		device->SetTransform(D3DTS_PROJECTION, &projection);
		device->SetTransform(D3DTS_VIEW, &matrix_identity);
	}

	draw_menu();
	draw_keyboard();
	draw_midi_keyboard();

	device->EndScene();
	device->Present(NULL, NULL, NULL, NULL);
}


// -----------------------------------------------------------------------------------------
// window functions
// -----------------------------------------------------------------------------------------
static LRESULT CALLBACK windowproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool in_sizemove = false;

	switch (uMsg)
	{
	case WM_CREATE:
		in_sizemove = false;
		break;

	case WM_ACTIVATE:
		keyboard_enable(WA_INACTIVE != LOWORD(wParam));
		break;

	case WM_PAINT:
		if (device)
		{
			draw_mainwindow();
			device->Present(NULL, NULL, NULL, NULL);
		}

		// validate entire window rect
		ValidateRect(hWnd, NULL);
		return 0;

	case WM_KEYBOARD_EVENT:
		{
			byte code = wParam;
			keyboard_states[code].status = lParam;
			InvalidateRect(hWnd, NULL, false);
		}
		break;

	case WM_MIDI_EVENT:
		{
			byte data1 = (byte)(lParam >> 0);
			byte data2 = (byte)(lParam >> 8);
			byte data3 = (byte)(lParam >> 16);

			switch(data1 & 0xf0)
			{
			case 0x90:	midi_key_states[data2].status = data3; break;
			case 0x80:	midi_key_states[data2].status = 0; break;
			}
			InvalidateRect(hWnd, NULL, false);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_SYSCOMMAND:
		// disable sys menu
		if (wParam == SC_KEYMENU)
			return 0;
		break;

	case WM_EXITSIZEMOVE:
		in_sizemove = false;
		puts("WM_EXITSIZEMOVE");
		d3d_reset_device();
		break;

	case WM_DESTROY:
		// quit application
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// init display
int display_init()
{
	// register window class
	WNDCLASSEX wc = { sizeof(wc), 0 };
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = &windowproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "EPianoMainWnd";
	wc.hIconSm = NULL;

	RegisterClassEx(&wc);
	
	int width = 600;
	int height = 400;
	uint style = WS_OVERLAPPEDWINDOW;

	int screenwidth = GetSystemMetrics(SM_CXSCREEN);
	int screenheight = GetSystemMetrics(SM_CYSCREEN);

	RECT rect;
	rect.left = (screenwidth - width) / 2;
	rect.top = (screenheight - height) / 2;
	rect.right = rect.left + width;
	rect.bottom = rect.top + height;

	AdjustWindowRect(&rect, style, FALSE);

	// create window
	mainhwnd = CreateWindow("EPianoMainWnd", APP_NAME, style,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, GetModuleHandle(NULL), NULL);

	if (mainhwnd == NULL)
	{
		fprintf(stderr, "failed to create window");
		return -1;
	}

	// disable ime
	ImmAssociateContext(mainhwnd, NULL);

	// initialize directx 9
	if (d3d_initialize(mainhwnd))
	{
		fprintf(stderr, "failed to initialize d3d");
		return -1;
	}

	// reset device
	d3d_reset_device();

	// initialize keyboard display
	init_keyboard_states();
	init_midi_keyboard_states();

	return 0;
}

// shutdown display
int display_shutdown()
{
	if (mainhwnd)
	{
		DestroyWindow(mainhwnd);
		mainhwnd = NULL;
	}

	d3d_shutdown();
	return 0;
}

// get display window handle
HWND display_get_hwnd()
{
	return mainhwnd;
}


// show main window
void display_show()
{
	// show main window
	ShowWindow(mainhwnd, SW_SHOW);

	// draw keyboard 
	draw_mainwindow();
}

// display set key down
void display_keyboard_event(byte code, uint status)
{
	PostMessage(mainhwnd, WM_KEYBOARD_EVENT, code, status);
}

// display update midi
void display_midi_event(byte data1, byte data2, byte data3, byte data4)
{
	PostMessage(mainhwnd, WM_MIDI_EVENT, 0, data1 | (data2 << 8) | (data3 << 16) | (data4 << 24));
}