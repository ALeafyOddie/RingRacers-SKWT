// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  g_input.c
/// \brief handle mouse/keyboard/joystick inputs,
///        maps inputs to game controls (forward, spin, jump...)

#include "doomdef.h"
#include "doomstat.h"
#include "g_input.h"
#include "keys.h"
#include "hu_stuff.h" // need HUFONT start & end
#include "d_net.h"
#include "console.h"

#define MAXMOUSESENSITIVITY 100 // sensitivity steps

static CV_PossibleValue_t mousesens_cons_t[] = {{1, "MIN"}, {MAXMOUSESENSITIVITY, "MAX"}, {0, NULL}};
static CV_PossibleValue_t onecontrolperkey_cons_t[] = {{1, "One"}, {2, "Several"}, {0, NULL}};

// mouse values are used once
consvar_t cv_mousesens = CVAR_INIT ("mousesens", "20", CV_SAVE, mousesens_cons_t, NULL);
consvar_t cv_mousesens2 = CVAR_INIT ("mousesens2", "20", CV_SAVE, mousesens_cons_t, NULL);
consvar_t cv_mouseysens = CVAR_INIT ("mouseysens", "20", CV_SAVE, mousesens_cons_t, NULL);
consvar_t cv_mouseysens2 = CVAR_INIT ("mouseysens2", "20", CV_SAVE, mousesens_cons_t, NULL);
consvar_t cv_controlperkey = CVAR_INIT ("controlperkey", "One", CV_SAVE, onecontrolperkey_cons_t, NULL);

// current state of the keys
// FRACUNIT for fully pressed, 0 for not pressed
INT32 gamekeydown[MAXDEVICES][NUMINPUTS];
boolean deviceResponding[MAXDEVICES]; 

// two key codes (or virtual key) per game control
INT32 gamecontrol[MAXSPLITSCREENPLAYERS][num_gamecontrols][MAXINPUTMAPPING];
INT32 gamecontroldefault[num_gamecontrols][MAXINPUTMAPPING]; // default control storage

// lists of GC codes for selective operation
/*
const INT32 gcl_accelerate[num_gcl_accelerate] = { gc_a };

const INT32 gcl_brake[num_gcl_brake] = { gc_b };

const INT32 gcl_drift[num_gcl_drift] = { gc_c };

const INT32 gcl_spindash[num_gcl_spindash] = {
	gc_a, gc_b, gc_c, gc_abc
};

const INT32 gcl_movement[num_gcl_movement] = {
	gc_a, gc_b, gc_c, gc_abc, gc_left, gc_right
};

const INT32 gcl_item[num_gcl_item] = {
	gc_fire, gc_aimforward, gc_aimbackward
};

const INT32 gcl_full[num_gcl_full] = {
	gc_a, gc_drift, gc_b, gc_spindash, gc_turnleft, gc_turnright,
	gc_fire, gc_aimforward, gc_aimbackward,
	gc_lookback
};
*/

INT32 G_GetDevicePlayer(INT32 deviceID)
{
	INT32 i;

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (deviceID == cv_usejoystick[i].value)
		{
			return i;
		}
	}

	return -1;
}

//
// Remaps the inputs to game controls.
//
// A game control can be triggered by one or more keys/buttons.
//
// Each key/mousebutton/joybutton triggers ONLY ONE game control.
//
void G_MapEventsToControls(event_t *ev)
{
	INT32 i;

	if (ev->device >= 0 && ev->device < MAXDEVICES)
	{
		switch (ev->type)
		{
			case ev_keydown:
			//case ev_keyup:
			//case ev_mouse:
			//case ev_joystick:
				deviceResponding[ev->device] = true;
				break;

			default:
				break;
		}
	}
	else
	{
		return;
	}

	switch (ev->type)
	{
		case ev_keydown:
			if (ev->data1 < NUMINPUTS)
			{
				gamekeydown[ev->device][ev->data1] = FRACUNIT;
			}
#ifdef PARANOIA
			else
			{
				CONS_Debug(DBG_GAMELOGIC, "Bad downkey input %d\n", ev->data1);
			}
#endif
			break;

		case ev_keyup:
			if (ev->data1 < NUMINPUTS)
			{
				gamekeydown[ev->device][ev->data1] = 0;
			}
#ifdef PARANOIA
			else
			{
				CONS_Debug(DBG_GAMELOGIC, "Bad upkey input %d\n", ev->data1);
			}
#endif
			break;

		case ev_mouse: // buttons are virtual keys
			// X axis
			if (ev->data2 < 0)
			{
				// Left
				gamekeydown[ev->device][KEY_MOUSEMOVE + 2] = abs(ev->data2);
				gamekeydown[ev->device][KEY_MOUSEMOVE + 3] = 0;
			}
			else
			{
				// Right
				gamekeydown[ev->device][KEY_MOUSEMOVE + 2] = 0;
				gamekeydown[ev->device][KEY_MOUSEMOVE + 3] = abs(ev->data2);
			}

			// Y axis
			if (ev->data3 < 0)
			{
				// Up
				gamekeydown[ev->device][KEY_MOUSEMOVE] = abs(ev->data3);
				gamekeydown[ev->device][KEY_MOUSEMOVE + 1] = 0;
			}
			else
			{
				// Down
				gamekeydown[ev->device][KEY_MOUSEMOVE] = 0;
				gamekeydown[ev->device][KEY_MOUSEMOVE + 1] = abs(ev->data3);
			}
			break;

		case ev_joystick: // buttons are virtual keys
			if (ev->data1 >= JOYAXISSET)
			{
#ifdef PARANOIA
				CONS_Debug(DBG_GAMELOGIC, "Bad joystick axis event %d\n", ev->data1);
#endif
				break;
			}

			i = ev->data1 * 4;

			if (ev->data2 != INT32_MAX)
			{
				if (ev->data2 < 0)
				{
					// Left
					gamekeydown[ev->device][KEY_AXIS1 + i] = abs(ev->data2);
					gamekeydown[ev->device][KEY_AXIS1 + i + 1] = 0;
				}
				else
				{
					// Right
					gamekeydown[ev->device][KEY_AXIS1 + i] = 0;
					gamekeydown[ev->device][KEY_AXIS1 + i + 1] = abs(ev->data2);
				}
			}

			if (ev->data3 != INT32_MAX)
			{
				if (ev->data3 < 0)
				{
					// Up
					gamekeydown[ev->device][KEY_AXIS1 + i + 2] = abs(ev->data3);
					gamekeydown[ev->device][KEY_AXIS1 + i + 3] = 0;
				}
				else
				{
					// Down
					gamekeydown[ev->device][KEY_AXIS1 + i + 2] = 0;
					gamekeydown[ev->device][KEY_AXIS1 + i + 3] = abs(ev->data3);
				}
			}
			break;

		default:
			break;
	}
}

typedef struct
{
	INT32 keynum;
	const char *name;
} keyname_t;

static keyname_t keynames[] =
{
	{KEY_SPACE, "SPACE"},
	{KEY_CAPSLOCK, "CAPS LOCK"},
	{KEY_ENTER, "ENTER"},
	{KEY_TAB, "TAB"},
	{KEY_ESCAPE, "ESCAPE"},
	{KEY_BACKSPACE, "BACKSPACE"},

	{KEY_NUMLOCK, "NUM LOCK"},
	{KEY_SCROLLLOCK, "SCROLL LOCK"},

	// bill gates keys
	{KEY_LEFTWIN, "LWINDOWS"},
	{KEY_RIGHTWIN, "RWINDOWS"},
	{KEY_MENU, "MENU"},

	{KEY_LSHIFT, "LSHIFT"},
	{KEY_RSHIFT, "RSHIFT"},
	{KEY_LSHIFT, "SHIFT"},
	{KEY_LCTRL, "LCTRL"},
	{KEY_RCTRL, "RCTRL"},
	{KEY_LCTRL, "CTRL"},
	{KEY_LALT, "LALT"},
	{KEY_RALT, "RALT"},
	{KEY_LALT, "ALT"},

	// keypad keys
	{KEY_KPADSLASH, "KEYPAD /"},
	{KEY_KEYPAD7, "KEYPAD 7"},
	{KEY_KEYPAD8, "KEYPAD 8"},
	{KEY_KEYPAD9, "KEYPAD 9"},
	{KEY_MINUSPAD, "KEYPAD -"},
	{KEY_KEYPAD4, "KEYPAD 4"},
	{KEY_KEYPAD5, "KEYPAD 5"},
	{KEY_KEYPAD6, "KEYPAD 6"},
	{KEY_PLUSPAD, "KEYPAD +"},
	{KEY_KEYPAD1, "KEYPAD 1"},
	{KEY_KEYPAD2, "KEYPAD 2"},
	{KEY_KEYPAD3, "KEYPAD 3"},
	{KEY_KEYPAD0, "KEYPAD 0"},
	{KEY_KPADDEL, "KEYPAD ."},

	// extended keys (not keypad)
	{KEY_HOME, "HOME"},
	{KEY_UPARROW, "UP ARROW"},
	{KEY_PGUP, "PAGE UP"},
	{KEY_LEFTARROW, "LEFT ARROW"},
	{KEY_RIGHTARROW, "RIGHT ARROW"},
	{KEY_END, "END"},
	{KEY_DOWNARROW, "DOWN ARROW"},
	{KEY_PGDN, "PAGE DOWN"},
	{KEY_INS, "INSERT"},
	{KEY_DEL, "DELETE"},

	// other keys
	{KEY_F1, "F1"},
	{KEY_F2, "F2"},
	{KEY_F3, "F3"},
	{KEY_F4, "F4"},
	{KEY_F5, "F5"},
	{KEY_F6, "F6"},
	{KEY_F7, "F7"},
	{KEY_F8, "F8"},
	{KEY_F9, "F9"},
	{KEY_F10, "F10"},
	{KEY_F11, "F11"},
	{KEY_F12, "F12"},

	// KEY_CONSOLE has an exception in the keyname code
	{'`', "TILDE"},
	{KEY_PAUSE, "PAUSE/BREAK"},

	// virtual keys for mouse buttons and joystick buttons
	{KEY_MOUSE1+0,"MOUSE1"},
	{KEY_MOUSE1+1,"MOUSE2"},
	{KEY_MOUSE1+2,"MOUSE3"},
	{KEY_MOUSE1+3,"MOUSE4"},
	{KEY_MOUSE1+4,"MOUSE5"},
	{KEY_MOUSE1+5,"MOUSE6"},
	{KEY_MOUSE1+6,"MOUSE7"},
	{KEY_MOUSE1+7,"MOUSE8"},
	{KEY_MOUSEMOVE+0,"Mouse Up"},
	{KEY_MOUSEMOVE+1,"Mouse Down"},
	{KEY_MOUSEMOVE+2,"Mouse Left"},
	{KEY_MOUSEMOVE+3,"Mouse Right"},
	{KEY_MOUSEWHEELUP, "Wheel Up"},
	{KEY_MOUSEWHEELDOWN, "Wheel Down"},

	{KEY_JOY1+0, "JOY1"},
	{KEY_JOY1+1, "JOY2"},
	{KEY_JOY1+2, "JOY3"},
	{KEY_JOY1+3, "JOY4"},
	{KEY_JOY1+4, "JOY5"},
	{KEY_JOY1+5, "JOY6"},
	{KEY_JOY1+6, "JOY7"},
	{KEY_JOY1+7, "JOY8"},
	{KEY_JOY1+8, "JOY9"},
#if !defined (NOMOREJOYBTN_1S)
	// we use up to 32 buttons in DirectInput
	{KEY_JOY1+9, "JOY10"},
	{KEY_JOY1+10, "JOY11"},
	{KEY_JOY1+11, "JOY12"},
	{KEY_JOY1+12, "JOY13"},
	{KEY_JOY1+13, "JOY14"},
	{KEY_JOY1+14, "JOY15"},
	{KEY_JOY1+15, "JOY16"},
	{KEY_JOY1+16, "JOY17"},
	{KEY_JOY1+17, "JOY18"},
	{KEY_JOY1+18, "JOY19"},
	{KEY_JOY1+19, "JOY20"},
	{KEY_JOY1+20, "JOY21"},
	{KEY_JOY1+21, "JOY22"},
	{KEY_JOY1+22, "JOY23"},
	{KEY_JOY1+23, "JOY24"},
	{KEY_JOY1+24, "JOY25"},
	{KEY_JOY1+25, "JOY26"},
	{KEY_JOY1+26, "JOY27"},
	{KEY_JOY1+27, "JOY28"},
	{KEY_JOY1+28, "JOY29"},
	{KEY_JOY1+29, "JOY30"},
	{KEY_JOY1+30, "JOY31"},
	{KEY_JOY1+31, "JOY32"},
#endif

	// the DOS version uses Allegro's joystick support
	{KEY_HAT1+0, "HATUP"},
	{KEY_HAT1+1, "HATDOWN"},
	{KEY_HAT1+2, "HATLEFT"},
	{KEY_HAT1+3, "HATRIGHT"},
	{KEY_HAT1+4, "HATUP2"},
	{KEY_HAT1+5, "HATDOWN2"},
	{KEY_HAT1+6, "HATLEFT2"},
	{KEY_HAT1+7, "HATRIGHT2"},
	{KEY_HAT1+8, "HATUP3"},
	{KEY_HAT1+9, "HATDOWN3"},
	{KEY_HAT1+10, "HATLEFT3"},
	{KEY_HAT1+11, "HATRIGHT3"},
	{KEY_HAT1+12, "HATUP4"},
	{KEY_HAT1+13, "HATDOWN4"},
	{KEY_HAT1+14, "HATLEFT4"},
	{KEY_HAT1+15, "HATRIGHT4"},

	{KEY_AXIS1+0, "AXISX-"},
	{KEY_AXIS1+1, "AXISX+"},
	{KEY_AXIS1+2, "AXISY-"},
	{KEY_AXIS1+3, "AXISY+"},
	{KEY_AXIS1+4, "AXISZ-"},
	{KEY_AXIS1+5, "AXISZ+"},
	{KEY_AXIS1+6, "AXISXRUDDER-"},
	{KEY_AXIS1+7, "AXISXRUDDER+"},
	{KEY_AXIS1+8, "AXISYRUDDER-"},
	{KEY_AXIS1+9, "AXISYRUDDER+"},
	{KEY_AXIS1+10, "AXISZRUDDER-"},
	{KEY_AXIS1+11, "AXISZRUDDER+"},
	{KEY_AXIS1+12, "AXISU-"},
	{KEY_AXIS1+13, "AXISU+"},
	{KEY_AXIS1+14, "AXISV-"},
	{KEY_AXIS1+15, "AXISV+"},
};

static const char *gamecontrolname[num_gamecontrols] =
{
	"null", // a key/button mapped to gc_null has no effect
	"up",
	"down",
	"left",
	"right",
	"a",
	"b",
	"c",
	"x",
	"y",
	"z",
	"l",
	"r",
	"start",
	"abc",
	"console",
	"talk",
	"teamtalk",
	"screenshot",
	"recordgif",
};

#define NUMKEYNAMES (sizeof (keynames)/sizeof (keyname_t))

//
// Detach any keys associated to the given game control
// - pass the pointer to the gamecontrol table for the player being edited
void G_ClearControlKeys(INT32 (*setupcontrols)[MAXINPUTMAPPING], INT32 control)
{
	INT32 i;
	for (i = 0; i < MAXINPUTMAPPING; i++)
	{
		setupcontrols[control][i] = KEY_NULL;
	}
}

void G_ClearAllControlKeys(void)
{
	INT32 i, j;
	for (j = 0; j < MAXSPLITSCREENPLAYERS; j++)
	{
		for (i = 0; i < num_gamecontrols; i++)
		{
			G_ClearControlKeys(gamecontrol[j], i);
		}
	}
}

//
// Returns the name of a key (or virtual key for mouse and joy)
// the input value being an keynum
//
const char *G_KeynumToString(INT32 keynum)
{
	static char keynamestr[8];

	UINT32 j;

	// return a string with the ascii char if displayable
	if (keynum > ' ' && keynum <= 'z' && keynum != KEY_CONSOLE)
	{
		keynamestr[0] = (char)keynum;
		keynamestr[1] = '\0';
		return keynamestr;
	}

	// find a description for special keys
	for (j = 0; j < NUMKEYNAMES; j++)
		if (keynames[j].keynum == keynum)
			return keynames[j].name;

	// create a name for unknown keys
	sprintf(keynamestr, "KEY%d", keynum);
	return keynamestr;
}

INT32 G_KeyStringtoNum(const char *keystr)
{
	UINT32 j;

	if (!keystr[1] && keystr[0] > ' ' && keystr[0] <= 'z')
		return keystr[0];

	if (!strncmp(keystr, "KEY", 3) && keystr[3] >= '0' && keystr[3] <= '9')
	{
		/* what if we out of range bruh? */
		j = atoi(&keystr[3]);
		if (j < NUMINPUTS)
			return j;
		return 0;
	}

	for (j = 0; j < NUMKEYNAMES; j++)
		if (!stricmp(keynames[j].name, keystr))
			return keynames[j].keynum;

	return 0;
}

void G_DefineDefaultControls(void)
{
	// These defaults are bad & temporary.
	// Keyboard controls
	gamecontroldefault[gc_up   ][0] = KEY_UPARROW;
	gamecontroldefault[gc_down ][0] = KEY_DOWNARROW;
	gamecontroldefault[gc_left ][0] = KEY_LEFTARROW;
	gamecontroldefault[gc_right][0] = KEY_RIGHTARROW;
	gamecontroldefault[gc_a    ][0] = 'z';
	gamecontroldefault[gc_b    ][0] = 'x';
	gamecontroldefault[gc_c    ][0] = 'c';
	gamecontroldefault[gc_x    ][0] = 'a';
	gamecontroldefault[gc_y    ][0] = 's';
	gamecontroldefault[gc_z    ][0] = 'd';
	gamecontroldefault[gc_l    ][0] = 'q';
	gamecontroldefault[gc_r    ][0] = 'e';
	gamecontroldefault[gc_start][0] = KEY_ENTER;

	// Gamepad controls
	gamecontroldefault[gc_up   ][1] = KEY_HAT1+0; // D-Pad Up
	gamecontroldefault[gc_down ][1] = KEY_HAT1+1; // D-Pad Down
	gamecontroldefault[gc_left ][1] = KEY_HAT1+2; // D-Pad Left
	gamecontroldefault[gc_right][1] = KEY_HAT1+3; // D-Pad Right
	gamecontroldefault[gc_a    ][1] = KEY_JOY1+0; // A
	gamecontroldefault[gc_b    ][1] = KEY_JOY1+1; // B
	gamecontroldefault[gc_c    ][1] = KEY_JOY1+2; // ?
	gamecontroldefault[gc_x    ][1] = KEY_JOY1+3;
	gamecontroldefault[gc_y    ][1] = KEY_JOY1+6;
	gamecontroldefault[gc_z    ][1] = KEY_JOY1+8;
	gamecontroldefault[gc_l    ][1] = KEY_JOY1+4; // LB
	gamecontroldefault[gc_r    ][1] = KEY_JOY1+5; // RB
	gamecontroldefault[gc_start][1] = KEY_JOY1+7; // Start

	gamecontroldefault[gc_up   ][2] = KEY_AXIS1+2; // Axis Y-
	gamecontroldefault[gc_down ][2] = KEY_AXIS1+3; // Axis Y+
	gamecontroldefault[gc_left ][2] = KEY_AXIS1+0; // Axis X-
	gamecontroldefault[gc_right][2] = KEY_AXIS1+1; // Axis X+
}

void G_CopyControls(INT32 (*setupcontrols)[MAXINPUTMAPPING], INT32 (*fromcontrols)[MAXINPUTMAPPING], const INT32 *gclist, INT32 gclen)
{
	INT32 i, j, gc;

	for (i = 0; i < (gclist && gclen ? gclen : num_gamecontrols); i++)
	{
		gc = (gclist && gclen) ? gclist[i] : i;

		for (j = 0; j < MAXINPUTMAPPING; j++)
		{
			setupcontrols[gc][j] = fromcontrols[gc][j];
		}
	}
}

void G_SaveKeySetting(FILE *f, INT32 (*fromcontrolsa)[MAXINPUTMAPPING], INT32 (*fromcontrolsb)[MAXINPUTMAPPING], INT32 (*fromcontrolsc)[MAXINPUTMAPPING], INT32 (*fromcontrolsd)[MAXINPUTMAPPING])
{
	INT32 i, j;

	// TODO: would be nice to get rid of this code duplication
	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol \"%s\" \"%s\"", gamecontrolname[i], G_KeynumToString(fromcontrolsa[i][0]));

		for (j = 1; j < MAXINPUTMAPPING+1; j++)
		{
			if (j < MAXINPUTMAPPING && fromcontrolsa[i][j])
			{
				fprintf(f, " \"%s\"", G_KeynumToString(fromcontrolsa[i][j]));
			}
			else
			{
				fprintf(f, "\n");
				break;
			}
		}
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol2 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(fromcontrolsb[i][0]));

		for (j = 1; j < MAXINPUTMAPPING+1; j++)
		{
			if (j < MAXINPUTMAPPING && fromcontrolsb[i][j])
			{
				fprintf(f, " \"%s\"", G_KeynumToString(fromcontrolsb[i][j]));
			}
			else
			{
				fprintf(f, "\n");
				break;
			}
		}
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol3 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(fromcontrolsc[i][0]));

		for (j = 1; j < MAXINPUTMAPPING+1; j++)
		{
			if (j < MAXINPUTMAPPING && fromcontrolsc[i][j])
			{
				fprintf(f, " \"%s\"", G_KeynumToString(fromcontrolsc[i][j]));
			}
			else
			{
				fprintf(f, "\n");
				break;
			}
		}
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol4 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(fromcontrolsd[i][0]));

		for (j = 1; j < MAXINPUTMAPPING+1; j++)
		{
			if (j < MAXINPUTMAPPING && fromcontrolsd[i][j])
			{
				fprintf(f, " \"%s\"", G_KeynumToString(fromcontrolsd[i][j]));
			}
			else
			{
				fprintf(f, "\n");
				break;
			}
		}
	}
}

INT32 G_CheckDoubleUsage(INT32 keynum, INT32 playernum, boolean modify)
{
	INT32 result = gc_null;

	if (cv_controlperkey.value == 1)
	{
		INT32 i, j;
		for (i = 0; i < num_gamecontrols; i++)
		{
			for (j = 0; j < MAXINPUTMAPPING; j++)
			{
				if (gamecontrol[playernum][i][j] == keynum)
				{
					result = i;
					if (modify)
					{
						gamecontrol[playernum][i][j] = KEY_NULL;
					}
				}

				if (result && !modify)
					return result;
			}
		}
	}

	return result;
}

static void setcontrol(UINT8 player)
{
	INT32 numctrl;
	const char *namectrl;
	INT32 keynum;
	INT32 inputMap = 0;
	INT32 i;

	namectrl = COM_Argv(1);

	for (numctrl = 0;
		numctrl < num_gamecontrols && stricmp(namectrl, gamecontrolname[numctrl]);
		numctrl++)
	{ ; }

	if (numctrl == num_gamecontrols)
	{
		CONS_Printf(M_GetText("Control '%s' unknown\n"), namectrl);
		return;
	}

	for (i = 0; i < MAXINPUTMAPPING; i++)
	{
		keynum = G_KeyStringtoNum(COM_Argv(inputMap + 2));

		if (keynum >= 0)
		{
			(void)G_CheckDoubleUsage(keynum, player, true);

			// if keynum was rejected, try it again with the next key.
			while (keynum == 0)
			{
				inputMap++;
				if (inputMap >= MAXINPUTMAPPING)
				{
					break;
				}

				keynum = G_KeyStringtoNum(COM_Argv(inputMap + 2));

				if (keynum >= 0)
				{
					(void)G_CheckDoubleUsage(keynum, player, true);
				}
			}
		}

		if (keynum >= 0)
		{
			gamecontrol[player][numctrl][i] = keynum;
		}

		inputMap++;
		if (inputMap >= MAXINPUTMAPPING)
		{
			break;
		}
	}
}

void Command_Setcontrol_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na < 3 || na > MAXINPUTMAPPING+2)
	{
		CONS_Printf(M_GetText("setcontrol <controlname> <keyname> [<keyname>] [<keyname>] [<keyname>]: set controls for player 1\n"));
		return;
	}

	setcontrol(0);
}

void Command_Setcontrol2_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na < 3 || na > MAXINPUTMAPPING+2)
	{
		CONS_Printf(M_GetText("setcontrol2 <controlname> <keyname> [<keyname>] [<keyname>] [<keyname>]: set controls for player 2\n"));
		return;
	}

	setcontrol(1);
}

void Command_Setcontrol3_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na < 3 || na > MAXINPUTMAPPING+2)
	{
		CONS_Printf(M_GetText("setcontrol3 <controlname> <keyname> [<keyname>] [<keyname>] [<keyname>]: set controls for player 3\n"));
		return;
	}

	setcontrol(2);
}

void Command_Setcontrol4_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na < 3 || na > MAXINPUTMAPPING+2)
	{
		CONS_Printf(M_GetText("setcontrol4 <controlname> <keyname> [<keyname>] [<keyname>] [<keyname>]: set controls for player 4\n"));
		return;
	}

	setcontrol(3);
}
