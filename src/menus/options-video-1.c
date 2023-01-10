/// \file  menus/options-video-1.c
/// \brief Video Options

#include "../k_menu.h"
#include "../r_main.h"	// cv_skybox
#include "../v_video.h" // cv_globalgamma
#include "../r_fps.h" // fps cvars

// options menu
menuitem_t OPTIONS_Video[] =
{

	{IT_STRING | IT_CALL, "Set Resolution...", "Change the screen resolution for the game.",
		NULL, {.routine = M_VideoModeMenu}, 0, 0},

// A check to see if you're not running on a fucking antique potato powered stone i guess???????

#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	{IT_STRING | IT_CVAR, "Fullscreen", "Set whether you want to use fullscreen or windowed mode.",
		NULL, {.cvar = &cv_fullscreen}, 0, 0},
#endif

	{IT_NOTHING|IT_SPACE, NULL, "Kanade best waifu! I promise!",
		NULL, {NULL}, 0, 0},

	// Everytime I see a screenshot at max gamma I die inside
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, "Gamma", "Adjusts the overall brightness of the game.",
		NULL, {.cvar = &cv_globalgamma}, 0, 0},

	{IT_STRING | IT_CVAR, "FPS Cap", "Handles the refresh rate of the game (does not affect gamelogic).",
		NULL, {.cvar = &cv_fpscap}, 0, 0},

	{IT_STRING | IT_CVAR, "Enable Skyboxes", "Turning this off will improve performance at the detriment of visuals for many maps.",
		NULL, {.cvar = &cv_skybox}, 0, 0},

	{IT_STRING | IT_CVAR, "Draw Distance", "How far objects can be drawn. Lower values may improve performance at the cost of visibility.",
		NULL, {.cvar = &cv_drawdist}, 0, 0},

	{IT_STRING | IT_CVAR, "Weather Draw Distance", "Affects how far weather visuals can be drawn. Lower values improve performance.",
		NULL, {.cvar = &cv_drawdist_precip}, 0, 0},

	{IT_STRING | IT_CVAR, "Show FPS", "Displays the game framerate at the lower right corner of the screen.",
		NULL, {.cvar = &cv_ticrate}, 0, 0},

	{IT_NOTHING|IT_SPACE, NULL, "Kanade best waifu! I promise!",
		NULL, {NULL}, 0, 0},

#ifdef HWRENDER
	{IT_STRING | IT_SUBMENU, "Hardware Options...", "For usage and configuration of the OpenGL renderer.",
		NULL, {.submenu = &OPTIONS_VideoOGLDef}, 0, 0},
#endif

};

menu_t OPTIONS_VideoDef = {
	sizeof (OPTIONS_Video) / sizeof (menuitem_t),
	&OPTIONS_MainDef,
	0,
	OPTIONS_Video,
	32, 80,
	SKINCOLOR_PLAGUE, 0,
	2, 5,
	M_DrawGenericOptions,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};
