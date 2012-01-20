#include "compat.h"
#include "dw.h"
#include "cc.h"
#include "stats.h"

/* These variables are used to define each monitor */
TextConfig netavg = { "In (now):    ", "Out (now):    ", 0 };
TextConfig netmax = { "In (max):    ", "Out (max):    ", 0 };
TextConfig nettot = { "Rcvd:   ", "Sent:   ", 0 };
TextConfig memory = { "Memory:    ", "", 0 };
TextConfig uptime = { "Uptime:", "", 0 };
TextConfig tclock = { "12:00AM", "", (void *)1 };
BarConfig drive = { 50, 100};
NetConfig network = { FALSE };
GraphConfig cpu = { FALSE, "" };
unsigned long desks = 4;

/* Enumerate all monitors */
enum display_list {
#if 0
	DISPLAY_DESK,
#endif
	DISPLAY_CPU,
	DISPLAY_NET,
	DISPLAY_NETAVG,
	DISPLAY_NETMAX,
	DISPLAY_NETTOT,
	DISPLAY_MAX
};

/* Define the monitors, these should corespond to the enumeration above */
Instance gList[40]=
{
#if 0
	{"Virtual Desktops"           , desk_draw  , desk_create  , 0,               0, 0, 8, 40, TRUE , &desks },
#endif
	{"CPU Meter"                  , graph_draw , graph_create , cpu_update,      0, 0, 8, 40, TRUE,  &cpu },
	{"Network Meter"              , net_draw   , net_create   , net_update,      0, 0, 8, 30, FALSE, &network },
	{"Network Average"            , text_draw  , text_create  , netavg_update,   0, 0, 8, 30, FALSE, &netavg },
	{"Network Max"                , text_draw  , text_create  , netmax_update,   0, 0, 8, 30, FALSE, &netmax },
	{"Network Total"              , text_draw  , text_create  , nettot_update,   0, 0, 8, 30, FALSE, &nettot },
	{"Memory Meter"               , text_draw  , text_create  , memory_update,   0, 0, 8, 20, FALSE, &memory },
	{"Uptime"                     , text_draw  , text_create  , uptime_update,   0, 0, 8, 20, FALSE, &uptime },
	{"Clock"                      , text_draw  , text_create  , clock_update,    0, 0, 8, 20, FALSE, &tclock },
	{0, 0, 0}
};

unsigned long *current_colors;
char *current_font;
int current_color = 1, current_font_num = 0, minimized = 0;
long x = 0, y = 0;
unsigned long width = 140, height = 600;

char *fonts[4] = {
	FONT1,
	FONT2,
	FONT3,
	FONT4
};

unsigned long colors[COLOR_MAX] = {
	DW_RGB(152, 160, 168),
	DW_RGB(128, 128, 128),
	DW_RGB(248, 252, 248),
	DW_RGB(0, 0, 0),
	DW_RGB(200, 204, 200),
	DW_RGB(200, 204, 200),
	DW_RGB(0, 0, 128),
	DW_RGB(200, 204, 200),
	DW_RGB(2, 130, 130),
	DW_RGB(2, 254, 250),
	DW_RGB(250, 254, 2)
};

unsigned long Sent = 0, Recv = 0, TotalSent = 0, TotalRecv = 0, MaxSent = 0, MaxRecv = 0;

/* Modify selected flags in the monitor list */
void set_flags(int entry, unsigned long value, unsigned long mask)
{
	unsigned long tmp;

	if(entry > -1)
	{
		tmp = gList[entry].Flags | mask;
		tmp ^= mask;
		tmp |= value;
		gList[entry].Flags = tmp;
	}
}

/* Update the monitor array with the current values */
void update_pos(void)
{
	dw_window_get_pos_size(hwndFrame, &x, &y, &width, &height);

}

/* Set the position of the monitors based on the saved positions in the array */
void restore_pos(void)
{
	dw_window_set_pos_size(hwndFrame, x, y, width, height);
}

/* Write the logview.ini file with all of the current settings */
void saveconfig(void)
{
	FILE *f;
	char *tmppath = INIDIR, *inidir, *inipath, *home = dw_user_dir();

	update_pos();

	if(strcmp(INIDIR, ".") == 0)
	{
		inipath = strdup("cc.ini");
		inidir = strdup(INIDIR);
	}
	else
	{
		if(home && tmppath[0] == '~')
		{
			inipath = malloc(strlen(home) + strlen(INIDIR) + 14);
			inidir = malloc(strlen(home) + strlen(INIDIR));
			strcpy(inipath, home);
			strcpy(inidir, home);
			strcat(inipath, &tmppath[1]);
			strcat(inidir, &tmppath[1]);
		}
		else
		{
			inipath = malloc(strlen(INIDIR) + 14);
			strcat(inipath, INIDIR);
			inidir = strdup(INIDIR);
		}
		strcat(inipath, DIRSEP);
		strcat(inipath, "cc.ini");
	}

	f=fopen(inipath, FOPEN_WRITE_TEXT);

	if(f==NULL)
	{
		if(strcmp(INIDIR, ".") != 0)
		{
			makedir(inidir);
			f=fopen(inipath, FOPEN_WRITE_TEXT);
		}
		if(f==NULL)
		{
			dw_messagebox("Gateway", DW_MB_ERROR | DW_MB_OK, "Could not save settings. Inipath = \"%s\"", inipath);
			free(inipath);
			free(inidir);
			return;
		}
	}

	free(inipath);
	free(inidir);

	fprintf(f, "WIDTH=%d\n", (int)width);
	fprintf(f, "HEIGHT=%d\n", (int)height);
	fprintf(f, "X=%d\n", (int)x);
	fprintf(f, "Y=%d\n", (int)y);

	fclose(f);
}

/* Generic function to parse information from a config file */
void cc_getline(FILE *f, char *entry, char *entrydata)
{
	char in[256];
	int z;

	memset(in, 0, 256);
	fgets(in, 255, f);

	if(in[strlen(in)-1] == '\n')
		in[strlen(in)-1] = 0;

	if(in[0] != '#')
	{
		for(z=0;z<strlen(in);z++)
		{
			if(in[z] == '=')
			{
				in[z] = 0;
				strcpy(entry, in);
				strcpy(entrydata, &in[z+1]);
				return;
			}
		}
	}
	strcpy(entry, "");
	strcpy(entrydata, "");
}

/* Load the logview.ini file from disk setting all the necessary flags */
void loadconfig(void)
{
	char *tmppath = INIDIR, *inipath, *home = dw_user_dir();
	char entry[256], entrydata[256];
	FILE *f;

	if(strcmp(INIDIR, ".") == 0)
		inipath = strdup("cc.ini");
	else
	{
		if(home && tmppath[0] == '~')
		{
			inipath = malloc(strlen(home) + strlen(INIDIR) + 14);
			strcpy(inipath, home);
			strcat(inipath, &tmppath[1]);
		}
		else
		{
			inipath = malloc(strlen(INIDIR) + 14);
			strcat(inipath, INIDIR);
		}
		strcat(inipath, DIRSEP);
		strcat(inipath, "cc.ini");
	}

	f = fopen(inipath, FOPEN_READ_TEXT);

	free(inipath);

	if(f)
	{
		while(!feof(f))
		{
			cc_getline(f, entry, entrydata);

			if(strcasecmp(entry, "width")==0)
				width = atoi(entrydata);
			if(strcasecmp(entry, "height")==0)
				height = atoi(entrydata);
			if(strcasecmp(entry, "x")==0)
				x = atoi(entrydata);
			if(strcasecmp(entry, "y")==0)
				y = atoi(entrydata);

		}
		fclose(f);
	}
}

/* Creates pull down or popup menu */
void DWSIGNAL menu_create(HWND handle, HWND hwnd)
{
	HMENUI hwndMenu;
	HWND menuitem;
	long px, py;

	hwndMenu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(hwndMenu, "Properties", IDM_PROP, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(display_menu), (void *)IDM_PROP);

	menuitem = dw_menu_append_item(hwndMenu, "~Minimize", IDM_MINIMIZE, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(display_menu), (void *)IDM_MINIMIZE);
	dw_menu_append_item(hwndMenu, "", 0L, 0L, TRUE, FALSE, 0L);
	menuitem = dw_menu_append_item(hwndMenu, "E~xit", IDM_EXIT, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(display_menu), (void *)IDM_EXIT);

	dw_pointer_query_pos(&px, &py);
	dw_menu_popup(&hwndMenu, hwnd, px, py);
}

/* This function creates the display windows */
void display_create(void)
{
	int z = 0;
	ULONG flStyle = DW_FCF_SIZEBORDER | DW_FCF_TASKLIST;

#if defined(__UNIX__) && !defined(__MAC__)
	/* Enlightenment, the window manager we anticipate on
	 * Red Hat does not make a size border unless it has
	 * a titlebar in it's default theme.  So we must force
	 * a titlebar.  This is not the case on other platorms.
	 */
	flStyle |= DW_FCF_TITLEBAR;
#endif

	hwndFrame = dw_window_new(HWND_DESKTOP, "Control Center", flStyle);

	hwndHbox = dw_box_new(BOXVERT, 0);

	dw_box_pack_start(hwndFrame, hwndHbox, 0, 0, TRUE, TRUE, 0);

	while(gList[z].Name)
	{
		if(!(gList[z].Flags & fHidden))
		{
			gList[z].Create(&gList[z], hwndHbox);
		}
		z++;
	}
	hMtx = dw_mutex_new();

	restore_pos();

	dw_window_set_icon(hwndFrame, DW_RESOURCE(MAIN_FRAME));

	dw_window_default(hwndFrame, gList[0].hwndDraw[0]);

	dw_window_show(hwndFrame);

	dw_thread_new((void *)display_update, NULL, 0xFFFF);

}

/* This function continually updates the display windows */
void DWSIGNAL display_update(void)
{
	srand(time(NULL));

	while(display_active)
	{
		int z = 0;

		msleep(1000);

		dw_mutex_lock(hMtx);

		current_time++;

		while(gList[z].Name)
		{
			if(gList[z].Update)
				gList[z].Update(&gList[z], 0);
			if(gList[z].Draw && !(gList[z].Flags & fHidden))
				gList[z].Draw(&gList[z]);
			z++;
		}
		dw_mutex_unlock(hMtx);

#if defined(__MAC__) && !defined(GARBAGE_COLLECT)
		_dw_pool_drain();
#endif
	}
	display_destroy();
}

void display_destroy_monitor(int entry)
{
	int x = 0;

	dw_mutex_lock(hMtx);

	dw_window_destroy(gList[entry].hwnd);
	gList[entry].hwnd = 0;
	if(gList[entry].hwndDraw)
	{
		free(gList[entry].hwndDraw);
		gList[entry].hwndDraw = NULL;
	}
	if(gList[entry].pixmap)
	{
		while(gList[entry].pixmap[x])
		{
			dw_pixmap_destroy(gList[entry].pixmap[x]);
			x++;
		}
		free(gList[entry].pixmap);
		gList[entry].pixmap = NULL;
	}
	dw_mutex_unlock(hMtx);
}

/* This function destroys the display windows */
void display_destroy(void)
{
	int z =0;

	while(gList[z].Name)
	{
		display_destroy_monitor(z);
		z++;
	}
}

/* Returns TRUE if the window handle passed is that of the display ID */
int is_window(HWND hwnd, int which)
{
	if(which < DISPLAY_MAX && which > -1 && gList[which].hwnd == hwnd)
		return TRUE;
	return FALSE;
}

void DWSIGNAL delete_event(HWND hwnd, void *data)
{
	display_destroy();
	exit(0);
}

/* Context menus */
void DWSIGNAL display_menu(HWND hwnd, void *data)
{
	switch(DW_POINTER_TO_INT(data))
	{
	case IDM_EXIT:
		{
			update_pos();
			saveconfig();
			dw_main_quit();
		}
		break;
	case IDM_PROP:
		break;

	case IDM_MINIMIZE:
		{
			dw_window_minimize(hwndFrame);
		}
		break;
	}
}

/* This gets called when the graph resizes */
int DWSIGNAL display_configure(HWND hwnd, int width, int height, void *data)
{
	Instance *inst = (Instance *)data;

	if(inst)
	{
		int z = 0;

		while(inst->hwndDraw[z])
		{
			if(hwnd == inst->hwndDraw[z])
			{
				dw_mutex_lock(hMtx);

				if(inst->pixmap[z])
					dw_pixmap_destroy(inst->pixmap[z]);

				inst->pixmap[z] = dw_pixmap_new(inst->hwndDraw[z], width, height, 0);

				inst->Draw(inst);

				dw_mutex_unlock(hMtx);

				break;
			}
			z++;
		}
	}
	return TRUE;
}

/* This gets called when a part of the graph needs to be repainted. */
int DWSIGNAL display_expose(HWND hwnd, DWExpose *exp, void *data)
{
	Instance *inst = (Instance *)data;

	if(inst)
	{
		dw_mutex_lock(hMtx);

		inst->Draw(inst);

		dw_mutex_unlock(hMtx);
	}
	return TRUE;
}

/* Handles button presses on the graph window */
int DWSIGNAL display_button_press(HWND hwnd, int x, int y, int button, void *data)
{
	/* When clicked CC should take focus, but
	 * do it before creating the menu.
	 */
	dw_window_show(hwndFrame);
	if(button == 1)
	{
		dragx = x;
		dragy = y;
		button_down = 1;
		dw_window_capture(hwnd);
	}
	else if (button == 2)
		menu_create(0, hwnd);
	return TRUE;
}

/* Handles button releases on the graph window */
int DWSIGNAL display_button_release(HWND hwnd, int x, int y, int button, void *data)
{
	if(button == 1)
	{
		button_down = 0;
		dw_window_release();
	}
	return TRUE;
}

/* Handles motion events to move the graph window when button 1 is down */
int DWSIGNAL display_motion_notify(HWND hwnd, int x, int y, int buttonmask, void *data)
{
	Instance *inst = (Instance *)data;
	static int lastx = 0, lasty = 0;

	if(lastx == x && lasty == y)
		return TRUE;

	if(inst && button_down == 1 && (buttonmask & DW_BUTTON1_MASK))
	{
		int delta_x = x - dragx, delta_y = y - dragy;
		long graphx, graphy;

		dw_window_get_pos_size(hwndFrame, &graphx, &graphy, NULL, NULL);
		dw_window_set_pos(hwndFrame, graphx + delta_x, graphy + delta_y);

		lastx = x;
		lasty = y;
	}
	return TRUE;
}

void draw_box(HPIXMAP hPixmap, int x, int y, int width, int height, int indent, unsigned long left, unsigned long top, unsigned long right, unsigned long bottom, int fill, unsigned long fillcolor)
{
	if(indent * 2 > width)
		return;

	dw_color_foreground_set(left);
	dw_draw_line(0, hPixmap, x + indent, y + indent, x + indent, height - indent - 1);
	dw_color_foreground_set(top);
	dw_draw_line(0, hPixmap, x + indent, y + indent, width - indent - 1, y + indent);
	dw_color_foreground_set(right);
	dw_draw_line(0, hPixmap, width - indent - 1, y + indent + 1, width - indent - 1, height - indent - 1);
	dw_color_foreground_set(bottom);
	dw_draw_line(0, hPixmap, x + indent, height - indent - 1, width - indent - 2, height - indent - 1);
	if(fill && ((indent + 1) * 2) < width)
	{
		dw_color_foreground_set(fillcolor);
		dw_draw_rect(0, hPixmap, TRUE, x + indent + 1, y + indent + 1, width - ((indent + 1) * 2), height - ((indent + 1) * 2));
	}
}

void graph_add_statistic(int id, unsigned long value)
{
	if(id > -1 && id < DISPLAY_MAX)
	{
		unsigned long *values = (unsigned long *)gList[id].internal;

		/* Advance the items */
		memmove(&values[1], values, (16384/GRID_STEP));

		values[0] = value;
	}
}

void net_add_statistic(int id, unsigned long value1, unsigned long value2)
{
	if(id > -1 && id < DISPLAY_MAX)
	{
		unsigned long *values = (unsigned long *)gList[id].internal;

		/* Advance the items */
		memmove(&values[1], values, (16384/GRID_STEP) - 1);

		values[0] = value2;

		values = &values[(16384/GRID_STEP)];

		/* Advance the items */
		memmove(&values[1], values, (16384/GRID_STEP) - 1);

		values[0] = value1;
	}
}

unsigned long graph_find_max(struct _instance *inst, int count)
{
	unsigned long max = 0, *values = (unsigned long *)inst->internal;
	int z;

	for(z=0;z<count;z++)
	{
		if(values[z] > max)
			max = values[z];
	}
	return max;
}

unsigned long graph_find_average(struct _instance *inst, int count)
{
	unsigned long max = 0, *values = (unsigned long *)inst->internal;
	double total = 0;
	int z;

	for(z=0;z<count;z++)
	{
		if(values[z] > max)
			total += values[z];
	}
	return (unsigned long)total/count;
}

/* Creates a graph style display */
void graph_create(struct _instance *inst, HWND owner)
{
	inst->hwndDraw = calloc(2, sizeof(HWND));
	inst->pixmap = calloc(2, sizeof(HPIXMAP));

	inst->internal = calloc((16384/GRID_STEP) + 1, sizeof(unsigned long));

	inst->hwndDraw[0] = dw_render_new(125);
	dw_window_set_font(inst->hwndDraw[0], current_font);

	dw_box_pack_start(owner, inst->hwndDraw[0], inst->width, inst->height, TRUE, inst->vsize, 0);

	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_CONFIGURE, DW_SIGNAL_FUNC(display_configure), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_EXPOSE, DW_SIGNAL_FUNC(display_expose), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_PRESS, DW_SIGNAL_FUNC(display_button_press), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_RELEASE, DW_SIGNAL_FUNC(display_button_release), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_MOTION_NOTIFY, DW_SIGNAL_FUNC(display_motion_notify), inst);
}

/* Draw a graph style display */
void graph_draw(struct _instance *inst)
{
	unsigned long width, height;
	int z;
	GraphConfig *graphconfig = (GraphConfig *)inst->custom;

	if(inst && inst->pixmap && *(inst->pixmap) && graphconfig)
	{
		HPIXMAP hPixmap;
		unsigned long *values = (unsigned long *)inst->internal;

		hPixmap = *(inst->pixmap);

		width = DW_PIXMAP_WIDTH(hPixmap);
		height = DW_PIXMAP_HEIGHT(hPixmap);

		dw_color_foreground_set(current_colors[COLOR_BACK]);

		/* Clear the graph area */
		dw_draw_rect(0, hPixmap, TRUE, LEFT_SIDE, TOP_BOTTOM, width - (LEFT_SIDE + RIGHT_SIDE), height - (TOP_BOTTOM * 2));

		dw_color_foreground_set(current_colors[COLOR_BAR]);

		if(width > (RIGHT_SIDE + LEFT_SIDE))
		{
			int item = 0, myheight = height - (TOP_BOTTOM * 2);
			unsigned long max, average;

			/* Draw vertical lines */
			for(z=((4)-(current_time%4))*GRID_STEP;z<(width - (RIGHT_SIDE + LEFT_SIDE));z+=(GRID_STEP*4))
				dw_draw_line(0, hPixmap, z + LEFT_SIDE, TOP_BOTTOM, z + LEFT_SIDE, height - TOP_BOTTOM - 1);

			/* Draw horizontal lines */
			for(z=0;z<height;z+=(GRID_STEP*3))
				dw_draw_line(0, hPixmap, LEFT_SIDE, z + TOP_BOTTOM, width - RIGHT_SIDE - 1, z + TOP_BOTTOM);

			max = 100; /*graph_find_max(inst, ((width - (RIGHT_SIDE - LEFT_SIDE))/GRID_STEP) + 1);*/

			if(graphconfig->average)
				average = graph_find_average(inst, ((width - (RIGHT_SIDE - LEFT_SIDE))/GRID_STEP) + 1);

			if(max < 10)
				max = 10;

			/* Draw the Average */
			if(graphconfig->average)
			{
				dw_color_foreground_set(current_colors[COLOR_AVERAGE]);

				dw_draw_line(0, hPixmap, LEFT_SIDE, (myheight + TOP_BOTTOM) - (int)((float)myheight * ((float)average/(float)max)), width - RIGHT_SIDE, (myheight + TOP_BOTTOM) - (int)((float)myheight * ((float)average/(float)max)));
			}

			/* Draw the actual graph values. */
			dw_color_foreground_set(current_colors[COLOR_TEXT]);

			for(z=(width - (RIGHT_SIDE - LEFT_SIDE));z>LEFT_SIDE;z-=GRID_STEP)
			{
				dw_draw_line(0, hPixmap, z, (myheight + TOP_BOTTOM) - (int)((float)myheight * ((float)values[item]/(float)max)), z - GRID_STEP, (myheight + TOP_BOTTOM) - (int)((float)myheight * ((float)values[item+1]/(float)max)));
				item++;
			}

		}

		/* Draw some labels */
		dw_color_foreground_set(DW_RGB(190,190,190));
		dw_draw_text(0, hPixmap, 2 + LEFT_SIDE, 3 + TOP_BOTTOM, graphconfig->text);

		dw_color_foreground_set(current_colors[COLOR_TEXT]);
		dw_draw_text(0, hPixmap, 1 + LEFT_SIDE, 2 + TOP_BOTTOM, graphconfig->text);

		if(graphconfig->average)
		{
			dw_color_foreground_set(current_colors[COLOR_AVERAGE]);

			dw_draw_text(0, hPixmap, 1 + LEFT_SIDE, 20 + TOP_BOTTOM, "Average");
		}

		/* Draw window border */
		dw_color_foreground_set(current_colors[COLOR_BORDER]);
		dw_draw_line(0, hPixmap, 0, 0, width, 0);
		dw_draw_line(0, hPixmap, 0, 0, 0, height);
		dw_draw_line(0, hPixmap, 0, height - 1, width, height - 1);
		dw_draw_line(0, hPixmap, width - 1, 0, width - 1, height);

		dw_color_foreground_set(current_colors[COLOR_LOW_LIGHT]);
		dw_draw_line(0, hPixmap, WINDOW_BORDER + 1, WINDOW_BORDER + 1, WINDOW_BORDER + 1, height - (WINDOW_BORDER + 1));
		dw_draw_line(0, hPixmap, WINDOW_BORDER + 1, WINDOW_BORDER + 1, width - RIGHT_SIDE, WINDOW_BORDER + 1);

		/* Draw thumb border */
		dw_color_foreground_set(current_colors[COLOR_THUMB]);
		dw_draw_line(0, hPixmap, WINDOW_BORDER, WINDOW_BORDER, width - 1, WINDOW_BORDER);
		dw_draw_line(0, hPixmap, WINDOW_BORDER, WINDOW_BORDER, WINDOW_BORDER, height - 1);
		dw_draw_line(0, hPixmap, WINDOW_BORDER, height - 1, width - 1, height - 1);

		/* Draw thumb */
		dw_draw_rect(0, hPixmap, TRUE, width - RIGHT_SIDE + 1, 1, THUMB_WIDTH + 1, height - 2);

		/* Draw last of the graph's border */
		dw_color_foreground_set(current_colors[COLOR_HIGH_LIGHT]);
		dw_draw_line(0, hPixmap, width - RIGHT_SIDE, WINDOW_BORDER + 1, width - RIGHT_SIDE, height - TOP_BOTTOM);
		dw_draw_line(0, hPixmap, WINDOW_BORDER + 1, height - TOP_BOTTOM, width - RIGHT_SIDE, height - TOP_BOTTOM);

		/* Blit the image from the memory pixmap to the screen window */
		dw_pixmap_bitblt(*(inst->hwndDraw), 0, 0, 0, width, height, 0, hPixmap, 0, 0);

		/* Make sure everything drawn is visible. */
		dw_flush();
	}

}

/* Creates a stats style display */
void text_create(struct _instance *inst, HWND owner)
{
	inst->hwndDraw = calloc(2, sizeof(HWND));
	inst->pixmap = calloc(2, sizeof(HPIXMAP));

	inst->hwndDraw[0] = dw_render_new(125);
	dw_window_set_font(inst->hwndDraw[0], current_font);

	dw_box_pack_start(owner, inst->hwndDraw[0], inst->width, inst->height, TRUE, inst->vsize, 0);

	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_CONFIGURE, DW_SIGNAL_FUNC(display_configure), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_EXPOSE, DW_SIGNAL_FUNC(display_expose), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_PRESS, DW_SIGNAL_FUNC(display_button_press), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_RELEASE, DW_SIGNAL_FUNC(display_button_release), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_MOTION_NOTIFY, DW_SIGNAL_FUNC(display_motion_notify), inst);
}

/* Draw a stats style display */
void text_draw(struct _instance *inst)
{
	unsigned long width, height;

	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		HPIXMAP hPixmap;

		hPixmap = inst->pixmap[0];

		if(hPixmap)
		{
			int start1 = 3, start2 = 3, textwidth;

			width = DW_PIXMAP_WIDTH(hPixmap);
			height = DW_PIXMAP_HEIGHT(hPixmap);

			/* Clear the graph area */
			dw_color_foreground_set(colors[COLOR_THUMB]);
			dw_draw_rect(0, hPixmap, TRUE, 0, 0, width, height);

			draw_box(hPixmap, 0, 0, width, height, 0, DW_RGB(0,0,0), DW_RGB(0,0,0), DW_RGB(255,255,255), DW_RGB(255,255,255), 0, 0);

			if(texts->user)
			{
				dw_font_text_extents_get(0, hPixmap, texts->text1, &textwidth, 0);
				start1 = ((width - textwidth) / 2);
				if(texts->text2[0])
				{
					dw_font_text_extents_get(0, hPixmap, texts->text2, &textwidth, 0);
					start2 = ((width - textwidth) / 2);
				}
			}

			/* Draw some labels */
			dw_color_foreground_set(DW_RGB(255,255,255));
			if(texts->text2[0])
			{
				dw_draw_text(0, hPixmap, start1 + 1 + LEFT_SIDE, ((((height - (TOP_BOTTOM * 2))/2) - 12)/2) + 1, texts->text1);
				dw_draw_text(0, hPixmap, start2 + 1 + LEFT_SIDE, ((height - (TOP_BOTTOM * 2))/2) + ((((height - (TOP_BOTTOM * 2))/2)-12)/2) + 1, texts->text2);
			}
			else
				dw_draw_text(0, hPixmap, start1 + 1 + LEFT_SIDE, TOP_BOTTOM + 1, texts->text1);


			dw_color_foreground_set(DW_RGB(0,0,0));
			if(texts->text2[0])
			{
				dw_draw_text(0, hPixmap, start1 + LEFT_SIDE, ((((height - (TOP_BOTTOM * 2))/2) - 12)/2), texts->text1);
				dw_draw_text(0, hPixmap, start2 + LEFT_SIDE, ((height - (TOP_BOTTOM * 2))/2) + ((((height - (TOP_BOTTOM * 2))/2)-12)/2), texts->text2);
			}
			else
				dw_draw_text(0, hPixmap, start1 + LEFT_SIDE, TOP_BOTTOM, texts->text1);

			/* Blit the image from the memory pixmap to the screen window */
			dw_pixmap_bitblt(inst->hwndDraw[0], 0, 0, 0, width, height, 0, hPixmap, 0, 0);

			/* Make sure everything drawn is visible. */
			dw_flush();
		}
	}

}

/* Draw a 1 bar style display */
void bar_draw(struct _instance *inst)
{
	unsigned long width, height;
	BarConfig *barconfig = (BarConfig *)inst->custom;

	if(inst && inst->pixmap && *(inst->pixmap))
	{
		HPIXMAP hPixmap;
		float ratio = ((float)barconfig->currentval)/((float)barconfig->maximum);
		int barwidth;
		unsigned long color;

		hPixmap = *(inst->pixmap);

		width = DW_PIXMAP_WIDTH(hPixmap);
		height = DW_PIXMAP_HEIGHT(hPixmap);

		barwidth = (int)(ratio * ((float)width));

		dw_color_foreground_set(colors[COLOR_THUMB]);
		dw_draw_rect(0, hPixmap, TRUE, 0, 0, width, height);

		draw_box(hPixmap, 0, 0, width, height, 0, DW_RGB(0,0,0), DW_RGB(0,0,0), DW_RGB(255,255,255), DW_RGB(255,255,255), 0, 0);

		if(ratio > 0.9)
			color = DW_RGB(255, 60, 60);
		else if(ratio > 0.8)
		{
			int val = ((0.9 - ratio) * 10) * 195;
			color = DW_RGB(255, 60 + val, 60);
		}
		else if(ratio > 0.7)
		{
			int val = ((0.8 - ratio) * 10) * 195;
			color = DW_RGB(255 - val, 255, 60);
		}
		else
			color = DW_RGB(60, 255, 60);

		draw_box(hPixmap, 0, 0, barwidth, height, 1, DW_RGB(255,255,255), DW_RGB(255,255,255), DW_RGB(0,0,0), DW_RGB(0,0,0), 1, color);

		dw_color_foreground_set(DW_RGB(190,190,190));
		dw_draw_text(0, hPixmap, 4 + LEFT_SIDE, TOP_BOTTOM + 1, barconfig->text);
		dw_color_foreground_set(DW_RGB(0,0,0));
		dw_draw_text(0, hPixmap, 3 + LEFT_SIDE, TOP_BOTTOM, barconfig->text);

		/* Blit the image from the memory pixmap to the screen window */
		dw_pixmap_bitblt(*(inst->hwndDraw), 0, 0, 0, width, height, 0, hPixmap, 0, 0);

		/* Make sure everything drawn is visible. */
		dw_flush();
	}

}

/* Creates a 1 bar style display */
void bar_create(struct _instance *inst, HWND owner)
{
	inst->hwndDraw = calloc(2, sizeof(HWND));
	inst->pixmap = calloc(2, sizeof(HPIXMAP));

	inst->hwndDraw[0] = dw_render_new(125);
	dw_window_set_font(inst->hwndDraw[0], current_font);

	dw_box_pack_start(owner, inst->hwndDraw[0], inst->width, inst->height, TRUE, inst->vsize, 0);

	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_CONFIGURE, DW_SIGNAL_FUNC(display_configure), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_EXPOSE, DW_SIGNAL_FUNC(display_expose), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_PRESS, DW_SIGNAL_FUNC(display_button_press), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_RELEASE, DW_SIGNAL_FUNC(display_button_release), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_MOTION_NOTIFY, DW_SIGNAL_FUNC(display_motion_notify), inst);
}

/* Creates the dock window */
void desk_create(struct _instance *inst, HWND owner)
{
	unsigned long *desks = (unsigned long *)inst->custom;
	int z;

	inst->hwndDraw = calloc(*desks + 1, sizeof(HWND));
	inst->pixmap = calloc(*desks + 1, sizeof(HPIXMAP));


	for(z=0;z<*desks;z++)
	{
		inst->hwndDraw[z] = dw_render_new(125+z);
		dw_window_set_font(inst->hwndDraw[z], current_font);

		dw_box_pack_start(owner, inst->hwndDraw[z], inst->width, inst->height, TRUE, inst->vsize, 0);

		dw_signal_connect(inst->hwndDraw[z], DW_SIGNAL_CONFIGURE, DW_SIGNAL_FUNC(display_configure), inst);
		dw_signal_connect(inst->hwndDraw[z], DW_SIGNAL_EXPOSE, DW_SIGNAL_FUNC(display_expose), inst);
		dw_signal_connect(inst->hwndDraw[z], DW_SIGNAL_BUTTON_PRESS, DW_SIGNAL_FUNC(display_button_press), inst);
		dw_signal_connect(inst->hwndDraw[z], DW_SIGNAL_BUTTON_RELEASE, DW_SIGNAL_FUNC(display_button_release), inst);
		dw_signal_connect(inst->hwndDraw[z], DW_SIGNAL_MOTION_NOTIFY, DW_SIGNAL_FUNC(display_motion_notify), inst);
	}
}

/* Draw a log style display */
void desk_draw(struct _instance *inst)
{
	unsigned long width, height, z;

	if(inst && inst->pixmap)
	{
		unsigned long *desks = (unsigned long *)inst->custom;

		for(z=0;z<*desks;z++)
		{
			HPIXMAP hPixmap;

			hPixmap = inst->pixmap[z];

			width = DW_PIXMAP_WIDTH(hPixmap);
			height = DW_PIXMAP_HEIGHT(hPixmap);

			/* Draw window border */
			dw_color_foreground_set(DW_RGB(0,0,0));
			dw_draw_rect(0, hPixmap, TRUE, 0, 0, width, height);

			draw_box(hPixmap, 0, 0, width, height, 0, DW_RGB(100,100,100), DW_RGB(100,100,100), DW_RGB(255,255,255), DW_RGB(255,255,255), 0, 0);

			/* Blit the image from the memory pixmap to the screen window */
			dw_pixmap_bitblt(inst->hwndDraw[z], 0, 0, 0, width, height, 0, hPixmap, 0, 0);

			/* Make sure everything drawn is visible. */
			dw_flush();
		}
	}

}

/* Creates the dock window */
void net_create(struct _instance *inst, HWND owner)
{
	inst->hwndDraw = calloc(2, sizeof(HWND));
	inst->pixmap = calloc(2, sizeof(HPIXMAP));

	inst->internal = calloc(((16384/GRID_STEP) * 2) + 1, sizeof(unsigned long));

	inst->hwndDraw[0] = dw_render_new(125);
	dw_window_set_font(inst->hwndDraw[0], current_font);

	dw_box_pack_start(owner, inst->hwndDraw[0], inst->width, inst->height, TRUE, inst->vsize, 0);

	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_CONFIGURE, DW_SIGNAL_FUNC(display_configure), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_EXPOSE, DW_SIGNAL_FUNC(display_expose), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_PRESS, DW_SIGNAL_FUNC(display_button_press), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_BUTTON_RELEASE, DW_SIGNAL_FUNC(display_button_release), inst);
	dw_signal_connect(inst->hwndDraw[0], DW_SIGNAL_MOTION_NOTIFY, DW_SIGNAL_FUNC(display_motion_notify), inst);
}

int net_find_max(unsigned long *values, int width)
{
	int z, item = 0, max = 1000;

	for(z=0;z<width;z+=GRID_STEP)
	{
		if(values[item] > max)
			max = (int)values[item];
		item++;
	}
	return max;
}

/* Draw a log style display */
void net_draw(struct _instance *inst)
{
	unsigned long width, height, z;

	if(inst && inst->pixmap)
	{
		HPIXMAP hPixmap;
		unsigned long *values = (unsigned long *)inst->internal;

		hPixmap = *(inst->pixmap);

		width = DW_PIXMAP_WIDTH(hPixmap);
		height = DW_PIXMAP_HEIGHT(hPixmap);

		/* Draw window border */
		dw_color_foreground_set(colors[COLOR_THUMB]);
		dw_draw_rect(0, hPixmap, TRUE, 0, 0, width, height);
		dw_color_foreground_set(DW_RGB(0,0,0));
		dw_draw_rect(0, hPixmap, TRUE, 2, 2, width - 4, height - 4);

		draw_box(hPixmap, 0, 0, width, height, 2, DW_RGB(100,100,100), DW_RGB(100,100,100), DW_RGB(255,255,255), DW_RGB(255,255,255), 0, 0);

		dw_color_foreground_set(current_colors[COLOR_GRID]);

		if(width > (RIGHT_SIDE + LEFT_SIDE))
		{
			int item = 0, myheight = (height - 6)/2, mywidth = width - 6;
			unsigned long max;

			/* Draw vertical lines */
			for(z=((4)-(current_time%4))*GRID_STEP;z<(width - (RIGHT_SIDE + LEFT_SIDE));z+=(GRID_STEP*4))
				dw_draw_line(0, hPixmap, z + LEFT_SIDE, TOP_BOTTOM + 1, z + LEFT_SIDE, height - TOP_BOTTOM - 2);

			/* Draw horizontal lines */
			for(z=0;z<(height-1);z+=(GRID_STEP*3))
				dw_draw_line(0, hPixmap, LEFT_SIDE, z + TOP_BOTTOM + 1, width - RIGHT_SIDE - 2, z + TOP_BOTTOM + 1);

			/* Draw the actual graph values. */
			max = net_find_max(values, mywidth);

			if(max)
			{
				dw_color_foreground_set(current_colors[COLOR_RECV]);

				for(z=(width - 4);z>2;z-=GRID_STEP)
				{
					int point = (int)((float)myheight * ((float)values[item]/(float)max));
					if(point)
						dw_draw_rect(0, hPixmap, TRUE, z,  (myheight - point) + 3, GRID_STEP, point);
					item++;
				}
			}

			/* Update the variables to the new section. */
			values = &values[(16384/GRID_STEP)];
			item = 0;
			max = net_find_max(values, mywidth);

			if(max)
			{

				dw_color_foreground_set(current_colors[COLOR_SENT]);

				for(z=(width - 4);z>2;z-=GRID_STEP)
				{
					int point = (int)((float)myheight * ((float)values[item]/(float)max));
					if(point)
						dw_draw_rect(0, hPixmap, TRUE, z,  myheight + 3, GRID_STEP, point);
					item++;
				}
			}
		}

		/* Blit the image from the memory pixmap to the screen window */
		dw_pixmap_bitblt(*(inst->hwndDraw), 0, 0, 0, width, height, 0, hPixmap, 0, 0);

		/* Make sure everything drawn is visible. */
		dw_flush();
	}

}

void ScaledPrint(char *cBuff, long double value, int decimals)
{
	char fstr[] = "%.0Lf ", *dec = &fstr[2], *not = &fstr[5];
	long double real_value = value;
	static long double giga = 1073741824.0;

	*dec = decimals + '0';

	if(value >= (giga*1024*1024))
	{
		real_value = (long double)value/(giga*1024.0*1024.0);
		*not = 'P';
	}
	else if(value >= (giga*1024))
	{
		real_value = (long double)value/(giga*1024.0);
		*not = 'T';
	}
	else if(value >= giga)
	{
		real_value = (long double)value/(long double)giga;
		*not = 'G';
	}
	else if(value >= (1024*1024))
	{
		real_value = (long double)value/(1024.0*1024.0);
		*not = 'M';
	}
	else if(value >= 1024)
	{
		real_value = (long double)value/1024.0;
		*not = 'K';
	}
	else
		*dec = '0';

	sprintf(cBuff, fstr, (long double)real_value);
}

void cpu_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		GraphConfig *graph = (GraphConfig *)inst->custom;
		double Load;
		unsigned long height;

		dw_window_get_pos_size(gList[DISPLAY_CPU].hwndDraw[0], 0, 0, 0, &height);

		Get_Load(&Load);

		sprintf(graph->text, "%d%%", (int)(Load*100));

		graph_add_statistic(DISPLAY_CPU, (unsigned long)(Load*100));
	}
}

void net_update(struct _instance *inst, HWND owner)
{
	Get_Net(&Sent, &Recv, &TotalSent, &TotalRecv);
	net_add_statistic(DISPLAY_NET, Sent, Recv);
}

void netavg_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		ScaledPrint(&texts->text1[12], Recv, 2);
		ScaledPrint(&texts->text2[12], Sent, 2);
	};
}

void netmax_update(struct _instance *inst, HWND owner)
{
	if(Sent > MaxSent)
		MaxSent = Sent;
	if(Recv > MaxRecv)
		MaxRecv = Recv;

	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		ScaledPrint(&texts->text1[12], MaxRecv, 2);
		ScaledPrint(&texts->text2[12], MaxSent, 2);
	}
}

void nettot_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
 		ScaledPrint(&texts->text1[7], TotalRecv, 2);
		ScaledPrint(&texts->text2[7], TotalSent, 2);
	}
}

void memory_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		long double Memory;

		Get_Memory(&Memory);
		ScaledPrint(&texts->text1[9], Memory, 2);
	}
}

void uptime_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		unsigned long Seconds;

		Get_Uptime(&Seconds);
		sprintf(texts->text1, "Uptime: %lud %lu:%02lu",(Seconds/(3600*24))%365,(Seconds/3600)%24,(Seconds/60)%60);
	}
}

void clock_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
		TextConfig *texts = (TextConfig *)inst->custom;
		time_t t = time(NULL);

		strftime(texts->text1, 100, "%I:%M%p", localtime(&t));
	}
}


void drive_update(struct _instance *inst, HWND owner)
{
	if(inst && inst->pixmap && *(inst->pixmap))
	{
	}
}

void DWSIGNAL drive_update_thread(struct _instance *inst)
{
	while(inst && inst->custom)
	{
		BarConfig *bars = (BarConfig *)inst->custom;
		int drive = DW_POINTER_TO_INT(bars->user);

		if(inst->pixmap && *(inst->pixmap))
		{
			long double free, size;

			free = drivefree(drive);
			size = drivesize(drive);

			dw_mutex_lock(hMtx);
			if(size > 0)
				bars->currentval = 100 - (unsigned long)((free/size)*100.0);
			else
				bars->currentval = 0;

			ScaledPrint(&bars->text[bars->len], free, 1);
			dw_mutex_unlock(hMtx);
		}
#if defined(__MAC__) && !defined(GARBAGE_COLLECT)
		_dw_pool_drain();
#endif
		msleep(1000);
	}
}

/* Skip over the floppy drives on OS/2 and Windows */
#if defined(__UNIX__) || defined(__MAC__)
#define DRIVE_START 1
#else
#define DRIVE_START 3
#endif

/* Build monitors for all accessible drives */
void init_drives(void)
{
	int z;

	for(z=DRIVE_START;z<27;z++)
	{
		if(isdrive(z) && drivefree(z) > 0)
		{
			int m = 0;
			BarConfig *bars;
			char fsname[100];

			while(gList[m].Name)
			{
				m++;
			}

			gList[m + 1].Name = NULL;

			gList[m].Name = "Drives";

			gList[m].Update = drive_update;
			gList[m].Create = bar_create;
			gList[m].Draw = bar_draw;
			gList[m].custom = bars = calloc(1, sizeof(BarConfig));

			gList[m].width = 8;
			gList[m].height = 20;
			gList[m].vsize = FALSE;

			bars->currentval = 0;
			bars->maximum = 100;

			getfsname(z, fsname, 100);

			sprintf(bars->text, "%s: ", fsname);

			bars->user = DW_INT_TO_POINTER(z);
			bars->len = strlen(bars->text);

			dw_thread_new((void *)drive_update_thread, &gList[m], 0xFFFF);
		}
	}
}

int main(int argc, char *argv[])
{
	current_colors = colors;
	current_font = fonts[0];

	loadconfig();

	dw_init(TRUE, argc, argv);

	sockinit();

	init_drives();

	display_create();

	dw_main();

	display_destroy();

	sockshutdown();

	dw_exit(0);
	return 0;
}

