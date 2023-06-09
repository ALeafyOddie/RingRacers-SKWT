/// \file  menus/main-1.c
/// \brief Main Menu

// ==========================================================================
// ORGANIZATION START.
// ==========================================================================
// Note: Never should we be jumping from one category of menu options to another
//       without first going to the Main Menu.
// Note: Ignore the above if you're working with the Pause menu.
// Note: (Prefix)_MainMenu should be the target of all Main Menu options that
//       point to submenus.

#include "../k_menu.h"
#include "../m_random.h"
#include "../s_sound.h"
#include "../i_time.h"
#include "../v_video.h"
#include "../z_zone.h"
#include "../i_video.h" // I_FinishUpdate
#include "../i_system.h" // I_Sleep

menuitem_t MainMenu[] =
{
	{IT_STRING | IT_CALL, "Play",
		"Cut to the chase and start the race!", NULL,
		{.routine = M_CharacterSelect}, 0, 0},

	{IT_STRING | IT_CALL, "Extras",
		"Check out some bonus features.", "MENUI001",
		{.routine = M_InitExtras}, 0, 0},

	{IT_STRING, "Options",
		"Configure your controls, settings, and preferences.", NULL,
		{.routine = M_InitOptions}, 0, 0},

	{IT_STRING | IT_CALL, "Quit",
		"Exit \"Dr. Robotnik's Ring Racers\".", NULL,
		{.routine = M_QuitSRB2}, 0, 0},
};

menu_t MainDef = KARTGAMEMODEMENU(MainMenu, NULL);

// Quit Game
static INT32 quitsounds[] =
{
	// holy shit we're changing things up!
	// srb2kart: you ain't seen nothing yet
	sfx_kc2e,
	sfx_kc2f,
	sfx_cdfm01,
	sfx_ddash,
	sfx_s3ka2,
	sfx_s3k49,
	sfx_slip,
	sfx_tossed,
	sfx_s3k7b,
	sfx_itrolf,
	sfx_itrole,
	sfx_cdpcm9,
	sfx_s3k4e,
	sfx_s259,
	sfx_3db06,
	sfx_s3k3a,
	sfx_peel,
	sfx_cdfm28,
	sfx_s3k96,
	sfx_s3kc0s,
	sfx_cdfm39,
	sfx_hogbom,
	sfx_kc5a,
	sfx_kc46,
	sfx_s3k92,
	sfx_s3k42,
	sfx_kpogos,
	sfx_screec
};

void M_QuitSRB2(INT32 choice)
{
	// We pick index 0 which is language sensitive, or one at random,
	// between 1 and maximum number.
	(void)choice;
	M_StartMessage("Quit Game", "Are you sure you want to quit playing?\n", &M_QuitResponse, MM_YESNO, "Leave the game", "No, I want to go back!");
}

void M_QuitResponse(INT32 ch)
{
	tic_t ptime;
	INT32 mrand;

	if (ch == MA_YES)
	{
		if (!(netgame || cht_debug))
		{
			mrand = M_RandomKey(sizeof(quitsounds) / sizeof(INT32));
			if (quitsounds[mrand])
				S_StartSound(NULL, quitsounds[mrand]);

			//added : 12-02-98: do that instead of I_WaitVbl which does not work
			ptime = I_GetTime() + NEWTICRATE*2; // Shortened the quit time, used to be 2 seconds Tails 03-26-2001
			while (ptime > I_GetTime())
			{
				V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
				V_DrawSmallScaledPatch(0, 0, 0, W_CachePatchName("GAMEQUIT", PU_CACHE)); // Demo 3 Quit Screen Tails 06-16-2001
				I_FinishUpdate(); // Update the screen with the image Tails 06-19-2001
				I_Sleep(cv_sleep.value);
				I_UpdateTime(cv_timescale.value);
			}
		}
		I_Quit();
	}
}
