/// \file  menus/play-local-race-time-attack.c
/// \brief Race Time Attack Menu

#include "../k_menu.h"
#include "../r_local.h" // SplitScreen_OnChange
#include "../s_sound.h"
#include "../f_finale.h" // F_WipeStartScreen
#include "../v_video.h"
#include "../d_main.h" // srb2home
#include "../m_misc.h" // M_MkdirEach
#include "../z_zone.h" // Z_StrDup/Z_Free
#include "../m_cond.h"

static void CV_SPBAttackChanged(void)
{
	G_UpdateTimeStickerMedals(levellist.choosemap, false);
}

consvar_t cv_dummyspbattack = CVAR_INIT ("dummyspbattack", "Off", CV_HIDDEN|CV_CALL, CV_OnOff, CV_SPBAttackChanged);

struct timeattackmenu_s timeattackmenu;

void M_TimeAttackTick(void)
{
	timeattackmenu.ticker++;
	if (timeattackmenu.spbflicker > 0)
	{
		timeattackmenu.spbflicker--;
	}
}

boolean M_TimeAttackInputs(INT32 ch)
{
	const UINT8 pid = 0;
	const boolean buttonR = M_MenuButtonPressed(pid, MBT_R);
	(void) ch;

	if (buttonR && levellist.newgametype == GT_RACE && M_SecretUnlocked(SECRET_SPBATTACK, true))
	{
		CV_AddValue(&cv_dummyspbattack, 1);
		timeattackmenu.spbflicker = TICRATE/6;
		if (cv_dummyspbattack.value)
		{
			S_StartSound(NULL, sfx_s3k9f);
			S_StopSoundByID(NULL, sfx_s3k92);
		}
		else
		{
			S_StartSound(NULL, sfx_s3k92);
			S_StopSoundByID(NULL, sfx_s3k9f);
		}

		return true;
	}

	return false;
}

// see ta_e
menuitem_t PLAY_TimeAttack[] =
{
	{IT_STRING | IT_SUBMENU, "Replay...", NULL, "MENUI006", {.submenu = &PLAY_TAReplayDef}, 0, 0},
	{IT_STRING | IT_SUBMENU, "Guest...", NULL, "MENUI006", {.submenu = &PLAY_TAReplayGuestDef}, 0, 0},
	{IT_STRING | IT_SUBMENU, "Ghosts...", NULL, "MENUI006", {.submenu = &PLAY_TAGhostsDef}, 0, 0},
	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},
	{IT_STRING | IT_CALL, "Start", NULL, "MENUI006", {.routine = M_StartTimeAttack}, 0, 0},
};

menu_t PLAY_TimeAttackDef = {
	sizeof(PLAY_TimeAttack) / sizeof(menuitem_t),
	&PLAY_LevelSelectDef,
	0,
	PLAY_TimeAttack,
	0, 0,
	0, 0,
	0,
	NULL,
	2, 5,
	M_DrawTimeAttack,
	M_TimeAttackTick,
	NULL,
	NULL,
	M_TimeAttackInputs
};


typedef enum
{
	tareplay_besttime = 0,
	tareplay_bestlap,
	tareplay_gap1,
	tareplay_last,
	tareplay_guest,
	tareplay_staff,
	tareplay_gap2,
	tareplay_back
} tareplay_e;

menuitem_t PLAY_TAReplay[] =
{
	{IT_STRING | IT_CALL, "Replay Best Time", NULL, "MENUI006", {.routine = M_ReplayTimeAttack}, 0, 0},
	{IT_STRING | IT_CALL, "Replay Best Lap", NULL, "MENUI006", {.routine = M_ReplayTimeAttack}, 0, 0},
	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},
	{IT_STRING | IT_CALL, "Replay Last", NULL, "MENUI006", {.routine = M_ReplayTimeAttack}, 0, 0},
	{IT_STRING | IT_CALL, "Replay Guest", NULL, "MENUI006", {.routine = M_ReplayTimeAttack}, 0, 0},
	{IT_STRING | IT_ARROWS, "Replay Staff", NULL, "MENUI006", {.routine = M_HandleStaffReplay}, 0, 0},
	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},

	{IT_STRING | IT_SUBMENU, "Back", NULL, "MENUI006", {.submenu = &PLAY_TimeAttackDef}, 0, 0},
};

menu_t PLAY_TAReplayDef = {
	sizeof(PLAY_TAReplay) / sizeof(menuitem_t),
	&PLAY_TimeAttackDef,
	0,
	PLAY_TAReplay,
	0, 0,
	0, 0,
	0,
	NULL,
	2, 5,
	M_DrawTimeAttack,
	NULL,
	NULL,
	NULL,
	NULL
};

typedef enum
{
	taguest_header = 0,
	taguest_besttime,
	taguest_bestlap,
	taguest_last,
	taguest_gap1,
	taguest_delete,
	taguest_gap2,
	taguest_back
} taguest_e;

menuitem_t PLAY_TAReplayGuest[] =
{
	{IT_HEADERTEXT|IT_HEADER, "Save as guest...", NULL, "MENUI006", {NULL}, 0, 0},

	{IT_STRING | IT_CALL, "Best Time", NULL, "MENUI006", {.routine = M_SetGuestReplay}, 0, 0},
	{IT_STRING | IT_CALL, "Best Lap", NULL, "MENUI006", {.routine = M_SetGuestReplay}, 0, 0},
	{IT_STRING | IT_CALL, "Last Run", NULL, "MENUI006", {.routine = M_SetGuestReplay}, 0, 0},

	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},
	{IT_STRING | IT_CALL, "Delete Guest", NULL, "MENUI006", {.routine = M_SetGuestReplay}, 0, 0},

	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},
	{IT_STRING | IT_SUBMENU, "Back", NULL, "MENUI006", {.submenu = &PLAY_TimeAttackDef}, 0, 0},

};

menu_t PLAY_TAReplayGuestDef = {
	sizeof(PLAY_TAReplayGuest) / sizeof(menuitem_t),
	&PLAY_TimeAttackDef,
	0,
	PLAY_TAReplayGuest,
	0, 0,
	0, 0,
	0,
	NULL,
	2, 5,
	M_DrawTimeAttack,
	NULL,
	NULL,
	NULL,
	NULL
};

typedef enum
{
	taghost_besttime = 0,
	taghost_bestlap,
	taghost_last,
	taghost_guest,
	taghost_staff,
	taghost_gap1,
	taghost_back
} taghost_e;

menuitem_t PLAY_TAGhosts[] =
{
	{IT_STRING | IT_CVAR, "Best Time", NULL, "MENUI006", {.cvar = &cv_ghost_besttime}, 0, 0},
	{IT_STRING | IT_CVAR, "Best Lap", NULL, "MENUI006", {.cvar = &cv_ghost_bestlap}, 0, 0},
	{IT_STRING | IT_CVAR, "Last", NULL, "MENUI006", {.cvar = &cv_ghost_last}, 0, 0},
	{IT_DISABLED, "Guest", NULL, "MENUI006", {.cvar = &cv_ghost_guest}, 0, 0},
	{IT_DISABLED, "Staff", NULL, "MENUI006", {.cvar = &cv_ghost_staff}, 0, 0},

	{IT_HEADERTEXT|IT_HEADER, "", NULL, NULL, {NULL}, 0, 0},
	{IT_STRING | IT_SUBMENU, "Back", NULL, "MENUI006", {.submenu = &PLAY_TimeAttackDef}, 0, 0},
};

menu_t PLAY_TAGhostsDef = {
	sizeof(PLAY_TAGhosts) / sizeof(menuitem_t),
	&PLAY_TimeAttackDef,
	0,
	PLAY_TAGhosts,
	0, 0,
	0, 0,
	0,
	NULL,
	2, 5,
	M_DrawTimeAttack,
	NULL,
	NULL,
	NULL,
	NULL
};

// autorecord demos for time attack
consvar_t cv_autorecord = CVAR_INIT ("autorecord", "Yes", 0, CV_YesNo, NULL);

CV_PossibleValue_t ghost_cons_t[] = {{0, "Hide"}, {1, "Show Character"}, {2, "Show All"}, {0, NULL}};
CV_PossibleValue_t ghost2_cons_t[] = {{0, "Hide"}, {1, "Show"}, {0, NULL}};

consvar_t cv_ghost_besttime  = CVAR_INIT ("ghost_besttime",  "Show All", CV_SAVE, ghost_cons_t, NULL);
consvar_t cv_ghost_bestlap   = CVAR_INIT ("ghost_bestlap",   "Show All", CV_SAVE, ghost_cons_t, NULL);
consvar_t cv_ghost_last      = CVAR_INIT ("ghost_last",      "Show All", CV_SAVE, ghost_cons_t, NULL);
consvar_t cv_ghost_guest     = CVAR_INIT ("ghost_guest",     "Show", CV_SAVE, ghost2_cons_t, NULL);
consvar_t cv_ghost_staff     = CVAR_INIT ("ghost_staff",     "Show", CV_SAVE, ghost2_cons_t, NULL);

// time attack stuff...
void M_PrepareTimeAttack(INT32 choice)
{
	(void) choice;

	// Gametype guess
	if (levellist.guessgt != MAXGAMETYPES)
	{
		levellist.newgametype = levellist.guessgt;
		if (!(gametypes[levellist.newgametype]->tol & mapheaderinfo[levellist.choosemap]->typeoflevel))
		{
			INT32 guess = G_GuessGametypeByTOL(mapheaderinfo[levellist.choosemap]->typeoflevel);
			if (guess != -1)
				levellist.newgametype = guess;
		}
	}

	if (levellist.levelsearch.timeattack == false || levellist.newgametype != GT_RACE || !M_SecretUnlocked(SECRET_SPBATTACK, true))
		CV_SetValue(&cv_dummyspbattack, 0);

	// Time-sticker Medals
	G_UpdateTimeStickerMedals(levellist.choosemap, false);

	// Menu options
	{
		// see also p_setup.c's P_LoadRecordGhosts
		char *gpath = Z_StrDup(va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s", srb2home, timeattackfolder, G_BuildMapName(levellist.choosemap+1)));
		UINT8 active;

		if (!gpath)
			return;

		active = false;
		PLAY_TimeAttack[ta_guest].status = IT_DISABLED;
		PLAY_TimeAttack[ta_replay].status = IT_DISABLED;
		PLAY_TimeAttack[ta_ghosts].status = IT_DISABLED;

		// Check if file exists, if not, disable options
		PLAY_TAReplay[tareplay_besttime].status =
			PLAY_TAReplayGuest[taguest_besttime].status = IT_DISABLED;
		if (FIL_FileExists(va("%s-%s-time-best.lmp", gpath, cv_skin[0].string)))
		{
			PLAY_TAReplay[tareplay_besttime].status = IT_STRING|IT_CALL;
			PLAY_TAReplayGuest[taguest_besttime].status = IT_STRING|IT_CALL;
			active |= (1|2|4);
		}
		else if (PLAY_TAReplayGuestDef.lastOn == taguest_besttime)
			PLAY_TAReplayGuestDef.lastOn = taguest_back;

		PLAY_TAReplay[tareplay_bestlap].status =
			PLAY_TAReplayGuest[taguest_bestlap].status =
			PLAY_TAGhosts[taghost_bestlap].status = IT_DISABLED;
		if ((gametypes[levellist.newgametype]->rules & GTR_CIRCUIT)
			&& (mapheaderinfo[levellist.choosemap]->numlaps != 1)
			&& FIL_FileExists(va("%s-%s-lap-best.lmp", gpath, cv_skin[0].string)))
		{
			PLAY_TAReplay[tareplay_bestlap].status = IT_STRING|IT_CALL;
			PLAY_TAReplayGuest[taguest_bestlap].status = IT_STRING|IT_CALL;
			PLAY_TAGhosts[taghost_bestlap].status = IT_STRING|IT_CVAR;
			active |= (1|2|4);
		}
		else if (PLAY_TAReplayGuestDef.lastOn == taguest_bestlap)
			PLAY_TAReplayGuestDef.lastOn = taguest_back;

		PLAY_TAReplay[tareplay_last].status =
			PLAY_TAReplayGuest[taguest_last].status = IT_DISABLED;
		if (FIL_FileExists(va("%s-%s-last.lmp", gpath, cv_skin[0].string)))
		{
			PLAY_TAReplay[tareplay_last].status = IT_STRING|IT_CALL;
			PLAY_TAReplayGuest[taguest_last].status = IT_STRING|IT_CALL;
			active |= (1|2|4);
		}
		else if (PLAY_TAReplayGuestDef.lastOn == taguest_last)
			PLAY_TAReplayGuestDef.lastOn = taguest_back;

		PLAY_TAReplay[tareplay_guest].status =
			PLAY_TAGhosts[taghost_guest].status =
			PLAY_TAReplayGuest[taguest_delete].status = IT_DISABLED;
		if (FIL_FileExists(va("%s-guest.lmp", gpath)))
		{
			PLAY_TAReplay[tareplay_guest].status = IT_STRING|IT_CALL;
			PLAY_TAReplayGuest[taguest_delete].status = IT_STRING|IT_CALL;
			PLAY_TAGhosts[taghost_guest].status = IT_STRING|IT_CVAR;
			active |= (1|2|4);
		}
		else if (PLAY_TAReplayGuestDef.lastOn == taguest_delete)
			PLAY_TAReplayGuestDef.lastOn = taguest_back;

		PLAY_TAReplay[tareplay_staff].status =
			PLAY_TAGhosts[taghost_staff].status = IT_DISABLED;
		if (mapheaderinfo[levellist.choosemap]->ghostCount > 0)
		{
			PLAY_TAReplay[tareplay_staff].status = IT_STRING|IT_ARROWS;
			PLAY_TAGhosts[taghost_staff].status = IT_STRING|IT_CVAR;
			CV_SetValue(&cv_dummystaff, 0);
			active |= 1|4;
		}

		if (active & 1)
			PLAY_TimeAttack[ta_replay].status = IT_STRING|IT_SUBMENU;
		else if (PLAY_TimeAttackDef.lastOn == ta_replay)
			PLAY_TimeAttackDef.lastOn = ta_start;
		if (active & 2)
			PLAY_TimeAttack[ta_guest].status = IT_STRING|IT_SUBMENU;
		else if (PLAY_TimeAttackDef.lastOn == ta_guest)
			PLAY_TimeAttackDef.lastOn = ta_start;
		//if (active & 4) -- for possible future use
			PLAY_TimeAttack[ta_ghosts].status = IT_STRING|IT_SUBMENU;
		/*else if (PLAY_TimeAttackDef.lastOn == ta_ghosts)
			PLAY_TimeAttackDef.lastOn = ta_start;*/

		Z_Free(gpath);
	}
}

void M_HandleStaffReplay(INT32 choice)
{
	if (choice == 2)
	{
		restoreMenu = &PLAY_TimeAttackDef;

		M_ClearMenus(true);
		demo.loadfiles = false;
		demo.ignorefiles = true; // Just assume that record attack replays have the files needed

		G_DoPlayDemo(va("%s/GHOST_%u", mapheaderinfo[levellist.choosemap]->lumpname, cv_dummystaff.value+1));
		return;
	}

	M_ChangeCvarDirect(choice, &cv_dummystaff);
}

void M_ReplayTimeAttack(INT32 choice)
{
	const char *which;

	restoreMenu = &PLAY_TimeAttackDef;

	M_ClearMenus(true);
	demo.loadfiles = false;
	demo.ignorefiles = true; // Just assume that record attack replays have the files needed

	switch (choice)
	{
		default:
		case tareplay_besttime:
			which = "time-best";
			break;
		case tareplay_bestlap:
			which = "lap-best";
			break;
		case tareplay_last:
			which = "last";
			break;
		case tareplay_guest:
			G_DoPlayDemo(va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(levellist.choosemap+1)));
			return;
	}

	G_DoPlayDemo(va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-%s-%s.lmp", srb2home, timeattackfolder, G_BuildMapName(levellist.choosemap+1), cv_skin[0].string, which));
}

static const char *TA_GuestReplay_Str = NULL;

static void M_WriteGuestReplay(INT32 ch)
{
	char *gpath, *rguest;

	UINT8 *buf;
	size_t len = 0;

	if (ch != MA_YES)
		return;

	gpath = Z_StrDup(va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s", srb2home, timeattackfolder, G_BuildMapName(levellist.choosemap+1)));

	if (TA_GuestReplay_Str != NULL)
	{
		len = FIL_ReadFile(va("%s-%s-%s.lmp", gpath, cv_skin[0].string, TA_GuestReplay_Str), &buf);
		if (!len)
		{
			M_StartMessage("Guest Replay", "Replay to copy no longer exists!", NULL, MM_NOTHING, NULL, NULL);
			Z_Free(gpath);
			return;
		}
	}

	rguest = Z_StrDup(va("%s-guest.lmp", gpath));

	if (FIL_FileExists(rguest))
	{
		remove(rguest);
	}

	if (len)
	{
		FIL_WriteFile(rguest, buf, len);
	}

	Z_Free(rguest);
	Z_Free(gpath);

	M_PrepareTimeAttack(0);
	M_SetupNextMenu(&PLAY_TimeAttackDef, false);

	// TODO the following isn't showing up and I'm not sure why
	//M_StartMessage("Guest Replay", va("Guest replay data %s.", (len ? "saved" : "erased")), NULL, MM_NOTHING, NULL, NULL);
}


void M_SetGuestReplay(INT32 choice)
{
	switch (choice)
	{
		case taguest_besttime:
			TA_GuestReplay_Str = "time-best";
			break;
		case taguest_bestlap:
			TA_GuestReplay_Str = "lap-best";
			break;
		case taguest_last:
			TA_GuestReplay_Str = "last";
			break;

		case taguest_delete:
		default:
			TA_GuestReplay_Str = NULL;
			break;
	}

	if (FIL_FileExists(va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(levellist.choosemap+1))))
	{
		M_StartMessage("Guest Replay", va("Are you sure you want to\n%s the guest replay data?\n", (TA_GuestReplay_Str != NULL ? "overwrite" : "delete")), &M_WriteGuestReplay, MM_YESNO, NULL, NULL);
	}
	else if (TA_GuestReplay_Str != NULL)
	{
		M_WriteGuestReplay(MA_YES);
	}
}

void M_StartTimeAttack(INT32 choice)
{
	char *gpath;
	char nameofdemo[256];

	(void)choice;

	modeattacking = ATTACKING_TIME;

	if ((gametypes[levellist.newgametype]->rules & GTR_CIRCUIT)
		&& (mapheaderinfo[levellist.choosemap]->numlaps != 1))
	{
		modeattacking |= ATTACKING_LAP;
	}

	if (cv_dummyspbattack.value)
	{
		modeattacking |= ATTACKING_SPB;
	}

	// Still need to reset devmode
	cht_debug = 0;

	if (demo.playback)
		G_StopDemo();
	if (metalrecording)
		G_StopMetalDemo();

	splitscreen = 0;
	SplitScreen_OnChange();

	S_StartSound(NULL, sfx_s3k63);

	paused = false;

	S_StopMusicCredit();

	// Early fadeout to let the sound finish playing
	F_WipeStartScreen();
	V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
	F_WipeEndScreen();
	F_RunWipe(wipe_level_toblack, wipedefs[wipe_level_toblack], false, "FADEMAP0", false, false);

	SV_StartSinglePlayerServer(levellist.newgametype, false);

	gpath = va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s",
			srb2home, timeattackfolder);
	M_MkdirEach(gpath, M_PathParts(gpath) - 3, 0755);

	strcat(gpath, PATHSEP);
	strcat(gpath, G_BuildMapName(levellist.choosemap+1));

	snprintf(nameofdemo, sizeof nameofdemo, "%s-%s-last", gpath, cv_skin[0].string);

	if (!cv_autorecord.value)
		remove(va("%s"PATHSEP"%s.lmp", srb2home, nameofdemo));
	else
		G_RecordDemo(nameofdemo);

	restoreMenu = &PLAY_TimeAttackDef;

	M_ClearMenus(true);
	D_MapChange(levellist.choosemap+1, levellist.newgametype, (cv_dummygpencore.value == 1), 1, 1, false, false);

	G_UpdateTimeStickerMedals(levellist.choosemap, true);
}
