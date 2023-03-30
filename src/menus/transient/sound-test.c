/// \file  menus/transient/sound-test.c
/// \brief Stereo Mode menu

#include "../../k_menu.h"
#include "../../s_sound.h"

static void M_SoundTestMainControl(INT32 choice)
{
	(void)choice;

	// Sound test exception
	if (soundtest.current == NULL || soundtest.current->numtracks == 0)
	{
		if (cv_soundtest.value != 0)
		{
			S_StopSounds();

			if (currentMenu->menuitems[itemOn].mvar1 == 0) // Stop
			{
				CV_SetValue(&cv_soundtest, 0);
			}
			else if (currentMenu->menuitems[itemOn].mvar1 == 1) // Play
			{
				S_StartSound(NULL, cv_soundtest.value);
			}
		}
		else if (currentMenu->menuitems[itemOn].mvar1 == 1) // Play
		{
			soundtest.playing = true;
			//soundtest.sequence = true;
			S_UpdateSoundTestDef(false, false, false);
		}

		return;
	}

	if (currentMenu->menuitems[itemOn].mvar1 == 1) // Play
	{
		if (soundtest.paused == true)
		{
			S_SoundTestTogglePause();
		}
		else if (soundtest.playing == false)
		{
			S_SoundTestPlay();
		}
	}
	else if (soundtest.playing == true)
	{
		if (currentMenu->menuitems[itemOn].mvar1 == 2) // Pause
		{
			if (soundtest.paused == false)
			{
				S_SoundTestTogglePause();
			}
		}
		else // Stop
		{
			S_SoundTestStop();
		}
	}
	else if (currentMenu->menuitems[itemOn].mvar1 == 0) // Stop while stopped?
	{
		soundtest.current = NULL;
		soundtest.currenttrack = 0;
		CV_SetValue(&cv_soundtest, 0);
	}
}

static void M_SoundTestNextPrev(INT32 choice)
{
	(void)choice;

	S_UpdateSoundTestDef((currentMenu->menuitems[itemOn].mvar1 < 0), true, false);
}

consvar_t *M_GetSoundTestVolumeCvar(void)
{
	if (soundtest.current == NULL)
	{
		if (cv_soundtest.value == 0)
			return NULL;

		return &cv_soundvolume;
	}

	return &cv_digmusicvolume;
}

static void M_SoundTestVol(INT32 choice)
{
	consvar_t *voltoadjust = M_GetSoundTestVolumeCvar();

	if (!voltoadjust)
		return;

	M_ChangeCvarDirect(choice, voltoadjust);
}

static void M_SoundTestTrack(INT32 choice)
{
	const UINT8 numtracks = (soundtest.current != NULL) ? soundtest.current->numtracks : 0;

	if (numtracks == 1 // No cycling
		|| choice == -1) // Extra
	{
		return;
	}

	// Confirm is generally treated as Up.

	// Soundtest exception
	if (numtracks == 0)
	{
		S_StopSounds();
		CV_AddValue(&cv_soundtest, ((choice == 0) ? -1 : 1));

		return;
	}

	if (choice == 0) // Down
	{
		soundtest.currenttrack--;
		if (soundtest.currenttrack < 0)
			soundtest.currenttrack = numtracks-1;
	}
	else //if (choice == 1) -- Up
	{
		soundtest.currenttrack++;
		if (soundtest.currenttrack >= numtracks)
			soundtest.currenttrack = 0;
	}

	if (soundtest.playing)
	{
		S_SoundTestPlay();
	}
}

static boolean M_SoundTestInputs(INT32 ch)
{
	(void)ch;
	soundtest.justopened = false;
	return false;
}

menuitem_t MISC_SoundTest[] =
{
	{IT_STRING | IT_CALL,   "Back",  "STER_IC0", NULL, {.routine = M_GoBack},               0,  stereospecial_back},
	{IT_SPACE, NULL, NULL, NULL, {NULL}, 11, 0},
	{IT_STRING | IT_CALL,   "Stop",  "STER_IC1", NULL, {.routine = M_SoundTestMainControl}, 0,  0},
	{IT_SPACE, NULL, NULL, NULL, {NULL},  8, 0},
	{IT_STRING | IT_CALL,   "Pause", "STER_IC2", NULL, {.routine = M_SoundTestMainControl}, 2,  stereospecial_pause},
	{IT_STRING | IT_CALL,   "Play",  "STER_IC3", NULL, {.routine = M_SoundTestMainControl}, 1,  stereospecial_play},
	{IT_SPACE, NULL, NULL, NULL, {NULL},  8, 0},
	{IT_STRING | IT_CALL,   "Prev",  "STER_IC4", NULL, {.routine = M_SoundTestNextPrev},   -1,  0},
	{IT_STRING | IT_CALL,   "Next",  "STER_IC5", NULL, {.routine = M_SoundTestNextPrev},    1,  0},
	{IT_SPACE, NULL, NULL, NULL, {NULL}, 0, 244},
	{IT_STRING | IT_ARROWS, "Vol",   NULL,       NULL, {.routine = M_SoundTestVol},         0,  stereospecial_vol},
	{IT_STRING | IT_ARROWS, "Track", NULL,       NULL, {.routine = M_SoundTestTrack},       0,  stereospecial_track},
};

menu_t MISC_SoundTestDef = {
	sizeof (MISC_SoundTest)/sizeof (menuitem_t),
	&MainDef,
	0,
	MISC_SoundTest,
	19, 140,
	0, 0,
	MBF_UD_LR_FLIPPED|MBF_SOUNDLESS,
	".",
	98, 0,
	M_DrawSoundTest,
	NULL,
	NULL,
	NULL,
	M_SoundTestInputs,
};

void M_SoundTest(INT32 choice)
{
	(void)choice;

	// I reserve the right to add some sort of setup here -- toast 250323
	soundtest.justopened = true;

	MISC_SoundTestDef.prevMenu = currentMenu;
	M_SetupNextMenu(&MISC_SoundTestDef, false);
}
