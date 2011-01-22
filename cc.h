#define MAIN_FRAME    155

#define IDM_EXIT      200
#define IDM_MINIMIZE  205
#define IDM_PROP      231

#ifndef RESOURCE_IDS_ONLY

#define GRID_STEP 2
#define THUMB_WIDTH 0
#define WINDOW_BORDER 0
#define CONTENTS_BORDER 2

#define LEFT_SIDE (WINDOW_BORDER + CONTENTS_BORDER)
#define RIGHT_SIDE (THUMB_WIDTH + LEFT_SIDE)
#define TOP_BOTTOM LEFT_SIDE

#ifdef __OS2__
#define FONT1 "8.Helv"
#define FONT1NAME "Helvetica"
#define FONT2 "9.WarpSans"
#define FONT2NAME "WarpSans"
#define FONT3 "9.WarpSans Bold"
#define FONT3NAME "WarpSans Bold"
#define FONT4 "9.Courier"
#define FONT4NAME "Courier"
#elif defined(__WIN32__)
#define FONT1 "9.Arial"
#define FONT1NAME "Arial"
#define FONT2 "9.Tahoma"
#define FONT2NAME "Tahoma"
#define FONT3 "9.Verdana"
#define FONT3NAME "Verdana"
#define FONT4 "9.Courier"
#define FONT4NAME "Courier"
#else
#define FONT1 "monospace 10"
#define FONT1NAME "Monospace"
#define FONT2 "sans 10"
#define FONT2NAME "Sans"
#define FONT3 "helvetica medium 12"
#define FONT3NAME "Helvetica"
#define FONT4 "courier medium 12"
#define FONT4NAME "Courier"
#endif

#define fAttached       0x00000001
#define fFloat          0x00000002
#define fTitle          0x00000004
#define fConfigure      0x00000008
#define fTraffic        0x00000010
#define fNoText         0x00000020
#define fGraph2         0x00000040
#define fLockInPlace    0x00000080
#define fMinimized      0x00000100
#define fHidden         0x00000200

typedef struct _instance {
	char *Name;
	void(*Draw)(struct _instance *);
	void(*Create)(struct _instance *, HWND owner);
	void(*Update)(struct _instance *, HWND owner);
	unsigned Flags;
	int page;
	int width, height, vsize;
	void *custom, *internal;
	HWND hwnd, other, *hwndDraw;
	HPIXMAP *pixmap;
} Instance;

typedef struct _textconfig {
	char text1[255];
	char text2[255];
	void *user;
} TextConfig;

typedef struct _barconfig {
	long currentval;
	long maximum;
	char text[255];
	int len;
	void *user;
} BarConfig;

typedef struct _graphconfig {
	int average;
	char text[255];
	void *user;
} GraphConfig;

typedef struct _netconfig {
	int count;
	void *user;
} NetConfig;

enum color_list {
	COLOR_BACK,
	COLOR_BAR,
	COLOR_HIGH_LIGHT,
	COLOR_LOW_LIGHT,
	COLOR_BORDER,
	COLOR_THUMB,
	COLOR_TEXT,
	COLOR_AVERAGE,
	COLOR_GRID,
	COLOR_RECV,
    COLOR_SENT,
	COLOR_MAX
};

/* Graph Variables */
int current_time = 0, display_active = 1;
int button_down = 0, dragx = 0, dragy = 0;
HMTX hMtx;
HWND hwndFrame, hwndHbox;

void display_create(void);
void display_update(void);
void display_destroy(void);
void DWSIGNAL display_menu(HWND hwnd, void *data);

/* Display function declarations */
void graph_draw(struct _instance *inst);
void text_draw(struct _instance *inst);
void bar_draw(struct _instance *inst);
void desk_draw(struct _instance *inst);
void net_draw(struct _instance *inst);
void graph_create(struct _instance *inst, HWND owner);
void text_create(struct _instance *inst, HWND owner);
void bar_create(struct _instance *inst, HWND owner);
void desk_create(struct _instance *inst, HWND owner);
void net_create(struct _instance *inst, HWND owner);
void cpu_update(struct _instance *inst, HWND owner);
void net_update(struct _instance *inst, HWND owner);
void netavg_update(struct _instance *inst, HWND owner);
void netmax_update(struct _instance *inst, HWND owner);
void nettot_update(struct _instance *inst, HWND owner);
void memory_update(struct _instance *inst, HWND owner);
void uptime_update(struct _instance *inst, HWND owner);
void clock_update(struct _instance *inst, HWND owner);
void drive_update(struct _instance *inst, HWND owner);

void graph_add_statistic(int id, unsigned long value);

void new_colors(int index);
void new_font(int index);
#endif
