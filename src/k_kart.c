// SONIC ROBO BLAST 2 KART ~ ZarroTsu
//-----------------------------------------------------------------------------
/// \file  k_kart.c
/// \brief SRB2kart general.
///        All of the SRB2kart-unique stuff.

#include "k_kart.h"
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "k_respawn.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "p_slopes.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_local.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "z_zone.h"
#include "m_misc.h"
#include "m_cond.h"
#include "f_finale.h"
#include "lua_hud.h"	// For Lua hud checks
#include "lua_hook.h"	// For MobjDamage and ShouldDamage

#include "k_waypoint.h"
#include "k_bot.h"

// SOME IMPORTANT VARIABLES DEFINED IN DOOMDEF.H:
// gamespeed is cc (0 for easy, 1 for normal, 2 for hard)
// franticitems is Frantic Mode items, bool
// encoremode is Encore Mode (duh), bool
// comeback is Battle Mode's karma comeback, also bool
// battlewanted is an array of the WANTED player nums, -1 for no player in that slot
// indirectitemcooldown is timer before anyone's allowed another Shrink/SPB
// mapreset is set when enough players fill an empty server

player_t *K_GetItemBoxPlayer(mobj_t *mobj)
{
	fixed_t closest = INT32_MAX;
	player_t *player = NULL;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!(playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo) && !players[i].spectator))
		{
			continue;
		}

		// Always use normal item box rules -- could pass in "2" for fakes but they blend in better like this
		if (P_CanPickupItem(&players[i], 1))
		{
			fixed_t dist = P_AproxDistance(P_AproxDistance(
				players[i].mo->x - mobj->x,
				players[i].mo->y - mobj->y),
				players[i].mo->z - mobj->z
			);

			if (dist > 8192*mobj->scale)
			{
				continue;
			}

			if (dist < closest)
			{
				player = &players[i];
				closest = dist;
			}
		}
	}

	return player;
}

//{ SRB2kart Net Variables

void K_RegisterKartStuff(void)
{
	CV_RegisterVar(&cv_sneaker);
	CV_RegisterVar(&cv_rocketsneaker);
	CV_RegisterVar(&cv_invincibility);
	CV_RegisterVar(&cv_banana);
	CV_RegisterVar(&cv_eggmanmonitor);
	CV_RegisterVar(&cv_orbinaut);
	CV_RegisterVar(&cv_jawz);
	CV_RegisterVar(&cv_mine);
	CV_RegisterVar(&cv_ballhog);
	CV_RegisterVar(&cv_selfpropelledbomb);
	CV_RegisterVar(&cv_grow);
	CV_RegisterVar(&cv_shrink);
	CV_RegisterVar(&cv_thundershield);
	CV_RegisterVar(&cv_bubbleshield);
	CV_RegisterVar(&cv_flameshield);
	CV_RegisterVar(&cv_hyudoro);
	CV_RegisterVar(&cv_pogospring);
	CV_RegisterVar(&cv_superring);
	CV_RegisterVar(&cv_kitchensink);

	CV_RegisterVar(&cv_triplesneaker);
	CV_RegisterVar(&cv_triplebanana);
	CV_RegisterVar(&cv_decabanana);
	CV_RegisterVar(&cv_tripleorbinaut);
	CV_RegisterVar(&cv_quadorbinaut);
	CV_RegisterVar(&cv_dualjawz);

	CV_RegisterVar(&cv_kartminimap);
	CV_RegisterVar(&cv_kartcheck);
	CV_RegisterVar(&cv_kartinvinsfx);
	CV_RegisterVar(&cv_kartspeed);
	CV_RegisterVar(&cv_kartbumpers);
	CV_RegisterVar(&cv_kartfrantic);
	CV_RegisterVar(&cv_kartcomeback);
	CV_RegisterVar(&cv_kartencore);
	CV_RegisterVar(&cv_kartvoterulechanges);
	CV_RegisterVar(&cv_kartspeedometer);
	CV_RegisterVar(&cv_kartvoices);
	CV_RegisterVar(&cv_kartbot);
	CV_RegisterVar(&cv_karteliminatelast);
	CV_RegisterVar(&cv_kartusepwrlv);
	CV_RegisterVar(&cv_votetime);

	CV_RegisterVar(&cv_kartdebugitem);
	CV_RegisterVar(&cv_kartdebugamount);
	CV_RegisterVar(&cv_kartdebugshrink);
	CV_RegisterVar(&cv_kartallowgiveitem);
	CV_RegisterVar(&cv_kartdebugdistribution);
	CV_RegisterVar(&cv_kartdebughuddrop);
	CV_RegisterVar(&cv_kartdebugwaypoints);

	CV_RegisterVar(&cv_kartdebugcheckpoint);
	CV_RegisterVar(&cv_kartdebugnodes);
	CV_RegisterVar(&cv_kartdebugcolorize);
}

//}

boolean K_IsPlayerLosing(player_t *player)
{
	INT32 winningpos = 1;
	UINT8 i, pcount = 0;

	if (battlecapsules && player->kartstuff[k_bumper] <= 0)
		return true; // DNF in break the capsules

	if (player->kartstuff[k_position] == 1)
		return false;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (players[i].kartstuff[k_position] > pcount)
			pcount = players[i].kartstuff[k_position];
	}

	if (pcount <= 1)
		return false;

	winningpos = pcount/2;
	if (pcount % 2) // any remainder?
		winningpos++;

	return (player->kartstuff[k_position] > winningpos);
}

fixed_t K_GetKartGameSpeedScalar(SINT8 value)
{
	// Easy = 81.25%
	// Normal = 100%
	// Hard = 118.75%
	// Nightmare = 137.5% ?!?!
	return ((13 + (3*value)) << FRACBITS) / 16;
}

//{ SRB2kart Roulette Code - Position Based

consvar_t *KartItemCVars[NUMKARTRESULTS-1] =
{
	&cv_sneaker,
	&cv_rocketsneaker,
	&cv_invincibility,
	&cv_banana,
	&cv_eggmanmonitor,
	&cv_orbinaut,
	&cv_jawz,
	&cv_mine,
	&cv_ballhog,
	&cv_selfpropelledbomb,
	&cv_grow,
	&cv_shrink,
	&cv_thundershield,
	&cv_bubbleshield,
	&cv_flameshield,
	&cv_hyudoro,
	&cv_pogospring,
	&cv_superring,
	&cv_kitchensink,
	&cv_triplesneaker,
	&cv_triplebanana,
	&cv_decabanana,
	&cv_tripleorbinaut,
	&cv_quadorbinaut,
	&cv_dualjawz
};

#define NUMKARTODDS 	80

// Less ugly 2D arrays
static INT32 K_KartItemOddsRace[NUMKARTRESULTS-1][8] =
{
				//P-Odds	 0  1  2  3  4  5  6  7
			   /*Sneaker*/ { 0, 0, 2, 4, 6, 0, 0, 0 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0, 0, 0, 0, 2, 4, 6 }, // Rocket Sneaker
		 /*Invincibility*/ { 0, 0, 0, 0, 1, 4, 7, 9 }, // Invincibility
				/*Banana*/ { 7, 3, 2, 0, 0, 0, 0, 0 }, // Banana
		/*Eggman Monitor*/ { 3, 2, 0, 0, 0, 0, 0, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 7, 4, 3, 2, 0, 0, 0, 0 }, // Orbinaut
				  /*Jawz*/ { 0, 3, 2, 1, 1, 0, 0, 0 }, // Jawz
				  /*Mine*/ { 0, 2, 2, 1, 0, 0, 0, 0 }, // Mine
			   /*Ballhog*/ { 0, 0, 2, 1, 0, 0, 0, 0 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 1, 2, 3, 4, 2, 2, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 0, 0, 0, 1, 2, 3, 0, 0 }, // Grow
				/*Shrink*/ { 0, 0, 0, 0, 0, 0, 2, 0 }, // Shrink
		/*Thunder Shield*/ { 1, 2, 0, 0, 0, 0, 0, 0 }, // Thunder Shield
		 /*Bubble Shield*/ { 0, 2, 3, 3, 1, 0, 0, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 0, 0, 0, 0, 0, 1, 3, 5 }, // Flame Shield
			   /*Hyudoro*/ { 0, 0, 0, 1, 2, 0, 0, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Pogo Spring
			/*Super Ring*/ { 2, 1, 1, 0, 0, 0, 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Kitchen Sink
			/*Sneaker x3*/ { 0, 0, 0, 2, 6,10, 5, 0 }, // Sneaker x3
			 /*Banana x3*/ { 0, 1, 1, 0, 0, 0, 0, 0 }, // Banana x3
			/*Banana x10*/ { 0, 0, 0, 1, 0, 0, 0, 0 }, // Banana x10
		   /*Orbinaut x3*/ { 0, 0, 1, 0, 0, 0, 0, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 0, 0, 0, 1, 1, 0, 0, 0 }, // Orbinaut x4
			   /*Jawz x2*/ { 0, 0, 1, 2, 0, 0, 0, 0 }  // Jawz x2
};

static INT32 K_KartItemOddsBattle[NUMKARTRESULTS-1][6] =
{
				//P-Odds	 0  1  2  3  4  5
			   /*Sneaker*/ { 3, 2, 2, 2, 0, 2 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0, 0, 0, 0, 0 }, // Rocket Sneaker
		 /*Invincibility*/ { 0, 1, 2, 3, 4, 2 }, // Invincibility
				/*Banana*/ { 2, 1, 0, 0, 0, 0 }, // Banana
		/*Eggman Monitor*/ { 1, 1, 0, 0, 0, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 6, 2, 1, 0, 0, 0 }, // Orbinaut
				  /*Jawz*/ { 3, 3, 3, 2, 0, 2 }, // Jawz
				  /*Mine*/ { 2, 3, 3, 1, 0, 2 }, // Mine
			   /*Ballhog*/ { 0, 1, 2, 1, 0, 2 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 0, 0, 0, 0, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 0, 0, 1, 2, 4, 2 }, // Grow
				/*Shrink*/ { 0, 0, 0, 0, 0, 0 }, // Shrink
		/*Thunder Shield*/ { 0, 0, 0, 0, 0, 0 }, // Thunder Shield
		 /*Bubble Shield*/ { 0, 0, 0, 0, 0, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 0, 0, 0, 0, 0, 0 }, // Flame Shield
			   /*Hyudoro*/ { 1, 1, 0, 0, 0, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 1, 1, 0, 0, 0, 0 }, // Pogo Spring
			/*Super Ring*/ { 0, 0, 0, 0, 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0, 0, 0, 0, 0 }, // Kitchen Sink
			/*Sneaker x3*/ { 0, 0, 0, 2, 4, 2 }, // Sneaker x3
			 /*Banana x3*/ { 1, 2, 1, 0, 0, 0 }, // Banana x3
			/*Banana x10*/ { 0, 0, 1, 1, 0, 2 }, // Banana x10
		   /*Orbinaut x3*/ { 0, 1, 2, 1, 0, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 0, 0, 1, 3, 4, 2 }, // Orbinaut x4
			   /*Jawz x2*/ { 0, 0, 1, 2, 4, 2 }  // Jawz x2
};

#define DISTVAR (2048) // Magic number distance for use with item roulette tiers

INT32 K_GetShieldFromItem(INT32 item)
{
	switch (item)
	{
		case KITEM_THUNDERSHIELD: return KSHIELD_THUNDER;
		case KITEM_BUBBLESHIELD: return KSHIELD_BUBBLE;
		case KITEM_FLAMESHIELD: return KSHIELD_FLAME;
		default: return KSHIELD_NONE;
	}
}

/**	\brief	Item Roulette for Kart

	\param	player		player
	\param	getitem		what item we're looking for

	\return	void
*/
static void K_KartGetItemResult(player_t *player, SINT8 getitem)
{
	if (getitem == KITEM_SPB || getitem == KITEM_SHRINK) // Indirect items
		indirectitemcooldown = 20*TICRATE;

	if (getitem == KITEM_HYUDORO) // Hyudoro cooldown
		hyubgone = 5*TICRATE;

	player->botvars.itemdelay = TICRATE;
	player->botvars.itemconfirm = 0;

	switch (getitem)
	{
		// Special roulettes first, then the generic ones are handled by default
		case KRITEM_TRIPLESNEAKER: // Sneaker x3
			player->kartstuff[k_itemtype] = KITEM_SNEAKER;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_TRIPLEBANANA: // Banana x3
			player->kartstuff[k_itemtype] = KITEM_BANANA;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_TENFOLDBANANA: // Banana x10
			player->kartstuff[k_itemtype] = KITEM_BANANA;
			player->kartstuff[k_itemamount] = 10;
			break;
		case KRITEM_TRIPLEORBINAUT: // Orbinaut x3
			player->kartstuff[k_itemtype] = KITEM_ORBINAUT;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_QUADORBINAUT: // Orbinaut x4
			player->kartstuff[k_itemtype] = KITEM_ORBINAUT;
			player->kartstuff[k_itemamount] = 4;
			break;
		case KRITEM_DUALJAWZ: // Jawz x2
			player->kartstuff[k_itemtype] = KITEM_JAWZ;
			player->kartstuff[k_itemamount] = 2;
			break;
		default:
			if (getitem <= 0 || getitem >= NUMKARTRESULTS) // Sad (Fallback)
			{
				if (getitem != 0)
					CONS_Printf("ERROR: P_KartGetItemResult - Item roulette gave bad item (%d) :(\n", getitem);
				player->kartstuff[k_itemtype] = KITEM_SAD;
			}
			else
				player->kartstuff[k_itemtype] = getitem;
			player->kartstuff[k_itemamount] = 1;
			break;
	}
}

/**	\brief	Item Roulette for Kart

	\param	player	player object passed from P_KartPlayerThink

	\return	void
*/

static INT32 K_KartGetItemOdds(UINT8 pos, SINT8 item, fixed_t mashed, boolean spbrush, boolean bot)
{
	INT32 newodds;
	INT32 i;
	UINT8 pingame = 0, pexiting = 0;
	SINT8 first = -1, second = -1;
	INT32 secondist = 0;
	INT32 shieldtype = KSHIELD_NONE;

	I_Assert(item > KITEM_NONE); // too many off by one scenarioes.
	I_Assert(KartItemCVars[NUMKARTRESULTS-2] != NULL); // Make sure this exists

	if (!KartItemCVars[item-1]->value && !modeattacking)
		return 0;

	if (G_BattleGametype())
	{
		I_Assert(pos < 6); // DO NOT allow positions past the bounds of the table
		newodds = K_KartItemOddsBattle[item-1][pos];
	}
	else
	{
		I_Assert(pos < 8); // Ditto
		newodds = K_KartItemOddsRace[item-1][pos];
	}

	// Base multiplication to ALL item odds to simulate fractional precision
	newodds *= 4;

	shieldtype = K_GetShieldFromItem(item);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (!G_BattleGametype() || players[i].kartstuff[k_bumper])
			pingame++;

		if (players[i].exiting)
			pexiting++;

		if (shieldtype != KSHIELD_NONE && shieldtype == K_GetShieldFromItem(players[i].kartstuff[k_itemtype]))
		{
			// Don't allow more than one of each shield type at a time
			return 0;
		}

		if (players[i].mo && G_RaceGametype())
		{
			if (players[i].kartstuff[k_position] == 1 && first == -1)
				first = i;
			if (players[i].kartstuff[k_position] == 2 && second == -1)
				second = i;
		}
	}

	if (first != -1 && second != -1) // calculate 2nd's distance from 1st, for SPB
	{
		secondist = players[second].distancetofinish - players[first].distancetofinish;
		if (franticitems)
			secondist = (15 * secondist) / 14;
		secondist = ((28 + (8-pingame)) * secondist) / 28;
	}

	// POWERITEMODDS handles all of the "frantic item" related functionality, for all of our powerful items.
	// First, it multiplies it by 2 if franticitems is true; easy-peasy.
	// Next, it multiplies it again if it's in SPB mode and 2nd needs to apply pressure to 1st.
	// Then, it multiplies it further if the player count isn't equal to 8.
	// This is done to make low player count races more interesting and high player count rates more fair.
	// (2P normal would be about halfway between 8P normal and 8P frantic.)
	// (This scaling is not done for SPB Rush, so that catchup strength is not weakened.)
	// Lastly, it *divides* it by your mashed value, which was determined in K_KartItemRoulette, for lesser items needed in a pinch.

#define PLAYERSCALING (8 - (spbrush ? 2 : pingame))

#define POWERITEMODDS(odds) {\
	if (franticitems) \
		odds <<= 1; \
	odds = FixedMul(odds<<FRACBITS, FRACUNIT + ((PLAYERSCALING << FRACBITS) / 25)) >> FRACBITS; \
	if (mashed > 0) \
		odds = FixedDiv(odds<<FRACBITS, FRACUNIT + mashed) >> FRACBITS; \
}

#define COOLDOWNONSTART (leveltime < (30*TICRATE)+starttime)

	/*
	if (bot)
	{
		// TODO: Item use on bots should all be passed-in functions.
		// Instead of manually inserting these, it should return 0
		// for any items without an item use function supplied

		switch (item)
		{
			case KITEM_SNEAKER:
			case KITEM_ROCKETSNEAKER:
			case KITEM_INVINCIBILITY:
			case KITEM_BANANA:
			case KITEM_EGGMAN:
			case KITEM_ORBINAUT:
			case KITEM_JAWZ:
			case KITEM_MINE:
			case KITEM_BALLHOG:
			case KITEM_SPB:
			case KITEM_GROW:
			case KITEM_SHRINK:
			case KITEM_HYUDORO:
			case KITEM_SUPERRING:
			case KITEM_THUNDERSHIELD:
			case KITEM_BUBBLESHIELD:
			case KITEM_FLAMESHIELD:
			case KRITEM_TRIPLESNEAKER:
			case KRITEM_TRIPLEBANANA:
			case KRITEM_TENFOLDBANANA:
			case KRITEM_TRIPLEORBINAUT:
			case KRITEM_QUADORBINAUT:
			case KRITEM_DUALJAWZ:
				break;
			default:
				return 0;
		}
	}
	*/
	(void)bot;

	switch (item)
	{
		case KITEM_ROCKETSNEAKER:
		case KITEM_JAWZ:
		case KITEM_BALLHOG:
		case KRITEM_TRIPLESNEAKER:
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TENFOLDBANANA:
		case KRITEM_TRIPLEORBINAUT:
		case KRITEM_QUADORBINAUT:
		case KRITEM_DUALJAWZ:
			POWERITEMODDS(newodds);
			break;
		case KITEM_INVINCIBILITY:
		case KITEM_MINE:
		case KITEM_GROW:
		case KITEM_BUBBLESHIELD:
		case KITEM_FLAMESHIELD:
			if (COOLDOWNONSTART)
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_SPB:
			if ((indirectitemcooldown > 0) || COOLDOWNONSTART
				|| (first != -1 && players[first].distancetofinish < 8*DISTVAR)) // No SPB near the end of the race
			{
				newodds = 0;
			}
			else
			{
				INT32 multiplier = (secondist - (5*DISTVAR)) / DISTVAR;

				if (multiplier < 0)
					multiplier = 0;
				if (multiplier > 3)
					multiplier = 3;

				newodds *= multiplier;
			}
			break;
		case KITEM_SHRINK:
			if ((indirectitemcooldown > 0) || COOLDOWNONSTART || (pingame-1 <= pexiting))
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_THUNDERSHIELD:
			if (spbplace != -1 || COOLDOWNONSTART)
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_HYUDORO:
			if ((hyubgone > 0) || COOLDOWNONSTART)
				newodds = 0;
			break;
		default:
			break;
	}

#undef POWERITEMODDS

	return newodds;
}

//{ SRB2kart Roulette Code - Distance Based, yes waypoints

static UINT8 K_FindUseodds(player_t *player, fixed_t mashed, UINT32 pdis, UINT8 bestbumper, boolean spbrush)
{
	UINT8 i;
	UINT8 n = 0;
	UINT8 useodds = 0;
	UINT8 disttable[14];
	UINT8 totallen = 0;
	UINT8 distlen = 0;
	boolean oddsvalid[8];

	for (i = 0; i < 8; i++)
	{
		UINT8 j;
		boolean available = false;

		if (G_BattleGametype() && i > 5)
		{
			oddsvalid[i] = false;
			break;
		}

		for (j = 1; j < NUMKARTRESULTS; j++)
		{
			if (K_KartGetItemOdds(i, j, mashed, spbrush, player->bot) > 0)
			{
				available = true;
				break;
			}
		}

		oddsvalid[i] = available;
	}

#define SETUPDISTTABLE(odds, num) \
	if (oddsvalid[odds]) \
		for (i = num; i; --i) \
			disttable[distlen++] = odds; \
	totallen += num;

	if (G_BattleGametype()) // Battle Mode
	{
		SETUPDISTTABLE(0,1);
		SETUPDISTTABLE(1,1);
		SETUPDISTTABLE(2,1);
		SETUPDISTTABLE(3,1);
		SETUPDISTTABLE(4,1);

		if (player->kartstuff[k_roulettetype] == 1 && oddsvalid[5]) // 5 is the extreme odds of player-controlled "Karma" items
			useodds = 5;
		else
		{
			SINT8 wantedpos = (bestbumper-player->kartstuff[k_bumper]); // 0 is the best player's bumper count, 1 is a bumper below best, 2 is two bumpers below, etc
			if (K_IsPlayerWanted(player))
				wantedpos++;
			if (wantedpos > 4) // Don't run off into karma items
				wantedpos = 4;
			if (wantedpos < 0) // Don't go below somehow
				wantedpos = 0;
			n = (wantedpos * distlen) / totallen;
			useodds = disttable[n];
		}
	}
	else
	{
		SETUPDISTTABLE(0,1);
		SETUPDISTTABLE(1,1);
		SETUPDISTTABLE(2,1);
		SETUPDISTTABLE(3,2);
		SETUPDISTTABLE(4,2);
		SETUPDISTTABLE(5,3);
		SETUPDISTTABLE(6,3);
		SETUPDISTTABLE(7,1);

		if (pdis == 0)
			useodds = disttable[0];
		else if (pdis > DISTVAR * ((12 * distlen) / 14))
			useodds = disttable[distlen-1];
		else
		{
			for (i = 1; i < 13; i++)
			{
				if (pdis <= DISTVAR * ((i * distlen) / 14))
				{
					useodds = disttable[((i * distlen) / 14)];
					break;
				}
			}
		}
	}

#undef SETUPDISTTABLE

	return useodds;
}

static void K_KartItemRoulette(player_t *player, ticcmd_t *cmd)
{
	INT32 i;
	UINT8 pingame = 0;
	UINT8 roulettestop;
	UINT32 pdis = 0;
	UINT8 useodds = 0;
	INT32 spawnchance[NUMKARTRESULTS];
	INT32 totalspawnchance = 0;
	UINT8 bestbumper = 0;
	fixed_t mashed = 0;
	boolean dontforcespb = false;
	boolean spbrush = false;

	// This makes the roulette cycle through items - if this is 0, you shouldn't be here.
	if (player->kartstuff[k_itemroulette])
		player->kartstuff[k_itemroulette]++;
	else
		return;

	// Gotta check how many players are active at this moment.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		pingame++;
		if (players[i].exiting)
			dontforcespb = true;
		if (players[i].kartstuff[k_bumper] > bestbumper)
			bestbumper = players[i].kartstuff[k_bumper];
	}

	// No forced SPB in 1v1s, it has to be randomly rolled
	if (pingame <= 2)
		dontforcespb = true;

	// This makes the roulette produce the random noises.
	if ((player->kartstuff[k_itemroulette] % 3) == 1 && P_IsDisplayPlayer(player) && !demo.freecam)
	{
#define PLAYROULETTESND S_StartSound(NULL, sfx_itrol1 + ((player->kartstuff[k_itemroulette] / 3) % 8))
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (player == &players[displayplayers[i]] && players[displayplayers[i]].kartstuff[k_itemroulette])
				PLAYROULETTESND;
		}
#undef PLAYROULETTESND
	}

	roulettestop = TICRATE + (3*(pingame - player->kartstuff[k_position]));

	// If the roulette finishes or the player presses BT_ATTACK, stop the roulette and calculate the item.
	// I'm returning via the exact opposite, however, to forgo having another bracket embed. Same result either way, I think.
	// Finally, if you get past this check, now you can actually start calculating what item you get.
	if ((cmd->buttons & BT_ATTACK) && (player->kartstuff[k_itemroulette] >= roulettestop)
		&& !(player->kartstuff[k_eggmanheld] || player->kartstuff[k_itemheld] || player->kartstuff[k_userings]))
	{
		// Mashing reduces your chances for the good items
		mashed = FixedDiv((player->kartstuff[k_itemroulette])*FRACUNIT, ((TICRATE*3)+roulettestop)*FRACUNIT) - FRACUNIT;
	}
	else if (!(player->kartstuff[k_itemroulette] >= (TICRATE*3)))
		return;

	if (cmd->buttons & BT_ATTACK)
		player->pflags |= PF_ATTACKDOWN;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator
			&& players[i].kartstuff[k_position] == 1)
		{
			// This player is first! Yay!

			if (player->distancetofinish <= players[i].distancetofinish)
			{
				// Guess you're in first / tied for first?
				pdis = 0;
			}
			else
			{
				// Subtract 1st's distance from your distance, to get your distance from 1st!
				pdis = player->distancetofinish - players[i].distancetofinish;
			}
			break;
		}
	}

	if (mapobjectscale != FRACUNIT)
		pdis = FixedDiv(pdis * FRACUNIT, mapobjectscale) / FRACUNIT;

	if (franticitems) // Frantic items make the distances between everyone artifically higher, for crazier items
		pdis = (15 * pdis) / 14;

	if (spbplace != -1 && player->kartstuff[k_position] == spbplace+1) // SPB Rush Mode: It's 2nd place's job to catch-up items and make 1st place's job hell
	{
		pdis = (3 * pdis) / 2;
		spbrush = true;
	}

	pdis = ((28 + (8-pingame)) * pdis) / 28; // scale with player count

	// SPECIAL CASE No. 1:
	// Fake Eggman items
	if (player->kartstuff[k_roulettetype] == 2)
	{
		player->kartstuff[k_eggmanexplode] = 4*TICRATE;
		//player->karthud[khud_itemblink] = TICRATE;
		//player->karthud[khud_itemblinkmode] = 1;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_itrole);
		return;
	}

	// SPECIAL CASE No. 2:
	// Give a debug item instead if specified
	if (cv_kartdebugitem.value != 0 && !modeattacking)
	{
		K_KartGetItemResult(player, cv_kartdebugitem.value);
		player->kartstuff[k_itemamount] = cv_kartdebugamount.value;
		player->karthud[khud_itemblink] = TICRATE;
		player->karthud[khud_itemblinkmode] = 2;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_dbgsal);
		return;
	}

	// SPECIAL CASE No. 3:
	// Record Attack / alone mashing behavior
	if (modeattacking || pingame == 1)
	{
		if (G_RaceGametype())
		{
			if (mashed && (modeattacking || cv_superring.value)) // ANY mashed value? You get rings.
			{
				K_KartGetItemResult(player, KITEM_SUPERRING);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else
			{
				if (modeattacking || cv_sneaker.value) // Waited patiently? You get a sneaker!
					K_KartGetItemResult(player, KITEM_SNEAKER);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}
		else if (G_BattleGametype())
		{
			if (mashed && (modeattacking || cv_banana.value)) // ANY mashed value? You get a banana.
			{
				K_KartGetItemResult(player, KITEM_BANANA);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else
			{
				if (modeattacking || cv_tripleorbinaut.value) // Waited patiently? You get Orbinaut x3!
					K_KartGetItemResult(player, KRITEM_TRIPLEORBINAUT);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}

		player->karthud[khud_itemblink] = TICRATE;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		return;
	}

	if (G_RaceGametype())
	{
		// SPECIAL CASE No. 4:
		// Being in ring debt occasionally forces Super Ring on you if you mashed
		if (mashed && player->kartstuff[k_rings] < 0 && cv_superring.value)
		{
			INT32 debtamount = min(20, abs(player->kartstuff[k_rings]));
			if (P_RandomChance((debtamount*FRACUNIT)/20))
			{
				K_KartGetItemResult(player, KITEM_SUPERRING);
				player->karthud[khud_itemblink] = TICRATE;
				player->karthud[khud_itemblinkmode] = 1;
				player->kartstuff[k_itemroulette] = 0;
				player->kartstuff[k_roulettetype] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
				return;
			}
		}

		// SPECIAL CASE No. 5:
		// Force SPB onto 2nd if they get too far behind
		if (player->kartstuff[k_position] == 2 && pdis > (DISTVAR*8)
			&& spbplace == -1 && !indirectitemcooldown && !dontforcespb
			&& cv_selfpropelledbomb.value)
		{
			K_KartGetItemResult(player, KITEM_SPB);
			player->karthud[khud_itemblink] = TICRATE;
			player->karthud[khud_itemblinkmode] = (mashed ? 1 : 0);
			player->kartstuff[k_itemroulette] = 0;
			player->kartstuff[k_roulettetype] = 0;
			if (P_IsDisplayPlayer(player))
				S_StartSound(NULL, (mashed ? sfx_itrolm : sfx_itrolf));
			return;
		}
	}

	// NOW that we're done with all of those specialized cases, we can move onto the REAL item roulette tables.
	// Initializes existing spawnchance values
	for (i = 0; i < NUMKARTRESULTS; i++)
		spawnchance[i] = 0;

	// Split into another function for a debug function below
	useodds = K_FindUseodds(player, mashed, pdis, bestbumper, spbrush);

	for (i = 1; i < NUMKARTRESULTS; i++)
		spawnchance[i] = (totalspawnchance += K_KartGetItemOdds(useodds, i, mashed, spbrush, player->bot));

	// Award the player whatever power is rolled
	if (totalspawnchance > 0)
	{
		totalspawnchance = P_RandomKey(totalspawnchance);
		for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);

		K_KartGetItemResult(player, i);
	}
	else
	{
		player->kartstuff[k_itemtype] = KITEM_SAD;
		player->kartstuff[k_itemamount] = 1;
	}

	if (P_IsDisplayPlayer(player) && !demo.freecam)
		S_StartSound(NULL, ((player->kartstuff[k_roulettetype] == 1) ? sfx_itrolk : (mashed ? sfx_itrolm : sfx_itrolf)));

	player->karthud[khud_itemblink] = TICRATE;
	player->karthud[khud_itemblinkmode] = ((player->kartstuff[k_roulettetype] == 1) ? 2 : (mashed ? 1 : 0));

	player->kartstuff[k_itemroulette] = 0; // Since we're done, clear the roulette number
	player->kartstuff[k_roulettetype] = 0; // This too
}

//}

//{ SRB2kart p_user.c Stuff

static fixed_t K_PlayerWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	if (!mobj->player)
		return weight;

	if (against && !P_MobjWasRemoved(against) && against->player
		&& ((!against->player->kartstuff[k_spinouttimer] && mobj->player->kartstuff[k_spinouttimer]) // You're in spinout
		|| (against->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD && mobj->player->kartstuff[k_itemtype] != KITEM_BUBBLESHIELD))) // They have a Bubble Shield
	{
		weight = 0; // This player does not cause any bump action
	}
	else
	{
		weight = (mobj->player->kartweight) * FRACUNIT;
		if (mobj->player->speed > K_GetKartSpeed(mobj->player, false))
			weight += (mobj->player->speed - K_GetKartSpeed(mobj->player, false))/8;
		if (mobj->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD)
			weight += 9*FRACUNIT;
	}

	return weight;
}

fixed_t K_GetMobjWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	switch (mobj->type)
	{
		case MT_PLAYER:
			if (!mobj->player)
				break;
			weight = K_PlayerWeight(mobj, against);
			break;
		case MT_BUBBLESHIELD:
			weight = K_PlayerWeight(mobj->target, against);
			break;
		case MT_FALLINGROCK:
			if (against->player)
			{
				if (against->player->kartstuff[k_invincibilitytimer] || against->player->kartstuff[k_growshrinktimer] > 0)
					weight = 0;
				else
					weight = K_PlayerWeight(against, NULL);
			}
			break;
		case MT_ORBINAUT:
		case MT_ORBINAUT_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL);
			break;
		case MT_JAWZ:
		case MT_JAWZ_DUD:
		case MT_JAWZ_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL) + (3*FRACUNIT);
			else
				weight += 3*FRACUNIT;
			break;
		default:
			break;
	}

	return FixedMul(weight, mobj->scale);
}

// This kind of wipeout happens with no rings -- doesn't remove a bumper, has no invulnerability, and is much shorter.
static void K_DebtStingPlayer(player_t *player, INT32 length)
{
	if (player->health <= 0)
		return;

	if (player->powers[pw_flashing] > 0 || player->kartstuff[k_squishedtimer] > 0 || player->kartstuff[k_spinouttimer] > 0
		|| player->kartstuff[k_invincibilitytimer] > 0 || player->kartstuff[k_growshrinktimer] > 0 || player->kartstuff[k_hyudorotimer] > 0
		|| (G_BattleGametype() && ((player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]) || player->kartstuff[k_comebackmode] == 1)))
		return;

	player->kartstuff[k_ringboost] = 0;
	player->kartstuff[k_driftboost] = 0;
	player->kartstuff[k_drift] = 0;
	player->kartstuff[k_driftcharge] = 0;
	player->kartstuff[k_pogospring] = 0;

	player->kartstuff[k_spinouttype] = 2;
	player->kartstuff[k_spinouttimer] = length;
	player->kartstuff[k_wipeoutslow] = min(length-1, wipeoutslowtime+1);

	if (player->mo->state != &states[S_KART_SPIN])
		P_SetPlayerMobjState(player->mo, S_KART_SPIN);

	K_DropHnextList(player, false);
	return;
}

void K_KartBouncing(mobj_t *mobj1, mobj_t *mobj2, boolean bounce, boolean solid)
{
	mobj_t *fx;
	fixed_t momdifx, momdify;
	fixed_t distx, disty;
	fixed_t dot, force;
	fixed_t mass1, mass2;

	if (!mobj1 || !mobj2)
		return;

	// Don't bump when you're being reborn
	if ((mobj1->player && mobj1->player->playerstate != PST_LIVE)
		|| (mobj2->player && mobj2->player->playerstate != PST_LIVE))
		return;

	if ((mobj1->player && mobj1->player->respawn.state != RESPAWNST_NONE)
		|| (mobj2->player && mobj2->player->respawn.state != RESPAWNST_NONE))
		return;

	{ // Don't bump if you're flashing
		INT32 flash;

		flash = K_GetKartFlashing(mobj1->player);
		if (mobj1->player && mobj1->player->powers[pw_flashing] > 0 && mobj1->player->powers[pw_flashing] < flash)
		{
			if (mobj1->player->powers[pw_flashing] < flash-1)
				mobj1->player->powers[pw_flashing]++;
			return;
		}

		flash = K_GetKartFlashing(mobj2->player);
		if (mobj2->player && mobj2->player->powers[pw_flashing] > 0 && mobj2->player->powers[pw_flashing] < flash)
		{
			if (mobj2->player->powers[pw_flashing] < flash-1)
				mobj2->player->powers[pw_flashing]++;
			return;
		}
	}

	// Don't bump if you've recently bumped
	if (mobj1->player && mobj1->player->kartstuff[k_justbumped])
	{
		mobj1->player->kartstuff[k_justbumped] = bumptime;
		return;
	}

	if (mobj2->player && mobj2->player->kartstuff[k_justbumped])
	{
		mobj2->player->kartstuff[k_justbumped] = bumptime;
		return;
	}

	mass1 = K_GetMobjWeight(mobj1, mobj2);

	if (solid == true && mass1 > 0)
		mass2 = mass1;
	else
		mass2 = K_GetMobjWeight(mobj2, mobj1);

	momdifx = mobj1->momx - mobj2->momx;
	momdify = mobj1->momy - mobj2->momy;

	// Adds the OTHER player's momentum times a bunch, for the best chance of getting the correct direction
	distx = (mobj1->x + mobj2->momx*3) - (mobj2->x + mobj1->momx*3);
	disty = (mobj1->y + mobj2->momy*3) - (mobj2->y + mobj1->momy*3);

	if (distx == 0 && disty == 0)
		// if there's no distance between the 2, they're directly on top of each other, don't run this
		return;

	{ // Normalize distance to the sum of the two objects' radii, since in a perfect world that would be the distance at the point of collision...
		fixed_t dist = P_AproxDistance(distx, disty);
		fixed_t nx = FixedDiv(distx, dist);
		fixed_t ny = FixedDiv(disty, dist);

		dist = dist ? dist : 1;
		distx = FixedMul(mobj1->radius+mobj2->radius, nx);
		disty = FixedMul(mobj1->radius+mobj2->radius, ny);

		if (momdifx == 0 && momdify == 0)
		{
			// If there's no momentum difference, they're moving at exactly the same rate. Pretend they moved into each other.
			momdifx = -nx;
			momdify = -ny;
		}
	}

	// if the speed difference is less than this let's assume they're going proportionately faster from each other
	if (P_AproxDistance(momdifx, momdify) < (25*mapobjectscale))
	{
		fixed_t momdiflength = P_AproxDistance(momdifx, momdify);
		fixed_t normalisedx = FixedDiv(momdifx, momdiflength);
		fixed_t normalisedy = FixedDiv(momdify, momdiflength);
		momdifx = FixedMul((25*mapobjectscale), normalisedx);
		momdify = FixedMul((25*mapobjectscale), normalisedy);
	}

	dot = FixedMul(momdifx, distx) + FixedMul(momdify, disty);

	if (dot >= 0)
	{
		// They're moving away from each other
		return;
	}

	force = FixedDiv(dot, FixedMul(distx, distx)+FixedMul(disty, disty));

	if (bounce == true && mass2 > 0) // Perform a Goomba Bounce.
		mobj1->momz = -mobj1->momz;
	else
	{
		fixed_t newz = mobj1->momz;
		if (mass2 > 0)
			mobj1->momz = mobj2->momz;
		if (mass1 > 0 && solid == false)
			mobj2->momz = newz;
	}

	if (mass2 > 0)
	{
		mobj1->momx = mobj1->momx - FixedMul(FixedMul(FixedDiv(2*mass2, mass1 + mass2), force), distx);
		mobj1->momy = mobj1->momy - FixedMul(FixedMul(FixedDiv(2*mass2, mass1 + mass2), force), disty);
	}

	if (mass1 > 0 && solid == false)
	{
		mobj2->momx = mobj2->momx - FixedMul(FixedMul(FixedDiv(2*mass1, mass1 + mass2), force), -distx);
		mobj2->momy = mobj2->momy - FixedMul(FixedMul(FixedDiv(2*mass1, mass1 + mass2), force), -disty);
	}

	// Do the bump fx when we've CONFIRMED we can bump.
	if ((mobj1->player && mobj1->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD) || (mobj2->player && mobj2->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD))
		S_StartSound(mobj1, sfx_s3k44);
	else
		S_StartSound(mobj1, sfx_s3k49);

	fx = P_SpawnMobj(mobj1->x/2 + mobj2->x/2, mobj1->y/2 + mobj2->y/2, mobj1->z/2 + mobj2->z/2, MT_BUMP);
	if (mobj1->eflags & MFE_VERTICALFLIP)
		fx->eflags |= MFE_VERTICALFLIP;
	else
		fx->eflags &= ~MFE_VERTICALFLIP;
	P_SetScale(fx, mobj1->scale);

	// Because this is done during collision now, rmomx and rmomy need to be recalculated
	// so that friction doesn't immediately decide to stop the player if they're at a standstill
	// Also set justbumped here
	if (mobj1->player)
	{
		mobj1->player->rmomx = mobj1->momx - mobj1->player->cmomx;
		mobj1->player->rmomy = mobj1->momy - mobj1->player->cmomy;
		mobj1->player->kartstuff[k_justbumped] = bumptime;

		if (mobj1->player->kartstuff[k_spinouttimer])
		{
			mobj1->player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;
			mobj1->player->kartstuff[k_spinouttimer] = max(wipeoutslowtime+1, mobj1->player->kartstuff[k_spinouttimer]);
			//mobj1->player->kartstuff[k_spinouttype] = 1; // Enforce type
		}
		else if (mobj2->player // Player VS player bumping only
			&& (K_GetShieldFromItem(mobj1->player->kartstuff[k_itemtype]) == KSHIELD_NONE)) // Ignore for shields
		{
			if (mobj1->player->kartstuff[k_rings] <= 0)
			{
				K_DebtStingPlayer(mobj1->player, TICRATE + (4 * (mobj2->player->kartweight - mobj1->player->kartweight)));
				K_KartPainEnergyFling(mobj1->player);
				P_PlayRinglossSound(mobj1);
			}
			P_PlayerRingBurst(mobj1->player, 1);
		}
	}

	if (mobj2->player)
	{
		mobj2->player->rmomx = mobj2->momx - mobj2->player->cmomx;
		mobj2->player->rmomy = mobj2->momy - mobj2->player->cmomy;
		mobj2->player->kartstuff[k_justbumped] = bumptime;

		if (mobj2->player->kartstuff[k_spinouttimer])
		{
			mobj2->player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;
			mobj2->player->kartstuff[k_spinouttimer] = max(wipeoutslowtime+1, mobj2->player->kartstuff[k_spinouttimer]);
			//mobj2->player->kartstuff[k_spinouttype] = 1; // Enforce type
		}
		else if (mobj1->player // Player VS player bumping only
			&& (K_GetShieldFromItem(mobj2->player->kartstuff[k_itemtype]) == KSHIELD_NONE)) // Ignore for shields
		{
			if (mobj2->player->kartstuff[k_rings] <= 0)
			{
				K_DebtStingPlayer(mobj2->player, TICRATE + (4 * (mobj1->player->kartweight - mobj2->player->kartweight)));
				K_KartPainEnergyFling(mobj2->player);
				P_PlayRinglossSound(mobj2);
			}
			P_PlayerRingBurst(mobj2->player, 1);
		}
	}
}

/**	\brief	Checks that the player is on an offroad subsector for realsies

	\param	mo	player mobj object

	\return	boolean
*/
static UINT8 K_CheckOffroadCollide(mobj_t *mo)
{
	UINT8 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 2; i < 5; i++)
	{
		if (P_MobjTouchingSectorSpecial(mo, 1, i, true))
			return i-1;
	}

	return 0;
}

/**	\brief	Updates the Player's offroad value once per frame

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
static void K_UpdateOffroad(player_t *player)
{
	fixed_t offroadstrength = (K_CheckOffroadCollide(player->mo) << FRACBITS);

	// If you are in offroad, a timer starts.
	if (offroadstrength)
	{
		if (player->kartstuff[k_offroad] < offroadstrength)
			player->kartstuff[k_offroad] += offroadstrength / TICRATE;

		if (player->kartstuff[k_offroad] > offroadstrength)
			player->kartstuff[k_offroad] = offroadstrength;
	}
	else
		player->kartstuff[k_offroad] = 0;
}

static void K_DrawDraftCombiring(player_t *player, player_t *victim, fixed_t curdist, fixed_t maxdist, boolean transparent)
{
#define CHAOTIXBANDLEN 15
#define CHAOTIXBANDCOLORS 9
	static const UINT8 colors[CHAOTIXBANDCOLORS] = {
		SKINCOLOR_SAPPHIRE,
		SKINCOLOR_PLATINUM,
		SKINCOLOR_TEA,
		SKINCOLOR_GARDEN,
		SKINCOLOR_BANANA,
		SKINCOLOR_GOLD,
		SKINCOLOR_ORANGE,
		SKINCOLOR_SCARLET,
		SKINCOLOR_TAFFY
	};
	fixed_t minimumdist = FixedMul(RING_DIST>>1, player->mo->scale);
	UINT8 n = CHAOTIXBANDLEN;
	UINT8 offset = ((leveltime / 3) % 3);
	fixed_t stepx, stepy, stepz;
	fixed_t curx, cury, curz;
	UINT8 c;

	if (maxdist == 0)
		c = 0;
	else
		c = FixedMul(CHAOTIXBANDCOLORS<<FRACBITS, FixedDiv(curdist-minimumdist, maxdist-minimumdist)) >> FRACBITS;

	stepx = (victim->mo->x - player->mo->x) / CHAOTIXBANDLEN;
	stepy = (victim->mo->y - player->mo->y) / CHAOTIXBANDLEN;
	stepz = ((victim->mo->z + (victim->mo->height / 2)) - (player->mo->z + (player->mo->height / 2))) / CHAOTIXBANDLEN;

	curx = player->mo->x + stepx;
	cury = player->mo->y + stepy;
	curz = player->mo->z + stepz;

	while (n)
	{
		if (offset == 0)
		{
			mobj_t *band = P_SpawnMobj(curx + (P_RandomRange(-12,12)*mapobjectscale),
				cury + (P_RandomRange(-12,12)*mapobjectscale),
				curz + (P_RandomRange(24,48)*mapobjectscale),
				MT_SIGNSPARKLE);
			P_SetMobjState(band, S_SIGNSPARK1 + (leveltime % 11));
			P_SetScale(band, (band->destscale = (3*player->mo->scale)/2));
			band->color = colors[c];
			band->colorized = true;
			band->fuse = 2;
			if (transparent)
				band->flags2 |= MF2_SHADOW;
			if (!P_IsDisplayPlayer(player) && !P_IsDisplayPlayer(victim))
				band->flags2 |= MF2_DONTDRAW;
		}

		curx += stepx;
		cury += stepy;
		curz += stepz;

		offset = abs(offset-1) % 3;
		n--;
	}
#undef CHAOTIXBANDLEN
}

/**	\brief	Updates the player's drafting values once per frame

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
static void K_UpdateDraft(player_t *player)
{
	fixed_t topspd = K_GetKartSpeed(player, false);
	fixed_t draftdistance;
	UINT8 leniency;
	UINT8 i;

	if (player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
	{
		// Flame Shield gets infinite draft distance as its passive effect.
		draftdistance = 0;
	}
	else
	{
		// Distance you have to be to draft. If you're still accelerating, then this distance is lessened.
		// This distance biases toward low weight! (min weight gets 4096 units, max weight gets 3072 units)
		// This distance is also scaled based on game speed.
		draftdistance = (3072 + (128 * (9 - player->kartweight))) * player->mo->scale;
		if (player->speed < topspd)
			draftdistance = FixedMul(draftdistance, FixedDiv(player->speed, topspd));
		draftdistance = FixedMul(draftdistance, K_GetKartGameSpeedScalar(gamespeed));
	}

	// On the contrary, the leniency period biases toward high weight.
	// (See also: the leniency variable in K_SpawnDraftDust)
	leniency = (3*TICRATE)/4 + ((player->kartweight-1) * (TICRATE/4));

	// Not enough speed to draft.
	if (player->speed >= 20*player->mo->scale)
	{
//#define EASYDRAFTTEST
		// Let's hunt for players to draft off of!
		for (i = 0; i < MAXPLAYERS; i++)
		{
			fixed_t dist, olddraft;
#ifndef EASYDRAFTTEST
			angle_t yourangle, theirangle, diff;
#endif

			if (!playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

#ifndef EASYDRAFTTEST
			// Don't draft on yourself :V
			if (&players[i] == player)
				continue;
#endif

			// Not enough speed to draft off of.
			if (players[i].speed < 20*players[i].mo->scale)
				continue;

#ifndef EASYDRAFTTEST
			yourangle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
			theirangle = R_PointToAngle2(0, 0, players[i].mo->momx, players[i].mo->momy);

			diff = R_PointToAngle2(player->mo->x, player->mo->y, players[i].mo->x, players[i].mo->y) - yourangle;
			if (diff > ANGLE_180)
				diff = InvAngle(diff);

			// Not in front of this player.
			if (diff > ANG10)
				continue;

			diff = yourangle - theirangle;
			if (diff > ANGLE_180)
				diff = InvAngle(diff);

			// Not moving in the same direction.
			if (diff > ANGLE_90)
				continue;
#endif

			dist = P_AproxDistance(P_AproxDistance(players[i].mo->x - player->mo->x, players[i].mo->y - player->mo->y), players[i].mo->z - player->mo->z);

#ifndef EASYDRAFTTEST
			// TOO close to draft.
			if (dist < FixedMul(RING_DIST>>1, player->mo->scale))
				continue;

			// Not close enough to draft.
			if (dist > draftdistance && draftdistance > 0)
				continue;
#endif

			olddraft = player->kartstuff[k_draftpower];

			player->kartstuff[k_draftleeway] = leniency;
			player->kartstuff[k_lastdraft] = i;

			// Draft power is used later in K_GetKartBoostPower, ranging from 0 for normal speed and FRACUNIT for max draft speed.
			// How much this increments every tic biases toward acceleration! (min speed gets 1.5% per tic, max speed gets 0.5% per tic)
			if (player->kartstuff[k_draftpower] < FRACUNIT)
				player->kartstuff[k_draftpower] += (FRACUNIT/200) + ((9 - player->kartspeed) * ((3*FRACUNIT)/1600));

			if (player->kartstuff[k_draftpower] > FRACUNIT)
				player->kartstuff[k_draftpower] = FRACUNIT;

			// Play draft finish noise
			if (olddraft < FRACUNIT && player->kartstuff[k_draftpower] >= FRACUNIT)
				S_StartSound(player->mo, sfx_cdfm62);

			// Spawn in the visual!
			K_DrawDraftCombiring(player, &players[i], dist, draftdistance, false);

			return; // Finished doing our draft.
		}
	}

	// No one to draft off of? Then you can knock that off.
	if (player->kartstuff[k_draftleeway]) // Prevent small disruptions from stopping your draft.
	{
		player->kartstuff[k_draftleeway]--;
		if (player->kartstuff[k_lastdraft] >= 0
			&& player->kartstuff[k_lastdraft] < MAXPLAYERS
			&& playeringame[player->kartstuff[k_lastdraft]]
			&& !players[player->kartstuff[k_lastdraft]].spectator
			&& players[player->kartstuff[k_lastdraft]].mo)
		{
			player_t *victim = &players[player->kartstuff[k_lastdraft]];
			fixed_t dist = P_AproxDistance(P_AproxDistance(victim->mo->x - player->mo->x, victim->mo->y - player->mo->y), victim->mo->z - player->mo->z);
			K_DrawDraftCombiring(player, victim, dist, draftdistance, true);
		}
	}
	else // Remove draft speed boost.
	{
		player->kartstuff[k_draftpower] = 0;
		player->kartstuff[k_lastdraft] = -1;
	}
}

void K_KartPainEnergyFling(player_t *player)
{
	static const UINT8 numfling = 5;
	INT32 i;
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	fixed_t z;

	// Better safe than sorry.
	if (!player)
		return;

	// P_PlayerRingBurst: "There's no ring spilling in kart, so I'm hijacking this for the same thing as TD"
	// :oh:

	for (i = 0; i < numfling; i++)
	{
		INT32 objType = mobjinfo[MT_FLINGENERGY].reactiontime;
		fixed_t momxy, momz; // base horizonal/vertical thrusts

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[objType].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, objType);

		mo->fuse = 8*TICRATE;
		P_SetTarget(&mo->target, player->mo);

		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle, then slightly offset by amount of fling
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT) - ((numfling-1)*FINEANGLES/32)) & FINEMASK;

		if (i > 15)
		{
			momxy = 3*FRACUNIT;
			momz = 4*FRACUNIT;
		}
		else
		{
			momxy = 28*FRACUNIT;
			momz = 3*FRACUNIT;
		}

		ns = FixedMul(momxy, mo->scale);
		mo->momx = FixedMul(FINECOSINE(fa),ns);

		ns = momz;
		P_SetObjectMomZ(mo, ns, false);

		if (i & 1)
			P_SetObjectMomZ(mo, ns, true);

		if (player->mo->eflags & MFE_VERTICALFLIP)
			mo->momz *= -1;
	}
}

// Adds gravity flipping to an object relative to its master and shifts the z coordinate accordingly.
void K_FlipFromObject(mobj_t *mo, mobj_t *master)
{
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	if (mo->eflags & MFE_VERTICALFLIP)
		mo->z += master->height - FixedMul(master->scale, mo->height);
}

void K_MatchGenericExtraFlags(mobj_t *mo, mobj_t *master)
{
	// flipping
	// handle z shifting from there too. This is here since there's no reason not to flip us if needed when we do this anyway;
	K_FlipFromObject(mo, master);

	// visibility (usually for hyudoro)
	mo->flags2 = (mo->flags2 & ~MF2_DONTDRAW)|(master->flags2 & MF2_DONTDRAW);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP1)|(master->eflags & MFE_DRAWONLYFORP1);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP2)|(master->eflags & MFE_DRAWONLYFORP2);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP3)|(master->eflags & MFE_DRAWONLYFORP3);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP4)|(master->eflags & MFE_DRAWONLYFORP4);
}

// same as above, but does not adjust Z height when flipping
void K_GenericExtraFlagsNoZAdjust(mobj_t *mo, mobj_t *master)
{
	// flipping
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	// visibility (usually for hyudoro)
	mo->flags2 = (mo->flags2 & ~MF2_DONTDRAW)|(master->flags2 & MF2_DONTDRAW);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP1)|(master->eflags & MFE_DRAWONLYFORP1);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP2)|(master->eflags & MFE_DRAWONLYFORP2);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP3)|(master->eflags & MFE_DRAWONLYFORP3);
	mo->eflags = (mo->eflags & ~MFE_DRAWONLYFORP4)|(master->eflags & MFE_DRAWONLYFORP4);
}


void K_SpawnDashDustRelease(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *dust;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo))
		return;

	if (!player->speed && !player->kartstuff[k_startboost])
		return;

	travelangle = player->mo->angle;

	if (player->kartstuff[k_drift] || player->kartstuff[k_driftend])
		travelangle -= (ANGLE_45/5)*player->kartstuff[k_drift];

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		dust = P_SpawnMobj(newx, newy, player->mo->z, MT_FASTDUST);

		P_SetTarget(&dust->target, player->mo);
		dust->angle = travelangle - ((i&1) ? -1 : 1)*ANGLE_45;
		dust->destscale = player->mo->scale;
		P_SetScale(dust, player->mo->scale);

		dust->momx = 3*player->mo->momx/5;
		dust->momy = 3*player->mo->momy/5;
		//dust->momz = 3*player->mo->momz/5;

		K_MatchGenericExtraFlags(dust, player->mo);
	}
}

static void K_SpawnBrakeDriftSparks(player_t *player) // Be sure to update the mobj thinker case too!
{
	mobj_t *sparks;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	// Position & etc are handled in its thinker, and its spawned invisible.
	// This avoids needing to dupe code if we don't need it.
	sparks = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BRAKEDRIFT);
	P_SetTarget(&sparks->target, player->mo);
	P_SetScale(sparks, (sparks->destscale = player->mo->scale));
	K_MatchGenericExtraFlags(sparks, player->mo);
	sparks->flags2 |= MF2_DONTDRAW;
}

/**	\brief Handles the state changing for moving players, moved here to eliminate duplicate code

	\param	player	player data

	\return	void
*/
void K_KartMoveAnimation(player_t *player)
{
	const INT16 minturn = KART_FULLTURN/8;
	SINT8 turndir = 0;

	const fixed_t fastspeed = (K_GetKartSpeed(player, false) * 17) / 20; // 85%
	const fixed_t speedthreshold = player->mo->scale / 8;

	const boolean onground = P_IsObjectOnGround(player->mo);

	ticcmd_t *cmd = &player->cmd;
	const boolean spinningwheels = ((cmd->buttons & BT_ACCELERATE) || (onground && player->speed > 0));

	if (cmd->driftturn < -minturn)
	{
		turndir = -1;
	}
	else if (cmd->driftturn > minturn)
	{
		turndir = 1;
	}

	if (!onground)
	{
		// Only use certain frames in the air, to make it look like your tires are spinning fruitlessly!

		if (player->kartstuff[k_drift] > 0)
		{
			if (!spinningwheels || !(player->mo->state >= &states[S_KART_DRIFT1_L] && player->mo->state <= &states[S_KART_DRIFT2_L]))
			{
				// Neutral drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_L);
			}
		}
		else if (player->kartstuff[k_drift] > 0)
		{
			if (!spinningwheels || !(player->mo->state >= &states[S_KART_DRIFT1_R] && player->mo->state <= &states[S_KART_DRIFT2_R]))
			{
				// Neutral drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_R);
			}
		}
		else
		{
			if ((turndir == -1)
			&& (!spinningwheels || !(player->mo->state >= &states[S_KART_FAST1_R] && player->mo->state <= &states[S_KART_FAST2_R])))
			{
				P_SetPlayerMobjState(player->mo, S_KART_FAST2_R);
			}
			else if ((turndir == 1)
			&& (!spinningwheels || !(player->mo->state >= &states[S_KART_FAST1_L] && player->mo->state <= &states[S_KART_FAST2_L])))
			{
				P_SetPlayerMobjState(player->mo, S_KART_FAST2_L);
			}
			else if ((turndir == 0)
			&& (!spinningwheels || !(player->mo->state >= &states[S_KART_FAST1] && player->mo->state <= &states[S_KART_FAST2])))
			{
				P_SetPlayerMobjState(player->mo, S_KART_FAST2);
			}
		}
	}
	else
	{
		if (player->kartstuff[k_drift] > 0)
		{
			// Drifting LEFT!

			if ((turndir == -1)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_L_OUT] && player->mo->state <= &states[S_KART_DRIFT2_L_OUT]))
			{
				// Right -- outwards drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_L_OUT);
			}
			else if ((turndir == 1)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_L_IN] && player->mo->state <= &states[S_KART_DRIFT4_L_IN]))
			{
				// Left -- inwards drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_L_IN);
			}
			else if ((turndir == 0)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_L] && player->mo->state <= &states[S_KART_DRIFT2_L]))
			{
				// Neutral drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_L);
			}
		}
		else if (player->kartstuff[k_drift] < 0)
		{
			// Drifting RIGHT!

			if ((turndir == -1)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_R_IN] && player->mo->state <= &states[S_KART_DRIFT4_R_IN]))
			{
				// Right -- inwards drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_R_IN);
			}
			else if ((turndir == 1)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_R_OUT] && player->mo->state <= &states[S_KART_DRIFT2_R_OUT]))
			{
				// Left -- outwards drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_R_OUT);
			}
			else if ((turndir == 0)
			&& !(player->mo->state >= &states[S_KART_DRIFT1_R] && player->mo->state <= &states[S_KART_DRIFT2_R]))
			{
				// Neutral drift
				P_SetPlayerMobjState(player->mo, S_KART_DRIFT1_R);
			}
		}
		else
		{
			if (player->speed >= fastspeed && player->speed >= (player->lastspeed - speedthreshold))
			{
				// Going REAL fast!

				if ((turndir == -1)
				&& !(player->mo->state >= &states[S_KART_FAST1_R] && player->mo->state <= &states[S_KART_FAST2_R]))
				{
					P_SetPlayerMobjState(player->mo, S_KART_FAST1_R);
				}
				else if ((turndir == 1)
				&& !(player->mo->state >= &states[S_KART_FAST1_L] && player->mo->state <= &states[S_KART_FAST2_L]))
				{
					P_SetPlayerMobjState(player->mo, S_KART_FAST1_L);
				}
				else if ((turndir == 0)
				&& !(player->mo->state >= &states[S_KART_FAST1] && player->mo->state <= &states[S_KART_FAST2]))
				{
					P_SetPlayerMobjState(player->mo, S_KART_FAST1);
				}
			}
			else
			{
				if (spinningwheels)
				{
					// Drivin' slow.

					if ((turndir == -1)
					&& !(player->mo->state >= &states[S_KART_SLOW1_R] && player->mo->state <= &states[S_KART_SLOW2_R]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_SLOW1_R);
					}
					else if ((turndir == 1)
					&& !(player->mo->state >= &states[S_KART_SLOW1_L] && player->mo->state <= &states[S_KART_SLOW2_L]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_SLOW1_L);
					}
					else if ((turndir == 0)
					&& !(player->mo->state >= &states[S_KART_SLOW1] && player->mo->state <= &states[S_KART_SLOW2]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_SLOW1);
					}
				}
				else
				{
					// Completely still.

					if ((turndir == -1)
					&& !(player->mo->state >= &states[S_KART_STILL1_R] && player->mo->state <= &states[S_KART_STILL2_R]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_STILL1_R);
					}
					else if ((turndir == 1)
					&& !(player->mo->state >= &states[S_KART_STILL1_L] && player->mo->state <= &states[S_KART_STILL2_L]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_STILL1_L);
					}
					else if ((turndir == 0)
					&& !(player->mo->state >= &states[S_KART_STILL1] && player->mo->state <= &states[S_KART_STILL2]))
					{
						P_SetPlayerMobjState(player->mo, S_KART_STILL1);
					}
				}
			}
		}
	}

	// Update lastspeed value -- we use to display slow driving frames instead of fast driving when slowing down.
	player->lastspeed = player->speed;
}

static void K_TauntVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_tauntvoices] = 6*TICRATE;
	player->karthud[khud_voices] = 4*TICRATE;
}

static void K_RegularVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_voices] = 4*TICRATE;

	if (player->karthud[khud_tauntvoices] < 4*TICRATE)
		player->karthud[khud_tauntvoices] = 4*TICRATE;
}

void K_PlayAttackTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kattk1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayBoostTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kbost1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayOvertakeSound(mobj_t *source)
{
	boolean tasteful = (!source->player || !source->player->karthud[khud_voices]);

	if (!G_RaceGametype()) // Only in race
		return;

	// 4 seconds from before race begins, 10 seconds afterwards
	if (leveltime < starttime+(10*TICRATE))
		return;

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kslow);

	if (!tasteful)
		return;

	K_RegularVoiceTimers(source->player);
}

void K_PlayPainSound(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons

	if (cv_kartvoices.value)
		S_StartSound(source, sfx_khurt1 + pick);

	K_RegularVoiceTimers(source->player);
}

void K_PlayHitEmSound(mobj_t *source)
{

	if (source->player->follower)
	{
		follower_t fl = followers[source->player->followerskin];
		source->player->follower->movecount = fl.hitconfirmtime;	// movecount is used to play the hitconfirm animation for followers.
	}

	if (cv_kartvoices.value)
		S_StartSound(source, sfx_khitem);
	else
		S_StartSound(source, sfx_s1c9); // The only lost gameplay functionality with voices disabled

	K_RegularVoiceTimers(source->player);
}

void K_PlayPowerGloatSound(mobj_t *source)
{
	if (cv_kartvoices.value)
		S_StartSound(source, sfx_kgloat);

	K_RegularVoiceTimers(source->player);
}

void K_MomentumToFacing(player_t *player)
{
	angle_t dangle = player->mo->angle - R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);

	if (dangle > ANGLE_180)
		dangle = InvAngle(dangle);

	// If you aren't on the ground or are moving in too different of a direction don't do this
	if (player->mo->eflags & MFE_JUSTHITFLOOR)
		; // Just hit floor ALWAYS redirects
	else if (!P_IsObjectOnGround(player->mo) || dangle > ANGLE_90)
		return;

	P_Thrust(player->mo, player->mo->angle, player->speed - FixedMul(player->speed, player->mo->friction));
	player->mo->momx = FixedMul(player->mo->momx - player->cmomx, player->mo->friction) + player->cmomx;
	player->mo->momy = FixedMul(player->mo->momy - player->cmomy, player->mo->friction) + player->cmomy;
}

boolean K_ApplyOffroad(player_t *player)
{
	if (player->kartstuff[k_invincibilitytimer] || player->kartstuff[k_hyudorotimer] || EITHERSNEAKER(player))
		return false;
	return true;
}

static fixed_t K_FlameShieldDashVar(INT32 val)
{
	// 1 second = 75% + 50% top speed
	return (3*FRACUNIT/4) + (((val * FRACUNIT) / TICRATE) / 2);
}

// sets k_boostpower, k_speedboost, and k_accelboost to whatever we need it to be
static void K_GetKartBoostPower(player_t *player)
{
	fixed_t boostpower = FRACUNIT;
	fixed_t speedboost = 0, accelboost = 0;
	UINT8 numboosts = 0;

	if (player->kartstuff[k_spinouttimer] && player->kartstuff[k_wipeoutslow] == 1) // Slow down after you've been bumped
	{
		player->kartstuff[k_boostpower] = player->kartstuff[k_speedboost] = player->kartstuff[k_accelboost] = 0;
		return;
	}

	// Offroad is separate, it's difficult to factor it in with a variable value anyway.
	if (K_ApplyOffroad(player) && player->kartstuff[k_offroad] >= 0)
		boostpower = FixedDiv(boostpower, FixedMul(player->kartstuff[k_offroad], K_GetKartGameSpeedScalar(gamespeed)) + FRACUNIT);

	if (player->kartstuff[k_bananadrag] > TICRATE)
		boostpower = (4*boostpower)/5;

#define ADDBOOST(s,a) { \
	numboosts++; \
	speedboost += (s) / numboosts; \
	accelboost += (a) / numboosts; \
}

	if (player->kartstuff[k_levelbooster]) // Level boosters
		ADDBOOST(FRACUNIT/2, 8*FRACUNIT); // + 50% top speed, + 800% acceleration

	if (player->kartstuff[k_sneakertimer]) // Sneaker
		ADDBOOST(FRACUNIT/2, 8*FRACUNIT); // + 50% top speed, + 800% acceleration

	if (player->kartstuff[k_invincibilitytimer]) // Invincibility
		ADDBOOST((3*FRACUNIT)/8, 3*FRACUNIT); // + 37.5% top speed, + 300% acceleration

	if (player->kartstuff[k_flamedash]) // Flame Shield dash
		ADDBOOST(K_FlameShieldDashVar(player->kartstuff[k_flamedash]), 3*FRACUNIT); // + infinite top speed, + 300% acceleration

	if (player->kartstuff[k_startboost]) // Startup Boost
		ADDBOOST(FRACUNIT/4, 6*FRACUNIT); // + 25% top speed, + 600% acceleration

	if (player->kartstuff[k_driftboost]) // Drift Boost
		ADDBOOST(FRACUNIT/4, 4*FRACUNIT); // + 25% top speed, + 400% acceleration

	if (player->kartstuff[k_ringboost]) // Ring Boost
		ADDBOOST(FRACUNIT/5, 4*FRACUNIT); // + 20% top speed, + 400% acceleration

	if (player->kartstuff[k_eggmanexplode]) // Ready-to-explode
		ADDBOOST(FRACUNIT/5, FRACUNIT); // + 20% top speed, + 100% acceleration

	if (player->kartstuff[k_draftpower] > 0) // Drafting
	{
		fixed_t draftspeed = ((3*FRACUNIT)/10) + ((player->kartspeed-1) * (FRACUNIT/50)); // min is 30%, max is 46%
		speedboost += FixedMul(draftspeed, player->kartstuff[k_draftpower]); // (Drafting suffers no boost stack penalty.)
		numboosts++;
	}

	player->kartstuff[k_boostpower] = boostpower;

	// value smoothing
	if (speedboost > player->kartstuff[k_speedboost])
		player->kartstuff[k_speedboost] = speedboost;
	else
		player->kartstuff[k_speedboost] += (speedboost - player->kartstuff[k_speedboost]) / (TICRATE/2);

	player->kartstuff[k_accelboost] = accelboost;
	player->kartstuff[k_numboosts] = numboosts;
}

// Returns kart speed from a stat. Boost power and scale are NOT taken into account, no player or object is necessary.
fixed_t K_GetKartSpeedFromStat(UINT8 kartspeed)
{
	const fixed_t xspd = (3*FRACUNIT)/64;
	fixed_t g_cc = K_GetKartGameSpeedScalar(gamespeed) + xspd;
	fixed_t k_speed = 150;
	fixed_t finalspeed;

	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(k_speed<<14, g_cc);
	return finalspeed;
}

fixed_t K_GetKartSpeed(player_t *player, boolean doboostpower)
{
	fixed_t finalspeed;
	UINT8 kartspeed = player->kartspeed;

	if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
		kartspeed = 1;

	finalspeed = K_GetKartSpeedFromStat(kartspeed);

	if (K_PlayerUsesBotMovement(player))
	{
		// Give top speed a buff for bots, since it's a fairly weak stat without drifting
		fixed_t speedmul = ((kartspeed-1) * FRACUNIT / 8) / 10; // +10% for speed 9
		finalspeed = FixedMul(finalspeed, FRACUNIT + speedmul);
	}

	if (player->mo && !P_MobjWasRemoved(player->mo))
		finalspeed = FixedMul(finalspeed, player->mo->scale);

	if (doboostpower)
	{
		if (K_PlayerUsesBotMovement(player))
		{
			finalspeed = FixedMul(finalspeed, K_BotTopSpeedRubberband(player));
		}

		return FixedMul(finalspeed, player->kartstuff[k_boostpower]+player->kartstuff[k_speedboost]);
	}

	return finalspeed;
}

fixed_t K_GetKartAccel(player_t *player)
{
	fixed_t k_accel = 32; // 36;
	UINT8 kartspeed = player->kartspeed;

	if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
		kartspeed = 1;

	//k_accel += 3 * (9 - kartspeed); // 36 - 60
	k_accel += 4 * (9 - kartspeed); // 32 - 64

	if (K_PlayerUsesBotMovement(player))
	{
		k_accel = FixedMul(k_accel, K_BotRubberband(player));
	}

	return FixedMul(k_accel, FRACUNIT+player->kartstuff[k_accelboost]);
}

UINT16 K_GetKartFlashing(player_t *player)
{
	UINT16 tics = flashingtics;

	if (!player)
		return tics;

	if (G_BattleGametype())
		tics *= 2;

	tics += (flashingtics/8) * (player->kartspeed);

	return tics;
}

fixed_t K_3dKartMovement(player_t *player, boolean onground, fixed_t forwardmove)
{
	fixed_t accelmax = 4000;
	fixed_t newspeed, oldspeed, finalspeed;
	fixed_t p_speed = K_GetKartSpeed(player, true);
	fixed_t p_accel = K_GetKartAccel(player);

	if (!onground) return 0; // If the player isn't on the ground, there is no change in speed

	// ACCELCODE!!!1!11!
	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy); // FixedMul(P_AproxDistance(player->rmomx, player->rmomy), player->mo->scale);
	// Don't calculate the acceleration as ever being above top speed
	if (oldspeed > p_speed)
		oldspeed = p_speed;
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), ORIG_FRICTION);

	if (player->kartstuff[k_pogospring]) // Pogo Spring minimum/maximum thrust
	{
		const fixed_t hscale = mapobjectscale /*+ (mapobjectscale - player->mo->scale)*/;
		const fixed_t minspeed = 24*hscale;
		const fixed_t maxspeed = 28*hscale;

		if (newspeed > maxspeed && player->kartstuff[k_pogospring] == 2)
			newspeed = maxspeed;
		if (newspeed < minspeed)
			newspeed = minspeed;
	}

	finalspeed = newspeed - oldspeed;

	// forwardmove is:
	//  50 while accelerating,
	//  25 while clutching,
	//   0 with no gas, and
	// -25 when only braking.

	if (EITHERSNEAKER(player))
		forwardmove = 50;

	finalspeed *= forwardmove/25;
	finalspeed /= 2;

	if (forwardmove < 0 && finalspeed > mapobjectscale*2)
		return finalspeed/2;
	else if (forwardmove < 0)
		return -mapobjectscale/2;

	if (finalspeed < 0)
		finalspeed = 0;

	return finalspeed;
}

void K_DoInstashield(player_t *player)
{
	mobj_t *layera;
	mobj_t *layerb;

	if (player->kartstuff[k_instashield] > 0)
		return;

	player->kartstuff[k_instashield] = 15; // length of instashield animation
	S_StartSound(player->mo, sfx_cdpcm9);

	layera = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDA);
	P_SetTarget(&layera->target, player->mo);

	layerb = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDB);
	P_SetTarget(&layerb->target, player->mo);
}

void K_SpinPlayer(player_t *player, mobj_t *source, INT32 type, mobj_t *inflictor, boolean trapitem)
{
	UINT8 scoremultiply = 1;
	// PS: Inflictor is unused for all purposes here and is actually only ever relevant to Lua. It may be nil too.
#ifdef HAVE_BLUA
	boolean force = false;	// Used to check if Lua ShouldSpin should get us damaged reguardless of flashtics or heck knows what.
	UINT8 shouldForce = LUAh_ShouldSpin(player, inflictor, source);
	if (P_MobjWasRemoved(player->mo))
		return; // mobj was removed (in theory that shouldn't happen)
	if (shouldForce == 1)
		force = true;
	else if (shouldForce == 2)
		return;
#else
	static const boolean force = false;
	(void)inflictor;	// in case some weirdo doesn't want Lua.
#endif

	if (!trapitem && G_BattleGametype())
	{
		if (K_IsPlayerWanted(player))
			scoremultiply = 3;
		else if (player->kartstuff[k_bumper] == 1)
			scoremultiply = 2;
	}

	if (player->health <= 0)
		return;

	if (player->powers[pw_flashing] > 0 || player->kartstuff[k_squishedtimer] > 0 || (player->kartstuff[k_spinouttimer] > 0 && player->kartstuff[k_spinouttype] != 2)
		|| player->kartstuff[k_invincibilitytimer] > 0 || player->kartstuff[k_growshrinktimer] > 0 || player->kartstuff[k_hyudorotimer] > 0
		|| (G_BattleGametype() && ((player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]) || player->kartstuff[k_comebackmode] == 1)))
	{
		if (!force)	// if shoulddamage force, we go THROUGH that.
		{
			K_DoInstashield(player);
			return;
		}
	}

#ifdef HAVE_BLUA
	if (LUAh_PlayerSpin(player, inflictor, source))	// Let Lua do its thing or overwrite if it wants to. Make sure to let any possible instashield happen because we didn't get "damaged" in this case.
		return;
#endif

	if (source && source != player->mo && source->player)
		K_PlayHitEmSound(source);

	player->kartstuff[k_sneakertimer] = 0;
	//player->kartstuff[k_levelbooster] = 0;
	player->kartstuff[k_driftboost] = 0;
	player->kartstuff[k_ringboost] = 0;

	player->kartstuff[k_drift] = 0;
	player->kartstuff[k_driftcharge] = 0;
	player->kartstuff[k_pogospring] = 0;

	if (G_BattleGametype())
	{
		if (source && source->player && player != source->player)
		{
			P_AddPlayerScore(source->player, scoremultiply);
			K_SpawnBattlePoints(source->player, player, scoremultiply);
			if (!trapitem)
			{
				source->player->kartstuff[k_wanted] -= wantedreduce;
				player->kartstuff[k_wanted] -= (wantedreduce/2);
			}
		}

		if (player->kartstuff[k_bumper] > 0)
		{
			if (player->kartstuff[k_bumper] == 1)
			{
				mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX); // Player hitbox is too small!!
				P_SetTarget(&karmahitbox->target, player->mo);
				karmahitbox->destscale = player->mo->scale;
				P_SetScale(karmahitbox, player->mo->scale);
				CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
			}
			player->kartstuff[k_bumper]--;
			if (K_IsPlayerWanted(player))
				K_CalculateBattleWanted();
		}

		if (!player->kartstuff[k_bumper])
		{
			player->kartstuff[k_comebacktimer] = comebacktime;
			if (player->kartstuff[k_comebackmode] == 2)
			{
				mobj_t *poof = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EXPLODE);
				S_StartSound(poof, mobjinfo[MT_KARMAHITBOX].seesound);
				player->kartstuff[k_comebackmode] = 0;
			}
		}

		K_CheckBumpers();
	}

	player->kartstuff[k_spinouttype] = type;

	if (player->kartstuff[k_spinouttype] <= 0) // type 0 is spinout, type 1 is wipeout, type 2 is no-invuln wipeout
	{
		// At spinout, player speed is increased to 1/4 their regular speed, moving them forward
		if (player->speed < K_GetKartSpeed(player, true)/4)
			P_InstaThrust(player->mo, player->mo->angle, FixedMul(K_GetKartSpeed(player, true)/4, player->mo->scale));
		S_StartSound(player->mo, sfx_slip);
	}

	player->kartstuff[k_spinouttimer] = (3*TICRATE/2)+2;
	player->powers[pw_flashing] = K_GetKartFlashing(player);

	P_PlayRinglossSound(player->mo);
	P_PlayerRingBurst(player, 5);
	K_PlayPainSound(player->mo);

	if (player->mo->state != &states[S_KART_SPIN])
		P_SetPlayerMobjState(player->mo, S_KART_SPIN);

	player->kartstuff[k_instashield] = 15;
	if (cv_kartdebughuddrop.value && !modeattacking)
		K_DropItems(player);
	else
		K_DropHnextList(player, false);
	return;
}

static void K_RemoveGrowShrink(player_t *player)
{
	if (player->mo && !P_MobjWasRemoved(player->mo))
	{
		if (player->kartstuff[k_growshrinktimer] > 0) // Play Shrink noise
			S_StartSound(player->mo, sfx_kc59);
		else if (player->kartstuff[k_growshrinktimer] < 0) // Play Grow noise
			S_StartSound(player->mo, sfx_kc5a);

		if (player->kartstuff[k_invincibilitytimer] == 0)
			player->mo->color = player->skincolor;

		player->mo->scalespeed = mapobjectscale/TICRATE;
		player->mo->destscale = mapobjectscale;
		if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	player->kartstuff[k_growshrinktimer] = 0;

	P_RestoreMusic(player);
}

void K_SquishPlayer(player_t *player, mobj_t *source, mobj_t *inflictor)
{
	UINT8 scoremultiply = 1;
	// PS: Inflictor is unused for all purposes here and is actually only ever relevant to Lua. It may be nil too.
#ifdef HAVE_BLUA
	boolean force = false;	// Used to check if Lua ShouldSquish should get us damaged reguardless of flashtics or heck knows what.
	UINT8 shouldForce = LUAh_ShouldSquish(player, inflictor, source);
	if (P_MobjWasRemoved(player->mo))
		return; // mobj was removed (in theory that shouldn't happen)
	if (shouldForce == 1)
		force = true;
	else if (shouldForce == 2)
		return;
#else
	static const boolean force = false;
	(void)inflictor;	// Please stop forgetting to put inflictor in yer functions thank -Lat'
#endif

	if (G_BattleGametype())
	{
		if (K_IsPlayerWanted(player))
			scoremultiply = 3;
		else if (player->kartstuff[k_bumper] == 1)
			scoremultiply = 2;
	}

	if (player->health <= 0)
		return;

	if (player->powers[pw_flashing] > 0 || player->kartstuff[k_squishedtimer] > 0 || player->kartstuff[k_invincibilitytimer] > 0
		|| player->kartstuff[k_growshrinktimer] > 0 || player->kartstuff[k_hyudorotimer] > 0
		|| (G_BattleGametype() && ((player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]) || player->kartstuff[k_comebackmode] == 1)))
	{
		if (!force)	// You know the drill by now.
		{
			K_DoInstashield(player);
			return;
		}
	}

#ifdef HAVE_BLUA
	if (LUAh_PlayerSquish(player, inflictor, source))	// Let Lua do its thing or overwrite if it wants to. Make sure to let any possible instashield happen because we didn't get "damaged" in this case.
		return;
#endif

	player->kartstuff[k_sneakertimer] = 0;
	player->kartstuff[k_levelbooster] = 0;
	player->kartstuff[k_driftboost] = 0;
	player->kartstuff[k_ringboost] = 0;

	player->kartstuff[k_drift] = 0;
	player->kartstuff[k_driftcharge] = 0;
	player->kartstuff[k_pogospring] = 0;

	if (G_BattleGametype())
	{
		if (source && source->player && player != source->player)
		{
			P_AddPlayerScore(source->player, scoremultiply);
			K_SpawnBattlePoints(source->player, player, scoremultiply);
			source->player->kartstuff[k_wanted] -= wantedreduce;
			player->kartstuff[k_wanted] -= (wantedreduce/2);
		}

		if (player->kartstuff[k_bumper] > 0)
		{
			if (player->kartstuff[k_bumper] == 1)
			{
				mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX); // Player hitbox is too small!!
				P_SetTarget(&karmahitbox->target, player->mo);
				karmahitbox->destscale = player->mo->scale;
				P_SetScale(karmahitbox, player->mo->scale);
				CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
			}
			player->kartstuff[k_bumper]--;
			if (K_IsPlayerWanted(player))
				K_CalculateBattleWanted();
		}

		if (!player->kartstuff[k_bumper])
		{
			player->kartstuff[k_comebacktimer] = comebacktime;
			if (player->kartstuff[k_comebackmode] == 2)
			{
				mobj_t *poof = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EXPLODE);
				S_StartSound(poof, mobjinfo[MT_KARMAHITBOX].seesound);
				player->kartstuff[k_comebackmode] = 0;
			}
		}

		K_CheckBumpers();
	}

	player->kartstuff[k_squishedtimer] = TICRATE;

	// Reduce Shrink timer
	if (player->kartstuff[k_growshrinktimer] < 0)
	{
		player->kartstuff[k_growshrinktimer] += TICRATE;
		if (player->kartstuff[k_growshrinktimer] >= 0)
			K_RemoveGrowShrink(player);
	}

	player->powers[pw_flashing] = K_GetKartFlashing(player);

	player->mo->flags |= MF_NOCLIP;

	if (player->mo->state != &states[S_KART_SQUISH]) // Squash
		P_SetPlayerMobjState(player->mo, S_KART_SQUISH);

	P_PlayRinglossSound(player->mo);
	P_PlayerRingBurst(player, 5);
	K_PlayPainSound(player->mo);

	player->kartstuff[k_instashield] = 15;
	if (cv_kartdebughuddrop.value && !modeattacking)
		K_DropItems(player);
	else
		K_DropHnextList(player, false);
	return;
}

void K_ExplodePlayer(player_t *player, mobj_t *source, mobj_t *inflictor) // A bit of a hack, we just throw the player up higher here and extend their spinout timer
{
	UINT8 scoremultiply = 1;
#ifdef HAVE_BLUA
	boolean force = false;	// Used to check if Lua ShouldExplode should get us damaged reguardless of flashtics or heck knows what.
	UINT8 shouldForce = LUAh_ShouldExplode(player, inflictor, source);

	if (P_MobjWasRemoved(player->mo))
		return; // mobj was removed (in theory that shouldn't happen)
	if (shouldForce == 1)
		force = true;
	else if (shouldForce == 2)
		return;

#else
	static const boolean force = false;
#endif

	if (G_BattleGametype())
	{
		if (K_IsPlayerWanted(player))
			scoremultiply = 3;
		else if (player->kartstuff[k_bumper] == 1)
			scoremultiply = 2;
	}

	if (player->health <= 0)
		return;

	if (player->kartstuff[k_invincibilitytimer] > 0 || player->kartstuff[k_growshrinktimer] > 0 || player->kartstuff[k_hyudorotimer] > 0 // Do not check spinout, because SPB and Eggman should combo
		|| (G_BattleGametype() && ((player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]) || player->kartstuff[k_comebackmode] == 1)))
	{
		if (!force)	// ShouldDamage can bypass that, again.
		{
			K_DoInstashield(player);
			return;
		}
	}

#ifdef HAVE_BLUA
	if (LUAh_PlayerExplode(player, inflictor, source))	// Same thing. Also make sure to let Instashield happen blah blah
		return;
#endif

	if (source && source != player->mo && source->player)
		K_PlayHitEmSound(source);

	player->mo->momz = 18*mapobjectscale*P_MobjFlip(player->mo);	// please stop forgetting mobjflip checks!!!!
	player->mo->momx = player->mo->momy = 0;

	player->kartstuff[k_sneakertimer] = 0;
	player->kartstuff[k_levelbooster] = 0;
	player->kartstuff[k_driftboost] = 0;
	player->kartstuff[k_ringboost] = 0;

	player->kartstuff[k_drift] = 0;
	player->kartstuff[k_driftcharge] = 0;
	player->kartstuff[k_pogospring] = 0;

	// This is the only part that SHOULDN'T combo :VVVVV
	if (G_BattleGametype() && !(player->powers[pw_flashing] > 0 || player->kartstuff[k_squishedtimer] > 0 || (player->kartstuff[k_spinouttimer] > 0 && player->kartstuff[k_spinouttype] != 2)))
	{
		if (source && source->player && player != source->player)
		{
			P_AddPlayerScore(source->player, scoremultiply);
			K_SpawnBattlePoints(source->player, player, scoremultiply);
			source->player->kartstuff[k_wanted] -= wantedreduce;
			player->kartstuff[k_wanted] -= (wantedreduce/2);
		}

		if (player->kartstuff[k_bumper] > 0)
		{
			if (player->kartstuff[k_bumper] == 1)
			{
				mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX); // Player hitbox is too small!!
				P_SetTarget(&karmahitbox->target, player->mo);
				karmahitbox->destscale = player->mo->scale;
				P_SetScale(karmahitbox, player->mo->scale);
				CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
			}
			player->kartstuff[k_bumper]--;
			if (K_IsPlayerWanted(player))
				K_CalculateBattleWanted();
		}

		if (!player->kartstuff[k_bumper])
		{
			player->kartstuff[k_comebacktimer] = comebacktime;
			if (player->kartstuff[k_comebackmode] == 2)
			{
				mobj_t *poof = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EXPLODE);
				S_StartSound(poof, mobjinfo[MT_KARMAHITBOX].seesound);
				player->kartstuff[k_comebackmode] = 0;
			}
		}

		K_CheckBumpers();
	}

	player->kartstuff[k_spinouttype] = 1;
	player->kartstuff[k_spinouttimer] = (3*TICRATE/2)+2;

	player->powers[pw_flashing] = K_GetKartFlashing(player);

	if (inflictor && inflictor->type == MT_SPBEXPLOSION && inflictor->extravalue1)
	{
		player->kartstuff[k_spinouttimer] = ((5*player->kartstuff[k_spinouttimer])/2)+1;
		player->mo->momz *= 2;
	}

	if (player->mo->eflags & MFE_UNDERWATER)
		player->mo->momz = (117 * player->mo->momz) / 200;

	if (player->mo->state != &states[S_KART_SPIN])
		P_SetPlayerMobjState(player->mo, S_KART_SPIN);

	P_PlayRinglossSound(player->mo);
	P_PlayerRingBurst(player, 5);
	K_PlayPainSound(player->mo);

	if (P_IsDisplayPlayer(player))
		P_StartQuake(64<<FRACBITS, 5);

	player->kartstuff[k_instashield] = 15;
	K_DropItems(player);

	return;
}

void K_StealBumper(player_t *player, player_t *victim, boolean force)
{
	INT32 newbumper;
	angle_t newangle, diff;
	fixed_t newx, newy;
	mobj_t *newmo;

	if (!G_BattleGametype())
		return;

	if (player->health <= 0 || victim->health <= 0)
		return;

	if (!force)
	{
		if (victim->kartstuff[k_bumper] <= 0) // || player->kartstuff[k_bumper] >= K_StartingBumperCount()+2
			return;

		if (player->kartstuff[k_squishedtimer] > 0 || player->kartstuff[k_spinouttimer] > 0)
			return;

		if (victim->powers[pw_flashing] > 0 || victim->kartstuff[k_squishedtimer] > 0 || (victim->kartstuff[k_spinouttimer] > 0 && victim->kartstuff[k_spinouttype] != 2)
			|| victim->kartstuff[k_invincibilitytimer] > 0 || victim->kartstuff[k_growshrinktimer] > 0 || victim->kartstuff[k_hyudorotimer] > 0)
		{
			K_DoInstashield(victim);
			return;
		}
	}

	if (netgame && player->kartstuff[k_bumper] <= 0)
		CONS_Printf(M_GetText("%s is back in the game!\n"), player_names[player-players]);

	newbumper = player->kartstuff[k_bumper];
	if (newbumper <= 1)
		diff = 0;
	else
		diff = FixedAngle(360*FRACUNIT/newbumper);

	newangle = player->mo->angle;
	newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, 64*FRACUNIT);
	newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, 64*FRACUNIT);

	newmo = P_SpawnMobj(newx, newy, player->mo->z, MT_BATTLEBUMPER);
	newmo->threshold = newbumper;
	P_SetTarget(&newmo->tracer, victim->mo);
	P_SetTarget(&newmo->target, player->mo);
	newmo->angle = (diff * (newbumper-1));
	newmo->color = victim->skincolor;

	if (newbumper+1 < 2)
		P_SetMobjState(newmo, S_BATTLEBUMPER3);
	else if (newbumper+1 < 3)
		P_SetMobjState(newmo, S_BATTLEBUMPER2);
	else
		P_SetMobjState(newmo, S_BATTLEBUMPER1);

	S_StartSound(player->mo, sfx_3db06);

	player->kartstuff[k_bumper]++;
	player->kartstuff[k_comebackpoints] = 0;
	player->powers[pw_flashing] = K_GetKartFlashing(player);
	player->kartstuff[k_comebacktimer] = comebacktime;

	/*victim->powers[pw_flashing] = K_GetKartFlashing(victim);
	victim->kartstuff[k_comebacktimer] = comebacktime;*/

	victim->kartstuff[k_instashield] = 15;
	if (cv_kartdebughuddrop.value && !modeattacking)
		K_DropItems(victim);
	else
		K_DropHnextList(victim, false);
	return;
}

// source is the mobj that originally threw the bomb that exploded etc.
// Spawns the sphere around the explosion that handles spinout
void K_SpawnKartExplosion(fixed_t x, fixed_t y, fixed_t z, fixed_t radius, INT32 number, mobjtype_t type, angle_t rotangle, boolean spawncenter, boolean ghostit, mobj_t *source)
{
	mobj_t *mobj;
	mobj_t *ghost = NULL;
	INT32 i;
	TVector v;
	TVector *res;
	fixed_t finalx, finaly, finalz, dist;
	//mobj_t hoopcenter;
	angle_t degrees, fa, closestangle;
	fixed_t mobjx, mobjy, mobjz;

	//hoopcenter.x = x;
	//hoopcenter.y = y;
	//hoopcenter.z = z;

	//hoopcenter.z = z - mobjinfo[type].height/2;

	degrees = FINEANGLES/number;

	closestangle = 0;

	// Create the hoop!
	for (i = 0; i < number; i++)
	{
		fa = (i*degrees);
		v[0] = FixedMul(FINECOSINE(fa),radius);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa),radius);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *RotateXMatrix(rotangle));
		M_Memcpy(&v, res, sizeof (v));
		res = VectorMatrixMultiply(v, *RotateZMatrix(closestangle));
		M_Memcpy(&v, res, sizeof (v));

		finalx = x + v[0];
		finaly = y + v[1];
		finalz = z + v[2];

		mobj = P_SpawnMobj(finalx, finaly, finalz, type);

		mobj->z -= mobj->height>>1;

		// change angle
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, x, y);

		// change slope
		dist = P_AproxDistance(P_AproxDistance(x - mobj->x, y - mobj->y), z - mobj->z);

		if (dist < 1)
			dist = 1;

		mobjx = mobj->x;
		mobjy = mobj->y;
		mobjz = mobj->z;

		if (ghostit)
		{
			ghost = P_SpawnGhostMobj(mobj);
			P_SetMobjState(mobj, S_NULL);
			mobj = ghost;
		}

		if (spawncenter)
		{
			mobj->x = x;
			mobj->y = y;
			mobj->z = z;
		}

		mobj->momx = FixedMul(FixedDiv(mobjx - x, dist), FixedDiv(dist, 6*FRACUNIT));
		mobj->momy = FixedMul(FixedDiv(mobjy - y, dist), FixedDiv(dist, 6*FRACUNIT));
		mobj->momz = FixedMul(FixedDiv(mobjz - z, dist), FixedDiv(dist, 6*FRACUNIT));

		if (source && !P_MobjWasRemoved(source))
			P_SetTarget(&mobj->target, source);
	}
}

#define MINEQUAKEDIST 4096

// Spawns the purely visual explosion
void K_SpawnMineExplosion(mobj_t *source, UINT8 color)
{
	INT32 i, radius, height;
	mobj_t *smoldering = P_SpawnMobj(source->x, source->y, source->z, MT_SMOLDERING);
	mobj_t *dust;
	mobj_t *truc;
	INT32 speed, speed2;
	INT32 pnum;
	player_t *p;

	// check for potential display players near the source so we can have a sick earthquake / flashpal.
	for (pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		p = &players[pnum];

		if (!playeringame[pnum] || !P_IsDisplayPlayer(p))
			continue;

		if (R_PointToDist2(p->mo->x, p->mo->y, source->x, source->y) < mapobjectscale*MINEQUAKEDIST)
		{
			P_StartQuake(55<<FRACBITS, 12);
			if (!bombflashtimer && P_CheckSight(p->mo, source))
			{
				bombflashtimer = TICRATE*2;
				P_FlashPal(p, 1, 1);
			}
			break;	// we can break right now because quakes are global to all split players somehow.
		}
	}

	K_MatchGenericExtraFlags(smoldering, source);
	smoldering->tics = TICRATE*3;
	radius = source->radius>>FRACBITS;
	height = source->height>>FRACBITS;

	if (!color)
		color = SKINCOLOR_KETCHUP;

	for (i = 0; i < 32; i++)
	{
		dust = P_SpawnMobj(source->x, source->y, source->z, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		dust->angle = (ANGLE_180/16) * i;
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		P_InstaThrust(dust, dust->angle, FixedMul(20*FRACUNIT, source->scale));

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMEXPLODE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*6;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(10*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(-speed, speed)*FRACUNIT*P_MobjFlip(truc);
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->color = color;
	}

	for (i = 0; i < 16; i++)
	{
		dust = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		dust->tics = 30;
		dust->momz = P_RandomRange(FixedMul(3*FRACUNIT, source->scale)>>FRACBITS, FixedMul(7*FRACUNIT, source->scale)>>FRACBITS)*FRACUNIT;

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMPARTICLE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*5;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(15*FRACUNIT, source->scale)>>FRACBITS;
		speed2 = FixedMul(45*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(speed, speed2)*FRACUNIT*P_MobjFlip(truc);
		if (P_RandomChance(FRACUNIT/2))
			truc->momz = -truc->momz;
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->tics = TICRATE*2;
		truc->color = color;
	}
}

#undef MINEQUAKEDIST

static mobj_t *K_SpawnKartMissile(mobj_t *source, mobjtype_t type, angle_t an, INT32 flags2, fixed_t speed)
{
	mobj_t *th;
	fixed_t x, y, z;
	fixed_t finalspeed = speed;
	mobj_t *throwmo;

	if (source->player && source->player->speed > K_GetKartSpeed(source->player, false))
	{
		angle_t input = source->angle - an;
		boolean invert = (input > ANGLE_180);
		if (invert)
			input = InvAngle(input);

		finalspeed = max(speed, FixedMul(speed, FixedMul(
			FixedDiv(source->player->speed, K_GetKartSpeed(source->player, false)), // Multiply speed to be proportional to your own, boosted maxspeed.
			(((180<<FRACBITS) - AngleFixed(input)) / 180) // multiply speed based on angle diff... i.e: don't do this for firing backward :V
			)));
	}

	x = source->x + source->momx + FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	y = source->y + source->momy + FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));
	z = source->z; // spawn on the ground please

	if (P_MobjFlip(source) < 0)
	{
		z = source->z+source->height - mobjinfo[type].height;
	}

	th = P_SpawnMobj(x, y, z, type);

	th->flags2 |= flags2;
	th->threshold = 10;

	if (th->info->seesound)
		S_StartSound(source, th->info->seesound);

	P_SetTarget(&th->target, source);

	P_SetScale(th, source->scale);
	th->destscale = source->destscale;

	if (P_IsObjectOnGround(source))
	{
		// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
		// This should set it for FOFs
		P_TeleportMove(th, th->x, th->y, th->z);
		// spawn on the ground if the player is on the ground
		if (P_MobjFlip(source) < 0)
		{
			th->z = th->ceilingz - th->height;
			th->eflags |= MFE_VERTICALFLIP;
		}
		else
			th->z = th->floorz;
	}

	th->angle = an;
	th->momx = FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	th->momy = FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));

	switch (type)
	{
		case MT_ORBINAUT:
			if (source && source->player)
				th->color = source->player->skincolor;
			else
				th->color = SKINCOLOR_GREY;
			th->movefactor = finalspeed;
			break;
		case MT_JAWZ:
			if (source && source->player)
			{
				INT32 lasttarg = source->player->kartstuff[k_lastjawztarget];
				th->cvmem = source->player->skincolor;
				if ((lasttarg >= 0 && lasttarg < MAXPLAYERS)
					&& playeringame[lasttarg]
					&& !players[lasttarg].spectator
					&& players[lasttarg].mo)
				{
					P_SetTarget(&th->tracer, players[lasttarg].mo);
				}
			}
			else
				th->cvmem = SKINCOLOR_KETCHUP;
			/* FALLTHRU */
		case MT_JAWZ_DUD:
			S_StartSound(th, th->info->activesound);
			/* FALLTHRU */
		case MT_SPB:
			th->movefactor = finalspeed;
			break;
		case MT_BUBBLESHIELDTRAP:
			P_SetScale(th, ((5*th->destscale)>>2)*4);
			th->destscale = (5*th->destscale)>>2;
			S_StartSound(th, sfx_s3kbfl);
			S_StartSound(th, sfx_cdfm35);
			break;
		default:
			break;
	}

	if (type != MT_BUBBLESHIELDTRAP)
	{
		x = x + P_ReturnThrustX(source, an, source->radius + th->radius);
		y = y + P_ReturnThrustY(source, an, source->radius + th->radius);
		throwmo = P_SpawnMobj(x, y, z, MT_FIREDITEM);
		throwmo->movecount = 1;
		throwmo->movedir = source->angle - an;
		P_SetTarget(&throwmo->target, source);
	}

	return NULL;
}

UINT8 K_DriftSparkColor(player_t *player, INT32 charge)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	UINT8 color = SKINCOLOR_NONE;

	if (charge < 0)
	{
		// Stage 0: Yellow
		color = SKINCOLOR_GOLD;
	}
	else if (charge >= ds*4)
	{
		// Stage 3: Rainbow
		if (charge <= (ds*4)+(32*3))
		{
			// transition
			color = SKINCOLOR_SILVER;
		}
		else
		{
			color = (UINT8)(1 + (leveltime % (MAXSKINCOLORS-1)));
		}
	}
	else if (charge >= ds*2)
	{
		// Stage 2: Blue
		if (charge <= (ds*2)+(32*3))
		{
			// transition
			color = SKINCOLOR_PURPLE;
		}
		else
		{
			color = SKINCOLOR_SAPPHIRE;
		}
	}
	else if (charge >= ds)
	{
		// Stage 1: Red
		if (charge <= (ds)+(32*3))
		{
			// transition
			color = SKINCOLOR_TANGERINE;
		}
		else
		{
			color = SKINCOLOR_KETCHUP;
		}
	}

	return color;
}

static void K_SpawnDriftSparks(player_t *player)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!player->kartstuff[k_drift]
		|| (player->kartstuff[k_driftcharge] < ds && !(player->kartstuff[k_driftcharge] < 0)))
		return;

	travelangle = player->mo->angle-(ANGLE_45/5)*player->kartstuff[k_drift];

	for (i = 0; i < 2; i++)
	{
		SINT8 size = 1;
		UINT8 trail = 0;

		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_DRIFTSPARK);

		P_SetTarget(&spark->target, player->mo);
		spark->angle = travelangle-(ANGLE_45/5)*player->kartstuff[k_drift];
		spark->destscale = player->mo->scale;
		P_SetScale(spark, player->mo->scale);

		spark->momx = player->mo->momx/2;
		spark->momy = player->mo->momy/2;
		//spark->momz = player->mo->momz/2;

		spark->color = K_DriftSparkColor(player, player->kartstuff[k_driftcharge]);

		if (player->kartstuff[k_driftcharge] < 0)
		{
			// Stage 0: Yellow
			size = 0;
		}
		else if (player->kartstuff[k_driftcharge] >= ds*4)
		{
			// Stage 3: Rainbow
			size = 2;
			trail = 2;

			if (player->kartstuff[k_driftcharge] <= (ds*4)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
			else
			{
				spark->colorized = true;
			}
		}
		else if (player->kartstuff[k_driftcharge] >= ds*2)
		{
			// Stage 2: Blue
			size = 2;
			trail = 1;

			if (player->kartstuff[k_driftcharge] <= (ds*2)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
		}
		else
		{
			// Stage 1: Red
			size = 1;

			if (player->kartstuff[k_driftcharge] <= (ds)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*2));
			}
		}

		if ((player->kartstuff[k_drift] > 0 && player->cmd.driftturn > 0) // Inward drifts
			|| (player->kartstuff[k_drift] < 0 && player->cmd.driftturn < 0))
		{
			if ((player->kartstuff[k_drift] < 0 && (i & 1))
				|| (player->kartstuff[k_drift] > 0 && !(i & 1)))
			{
				size++;
			}
			else if ((player->kartstuff[k_drift] < 0 && !(i & 1))
				|| (player->kartstuff[k_drift] > 0 && (i & 1)))
			{
				size--;
			}
		}
		else if ((player->kartstuff[k_drift] > 0 && player->cmd.driftturn < 0) // Outward drifts
			|| (player->kartstuff[k_drift] < 0 && player->cmd.driftturn > 0))
		{
			if ((player->kartstuff[k_drift] < 0 && (i & 1))
				|| (player->kartstuff[k_drift] > 0 && !(i & 1)))
			{
				size--;
			}
			else if ((player->kartstuff[k_drift] < 0 && !(i & 1))
				|| (player->kartstuff[k_drift] > 0 && (i & 1)))
			{
				size++;
			}
		}

		if (size == 2)
			P_SetMobjState(spark, S_DRIFTSPARK_A1);
		else if (size < 1)
			P_SetMobjState(spark, S_DRIFTSPARK_C1);
		else if (size > 2)
			P_SetMobjState(spark, S_DRIFTSPARK_D1);

		if (trail > 0)
			spark->tics += trail;

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

static void K_SpawnAIZDust(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!P_IsObjectOnGround(player->mo))
		return;

	travelangle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
	//S_StartSound(player->mo, sfx_s3k47);

	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle - (player->kartstuff[k_aizdriftstrat]*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle - (player->kartstuff[k_aizdriftstrat]*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_AIZDRIFTSTRAT);

		spark->angle = travelangle+(player->kartstuff[k_aizdriftstrat]*ANGLE_90);
		P_SetScale(spark, (spark->destscale = (3*player->mo->scale)>>2));

		spark->momx = (6*player->mo->momx)/5;
		spark->momy = (6*player->mo->momy)/5;
		//spark->momz = player->mo->momz/2;

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

void K_SpawnBoostTrail(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	fixed_t ground;
	mobj_t *flame;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo)
		|| player->kartstuff[k_hyudorotimer] != 0
		|| (G_BattleGametype() && player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]))
		return;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		ground = player->mo->ceilingz - FixedMul(mobjinfo[MT_SNEAKERTRAIL].height, player->mo->scale);
	else
		ground = player->mo->floorz;

	if (player->kartstuff[k_drift] != 0)
		travelangle = player->mo->angle;
	else
		travelangle = R_PointToAngle2(0, 0, player->rmomx, player->rmomy);

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
#ifdef ESLOPE
		if (player->mo->standingslope)
		{
			ground = P_GetZAt(player->mo->standingslope, newx, newy);
			if (player->mo->eflags & MFE_VERTICALFLIP)
				ground -= FixedMul(mobjinfo[MT_SNEAKERTRAIL].height, player->mo->scale);
		}
#endif
		flame = P_SpawnMobj(newx, newy, ground, MT_SNEAKERTRAIL);

		P_SetTarget(&flame->target, player->mo);
		flame->angle = travelangle;
		flame->fuse = TICRATE*2;
		flame->destscale = player->mo->scale;
		P_SetScale(flame, player->mo->scale);
		// not K_MatchGenericExtraFlags so that a stolen sneaker can be seen
		K_FlipFromObject(flame, player->mo);

		flame->momx = 8;
		P_XYMovement(flame);
		if (P_MobjWasRemoved(flame))
			continue;

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			if (flame->z + flame->height < flame->ceilingz)
				P_RemoveMobj(flame);
		}
		else if (flame->z > flame->floorz)
			P_RemoveMobj(flame);
	}
}

void K_SpawnSparkleTrail(mobj_t *mo)
{
	const INT32 rad = (mo->radius*2)>>FRACBITS;
	mobj_t *sparkle;
	INT32 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 0; i < 3; i++)
	{
		fixed_t newx = mo->x + mo->momx + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newy = mo->y + mo->momy + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newz = mo->z + mo->momz + (P_RandomRange(0, mo->height>>FRACBITS)<<FRACBITS);

		sparkle = P_SpawnMobj(newx, newy, newz, MT_SPARKLETRAIL);
		K_FlipFromObject(sparkle, mo);

		//if (i == 0)
			//P_SetMobjState(sparkle, S_KARTINVULN_LARGE1);

		P_SetTarget(&sparkle->target, mo);
		sparkle->destscale = mo->destscale;
		P_SetScale(sparkle, mo->scale);
		sparkle->color = mo->color;
		//sparkle->colorized = mo->colorized;
	}

	P_SetMobjState(sparkle, S_KARTINVULN_LARGE1);
}

void K_SpawnWipeoutTrail(mobj_t *mo, boolean translucent)
{
	mobj_t *dust;
	angle_t aoff;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (mo->player)
		aoff = (mo->player->frameangle + ANGLE_180);
	else
		aoff = (mo->angle + ANGLE_180);

	if ((leveltime / 2) & 1)
		aoff -= ANGLE_45;
	else
		aoff += ANGLE_45;

	dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->z, MT_WIPEOUTTRAIL);

	P_SetTarget(&dust->target, mo);
	dust->angle = R_PointToAngle2(0,0,mo->momx,mo->momy);
	dust->destscale = mo->scale;
	P_SetScale(dust, mo->scale);
	K_FlipFromObject(dust, mo);

	if (translucent) // offroad effect
	{
		dust->momx = mo->momx/2;
		dust->momy = mo->momy/2;
		dust->momz = mo->momz/2;
	}

	if (translucent)
		dust->flags2 |= MF2_SHADOW;
}

void K_SpawnDraftDust(mobj_t *mo)
{
	UINT8 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 0; i < 2; i++)
	{
		angle_t ang, aoff;
		SINT8 sign = 1;
		UINT8 foff = 0;
		mobj_t *dust;
		boolean drifting = false;

		if (mo->player)
		{
			UINT8 leniency = (3*TICRATE)/4 + ((mo->player->kartweight-1) * (TICRATE/4));

			ang = mo->player->frameangle;

			if (mo->player->kartstuff[k_drift] != 0)
			{
				drifting = true;
				ang += (mo->player->kartstuff[k_drift] * ((ANGLE_270 + ANGLE_22h) / 5)); // -112.5 doesn't work. I fucking HATE SRB2 angles
				if (mo->player->kartstuff[k_drift] < 0)
					sign = 1;
				else
					sign = -1;
			}

			foff = 5 - ((mo->player->kartstuff[k_draftleeway] * 5) / leniency);

			// this shouldn't happen
			if (foff > 4)
				foff = 4;
		}
		else
			ang = mo->angle;

		if (!drifting)
		{
			if (i & 1)
				sign = -1;
			else
				sign = 1;
		}

		aoff = (ang + ANGLE_180) + (ANGLE_45 * sign);

		dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)),
			mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)),
			mo->z, MT_DRAFTDUST);

		P_SetMobjState(dust, S_DRAFTDUST1 + foff);

		P_SetTarget(&dust->target, mo);
		dust->angle = ang - (ANGLE_90 * sign); // point completely perpendicular from the player
		dust->destscale = mo->scale;
		P_SetScale(dust, mo->scale);
		K_FlipFromObject(dust, mo);

		if (leveltime & 1)
			dust->tics++; // "randomize" animation

		dust->momx = (4*mo->momx)/5;
		dust->momy = (4*mo->momy)/5;
		//dust->momz = (4*mo->momz)/5;

		P_Thrust(dust, dust->angle, 4*mo->scale);

		if (drifting) // only 1 trail while drifting
			break;
	}
}

//	K_DriftDustHandling
//	Parameters:
//		spawner: The map object that is spawning the drift dust
//	Description: Spawns the drift dust for objects, players use rmomx/y, other objects use regular momx/y.
//		Also plays the drift sound.
//		Other objects should be angled towards where they're trying to go so they don't randomly spawn dust
//		Do note that most of the function won't run in odd intervals of frames
void K_DriftDustHandling(mobj_t *spawner)
{
	angle_t anglediff;
	const INT16 spawnrange = spawner->radius >> FRACBITS;

	if (!P_IsObjectOnGround(spawner) || leveltime % 2 != 0)
		return;

	if (spawner->player)
	{
		if (spawner->player->pflags & PF_SKIDDOWN)
		{
			anglediff = abs((signed)(spawner->angle - spawner->player->frameangle));
			if (leveltime % 6 == 0)
				S_StartSound(spawner, sfx_screec); // repeated here because it doesn't always happen to be within the range when this is the case
		}
		else
		{
			angle_t playerangle = spawner->angle;

			if (spawner->player->speed < 5*spawner->scale)
				return;

			if (spawner->player->cmd.forwardmove < 0)
				playerangle += ANGLE_180;

			anglediff = abs((signed)(playerangle - R_PointToAngle2(0, 0, spawner->player->rmomx, spawner->player->rmomy)));
		}
	}
	else
	{
		if (P_AproxDistance(spawner->momx, spawner->momy) < 5*spawner->scale)
			return;

		anglediff = abs((signed)(spawner->angle - R_PointToAngle2(0, 0, spawner->momx, spawner->momy)));
	}

	if (anglediff > ANGLE_180)
		anglediff = InvAngle(anglediff);

	if (anglediff > ANG10*4) // Trying to turn further than 40 degrees
	{
		fixed_t spawnx = P_RandomRange(-spawnrange, spawnrange) << FRACBITS;
		fixed_t spawny = P_RandomRange(-spawnrange, spawnrange) << FRACBITS;
		INT32 speedrange = 2;
		mobj_t *dust = P_SpawnMobj(spawner->x + spawnx, spawner->y + spawny, spawner->z, MT_DRIFTDUST);
		dust->momx = FixedMul(spawner->momx + (P_RandomRange(-speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momy = FixedMul(spawner->momy + (P_RandomRange(-speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momz = P_MobjFlip(spawner) * (P_RandomRange(1, 4) * (spawner->scale));
		P_SetScale(dust, spawner->scale/2);
		dust->destscale = spawner->scale * 3;
		dust->scalespeed = spawner->scale/12;

		if (leveltime % 6 == 0)
			S_StartSound(spawner, sfx_screec);

		K_MatchGenericExtraFlags(dust, spawner);

		// Sparkle-y warning for when you're about to change drift sparks!
		if (spawner->player && spawner->player->kartstuff[k_drift])
		{
			INT32 driftval = K_GetKartDriftSparkValue(spawner->player);
			INT32 warntime = driftval/3;
			INT32 dc = spawner->player->kartstuff[k_driftcharge];
			UINT8 c = SKINCOLOR_NONE;
			boolean rainbow = false;

			if (dc >= 0)
			{
				dc += warntime;
			}

			c = K_DriftSparkColor(spawner->player, dc);

			if (dc > (4*driftval)+(32*3))
			{
				rainbow = true;
			}

			if (c != SKINCOLOR_NONE)
			{
				P_SetMobjState(dust, S_DRIFTWARNSPARK1);
				dust->color = c;
				dust->colorized = rainbow;
			}
		}
	}
}

static mobj_t *K_FindLastTrailMobj(player_t *player)
{
	mobj_t *trail;

	if (!player || !(trail = player->mo) || !player->mo->hnext || !player->mo->hnext->health)
		return NULL;

	while (trail->hnext && !P_MobjWasRemoved(trail->hnext) && trail->hnext->health)
	{
		trail = trail->hnext;
	}

	return trail;
}

static mobj_t *K_ThrowKartItem(player_t *player, boolean missile, mobjtype_t mapthing, INT32 defaultDir, INT32 altthrow)
{
	mobj_t *mo;
	INT32 dir;
	fixed_t PROJSPEED;
	angle_t newangle;
	fixed_t newx, newy, newz;
	mobj_t *throwmo;

	if (!player)
		return NULL;

	// Figure out projectile speed by game speed
	if (missile)
	{
		// Use info->speed for missiles
		PROJSPEED = FixedMul(mobjinfo[mapthing].speed, K_GetKartGameSpeedScalar(gamespeed));
	}
	else
	{
		// Use pre-determined speed for tossing
		PROJSPEED = FixedMul(82 << FRACBITS, K_GetKartGameSpeedScalar(gamespeed));
	}

	// Scale to map size
	PROJSPEED = FixedMul(PROJSPEED, mapobjectscale);

	if (altthrow)
	{
		if (altthrow == 2) // Kitchen sink throwing
		{
#if 0
			if (player->kartstuff[k_throwdir] == 1)
				dir = 3;
			else if (player->kartstuff[k_throwdir] == -1)
				dir = 1;
			else
				dir = 2;
#else
			if (player->kartstuff[k_throwdir] == 1)
				dir = 2;
			else
				dir = 1;
#endif
		}
		else
		{
			if (player->kartstuff[k_throwdir] == 1)
				dir = 2;
			else if (player->kartstuff[k_throwdir] == -1)
				dir = -1;
			else
				dir = 1;
		}
	}
	else
	{
		if (player->kartstuff[k_throwdir] != 0)
			dir = player->kartstuff[k_throwdir];
		else
			dir = defaultDir;
	}

	if (missile) // Shootables
	{
		if (mapthing == MT_BALLHOG) // Messy
		{
			if (dir == -1)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x06000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x06000000, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x06000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x06000000, 0, PROJSPEED);
			}
		}
		else
		{
			if (dir == -1 && mapthing != MT_SPB)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
			}
		}
	}
	else
	{
		player->kartstuff[k_bananadrag] = 0; // RESET timer, for multiple bananas

		if (dir > 0)
		{
			// Shoot forward
			mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, mapthing);
			//K_FlipFromObject(mo, player->mo);
			// These are really weird so let's make it a very specific case to make SURE it works...
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				mo->z -= player->mo->height;
				mo->flags2 |= MF2_OBJECTFLIP;
				mo->eflags |= MFE_VERTICALFLIP;
			}

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			S_StartSound(player->mo, mo->info->seesound);

			if (mo)
			{
				angle_t fa = player->mo->angle>>ANGLETOFINESHIFT;
				fixed_t HEIGHT = (20 + (dir*10))*FRACUNIT + (player->mo->momz*P_MobjFlip(player->mo));

				P_SetObjectMomZ(mo, HEIGHT, false);
				mo->momx = player->mo->momx + FixedMul(FINECOSINE(fa), PROJSPEED*dir);
				mo->momy = player->mo->momy + FixedMul(FINESINE(fa), PROJSPEED*dir);

				mo->extravalue2 = dir;

				if (mo->eflags & MFE_UNDERWATER)
					mo->momz = (117 * mo->momz) / 200;
			}

			// this is the small graphic effect that plops in you when you throw an item:
			throwmo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FIREDITEM);
			P_SetTarget(&throwmo->target, player->mo);
			// Ditto:
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				throwmo->z -= player->mo->height;
				throwmo->flags2 |= MF2_OBJECTFLIP;
				throwmo->eflags |= MFE_VERTICALFLIP;
			}

			throwmo->movecount = 0; // above player
		}
		else
		{
			mobj_t *lasttrail = K_FindLastTrailMobj(player);

			if (mapthing == MT_BUBBLESHIELDTRAP) // Drop directly on top of you.
			{
				newangle = player->mo->angle;
				newx = player->mo->x + player->mo->momx;
				newy = player->mo->y + player->mo->momy;
				newz = player->mo->z;
			}
			else if (lasttrail)
			{
				newangle = lasttrail->angle;
				newx = lasttrail->x;
				newy = lasttrail->y;
				newz = lasttrail->z;
			}
			else
			{
				// Drop it directly behind you.
				fixed_t dropradius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(mobjinfo[mapthing].radius, mobjinfo[mapthing].radius);

				newangle = player->mo->angle;

				newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, dropradius);
				newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, dropradius);
				newz = player->mo->z;
			}

			mo = P_SpawnMobj(newx, newy, newz, mapthing); // this will never return null because collision isn't processed here
			K_FlipFromObject(mo, player->mo);

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			P_SetScale(mo, player->mo->scale);
			mo->destscale = player->mo->destscale;

			if (P_IsObjectOnGround(player->mo))
			{
				// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
				// This should set it for FOFs
				P_TeleportMove(mo, mo->x, mo->y, mo->z); // however, THIS can fuck up your day. just absolutely ruin you.
				if (P_MobjWasRemoved(mo))
					return NULL;

				if (P_MobjFlip(mo) > 0)
				{
					if (mo->floorz > mo->target->z - mo->height)
					{
						mo->z = mo->floorz;
					}
				}
				else
				{
					if (mo->ceilingz < mo->target->z + mo->target->height + mo->height)
					{
						mo->z = mo->ceilingz - mo->height;
					}
				}
			}

			if (player->mo->eflags & MFE_VERTICALFLIP)
				mo->eflags |= MFE_VERTICALFLIP;

			if (mapthing == MT_SSMINE)
				mo->extravalue1 = 49; // Pads the start-up length from 21 frames to a full 2 seconds
			else if (mapthing == MT_BUBBLESHIELDTRAP)
			{
				P_SetScale(mo, ((5*mo->destscale)>>2)*4);
				mo->destscale = (5*mo->destscale)>>2;
				S_StartSound(mo, sfx_s3kbfl);
			}
		}
	}

	return mo;
}

void K_PuntMine(mobj_t *thismine, mobj_t *punter)
{
	angle_t fa = R_PointToAngle2(0, 0, punter->momx, punter->momy) >> ANGLETOFINESHIFT;
	fixed_t z = 30*mapobjectscale + punter->momz;
	fixed_t spd;
	mobj_t *mine;

	if (!thismine || P_MobjWasRemoved(thismine))
		return;

	if (thismine->type == MT_SSMINE_SHIELD) // Create a new mine
	{
		mine = P_SpawnMobj(thismine->x, thismine->y, thismine->z, MT_SSMINE);
		P_SetTarget(&mine->target, thismine->target);
		mine->angle = thismine->angle;
		mine->flags2 = thismine->flags2;
		mine->floorz = thismine->floorz;
		mine->ceilingz = thismine->ceilingz;
		P_RemoveMobj(thismine);
	}
	else
		mine = thismine;

	if (!mine || P_MobjWasRemoved(mine))
		return;

	spd = (82 + ((gamespeed-1) * 14))*mapobjectscale; // Avg Speed is 41 in Normal

	mine->flags |= MF_NOCLIPTHING;

	P_SetMobjState(mine, S_SSMINE_AIR1);
	mine->threshold = 10;
	mine->extravalue1 = 0;
	mine->reactiontime = mine->info->reactiontime;

	mine->momx = punter->momx + FixedMul(FINECOSINE(fa), spd);
	mine->momy = punter->momy + FixedMul(FINESINE(fa), spd);
	mine->momz = P_MobjFlip(mine) * z;

	mine->flags &= ~MF_NOCLIPTHING;
}

#define THUNDERRADIUS 320

static void K_DoThunderShield(player_t *player)
{
	mobj_t *mo;
	int i = 0;
	fixed_t sx;
	fixed_t sy;
	angle_t an;

	S_StartSound(player->mo, sfx_zio3);
	P_NukeEnemies(player->mo, player->mo, RING_DIST/4);

	// spawn vertical bolt
	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO11);
	mo->color = SKINCOLOR_TEAL;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO21);
	mo->color = SKINCOLOR_CYAN;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	// spawn horizontal bolts;
	for (i=0; i<7; i++)
	{
		mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
		mo->angle = P_RandomRange(0, 359)*ANG1;
		mo->fuse = P_RandomRange(20, 50);
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KLIT1);
	}

	// spawn the radius thing:
	an = ANGLE_22h;
	for (i=0; i<15; i++)
	{
		sx = player->mo->x + FixedMul((player->mo->scale*THUNDERRADIUS), FINECOSINE((an*i)>>ANGLETOFINESHIFT));
		sy = player->mo->y + FixedMul((player->mo->scale*THUNDERRADIUS), FINESINE((an*i)>>ANGLETOFINESHIFT));
		mo = P_SpawnMobj(sx, sy, player->mo->z, MT_THOK);
		mo-> angle = an*i;
		mo->extravalue1 = THUNDERRADIUS;	// Used to know whether we should teleport by radius or something.
		mo->scale = player->mo->scale*3;
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KSPARK1);
	}
}

#undef THUNDERRADIUS

static void K_FlameDashLeftoverSmoke(mobj_t *src)
{
	UINT8 i;

	for (i = 0; i < 2; i++)
	{
		mobj_t *smoke = P_SpawnMobj(src->x, src->y, src->z+(8<<FRACBITS), MT_BOOSTSMOKE);

		P_SetScale(smoke, src->scale);
		smoke->destscale = 3*src->scale/2;
		smoke->scalespeed = src->scale/12;

		smoke->momx = 3*src->momx/4;
		smoke->momy = 3*src->momy/4;
		smoke->momz = 3*src->momz/4;

		P_Thrust(smoke, src->angle + FixedAngle(P_RandomRange(135, 225)<<FRACBITS), P_RandomRange(0, 8) * src->scale);
		smoke->momz += P_RandomRange(0, 4) * src->scale;
	}
}

static void K_DoHyudoroSteal(player_t *player)
{
	INT32 i, numplayers = 0;
	INT32 playerswappable[MAXPLAYERS];
	INT32 stealplayer = -1; // The player that's getting stolen from
	INT32 prandom = 0;
	boolean sink = P_RandomChance(FRACUNIT/64);
	INT32 hyu = hyudorotime;

	if (G_RaceGametype())
		hyu *= 2; // double in race

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && players[i].mo->health > 0 && players[i].playerstate == PST_LIVE
			&& player != &players[i] && !players[i].exiting && !players[i].spectator // Player in-game

			// Can steal from this player
			&& (G_RaceGametype() //&& players[i].kartstuff[k_position] < player->kartstuff[k_position])
			|| (G_BattleGametype() && players[i].kartstuff[k_bumper] > 0))

			// Has an item
			&& (players[i].kartstuff[k_itemtype]
			&& players[i].kartstuff[k_itemamount]
			&& !players[i].kartstuff[k_itemheld]
			&& !players[i].karthud[khud_itemblink]))
		{
			playerswappable[numplayers] = i;
			numplayers++;
		}
	}

	prandom = P_RandomFixed();
	S_StartSound(player->mo, sfx_s3k92);

	if (sink && numplayers > 0 && cv_kitchensink.value) // BEHOLD THE KITCHEN SINK
	{
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;

		player->kartstuff[k_itemtype] = KITEM_KITCHENSINK;
		player->kartstuff[k_itemamount] = 1;
		player->kartstuff[k_itemheld] = 0;
		return;
	}
	else if ((G_RaceGametype() && player->kartstuff[k_position] == 1) || numplayers == 0) // No-one can be stolen from? Oh well...
	{
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;
		return;
	}
	else if (numplayers == 1) // With just 2 players, we just need to set the other player to be the one to steal from
	{
		stealplayer = playerswappable[numplayers-1];
	}
	else if (numplayers > 1) // We need to choose between the available candidates for the 2nd player
	{
		stealplayer = playerswappable[prandom%(numplayers-1)];
	}

	if (stealplayer > -1) // Now here's where we do the stealing, has to be done here because we still know the player we're stealing from
	{
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;
		players[stealplayer].kartstuff[k_stolentimer] = stealtime;

		player->kartstuff[k_itemtype] = players[stealplayer].kartstuff[k_itemtype];
		player->kartstuff[k_itemamount] = players[stealplayer].kartstuff[k_itemamount];
		player->kartstuff[k_itemheld] = 0;

		players[stealplayer].kartstuff[k_itemtype] = KITEM_NONE;
		players[stealplayer].kartstuff[k_itemamount] = 0;
		players[stealplayer].kartstuff[k_itemheld] = 0;

		if (P_IsDisplayPlayer(&players[stealplayer]) && !r_splitscreen)
			S_StartSound(NULL, sfx_s3k92);
	}
}

void K_DoSneaker(player_t *player, INT32 type)
{
	const fixed_t intendedboost = FRACUNIT/2;

	if (!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3)
	{
		S_StartSound(player->mo, sfx_cdfm01);
		K_SpawnDashDustRelease(player);
		if (intendedboost > player->kartstuff[k_speedboost])
			player->karthud[khud_destboostcam] = FixedMul(FRACUNIT, FixedDiv((intendedboost - player->kartstuff[k_speedboost]), intendedboost));
	}

	if (!EITHERSNEAKER(player))
	{
		if (type == 2)
		{
			if (player->mo->hnext)
			{
				mobj_t *cur = player->mo->hnext;
				while (cur && !P_MobjWasRemoved(cur))
				{
					if (!cur->tracer)
					{
						mobj_t *overlay = P_SpawnMobj(cur->x, cur->y, cur->z, MT_BOOSTFLAME);
						P_SetTarget(&overlay->target, cur);
						P_SetTarget(&cur->tracer, overlay);
						P_SetScale(overlay, (overlay->destscale = 3*cur->scale/4));
						K_FlipFromObject(overlay, cur);
					}
					cur = cur->hnext;
				}
			}
		}
		else
		{
			mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BOOSTFLAME);
			P_SetTarget(&overlay->target, player->mo);
			P_SetScale(overlay, (overlay->destscale = player->mo->scale));
			K_FlipFromObject(overlay, player->mo);
		}
	}

	if (type != 0)
	{
		player->pflags |= PF_ATTACKDOWN;
		K_PlayBoostTaunt(player->mo);
		player->kartstuff[k_sneakertimer] = sneakertime;
	}
	else
		player->kartstuff[k_levelbooster] = sneakertime;

	// set angle for spun out players:
	player->kartstuff[k_boostangle] = (INT32)player->mo->angle;
}

static void K_DoShrink(player_t *user)
{
	INT32 i;
	mobj_t *mobj, *next;

	S_StartSound(user->mo, sfx_kc46); // Sound the BANG!
	user->pflags |= PF_ATTACKDOWN;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;
		if (&players[i] == user)
			continue;
		if (players[i].kartstuff[k_position] < user->kartstuff[k_position])
		{
			//P_FlashPal(&players[i], PAL_NUKE, 10);

			// Grow should get taken away.
			if (players[i].kartstuff[k_growshrinktimer] > 0)
				K_RemoveGrowShrink(&players[i]);
			else
			{
				// Start shrinking!
				K_DropItems(&players[i]);
				players[i].kartstuff[k_growshrinktimer] = -(15*TICRATE);

				if (players[i].mo && !P_MobjWasRemoved(players[i].mo))
				{
					players[i].mo->scalespeed = mapobjectscale/TICRATE;
					players[i].mo->destscale = (6*mapobjectscale)/8;
					if (cv_kartdebugshrink.value && !modeattacking && !players[i].bot)
						players[i].mo->destscale = (6*players[i].mo->destscale)/8;
					S_StartSound(players[i].mo, sfx_kc59);
				}
			}
		}
	}

	// kill everything in the kitem list while we're at it:
	for (mobj = kitemcap; mobj; mobj = next)
	{
		next = mobj->itnext;

		// check if the item is being held by a player behind us before removing it.
		// check if the item is a "shield" first, bc i'm p sure thrown items keep the player that threw em as target anyway

		if (mobj->type == MT_BANANA_SHIELD || mobj->type == MT_JAWZ_SHIELD ||
		mobj->type == MT_SSMINE_SHIELD || mobj->type == MT_EGGMANITEM_SHIELD ||
		mobj->type == MT_SINK_SHIELD || mobj->type == MT_ORBINAUT_SHIELD)
		{
			if (mobj->target && mobj->target->player)
			{
				if (mobj->target->player->kartstuff[k_position] > user->kartstuff[k_position])
					continue; // this guy's behind us, don't take his stuff away!
			}
		}

		mobj->destscale = 0;
		mobj->flags &= ~(MF_SOLID|MF_SHOOTABLE|MF_SPECIAL);
		mobj->flags |= MF_NOCLIPTHING; // Just for safety

		if (mobj->type == MT_SPB)
			spbplace = -1;
	}
}


void K_DoPogoSpring(mobj_t *mo, fixed_t vertispeed, UINT8 sound)
{
	const fixed_t vscale = mapobjectscale + (mo->scale - mapobjectscale);

	if (mo->player && mo->player->spectator)
		return;

	if (mo->eflags & MFE_SPRUNG)
		return;

#ifdef ESLOPE
	mo->standingslope = NULL;
#endif

	mo->eflags |= MFE_SPRUNG;

	if (mo->eflags & MFE_VERTICALFLIP)
		vertispeed *= -1;

	if (vertispeed == 0)
	{
		fixed_t thrust;

		if (mo->player)
		{
			thrust = 3*mo->player->speed/2;
			if (thrust < 48<<FRACBITS)
				thrust = 48<<FRACBITS;
			if (thrust > 72<<FRACBITS)
				thrust = 72<<FRACBITS;
			if (mo->player->kartstuff[k_pogospring] != 2)
			{
				if (EITHERSNEAKER(mo->player))
					thrust = FixedMul(thrust, (5*FRACUNIT)/4);
				else if (mo->player->kartstuff[k_invincibilitytimer])
					thrust = FixedMul(thrust, (9*FRACUNIT)/8);
			}
		}
		else
		{
			thrust = FixedDiv(3*P_AproxDistance(mo->momx, mo->momy)/2, 5*FRACUNIT/2);
			if (thrust < 16<<FRACBITS)
				thrust = 16<<FRACBITS;
			if (thrust > 32<<FRACBITS)
				thrust = 32<<FRACBITS;
		}

		mo->momz = P_MobjFlip(mo)*FixedMul(FINESINE(ANGLE_22h>>ANGLETOFINESHIFT), FixedMul(thrust, vscale));
	}
	else
		mo->momz = FixedMul(vertispeed, vscale);

	if (mo->eflags & MFE_UNDERWATER)
		mo->momz = (117 * mo->momz) / 200;

	if (sound)
		S_StartSound(mo, (sound == 1 ? sfx_kc2f : sfx_kpogos));
}

void K_KillBananaChain(mobj_t *banana, mobj_t *inflictor, mobj_t *source)
{
	mobj_t *cachenext;

killnext:
	cachenext = banana->hnext;

	if (banana->health)
	{
		if (banana->eflags & MFE_VERTICALFLIP)
			banana->z -= banana->height;
		else
			banana->z += banana->height;

		S_StartSound(banana, banana->info->deathsound);
		P_KillMobj(banana, inflictor, source);

		P_SetObjectMomZ(banana, 8*FRACUNIT, false);
		if (inflictor)
			P_InstaThrust(banana, R_PointToAngle2(inflictor->x, inflictor->y, banana->x, banana->y)+ANGLE_90, 16*FRACUNIT);
	}

	if ((banana = cachenext))
		goto killnext;
}

// Just for firing/dropping items.
void K_UpdateHnextList(player_t *player, boolean clean)
{
	mobj_t *work = player->mo, *nextwork;

	if (!work)
		return;

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		if (!clean && (!work->movedir || work->movedir <= (UINT16)player->kartstuff[k_itemamount]))
		{
			continue;
		}

		P_RemoveMobj(work);
	}

	if (player->mo->hnext == NULL || P_MobjWasRemoved(player->mo->hnext))
	{
		// Like below, try to clean up the pointer if it's NULL.
		// Maybe this was a cause of the shrink/eggbox fails?
		P_SetTarget(&player->mo->hnext, NULL);
	}
}

// For getting hit!
void K_DropHnextList(player_t *player, boolean keepshields)
{
	mobj_t *work = player->mo, *nextwork, *dropwork;
	INT32 flip;
	mobjtype_t type;
	boolean orbit, ponground, dropall = true;
	INT32 shield = K_GetShieldFromItem(player->kartstuff[k_itemtype]);

	if (work == NULL || P_MobjWasRemoved(work))
	{
		return;
	}

	flip = P_MobjFlip(player->mo);
	ponground = P_IsObjectOnGround(player->mo);

	if (shield != KSHIELD_NONE && !keepshields)
	{
		if (shield == KSHIELD_THUNDER)
		{
			K_DoThunderShield(player);
		}

		player->kartstuff[k_curshield] = KSHIELD_NONE;
		player->kartstuff[k_itemtype] = KITEM_NONE;
		player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
	}

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		switch (work->type)
		{
			// Kart orbit items
			case MT_ORBINAUT_SHIELD:
				orbit = true;
				type = MT_ORBINAUT;
				break;
			case MT_JAWZ_SHIELD:
				orbit = true;
				type = MT_JAWZ_DUD;
				break;
			// Kart trailing items
			case MT_BANANA_SHIELD:
				orbit = false;
				type = MT_BANANA;
				break;
			case MT_SSMINE_SHIELD:
				orbit = false;
				dropall = false;
				type = MT_SSMINE;
				break;
			case MT_EGGMANITEM_SHIELD:
				orbit = false;
				type = MT_EGGMANITEM;
				break;
			// intentionally do nothing
			case MT_ROCKETSNEAKER:
			case MT_SINK_SHIELD:
				return;
			default:
				continue;
		}

		dropwork = P_SpawnMobj(work->x, work->y, work->z, type);

		P_SetTarget(&dropwork->target, player->mo);
		P_AddKartItem(dropwork); // needs to be called here so shrink can bust items off players in front of the user.

		dropwork->angle = work->angle;

		dropwork->flags |= MF_NOCLIPTHING;
		dropwork->flags2 = work->flags2;

		dropwork->floorz = work->floorz;
		dropwork->ceilingz = work->ceilingz;

		if (ponground)
		{
			// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
			// This should set it for FOFs
			//P_TeleportMove(dropwork, dropwork->x, dropwork->y, dropwork->z); -- handled better by above floorz/ceilingz passing

			if (flip == 1)
			{
				if (dropwork->floorz > dropwork->target->z - dropwork->height)
				{
					dropwork->z = dropwork->floorz;
				}
			}
			else
			{
				if (dropwork->ceilingz < dropwork->target->z + dropwork->target->height + dropwork->height)
				{
					dropwork->z = dropwork->ceilingz - dropwork->height;
				}
			}
		}

		if (orbit) // splay out
		{
			dropwork->flags2 |= MF2_AMBUSH;

			dropwork->z += flip;

			dropwork->momx = player->mo->momx>>1;
			dropwork->momy = player->mo->momy>>1;
			dropwork->momz = 3*flip*mapobjectscale;

			if (dropwork->eflags & MFE_UNDERWATER)
				dropwork->momz = (117 * dropwork->momz) / 200;

			P_Thrust(dropwork, work->angle - ANGLE_90, 6*mapobjectscale);

			dropwork->movecount = 2;
			dropwork->movedir = work->angle - ANGLE_90;

			P_SetMobjState(dropwork, dropwork->info->deathstate);

			dropwork->tics = -1;

			if (type == MT_JAWZ_DUD)
			{
				dropwork->z += 20*flip*dropwork->scale;
			}
			else
			{
				dropwork->color = work->color;
				dropwork->angle -= ANGLE_90;
			}
		}
		else // plop on the ground
		{
			dropwork->flags &= ~MF_NOCLIPTHING;
			dropwork->threshold = 10;
		}

		P_RemoveMobj(work);
	}

	// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
	P_SetTarget(&player->mo->hnext, NULL);

	player->kartstuff[k_bananadrag] = 0;

	if (player->kartstuff[k_eggmanheld])
	{
		player->kartstuff[k_eggmanheld] = 0;
	}
	else if (player->kartstuff[k_itemheld]
		&& (dropall || (--player->kartstuff[k_itemamount] <= 0)))
	{
		player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
		player->kartstuff[k_itemtype] = KITEM_NONE;
	}
}

// For getting EXTRA hit!
void K_DropItems(player_t *player)
{
	K_DropHnextList(player, true);

	if (player->mo && !P_MobjWasRemoved(player->mo) && player->kartstuff[k_itemamount] > 0)
	{
		mobj_t *drop = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FLOATINGITEM);
		P_SetScale(drop, drop->scale>>4);
		drop->destscale = (3*drop->destscale)/2;

		drop->angle = player->mo->angle + ANGLE_90;
		P_Thrust(drop,
			FixedAngle(P_RandomFixed()*180) + player->mo->angle + ANGLE_90,
			16*mapobjectscale);
		drop->momz = P_MobjFlip(player->mo)*3*mapobjectscale;
		if (drop->eflags & MFE_UNDERWATER)
			drop->momz = (117 * drop->momz) / 200;

		drop->threshold = player->kartstuff[k_itemtype];
		drop->movecount = player->kartstuff[k_itemamount];

		drop->flags |= MF_NOCLIPTHING;
	}

	K_StripItems(player);
}

// When an item in the hnext chain dies.
void K_RepairOrbitChain(mobj_t *orbit)
{
	mobj_t *cachenext = orbit->hnext;

	// First, repair the chain
	if (orbit->hnext && orbit->hnext->health && !P_MobjWasRemoved(orbit->hnext))
	{
		P_SetTarget(&orbit->hnext->hprev, orbit->hprev);
		P_SetTarget(&orbit->hnext, NULL);
	}

	if (orbit->hprev && orbit->hprev->health && !P_MobjWasRemoved(orbit->hprev))
	{
		P_SetTarget(&orbit->hprev->hnext, cachenext);
		P_SetTarget(&orbit->hprev, NULL);
	}

	// Then recount to make sure item amount is correct
	if (orbit->target && orbit->target->player)
	{
		INT32 num = 0;

		mobj_t *cur = orbit->target->hnext;
		mobj_t *prev = NULL;

		while (cur && !P_MobjWasRemoved(cur))
		{
			prev = cur;
			cur = cur->hnext;
			if (++num > orbit->target->player->kartstuff[k_itemamount])
				P_RemoveMobj(prev);
			else
				prev->movedir = num;
		}

		if (orbit->target->player->kartstuff[k_itemamount] != num)
			orbit->target->player->kartstuff[k_itemamount] = num;
	}
}

// Simplified version of a code bit in P_MobjFloorZ
static fixed_t K_BananaSlopeZ(pslope_t *slope, fixed_t x, fixed_t y, fixed_t radius, boolean ceiling)
{
	fixed_t testx, testy;

	if (slope->d.x < 0)
		testx = radius;
	else
		testx = -radius;

	if (slope->d.y < 0)
		testy = radius;
	else
		testy = -radius;

	if ((slope->zdelta > 0) ^ !!(ceiling))
	{
		testx = -testx;
		testy = -testy;
	}

	testx += x;
	testy += y;

	return P_GetZAt(slope, testx, testy);
}

static void K_CalculateBananaSlope(mobj_t *mobj, fixed_t x, fixed_t y, fixed_t z, fixed_t radius, fixed_t height, boolean flip, boolean player)
{
	fixed_t newz;
	sector_t *sec;
#ifdef ESLOPE
	pslope_t *slope = NULL;
#endif

	sec = R_PointInSubsector(x, y)->sector;

	if (flip)
	{
#ifdef ESLOPE
		if (sec->c_slope)
		{
			slope = sec->c_slope;
			newz = K_BananaSlopeZ(slope, x, y, radius, true);
		}
		else
#endif
			newz = sec->ceilingheight;
	}
	else
	{
#ifdef ESLOPE
		if (sec->f_slope)
		{
			slope = sec->f_slope;
			newz = K_BananaSlopeZ(slope, x, y, radius, false);
		}
		else
#endif
			newz = sec->floorheight;
	}

	// Check FOFs for a better suited slope
	if (sec->ffloors)
	{
		ffloor_t *rover;

		for (rover = sec->ffloors; rover; rover = rover->next)
		{
			fixed_t top, bottom;
			fixed_t d1, d2;

			if (!(rover->flags & FF_EXISTS))
				continue;

			if ((!(((rover->flags & FF_BLOCKPLAYER && player)
				|| (rover->flags & FF_BLOCKOTHERS && !player))
				|| (rover->flags & FF_QUICKSAND))
				|| (rover->flags & FF_SWIMMABLE)))
				continue;

#ifdef ESLOPE
			if (*rover->t_slope)
				top = K_BananaSlopeZ(*rover->t_slope, x, y, radius, false);
			else
#endif
				top = *rover->topheight;

#ifdef ESLOPE
			if (*rover->b_slope)
				bottom = K_BananaSlopeZ(*rover->b_slope, x, y, radius, true);
			else
#endif
				bottom = *rover->bottomheight;

			if (flip)
			{
				if (rover->flags & FF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz > (z + height))
						{
							newz = (z + height);
							slope = NULL;
						}
					}
					continue;
				}

				d1 = (z + height) - (top + ((bottom - top)/2));
				d2 = z - (top + ((bottom - top)/2));

				if (bottom < newz && abs(d1) < abs(d2))
				{
					newz = bottom;
#ifdef ESLOPE
					if (*rover->b_slope)
						slope = *rover->b_slope;
#endif
				}
			}
			else
			{
				if (rover->flags & FF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz < z)
						{
							newz = z;
							slope = NULL;
						}
					}
					continue;
				}

				d1 = z - (bottom + ((top - bottom)/2));
				d2 = (z + height) - (bottom + ((top - bottom)/2));

				if (top > newz && abs(d1) < abs(d2))
				{
					newz = top;
#ifdef ESLOPE
					if (*rover->t_slope)
						slope = *rover->t_slope;
#endif
				}
			}
		}
	}

#if 0
	mobj->standingslope = slope;
#endif

#ifdef HWRENDER
	mobj->modeltilt = slope;
#endif
}

// Move the hnext chain!
static void K_MoveHeldObjects(player_t *player)
{
	if (!player->mo)
		return;

	if (!player->mo->hnext)
	{
		player->kartstuff[k_bananadrag] = 0;
		if (player->kartstuff[k_eggmanheld])
			player->kartstuff[k_eggmanheld] = 0;
		else if (player->kartstuff[k_itemheld])
		{
			player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}
		return;
	}

	if (P_MobjWasRemoved(player->mo->hnext))
	{
		// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
		P_SetTarget(&player->mo->hnext, NULL);
		player->kartstuff[k_bananadrag] = 0;
		if (player->kartstuff[k_eggmanheld])
			player->kartstuff[k_eggmanheld] = 0;
		else if (player->kartstuff[k_itemheld])
		{
			player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}
		return;
	}

	switch (player->mo->hnext->type)
	{
		case MT_ORBINAUT_SHIELD: // Kart orbit items
		case MT_JAWZ_SHIELD:
			{
				mobj_t *cur = player->mo->hnext;
				fixed_t speed = ((8 - min(4, player->kartstuff[k_itemamount])) * cur->info->speed) / 7;

				player->kartstuff[k_bananadrag] = 0; // Just to make sure

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius); // mobj's distance from its Target, or Radius.
					fixed_t z;

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					cur->color = player->skincolor;

					cur->angle -= ANGLE_90;
					cur->angle += FixedAngle(speed);

					if (cur->extravalue1 < radius)
						cur->extravalue1 += P_AproxDistance(cur->extravalue1, radius) / 12;
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// If the player is on the ceiling, then flip your items as well.
					if (player && player->mo->eflags & MFE_VERTICALFLIP)
						cur->eflags |= MFE_VERTICALFLIP;
					else
						cur->eflags &= ~MFE_VERTICALFLIP;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

					if (P_MobjFlip(cur) > 0)
						z = player->mo->z;
					else
						z = player->mo->z + player->mo->height - cur->height;

					cur->flags |= MF_NOCLIPTHING; // temporarily make them noclip other objects so they can't hit anyone while in the player
					P_TeleportMove(cur, player->mo->x, player->mo->y, z);
					cur->momx = FixedMul(FINECOSINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->momy = FixedMul(FINESINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->flags &= ~MF_NOCLIPTHING;
					if (!P_TryMove(cur, player->mo->x + cur->momx, player->mo->y + cur->momy, true))
						P_SlideMove(cur, true);
					if (P_IsObjectOnGround(player->mo))
					{
						if (P_MobjFlip(cur) > 0)
						{
							if (cur->floorz > player->mo->z - cur->height)
								z = cur->floorz;
						}
						else
						{
							if (cur->ceilingz < player->mo->z + player->mo->height + cur->height)
								z = cur->ceilingz - cur->height;
						}
					}

					// Center it during the scale up animation
					z += (FixedMul(mobjinfo[cur->type].height, player->mo->scale - cur->scale)>>1) * P_MobjFlip(cur);

					cur->z = z;
					cur->momx = cur->momy = 0;
					cur->angle += ANGLE_90;

					cur = cur->hnext;
				}
			}
			break;
		case MT_BANANA_SHIELD: // Kart trailing items
		case MT_SSMINE_SHIELD:
		case MT_EGGMANITEM_SHIELD:
		case MT_SINK_SHIELD:
			{
				mobj_t *cur = player->mo->hnext;
				mobj_t *targ = player->mo;

				if (P_IsObjectOnGround(player->mo) && player->speed > 0)
					player->kartstuff[k_bananadrag]++;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(targ->radius, targ->radius) + FixedHypot(cur->radius, cur->radius);
					angle_t ang;
					fixed_t targx, targy, targz;
					fixed_t speed, dist;

					if (cur->type == MT_EGGMANITEM_SHIELD)
					{
						// Decided that this should use their "canon" color.
						cur->color = SKINCOLOR_BLACK;
					}

					cur->flags &= ~MF_NOCLIPTHING;

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					if (cur != player->mo->hnext)
					{
						targ = cur->hprev;
						dist = cur->extravalue1/4;
					}
					else
						dist = cur->extravalue1/2;

					if (!targ || P_MobjWasRemoved(targ))
						continue;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

					ang = targ->angle;
					targx = targ->x + P_ReturnThrustX(cur, ang + ANGLE_180, dist);
					targy = targ->y + P_ReturnThrustY(cur, ang + ANGLE_180, dist);
					targz = targ->z;

					speed = FixedMul(R_PointToDist2(cur->x, cur->y, targx, targy), 3*FRACUNIT/4);
					if (P_IsObjectOnGround(targ))
						targz = cur->floorz;

					cur->angle = R_PointToAngle2(cur->x, cur->y, targx, targy);

					/*if (P_IsObjectOnGround(player->mo) && player->speed > 0 && player->kartstuff[k_bananadrag] > TICRATE
						&& P_RandomChance(min(FRACUNIT/2, FixedDiv(player->speed, K_GetKartSpeed(player, false))/2)))
					{
						if (leveltime & 1)
							targz += 8*(2*FRACUNIT)/7;
						else
							targz -= 8*(2*FRACUNIT)/7;
					}*/

					if (speed > dist)
						P_InstaThrust(cur, cur->angle, speed-dist);

					P_SetObjectMomZ(cur, FixedMul(targz - cur->z, 7*FRACUNIT/8) - gravity, false);

					if (R_PointToDist2(cur->x, cur->y, targx, targy) > 768*FRACUNIT)
						P_TeleportMove(cur, targx, targy, cur->z);

#ifdef ESLOPE
					if (P_IsObjectOnGround(cur))
					{
						K_CalculateBananaSlope(cur, cur->x, cur->y, cur->z,
							cur->radius, cur->height, (cur->eflags & MFE_VERTICALFLIP), false);
					}
#endif

					cur = cur->hnext;
				}
			}
			break;
		case MT_ROCKETSNEAKER: // Special rocket sneaker stuff
			{
				mobj_t *cur = player->mo->hnext;
				INT32 num = 0;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius);
					boolean vibrate = ((leveltime & 1) && !cur->tracer);
					angle_t angoffset;
					fixed_t targx, targy, targz;

					cur->flags &= ~MF_NOCLIPTHING;

					if (player->kartstuff[k_rocketsneakertimer] <= TICRATE && (leveltime & 1))
						cur->flags2 |= MF2_DONTDRAW;
					else
						cur->flags2 &= ~MF2_DONTDRAW;

					if (num & 1)
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_LVIBRATE : S_ROCKETSNEAKER_L));
					else
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_RVIBRATE : S_ROCKETSNEAKER_R));

					if (!player->kartstuff[k_rocketsneakertimer] || cur->extravalue2 || !cur->health)
					{
						num = (num+1) % 2;
						cur = cur->hnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

#if 1
					{
						angle_t input = player->frameangle - cur->angle;
						boolean invert = (input > ANGLE_180);
						if (invert)
							input = InvAngle(input);

						input = FixedAngle(AngleFixed(input)/4);
						if (invert)
							input = InvAngle(input);

						cur->angle = cur->angle + input;
					}
#else
					cur->angle = player->frameangle;
#endif

					angoffset = ANGLE_90 + (ANGLE_180 * num);

					targx = player->mo->x + P_ReturnThrustX(cur, cur->angle + angoffset, cur->extravalue1);
					targy = player->mo->y + P_ReturnThrustY(cur, cur->angle + angoffset, cur->extravalue1);

					{ // bobbing, copy pasted from my kimokawaiii entry
						const fixed_t pi = (22<<FRACBITS) / 7; // loose approximation, this doesn't need to be incredibly precise
						fixed_t sine = FixedMul(player->mo->scale, 8 * FINESINE((((2*pi*(4*TICRATE)) * leveltime)>>ANGLETOFINESHIFT) & FINEMASK));
						targz = (player->mo->z + (player->mo->height/2)) + sine;
						if (player->mo->eflags & MFE_VERTICALFLIP)
							targz += (player->mo->height/2 - 32*player->mo->scale)*6;

					}

					if (cur->tracer)
					{
						fixed_t diffx, diffy, diffz;

						diffx = targx - cur->x;
						diffy = targy - cur->y;
						diffz = targz - cur->z;

						P_TeleportMove(cur->tracer, cur->tracer->x + diffx + P_ReturnThrustX(cur, cur->angle + angoffset, 6*cur->scale),
							cur->tracer->y + diffy + P_ReturnThrustY(cur, cur->angle + angoffset, 6*cur->scale), cur->tracer->z + diffz);
						P_SetScale(cur->tracer, (cur->tracer->destscale = 3*cur->scale/4));
					}

					P_TeleportMove(cur, targx, targy, targz);
					K_FlipFromObject(cur, player->mo);	// Update graviflip in real time thanks.
#ifdef HWRENDER
					cur->modeltilt = player->mo->modeltilt;
#endif
					num = (num+1) % 2;
					cur = cur->hnext;
				}
			}
			break;
		default:
			break;
	}
}

player_t *K_FindJawzTarget(mobj_t *actor, player_t *source)
{
	fixed_t best = -1;
	player_t *wtarg = NULL;
	INT32 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		angle_t thisang;
		player_t *player;

		if (!playeringame[i])
			continue;

		player = &players[i];

		if (player->spectator)
			continue; // spectator

		if (!player->mo)
			continue;

		if (player->mo->health <= 0)
			continue; // dead

		// Don't target yourself, stupid.
		if (player == source)
			continue;

		// Don't home in on teammates.
		if (G_GametypeHasTeams() && source->ctfteam == player->ctfteam)
			continue;

		// Invisible, don't bother
		if (player->kartstuff[k_hyudorotimer])
			continue;

		// Find the angle, see who's got the best.
		thisang = actor->angle - R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y);
		if (thisang > ANGLE_180)
			thisang = InvAngle(thisang);

		// Jawz only go after the person directly ahead of you in race... sort of literally now!
		if (G_RaceGametype())
		{
			// Don't go for people who are behind you
			if (thisang > ANGLE_67h)
				continue;
			// Don't pay attention to people who aren't above your position
			if (player->kartstuff[k_position] >= source->kartstuff[k_position])
				continue;
			if ((best == -1) || (player->kartstuff[k_position] > best))
			{
				wtarg = player;
				best = player->kartstuff[k_position];
			}
		}
		else
		{
			fixed_t thisdist;
			fixed_t thisavg;

			// Don't go for people who are behind you
			if (thisang > ANGLE_45)
				continue;

			// Don't pay attention to dead players
			if (player->kartstuff[k_bumper] <= 0)
				continue;

			// Z pos too high/low
			if (abs(player->mo->z - (actor->z + actor->momz)) > RING_DIST/8)
				continue;

			thisdist = P_AproxDistance(player->mo->x - (actor->x + actor->momx), player->mo->y - (actor->y + actor->momy));

			if (thisdist > 2*RING_DIST) // Don't go for people who are too far away
				continue;

			thisavg = (AngleFixed(thisang) + thisdist) / 2;

			//CONS_Printf("got avg %d from player # %d\n", thisavg>>FRACBITS, i);

			if ((best == -1) || (thisavg < best))
			{
				wtarg = player;
				best = thisavg;
			}
		}
	}

	return wtarg;
}

// Engine Sounds.
static void K_UpdateEngineSounds(player_t *player, ticcmd_t *cmd)
{
	const INT32 numsnds = 13;
	INT32 class, s, w; // engine class number
	UINT8 volume = 255;
	fixed_t volumedampen = 0;
	INT32 targetsnd = 0;
	INT32 i;

	s = (player->kartspeed-1)/3;
	w = (player->kartweight-1)/3;

#define LOCKSTAT(stat) \
	if (stat < 0) { stat = 0; } \
	if (stat > 2) { stat = 2; }
	LOCKSTAT(s);
	LOCKSTAT(w);
#undef LOCKSTAT

	class = s+(3*w);

	// Silence the engines
	if (leveltime < 8 || player->spectator)
	{
		player->karthud[khud_enginesnd] = 0; // Reset sound number
		return;
	}

#if 0
	if ((leveltime % 8) != ((player-players) % 8)) // Per-player offset, to make engines sound distinct!
#else
	if (leveltime % 8) // .25 seconds of wait time between engine sounds
#endif
		return;

	if ((leveltime >= starttime-(2*TICRATE) && leveltime <= starttime) || (player->respawn.state == RESPAWNST_DROP)) // Startup boosts
		targetsnd = ((cmd->buttons & BT_ACCELERATE) ? 12 : 0);
	else
		targetsnd = (((6*cmd->forwardmove)/25) + ((player->speed / mapobjectscale)/5))/2;

	if (targetsnd < 0)
		targetsnd = 0;
	if (targetsnd > 12)
		targetsnd = 12;

	if (player->karthud[khud_enginesnd] < targetsnd)
		player->karthud[khud_enginesnd]++;
	if (player->karthud[khud_enginesnd] > targetsnd)
		player->karthud[khud_enginesnd]--;

	if (player->karthud[khud_enginesnd] < 0)
		player->karthud[khud_enginesnd] = 0;
	if (player->karthud[khud_enginesnd] > 12)
		player->karthud[khud_enginesnd] = 12;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT8 thisvol = 0;
		fixed_t dist;

		if (!playeringame[i] || !players[i].mo || players[i].spectator || players[i].exiting)
			continue;

		if (P_IsDisplayPlayer(&players[i]))
		{
			volumedampen += FRACUNIT; // We already know what this is gonna be, let's not waste our time.
			continue;
		}

		dist = P_AproxDistance(P_AproxDistance(player->mo->x-players[i].mo->x,
			player->mo->y-players[i].mo->y), player->mo->z-players[i].mo->z) / 2;

		dist = FixedDiv(dist, mapobjectscale);

		if (dist > 1536<<FRACBITS)
			continue;
		else if (dist < 160<<FRACBITS) // engine sounds' approx. range
			thisvol = 255;
		else
			thisvol = (15 * (((160<<FRACBITS) - dist)>>FRACBITS)) / (((1536<<FRACBITS)-(160<<FRACBITS))>>(FRACBITS+4));

		if (thisvol == 0)
			continue;

		volumedampen += (thisvol * 257); // 255 * 257 = FRACUNIT
	}

	if (volumedampen > FRACUNIT)
		volume = FixedDiv(volume<<FRACBITS, volumedampen)>>FRACBITS;

	if (volume <= 0) // Might as well
		return;

	S_StartSoundAtVolume(player->mo, (sfx_krta00 + player->karthud[khud_enginesnd]) + (class*numsnds), volume);
}

static void K_UpdateInvincibilitySounds(player_t *player)
{
	INT32 sfxnum = sfx_None;

	if (player->mo->health > 0 && !P_IsDisplayPlayer(player))
	{
		if (cv_kartinvinsfx.value)
		{
			if (player->kartstuff[k_invincibilitytimer] > 0) // Prioritize invincibility
				sfxnum = sfx_alarmi;
			else if (player->kartstuff[k_growshrinktimer] > 0)
				sfxnum = sfx_alarmg;
		}
		else
		{
			if (player->kartstuff[k_invincibilitytimer] > 0)
				sfxnum = sfx_kinvnc;
			else if (player->kartstuff[k_growshrinktimer] > 0)
				sfxnum = sfx_kgrow;
		}
	}

	if (sfxnum != sfx_None && !S_SoundPlaying(player->mo, sfxnum))
		S_StartSound(player->mo, sfxnum);

#define STOPTHIS(this) \
	if (sfxnum != this && S_SoundPlaying(player->mo, this)) \
		S_StopSoundByID(player->mo, this);
	STOPTHIS(sfx_alarmi);
	STOPTHIS(sfx_alarmg);
	STOPTHIS(sfx_kinvnc);
	STOPTHIS(sfx_kgrow);
#undef STOPTHIS
}

#define RINGANIM_NUMFRAMES 10
#define RINGANIM_DELAYMAX 5

void K_KartPlayerHUDUpdate(player_t *player)
{
	if (player->karthud[khud_lapanimation])
		player->karthud[khud_lapanimation]--;

	if (player->karthud[khud_yougotem])
		player->karthud[khud_yougotem]--;

	if (player->karthud[khud_voices])
		player->karthud[khud_voices]--;

	if (player->karthud[khud_tauntvoices])
		player->karthud[khud_tauntvoices]--;

	if (G_RaceGametype())
	{
		// 0 is the fast spin animation, set at 30 tics of ring boost or higher!
		if (player->kartstuff[k_ringboost] >= 30)
			player->karthud[khud_ringdelay] = 0;
		else
			player->karthud[khud_ringdelay] = ((RINGANIM_DELAYMAX+1) * (30 - player->kartstuff[k_ringboost])) / 30;

		if (player->karthud[khud_ringframe] == 0 && player->karthud[khud_ringdelay] > RINGANIM_DELAYMAX)
		{
			player->karthud[khud_ringframe] = 0;
			player->karthud[khud_ringtics] = 0;
		}
		else if ((player->karthud[khud_ringtics]--) <= 0)
		{
			if (player->karthud[khud_ringdelay] == 0) // fast spin animation
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+2) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = 0;
			}
			else
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+1) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = min(RINGANIM_DELAYMAX, player->karthud[khud_ringdelay])-1;
			}
		}

		if (player->kartstuff[k_ringlock])
		{
			UINT8 normalanim = (leveltime % 14);
			UINT8 debtanim = 14 + (leveltime % 2);

			if (player->karthud[khud_ringspblock] >= 14) // debt animation
			{
				if ((player->kartstuff[k_rings] > 0) // Get out of 0 ring animation
					&& (normalanim == 3 || normalanim == 10)) // on these transition frames.
					player->karthud[khud_ringspblock] = normalanim;
				else
					player->karthud[khud_ringspblock] = debtanim;
			}
			else // normal animation
			{
				if ((player->kartstuff[k_rings] <= 0) // Go into 0 ring animation
					&& (player->karthud[khud_ringspblock] == 1 || player->karthud[khud_ringspblock] == 8)) // on these transition frames.
					player->karthud[khud_ringspblock] = debtanim;
				else
					player->karthud[khud_ringspblock] = normalanim;
			}
		}
		else
			player->karthud[khud_ringspblock] = (leveltime % 14); // reset to normal anim next time
	}

	if (G_BattleGametype() && (player->exiting || player->kartstuff[k_comebacktimer]))
	{
		if (player->exiting)
		{
			if (player->exiting < 6*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->exiting == 6*TICRATE)
				player->karthud[khud_cardanimation] = 0;
			else if (player->karthud[khud_cardanimation] < 2*TICRATE)
				player->karthud[khud_cardanimation]++;
		}
		else
		{
			if (player->kartstuff[k_comebacktimer] < 6*TICRATE)
				player->karthud[khud_cardanimation] -= ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->kartstuff[k_comebacktimer] < 9*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
		}

		if (player->karthud[khud_cardanimation] > 164)
			player->karthud[khud_cardanimation] = 164;
		if (player->karthud[khud_cardanimation] < 0)
			player->karthud[khud_cardanimation] = 0;
	}
	else if (G_RaceGametype() && player->exiting)
	{
		if (player->karthud[khud_cardanimation] < 2*TICRATE)
			player->karthud[khud_cardanimation]++;
	}
	else
		player->karthud[khud_cardanimation] = 0;
}

#undef RINGANIM_DELAYMAX

// SRB2Kart: blockmap iterate for attraction shield users
static mobj_t *attractmo;
static fixed_t attractdist;
static inline boolean PIT_AttractingRings(mobj_t *thing)
{
	if (!attractmo || P_MobjWasRemoved(attractmo))
		return false;

	if (!attractmo->player)
		return false; // not a player

	if (thing->health <= 0 || !thing)
		return true; // dead

	if (thing->type != MT_RING && thing->type != MT_FLINGRING)
		return true; // not a ring

	if (thing->extravalue1)
		return true; // in special ring animation

	if (thing->cusval)
		return true; // already attracted

	// see if it went over / under
	if (attractmo->z - (attractdist>>2) > thing->z + thing->height)
		return true; // overhead
	if (attractmo->z + attractmo->height + (attractdist>>2) < thing->z)
		return true; // underneath

	if (P_AproxDistance(attractmo->x - thing->x, attractmo->y - thing->y) < attractdist)
		return true; // Too far away

	// set target
	P_SetTarget(&thing->tracer, attractmo);
	// flag to show it's been attracted once before
	thing->cusval = 1;
	return true; // find other rings
}

/** Looks for rings near a player in the blockmap.
  *
  * \param pmo Player object looking for rings to attract
  * \sa A_AttractChase
  */
static void K_LookForRings(mobj_t *pmo)
{
	INT32 bx, by, xl, xh, yl, yh;
	attractdist = FixedMul(RING_DIST, pmo->scale)>>2;

	// Use blockmap to check for nearby rings
	yh = (unsigned)(pmo->y + attractdist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(pmo->y - attractdist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(pmo->x + attractdist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(pmo->x - attractdist - bmaporgx)>>MAPBLOCKSHIFT;

	attractmo = pmo;

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_AttractingRings);
}

/**	\brief	Decreases various kart timers and powers per frame. Called in P_PlayerThink in p_user.c

	\param	player	player object passed from P_PlayerThink
	\param	cmd		control input from player

	\return	void
*/
void K_KartPlayerThink(player_t *player, ticcmd_t *cmd)
{
	K_UpdateOffroad(player);
	K_UpdateDraft(player);
	K_UpdateEngineSounds(player, cmd); // Thanks, VAda!

	// update boost angle if not spun out
	if (!player->kartstuff[k_spinouttimer] && !player->kartstuff[k_wipeoutslow])
		player->kartstuff[k_boostangle] = (INT32)player->mo->angle;

	K_GetKartBoostPower(player);

	// Special effect objects!
	if (player->mo && !player->spectator)
	{
		if (player->kartstuff[k_dashpadcooldown]) // Twinkle Circuit afterimages
		{
			mobj_t *ghost;
			ghost = P_SpawnGhostMobj(player->mo);
			ghost->fuse = player->kartstuff[k_dashpadcooldown]+1;
			ghost->momx = player->mo->momx / (player->kartstuff[k_dashpadcooldown]+1);
			ghost->momy = player->mo->momy / (player->kartstuff[k_dashpadcooldown]+1);
			ghost->momz = player->mo->momz / (player->kartstuff[k_dashpadcooldown]+1);
			player->kartstuff[k_dashpadcooldown]--;
		}

		if (player->speed > 0)
		{
			// Speed lines
			if (EITHERSNEAKER(player) || player->kartstuff[k_ringboost]
				|| player->kartstuff[k_driftboost] || player->kartstuff[k_startboost]
				|| player->kartstuff[k_eggmanexplode])
			{
				mobj_t *fast = P_SpawnMobj(player->mo->x + (P_RandomRange(-36,36) * player->mo->scale),
					player->mo->y + (P_RandomRange(-36,36) * player->mo->scale),
					player->mo->z + (player->mo->height/2) + (P_RandomRange(-20,20) * player->mo->scale),
					MT_FASTLINE);

				P_SetTarget(&fast->target, player->mo);
				fast->angle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
				fast->momx = 3*player->mo->momx/4;
				fast->momy = 3*player->mo->momy/4;
				fast->momz = 3*player->mo->momz/4;

				K_MatchGenericExtraFlags(fast, player->mo);

				// Make it red when you have the eggman speed boost
				if (player->kartstuff[k_eggmanexplode])
				{
					fast->color = SKINCOLOR_RED;
					fast->colorized = true;
				}
			}

			if (player->kartstuff[k_numboosts] > 0) // Boosting after images
			{
				mobj_t *ghost;
				ghost = P_SpawnGhostMobj(player->mo);
				ghost->extravalue1 = player->kartstuff[k_numboosts]+1;
				ghost->extravalue2 = (leveltime % ghost->extravalue1);
				ghost->fuse = ghost->extravalue1;
				ghost->frame |= FF_FULLBRIGHT;
				ghost->colorized = true;
				//ghost->color = player->skincolor;
				//ghost->momx = (3*player->mo->momx)/4;
				//ghost->momy = (3*player->mo->momy)/4;
				//ghost->momz = (3*player->mo->momz)/4;
				if (leveltime & 1)
					ghost->flags2 |= MF2_DONTDRAW;
			}

			if (P_IsObjectOnGround(player->mo))
			{
				// Offroad dust
				if (player->kartstuff[k_boostpower] < FRACUNIT)
				{
					K_SpawnWipeoutTrail(player->mo, true);
					if (leveltime % 6 == 0)
						S_StartSound(player->mo, sfx_cdfm70);
				}

				// Draft dust
				if (player->kartstuff[k_draftpower] >= FRACUNIT)
				{
					K_SpawnDraftDust(player->mo);
					/*if (leveltime % 23 == 0 || !S_SoundPlaying(player->mo, sfx_s265))
						S_StartSound(player->mo, sfx_s265);*/
				}
			}
		}

		if (G_RaceGametype() && player->kartstuff[k_rings] <= 0) // spawn ring debt indicator
		{
			mobj_t *debtflag = P_SpawnMobj(player->mo->x + player->mo->momx, player->mo->y + player->mo->momy,
				player->mo->z + player->mo->momz + player->mo->height + (24*player->mo->scale), MT_THOK);
			P_SetMobjState(debtflag, S_RINGDEBT);
			P_SetScale(debtflag, (debtflag->destscale = player->mo->scale));
			K_MatchGenericExtraFlags(debtflag, player->mo);
			debtflag->frame += (leveltime % 4);
			if ((leveltime/12) & 1)
				debtflag->frame += 4;
			debtflag->color = player->skincolor;
			debtflag->fuse = 2;
			if (P_IsDisplayPlayer(player))
				debtflag->flags2 |= MF2_DONTDRAW;
		}

		if (player->kartstuff[k_springstars] && (leveltime & 1))
		{
			fixed_t randx = P_RandomRange(-40, 40) * player->mo->scale;
			fixed_t randy = P_RandomRange(-40, 40) * player->mo->scale;
			fixed_t randz = P_RandomRange(0, player->mo->height >> FRACBITS) << FRACBITS;
			mobj_t *star = P_SpawnMobj(
				player->mo->x + randx,
				player->mo->y + randy,
				player->mo->z + randz,
				MT_KARMAFIREWORK);

			star->color = player->kartstuff[k_springcolor];
			star->flags |= MF_NOGRAVITY;
			star->momx = player->mo->momx / 2;
			star->momy = player->mo->momy / 2;
			star->momz = player->mo->momz / 2;
			star->fuse = 12;
			star->scale = player->mo->scale;
			star->destscale = star->scale / 2;

			player->kartstuff[k_springstars]--;
		}
	}

	if (player->playerstate == PST_DEAD || (player->respawn.state == RESPAWNST_MOVE)) // Ensure these are set correctly here
	{
		player->mo->colorized = false;
		player->mo->color = player->skincolor;
	}
	else if (player->kartstuff[k_eggmanexplode]) // You're gonna diiiiie
	{
		const INT32 flashtime = 4<<(player->kartstuff[k_eggmanexplode]/TICRATE);
		if (player->kartstuff[k_eggmanexplode] == 1 || (player->kartstuff[k_eggmanexplode] % (flashtime/2) != 0))
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
		else if (player->kartstuff[k_eggmanexplode] % flashtime == 0)
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_BLACK;
		}
		else
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_CRIMSON;
		}
	}
	else if (player->kartstuff[k_invincibilitytimer]) // setting players to use the star colormap and spawning afterimages
	{
		player->mo->colorized = true;
	}
	else if (player->kartstuff[k_growshrinktimer]) // Ditto, for grow/shrink
	{
		if (player->kartstuff[k_growshrinktimer] % 5 == 0)
		{
			player->mo->colorized = true;
			player->mo->color = (player->kartstuff[k_growshrinktimer] < 0 ? SKINCOLOR_CREAMSICLE : SKINCOLOR_PERIWINKLE);
		}
		else
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
	}
	else if (player->kartstuff[k_killfield]) // You're gonna REALLY diiiiie
	{
		const INT32 flashtime = 4<<(4-(player->kartstuff[k_killfield]/TICRATE));
		if (player->kartstuff[k_killfield] == 1 || (player->kartstuff[k_killfield] % (flashtime/2) != 0))
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
		else if (player->kartstuff[k_killfield] % flashtime == 0)
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_BYZANTIUM;
		}
		else
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_RUBY;
		}
	}
	else if (player->kartstuff[k_ringboost] && (leveltime & 1)) // ring boosting
	{
		player->mo->colorized = true;
	}
	else
	{
		player->mo->colorized = false;
	}

	if (player->kartstuff[k_itemtype] == KITEM_NONE)
		player->kartstuff[k_holdready] = 0;

	// DKR style camera for boosting
	if (player->karthud[khud_boostcam] != 0 || player->karthud[khud_destboostcam] != 0)
	{
		if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam]
			&& player->karthud[khud_destboostcam] != 0)
		{
			player->karthud[khud_boostcam] += FRACUNIT/(TICRATE/4);
			if (player->karthud[khud_boostcam] >= player->karthud[khud_destboostcam])
				player->karthud[khud_destboostcam] = 0;
		}
		else
		{
			player->karthud[khud_boostcam] -= FRACUNIT/TICRATE;
			if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam])
				player->karthud[khud_boostcam] = player->karthud[khud_destboostcam] = 0;
		}
		//CONS_Printf("cam: %d, dest: %d\n", player->karthud[khud_boostcam], player->karthud[khud_destboostcam]);
	}

	player->karthud[khud_timeovercam] = 0;

	// Specific hack because it insists on setting flashing tics during this anyway...
	if (player->kartstuff[k_spinouttype] == 2)
	{
		player->powers[pw_flashing] = 0;
	}
	// Make ABSOLUTELY SURE that your flashing tics don't get set WHILE you're still in hit animations.
	else if (player->kartstuff[k_spinouttimer] != 0
		|| player->kartstuff[k_wipeoutslow] != 0
		|| player->kartstuff[k_squishedtimer] != 0)
	{
		player->powers[pw_flashing] = K_GetKartFlashing(player);
	}
	else if (player->powers[pw_flashing] >= K_GetKartFlashing(player))
	{
		player->powers[pw_flashing]--;
	}

	if (player->kartstuff[k_spinouttimer])
	{
		if ((P_IsObjectOnGround(player->mo)
			|| (player->kartstuff[k_spinouttype] != 0))
			&& (!EITHERSNEAKER(player)))
		{
			player->kartstuff[k_spinouttimer]--;
			if (player->kartstuff[k_wipeoutslow] > 1)
				player->kartstuff[k_wipeoutslow]--;
			// Actually, this caused more problems than it solved. Just make sure you set type before you spinout. Which K_SpinPlayer always does.
			/*if (player->kartstuff[k_spinouttimer] == 0)
				player->kartstuff[k_spinouttype] = 0;*/ // Reset type
		}
	}
	else
	{
		if (player->kartstuff[k_wipeoutslow] >= 1)
			player->mo->friction = ORIG_FRICTION;
		player->kartstuff[k_wipeoutslow] = 0;
		if (!comeback)
			player->kartstuff[k_comebacktimer] = comebacktime;
		else if (player->kartstuff[k_comebacktimer])
		{
			player->kartstuff[k_comebacktimer]--;
			if (P_IsDisplayPlayer(player) && player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer] <= 0)
				comebackshowninfo = true; // client has already seen the message
		}
	}

	if (player->kartstuff[k_rings] > 20)
		player->kartstuff[k_rings] = 20;
	else if (player->kartstuff[k_rings] < -20)
		player->kartstuff[k_rings] = -20;

	if (player->kartstuff[k_ringdelay])
		player->kartstuff[k_ringdelay]--;

	if (player->kartstuff[k_spinouttimer] || player->kartstuff[k_squishedtimer])
		player->kartstuff[k_ringboost] = 0;
	else if (player->kartstuff[k_ringboost])
		player->kartstuff[k_ringboost]--;

	if (player->kartstuff[k_sneakertimer])
		player->kartstuff[k_sneakertimer]--;

	if (player->kartstuff[k_levelbooster])
		player->kartstuff[k_levelbooster]--;

	if (player->kartstuff[k_flamedash])
		player->kartstuff[k_flamedash]--;

	if (EITHERSNEAKER(player) && player->kartstuff[k_wipeoutslow] > 0 && player->kartstuff[k_wipeoutslow] < wipeoutslowtime+1)
		player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;

	if (player->kartstuff[k_floorboost])
		player->kartstuff[k_floorboost]--;

	if (player->kartstuff[k_driftboost])
		player->kartstuff[k_driftboost]--;

	if (player->kartstuff[k_startboost])
		player->kartstuff[k_startboost]--;

	if (player->kartstuff[k_invincibilitytimer])
		player->kartstuff[k_invincibilitytimer]--;

	if ((player->respawn.state == RESPAWNST_NONE) && player->kartstuff[k_growshrinktimer] != 0)
	{
		if (player->kartstuff[k_growshrinktimer] > 0)
			player->kartstuff[k_growshrinktimer]--;
		if (player->kartstuff[k_growshrinktimer] < 0)
			player->kartstuff[k_growshrinktimer]++;

		// Back to normal
		if (player->kartstuff[k_growshrinktimer] == 0)
			K_RemoveGrowShrink(player);
	}

	if (player->kartstuff[k_superring])
	{
		if (player->kartstuff[k_superring] % 3 == 0)
		{
			mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
			ring->extravalue1 = 1; // Ring collect animation timer
			ring->angle = player->mo->angle; // animation angle
			P_SetTarget(&ring->target, player->mo); // toucher for thinker
			player->kartstuff[k_pickuprings]++;
			if (player->kartstuff[k_superring] <= 3)
				ring->cvmem = 1; // play caching when collected
		}
		player->kartstuff[k_superring]--;
	}

	if (player->kartstuff[k_stealingtimer] == 0 && player->kartstuff[k_stolentimer] == 0
		&& player->kartstuff[k_rocketsneakertimer])
		player->kartstuff[k_rocketsneakertimer]--;

	if (player->kartstuff[k_hyudorotimer])
		player->kartstuff[k_hyudorotimer]--;

	if (player->kartstuff[k_sadtimer])
		player->kartstuff[k_sadtimer]--;

	if (player->kartstuff[k_stealingtimer])
		player->kartstuff[k_stealingtimer]--;

	if (player->kartstuff[k_stolentimer])
		player->kartstuff[k_stolentimer]--;

	if (player->kartstuff[k_squishedtimer])
	{
		player->kartstuff[k_squishedtimer]--;

		if ((player->kartstuff[k_squishedtimer] == 0) && !(player->pflags & PF_NOCLIP))
		{
			player->mo->flags &= ~MF_NOCLIP;
		}
	}

	if (player->kartstuff[k_justbumped])
		player->kartstuff[k_justbumped]--;

	if (player->kartstuff[k_tiregrease])
		player->kartstuff[k_tiregrease]--;

	// This doesn't go in HUD update because it has potential gameplay ramifications
	if (player->karthud[khud_itemblink] && player->karthud[khud_itemblink]-- <= 0)
	{
		player->karthud[khud_itemblinkmode] = 0;
		player->karthud[khud_itemblink] = 0;
	}

	K_KartPlayerHUDUpdate(player);

	if (G_BattleGametype() && player->kartstuff[k_bumper] > 0
		&& !player->kartstuff[k_spinouttimer] && !player->kartstuff[k_squishedtimer]
		&& (player->respawn.state == RESPAWNST_DROP) && !player->powers[pw_flashing])
	{
		player->kartstuff[k_wanted]++;
		if (battleovertime.enabled >= 10*TICRATE)
		{
			if (P_AproxDistance(player->mo->x - battleovertime.x, player->mo->y - battleovertime.y) > battleovertime.radius)
			{
				player->kartstuff[k_killfield]++;
				if (player->kartstuff[k_killfield] > 4*TICRATE)
				{
					K_SpinPlayer(player, NULL, 0, NULL, false);
					//player->kartstuff[k_killfield] = 1;
				}
			}
			else if (player->kartstuff[k_killfield] > 0)
				player->kartstuff[k_killfield]--;
		}
	}
	else if (player->kartstuff[k_killfield] > 0)
		player->kartstuff[k_killfield]--;

	if (P_IsObjectOnGround(player->mo))
		player->kartstuff[k_waterskip] = 0;

	if (player->kartstuff[k_instashield])
		player->kartstuff[k_instashield]--;

	if (player->kartstuff[k_eggmanexplode])
	{
		if (player->spectator || (G_BattleGametype() && !player->kartstuff[k_bumper]))
			player->kartstuff[k_eggmanexplode] = 0;
		else
		{
			player->kartstuff[k_eggmanexplode]--;
			if (player->kartstuff[k_eggmanexplode] <= 0)
			{
				mobj_t *eggsexplode;
				//player->powers[pw_flashing] = 0;
				eggsexplode = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SPBEXPLOSION);
				if (player->kartstuff[k_eggmanblame] >= 0
				&& player->kartstuff[k_eggmanblame] < MAXPLAYERS
				&& playeringame[player->kartstuff[k_eggmanblame]]
				&& !players[player->kartstuff[k_eggmanblame]].spectator
				&& players[player->kartstuff[k_eggmanblame]].mo)
					P_SetTarget(&eggsexplode->target, players[player->kartstuff[k_eggmanblame]].mo);
			}
		}
	}

	if (player->kartstuff[k_itemtype] == KITEM_THUNDERSHIELD)
	{
		if (RINGTOTAL(player) < 20 && !player->kartstuff[k_ringlock])
			K_LookForRings(player->mo);
	}

	if (player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD)
	{
		if (player->kartstuff[k_bubblecool])
			player->kartstuff[k_bubblecool]--;
	}
	else
	{
		player->kartstuff[k_bubbleblowup] = 0;
		player->kartstuff[k_bubblecool] = 0;
	}

	if (player->kartstuff[k_itemtype] != KITEM_FLAMESHIELD)
	{
		if (player->kartstuff[k_flamedash])
			K_FlameDashLeftoverSmoke(player->mo);
	}

	if (player->kartstuff[k_comebacktimer])
		player->kartstuff[k_comebackmode] = 0;

	if (P_IsObjectOnGround(player->mo) && player->kartstuff[k_pogospring])
	{
		if (P_MobjFlip(player->mo)*player->mo->momz <= 0)
			player->kartstuff[k_pogospring] = 0;
	}

	if (cmd->buttons & BT_DRIFT)
		player->kartstuff[k_jmp] = 1;
	else
		player->kartstuff[k_jmp] = 0;

	// Roulette Code
	K_KartItemRoulette(player, cmd);

	// Handle invincibility sfx
	K_UpdateInvincibilitySounds(player); // Also thanks, VAda!

	// Plays the music after the starting countdown.
	if (P_IsLocalPlayer(player) && leveltime == (starttime + (TICRATE/2)))
	{
		S_ChangeMusic(mapmusname, mapmusflags, true);
		S_ShowMusicCredit();
	}
}

void K_KartPlayerAfterThink(player_t *player)
{
	if (player->kartstuff[k_curshield]
		|| player->kartstuff[k_invincibilitytimer]
		|| (player->kartstuff[k_growshrinktimer] != 0 && player->kartstuff[k_growshrinktimer] % 5 == 4)) // 4 instead of 0 because this is afterthink!
	{
		player->mo->frame |= FF_FULLBRIGHT;
	}
	else
	{
		if (!(player->mo->state->frame & FF_FULLBRIGHT))
			player->mo->frame &= ~FF_FULLBRIGHT;
	}

	// Move held objects (Bananas, Orbinaut, etc)
	K_MoveHeldObjects(player);

	// Jawz reticule (seeking)
	if (player->kartstuff[k_itemtype] == KITEM_JAWZ && player->kartstuff[k_itemheld])
	{
		INT32 lasttarg = player->kartstuff[k_lastjawztarget];
		player_t *targ;
		mobj_t *ret;

		if (player->kartstuff[k_jawztargetdelay] && playeringame[lasttarg] && !players[lasttarg].spectator)
		{
			targ = &players[lasttarg];
			player->kartstuff[k_jawztargetdelay]--;
		}
		else
			targ = K_FindJawzTarget(player->mo, player);

		if (!targ || !targ->mo || P_MobjWasRemoved(targ->mo))
		{
			player->kartstuff[k_lastjawztarget] = -1;
			player->kartstuff[k_jawztargetdelay] = 0;
			return;
		}

		ret = P_SpawnMobj(targ->mo->x, targ->mo->y, targ->mo->z, MT_PLAYERRETICULE);
		P_SetTarget(&ret->target, targ->mo);
		ret->frame |= ((leveltime % 10) / 2);
		ret->tics = 1;
		ret->color = player->skincolor;

		if (targ-players != lasttarg)
		{
			if (P_IsDisplayPlayer(player) || P_IsDisplayPlayer(targ))
				S_StartSound(NULL, sfx_s3k89);
			else
				S_StartSound(targ->mo, sfx_s3k89);

			player->kartstuff[k_lastjawztarget] = targ-players;
			player->kartstuff[k_jawztargetdelay] = 5;
		}
	}
	else
	{
		player->kartstuff[k_lastjawztarget] = -1;
		player->kartstuff[k_jawztargetdelay] = 0;
	}
}

/*--------------------------------------------------
	static waypoint_t *K_GetPlayerNextWaypoint(player_t *player)

		Gets the next waypoint of a player, by finding their closest waypoint, then checking which of itself and next or
		previous waypoints are infront of the player.

	Input Arguments:-
		player - The player the next waypoint is being found for

	Return:-
		The waypoint that is the player's next waypoint
--------------------------------------------------*/
static waypoint_t *K_GetPlayerNextWaypoint(player_t *player)
{
	waypoint_t *bestwaypoint = NULL;

	if ((player != NULL) && (player->mo != NULL) && (P_MobjWasRemoved(player->mo) == false))
	{
		waypoint_t *waypoint     = K_GetBestWaypointForMobj(player->mo);
		boolean    updaterespawn = false;

		bestwaypoint = waypoint;

		// check the waypoint's location in relation to the player
		// If it's generally in front, it's fine, otherwise, use the best next/previous waypoint.
		// EXCEPTION: If our best waypoint is the finishline AND we're facing towards it, don't do this.
		// Otherwise it breaks the distance calculations.
		if (waypoint != NULL)
		{
			boolean finishlinehack  = false;
			angle_t playerangle     = player->mo->angle;
			angle_t momangle        = player->mo->angle;
			angle_t angletowaypoint =
				R_PointToAngle2(player->mo->x, player->mo->y, waypoint->mobj->x, waypoint->mobj->y);
			angle_t angledelta      = ANGLE_MAX;
			angle_t momdelta        = ANGLE_MAX;

			if (player->mo->momx != 0 || player->mo->momy != 0)
			{
				// Defaults to facing angle if you're not moving.
				momangle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
			}

			angledelta = playerangle - angletowaypoint;
			if (angledelta > ANGLE_180)
			{
				angledelta = InvAngle(angledelta);
			}

			momdelta = momangle - angletowaypoint;
			if (momdelta > ANGLE_180)
			{
				momdelta = InvAngle(momdelta);
			}

			if (bestwaypoint == K_GetFinishLineWaypoint())
			{
				// facing towards the finishline
				if (angledelta <= ANGLE_90)
				{
					finishlinehack = true;
				}
			}

			// We're using a lot of angle calculations here, because only using facing angle or only using momentum angle both have downsides.
			// nextwaypoints will be picked if you're facing OR moving forward.
			// prevwaypoints will be picked if you're facing AND moving backward.
			if ((angledelta > ANGLE_45 || momdelta > ANGLE_45)
			&& (finishlinehack == false))
			{
				angle_t nextbestdelta = angledelta;
				angle_t nextbestmomdelta = momdelta;
				size_t i = 0U;

				if (K_PlayerUsesBotMovement(player))
				{
					// Try to force bots to use a next waypoint
					nextbestdelta = ANGLE_MAX;
					nextbestmomdelta = ANGLE_MAX;
				}

				if ((waypoint->nextwaypoints != NULL) && (waypoint->numnextwaypoints > 0U))
				{
					for (i = 0U; i < waypoint->numnextwaypoints; i++)
					{
						if (K_PlayerUsesBotMovement(player) == true
						&& K_GetWaypointIsShortcut(waypoint->nextwaypoints[i]) == true
						&& K_BotCanTakeCut(player) == false)
						{
							// Bots that aren't able to take a shortcut will ignore shortcut waypoints.
							// (However, if they're already on a shortcut, then we want them to keep going.)

							if (player->nextwaypoint == NULL
							|| K_GetWaypointIsShortcut(player->nextwaypoint) == false)
							{
								continue;
							}
						}

						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->nextwaypoints[i]->mobj->x, waypoint->nextwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestdelta || momdelta < nextbestmomdelta)
						{
							bestwaypoint = waypoint->nextwaypoints[i];

							if (angledelta < nextbestdelta)
							{
								nextbestdelta = angledelta;
							}
							if (momdelta < nextbestmomdelta)
							{
								nextbestmomdelta = momdelta;
							}

							// Remove wrong way flag if we're using nextwaypoints
							player->kartstuff[k_wrongway] = 0;
							updaterespawn = true;
						}
					}
				}

				if ((waypoint->prevwaypoints != NULL) && (waypoint->numprevwaypoints > 0U)
				&& !(K_PlayerUsesBotMovement(player))) // Bots do not need prev waypoints
				{
					for (i = 0U; i < waypoint->numprevwaypoints; i++)
					{
						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->prevwaypoints[i]->mobj->x, waypoint->prevwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestdelta && momdelta < nextbestmomdelta)
						{
							bestwaypoint = waypoint->prevwaypoints[i];

							nextbestdelta = angledelta;
							nextbestmomdelta = momdelta;

							// Set wrong way flag if we're using prevwaypoints
							player->kartstuff[k_wrongway] = 1;
							updaterespawn = false;
						}
					}
				}
			}
		}

		if (!P_IsObjectOnGround(player->mo))
		{
			updaterespawn = false;
		}

		// Respawn point should only be updated when we're going to a nextwaypoint
		if ((updaterespawn) &&
		(player->respawn.state == RESPAWNST_NONE) &&
		(bestwaypoint != NULL) &&
		(bestwaypoint != player->nextwaypoint) &&
		(K_GetWaypointIsSpawnpoint(bestwaypoint)) &&
		(K_GetWaypointIsEnabled(bestwaypoint) == true))
		{
			player->respawn.wp = bestwaypoint;
		}
	}

	return bestwaypoint;
}

static boolean K_PlayerCloserToNextWaypoints(waypoint_t *const waypoint, player_t *const player)
{
	boolean nextiscloser = true;

	if ((waypoint != NULL) && (player != NULL) && (player->mo != NULL))
	{
		size_t     i                   = 0U;
		waypoint_t *currentwpcheck     = NULL;
		angle_t    angletoplayer       = ANGLE_MAX;
		angle_t    currentanglecheck   = ANGLE_MAX;
		angle_t    bestangle           = ANGLE_MAX;

		angletoplayer = R_PointToAngle2(waypoint->mobj->x, waypoint->mobj->y,
			player->mo->x, player->mo->y);

		for (i = 0U; i < waypoint->numnextwaypoints; i++)
		{
			currentwpcheck = waypoint->nextwaypoints[i];
			currentanglecheck = R_PointToAngle2(
				waypoint->mobj->x, waypoint->mobj->y, currentwpcheck->mobj->x, currentwpcheck->mobj->y);

			// Get delta angle
			currentanglecheck = currentanglecheck - angletoplayer;

			if (currentanglecheck > ANGLE_180)
			{
				currentanglecheck = InvAngle(currentanglecheck);
			}

			if (currentanglecheck < bestangle)
			{
				bestangle = currentanglecheck;
			}
		}

		for (i = 0U; i < waypoint->numprevwaypoints; i++)
		{
			currentwpcheck = waypoint->prevwaypoints[i];
			currentanglecheck = R_PointToAngle2(
				waypoint->mobj->x, waypoint->mobj->y, currentwpcheck->mobj->x, currentwpcheck->mobj->y);

			// Get delta angle
			currentanglecheck = currentanglecheck - angletoplayer;

			if (currentanglecheck > ANGLE_180)
			{
				currentanglecheck = InvAngle(currentanglecheck);
			}

			if (currentanglecheck < bestangle)
			{
				bestangle = currentanglecheck;
				nextiscloser = false;
				break;
			}
		}
	}

	return nextiscloser;
}

/*--------------------------------------------------
	void K_UpdateDistanceFromFinishLine(player_t *const player)

		Updates the distance a player has to the finish line.

	Input Arguments:-
		player - The player the distance is being updated for

	Return:-
		None
--------------------------------------------------*/
void K_UpdateDistanceFromFinishLine(player_t *const player)
{
	if ((player != NULL) && (player->mo != NULL))
	{
		waypoint_t *finishline   = K_GetFinishLineWaypoint();
		waypoint_t *nextwaypoint = NULL;

		if (player->spectator)
		{
			// Don't update waypoints while spectating
			nextwaypoint = finishline;
		}
		else
		{
			nextwaypoint = K_GetPlayerNextWaypoint(player);
		}

		if (nextwaypoint != NULL)
		{
			// If nextwaypoint is NULL, it means we don't want to update the waypoint until we touch another one.
			// player->nextwaypoint will keep its previous value in this case.
			player->nextwaypoint = nextwaypoint;
		}

		// nextwaypoint is now the waypoint that is in front of us
		if (player->exiting || player->spectator)
		{
			// Player has finished, we don't need to calculate this
			player->distancetofinish = 0U;
		}
		else if ((player->nextwaypoint != NULL) && (finishline != NULL))
		{
			const boolean useshortcuts = false;
			const boolean huntbackwards = false;
			boolean pathfindsuccess = false;
			path_t pathtofinish = {};

			pathfindsuccess =
				K_PathfindToWaypoint(player->nextwaypoint, finishline, &pathtofinish, useshortcuts, huntbackwards);

			// Update the player's distance to the finish line if a path was found.
			// Using shortcuts won't find a path, so distance won't be updated until the player gets back on track
			if (pathfindsuccess == true)
			{
				// Add euclidean distance to the next waypoint to the distancetofinish
				UINT32 adddist;
				fixed_t disttowaypoint =
					P_AproxDistance(
						(player->mo->x >> FRACBITS) - (player->nextwaypoint->mobj->x >> FRACBITS),
						(player->mo->y >> FRACBITS) - (player->nextwaypoint->mobj->y >> FRACBITS));
				disttowaypoint = P_AproxDistance(disttowaypoint, (player->mo->z >> FRACBITS) - (player->nextwaypoint->mobj->z >> FRACBITS));

				adddist = (UINT32)disttowaypoint;

				player->distancetofinish = pathtofinish.totaldist + adddist;
				Z_Free(pathtofinish.array);

				// distancetofinish is currently a flat distance to the finish line, but in order to be fully
				// correct we need to add to it the length of the entire circuit multiplied by the number of laps
				// left after this one. This will give us the total distance to the finish line, and allow item
				// distance calculation to work easily
				if ((mapheaderinfo[gamemap - 1]->levelflags & LF_SECTIONRACE) == 0U)
				{
					const UINT8 numfulllapsleft = ((UINT8)cv_numlaps.value - player->laps);

					player->distancetofinish += numfulllapsleft * K_GetCircuitLength();

					// An additional HACK, to fix looking backwards towards the finish line
					// If the player's next waypoint is the finishline and the angle distance from player to
					// connectin waypoints implies they're closer to a next waypoint, add a full track distance
					if (player->nextwaypoint == finishline)
					{
						if (K_PlayerCloserToNextWaypoints(player->nextwaypoint, player) == true)
						{
							player->distancetofinish += K_GetCircuitLength();
						}
					}
				}
			}
		}
	}
}

INT32 K_GetKartRingPower(player_t *player)
{
	return (((9 - player->kartspeed) + (9 - player->kartweight)) / 2);
}

// Returns false if this player being placed here causes them to collide with any other player
// Used in g_game.c for match etc. respawning
// This does not check along the z because the z is not correctly set for the spawnee at this point
boolean K_CheckPlayersRespawnColliding(INT32 playernum, fixed_t x, fixed_t y)
{
	INT32 i;
	fixed_t p1radius = players[playernum].mo->radius;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playernum == i || !playeringame[i] || players[i].spectator || !players[i].mo || players[i].mo->health <= 0
			|| players[i].playerstate != PST_LIVE || (players[i].mo->flags & MF_NOCLIP) || (players[i].mo->flags & MF_NOCLIPTHING))
			continue;

		if (abs(x - players[i].mo->x) < (p1radius + players[i].mo->radius)
			&& abs(y - players[i].mo->y) < (p1radius + players[i].mo->radius))
		{
			return false;
		}
	}
	return true;
}

// countersteer is how strong the controls are telling us we are turning
// turndir is the direction the controls are telling us to turn, -1 if turning right and 1 if turning left
static INT16 K_GetKartDriftValue(player_t *player, fixed_t countersteer)
{
	INT16 basedrift, driftadjust;
	fixed_t driftweight = player->kartweight*14; // 12

	if (player->kartstuff[k_drift] == 0 || !P_IsObjectOnGround(player->mo))
	{
		// If they aren't drifting or on the ground, this doesn't apply
		return 0;
	}

	if (player->kartstuff[k_driftend] != 0)
	{
		// Drift has ended and we are tweaking their angle back a bit
		return -266*player->kartstuff[k_drift];
	}

	basedrift = (83 * player->kartstuff[k_drift]) - (((driftweight - 14) * player->kartstuff[k_drift]) / 5); // 415 - 303
	driftadjust = abs((252 - driftweight) * player->kartstuff[k_drift] / 5);

	if (player->kartstuff[k_tiregrease] > 0) // Buff drift-steering while in greasemode
	{
		basedrift += (basedrift / greasetics) * player->kartstuff[k_tiregrease];
	}

	if (player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER))
	{
		countersteer = 3*countersteer/2;
	}

	return basedrift + (FixedMul(driftadjust * FRACUNIT, countersteer) / FRACUNIT);
}

INT16 K_GetKartTurnValue(player_t *player, INT16 turnvalue)
{
	fixed_t p_maxspeed = K_GetKartSpeed(player, false);
	fixed_t p_speed = min(player->speed, (p_maxspeed * 2));
	fixed_t weightadjust = FixedDiv((p_maxspeed * 3) - p_speed, (p_maxspeed * 3) + (player->kartweight * FRACUNIT));

	if (player->spectator)
	{
		return turnvalue;
	}

	if (K_PlayerUsesBotMovement(player))
	{
		turnvalue = 5*turnvalue/4; // Base increase to turning
		turnvalue = FixedMul(
			turnvalue * FRACUNIT,
			K_BotRubberband(player)
		) / FRACUNIT;
	}

	if (player->kartstuff[k_drift] != 0 && P_IsObjectOnGround(player->mo))
	{
		fixed_t countersteer = FixedDiv(turnvalue*FRACUNIT, KART_FULLTURN*FRACUNIT);

		// If we're drifting we have a completely different turning value

		if (player->kartstuff[k_driftend] != 0)
		{
			countersteer = FRACUNIT;
		}

		turnvalue = K_GetKartDriftValue(player, countersteer);

		return turnvalue;
	}

	if (EITHERSNEAKER(player) || player->kartstuff[k_invincibilitytimer] || player->kartstuff[k_growshrinktimer] > 0)
	{
		turnvalue = 5*turnvalue/4;
	}

	if (player->kartstuff[k_flamedash] > 0)
	{
		fixed_t multiplier = K_FlameShieldDashVar(player->kartstuff[k_flamedash]);
		multiplier = FRACUNIT + (FixedDiv(multiplier, FRACUNIT/2) / 4);
		turnvalue = FixedMul(turnvalue * FRACUNIT, multiplier) / FRACUNIT;
	}

	if (player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER))
	{
		turnvalue = 3*turnvalue/2;
	}

	// Weight has a small effect on turning
	turnvalue = FixedMul(turnvalue * FRACUNIT, weightadjust) / FRACUNIT;

	return turnvalue;
}

INT32 K_GetKartDriftSparkValue(player_t *player)
{
	UINT8 kartspeed = (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
		? 1
		: player->kartspeed;
	return (26*4 + kartspeed*2 + (9 - player->kartweight))*8;
}

static void K_KartDrift(player_t *player, boolean onground)
{
	fixed_t minspeed = (10 * player->mo->scale);
	INT32 dsone = K_GetKartDriftSparkValue(player);
	INT32 dstwo = dsone*2;
	INT32 dsthree = dstwo*2;

	// Drifting is actually straffing + automatic turning.
	// Holding the Jump button will enable drifting.

	// Drift Release (Moved here so you can't "chain" drifts)
	if (player->kartstuff[k_drift] != -5 && player->kartstuff[k_drift] != 5)
	{
		if (player->kartstuff[k_driftcharge] < 0 || player->kartstuff[k_driftcharge] >= dsone)
		{
			angle_t pushdir = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
			//mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_DRIFTEXPLODE);
			//P_SetTarget(&overlay->target, player->mo);
			//P_SetScale(overlay, (overlay->destscale = player->mo->scale));
			//K_FlipFromObject(overlay, player->mo);

			S_StartSound(player->mo, sfx_s23c);
			//K_SpawnDashDustRelease(player);

			if (player->kartstuff[k_driftcharge] < 0)
			{
				// Stage 0: Yellow sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 8);

				if (player->kartstuff[k_driftboost] < 15)
					player->kartstuff[k_driftboost] = 15;

				//overlay->color = SKINCOLOR_GOLD;
				//overlay->fuse = 8;
			}
			else if (player->kartstuff[k_driftcharge] >= dsone && player->kartstuff[k_driftcharge] < dstwo)
			{
				// Stage 1: Red sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 4);

				if (player->kartstuff[k_driftboost] < 20)
					player->kartstuff[k_driftboost] = 20;

				//overlay->color = SKINCOLOR_KETCHUP;
				//overlay->fuse = 16;
			}
			else if (player->kartstuff[k_driftcharge] < dsthree)
			{
				// Stage 2: Blue sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 3);

				if (player->kartstuff[k_driftboost] < 50)
					player->kartstuff[k_driftboost] = 50;

				//overlay->color = SKINCOLOR_SAPPHIRE;
				//overlay->fuse = 32;
			}
			else if (player->kartstuff[k_driftcharge] >= dsthree)
			{
				// Stage 3: Rainbow sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 2);

				if (player->kartstuff[k_driftboost] < 125)
					player->kartstuff[k_driftboost] = 125;

				//overlay->color = SKINCOLOR_SILVER;
				//overlay->fuse = 120;
			}
		}

		// Remove charge
		player->kartstuff[k_driftcharge] = 0;
	}

	// Drifting: left or right?
	if ((player->cmd.driftturn > 0) && player->speed > minspeed && player->kartstuff[k_jmp] == 1
		&& (player->kartstuff[k_drift] == 0 || player->kartstuff[k_driftend] == 1)) // && player->kartstuff[k_drift] != 1)
	{
		// Starting left drift
		player->kartstuff[k_drift] = 1;
		player->kartstuff[k_driftend] = player->kartstuff[k_driftcharge] = 0;
	}
	else if ((player->cmd.driftturn < 0) && player->speed > minspeed && player->kartstuff[k_jmp] == 1
		&& (player->kartstuff[k_drift] == 0 || player->kartstuff[k_driftend] == 1)) // && player->kartstuff[k_drift] != -1)
	{
		// Starting right drift
		player->kartstuff[k_drift] = -1;
		player->kartstuff[k_driftend] = player->kartstuff[k_driftcharge] = 0;
	}
	else if (player->kartstuff[k_jmp] == 0) // || player->kartstuff[k_turndir] == 0)
	{
		// drift is not being performed so if we're just finishing set driftend and decrement counters
		if (player->kartstuff[k_drift] > 0)
		{
			player->kartstuff[k_drift]--;
			player->kartstuff[k_driftend] = 1;
		}
		else if (player->kartstuff[k_drift] < 0)
		{
			player->kartstuff[k_drift]++;
			player->kartstuff[k_driftend] = 1;
		}
		else
			player->kartstuff[k_driftend] = 0;
	}

	if (player->kartstuff[k_spinouttimer] > 0 || player->speed == 0)
	{
		// Stop drifting
		player->kartstuff[k_drift] = player->kartstuff[k_driftcharge] = 0;
		player->kartstuff[k_aizdriftstrat] = player->kartstuff[k_brakedrift] = 0;
		player->kartstuff[k_getsparks] = 0;
	}
	else if (player->kartstuff[k_jmp] == 1 && player->kartstuff[k_drift] != 0)
	{
		// Incease/decrease the drift value to continue drifting in that direction
		fixed_t driftadditive = 24;
		boolean playsound = false;

		if (onground)
		{
			if (player->kartstuff[k_drift] >= 1) // Drifting to the left
			{
				player->kartstuff[k_drift]++;
				if (player->kartstuff[k_drift] > 5)
					player->kartstuff[k_drift] = 5;

				if (player->cmd.driftturn > 0) // Inward
					driftadditive += abs(player->cmd.driftturn)/100;
				if (player->cmd.driftturn < 0) // Outward
					driftadditive -= abs(player->cmd.driftturn)/75;
			}
			else if (player->kartstuff[k_drift] <= -1) // Drifting to the right
			{
				player->kartstuff[k_drift]--;
				if (player->kartstuff[k_drift] < -5)
					player->kartstuff[k_drift] = -5;

				if (player->cmd.driftturn < 0) // Inward
					driftadditive += abs(player->cmd.driftturn)/100;
				if (player->cmd.driftturn > 0) // Outward
					driftadditive -= abs(player->cmd.driftturn)/75;
			}

			// Disable drift-sparks until you're going fast enough
			if (player->kartstuff[k_getsparks] == 0
				|| (player->kartstuff[k_offroad] && K_ApplyOffroad(player)))
				driftadditive = 0;

			// Inbetween minspeed and minspeed*2, it'll keep your previous drift-spark state.
			if (player->speed > minspeed*2)
			{
				player->kartstuff[k_getsparks] = 1;

				if (player->kartstuff[k_driftcharge] <= -1)
				{
					player->kartstuff[k_driftcharge] = dsone; // Back to red
					playsound = true;
				}
			}
			else if (player->speed <= minspeed)
			{
				player->kartstuff[k_getsparks] = 0;
				driftadditive = 0;

				if (player->kartstuff[k_driftcharge] >= dsone)
				{
					player->kartstuff[k_driftcharge] = -1; // Set yellow sparks
					playsound = true;
				}
			}
		}
		else
		{
			driftadditive = 0;
		}

		// This spawns the drift sparks
		if ((player->kartstuff[k_driftcharge] + driftadditive >= dsone)
			|| (player->kartstuff[k_driftcharge] < 0))
		{
			K_SpawnDriftSparks(player);
		}

		if ((player->kartstuff[k_driftcharge] < dsone && player->kartstuff[k_driftcharge]+driftadditive >= dsone)
			|| (player->kartstuff[k_driftcharge] < dstwo && player->kartstuff[k_driftcharge]+driftadditive >= dstwo)
			|| (player->kartstuff[k_driftcharge] < dsthree && player->kartstuff[k_driftcharge]+driftadditive >= dsthree))
		{
			playsound = true;
		}

		// Sound whenever you get a different tier of sparks
		if (playsound && P_IsDisplayPlayer(player))
		{
			if (player->kartstuff[k_driftcharge] == -1)
				S_StartSoundAtVolume(player->mo, sfx_sploss, 192); // Yellow spark sound
			else
				S_StartSoundAtVolume(player->mo, sfx_s3ka2, 192);
		}

		player->kartstuff[k_driftcharge] += driftadditive;
		player->kartstuff[k_driftend] = 0;
	}

	if ((!EITHERSNEAKER(player))
	|| (!player->cmd.driftturn)
	|| (!player->kartstuff[k_aizdriftstrat])
	|| (player->cmd.driftturn > 0) != (player->kartstuff[k_aizdriftstrat] > 0))
	{
		if (!player->kartstuff[k_drift])
			player->kartstuff[k_aizdriftstrat] = 0;
		else
			player->kartstuff[k_aizdriftstrat] = ((player->kartstuff[k_drift] > 0) ? 1 : -1);
	}
	else if (player->kartstuff[k_aizdriftstrat] && !player->kartstuff[k_drift])
		K_SpawnAIZDust(player);

	if (player->kartstuff[k_drift]
		&& ((player->cmd.buttons & BT_BRAKE)
		|| !(player->cmd.buttons & BT_ACCELERATE))
		&& P_IsObjectOnGround(player->mo))
	{
		if (!player->kartstuff[k_brakedrift])
			K_SpawnBrakeDriftSparks(player);
		player->kartstuff[k_brakedrift] = 1;
	}
	else
		player->kartstuff[k_brakedrift] = 0;
}
//
// K_KartUpdatePosition
//
void K_KartUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->kartstuff[k_position];
	fixed_t i;

	if (player->spectator || !player->mo)
	{
		// Ensure these are reset for spectators
		player->kartstuff[k_position] = 0;
		player->kartstuff[k_positiondelay] = 0;
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

		if (G_RaceGametype())
		{
			if (player->exiting) // End of match standings
			{
				// Only time matters
				if (players[i].realtime < player->realtime)
					position++;
			}
			else
			{
				// I'm a lap behind this player OR
				// My distance to the finish line is higher, so I'm behind
				if ((players[i].laps > player->laps)
					|| (players[i].distancetofinish < player->distancetofinish))
				{
					position++;
				}
			}
		}
		else if (G_BattleGametype())
		{
			if (player->exiting) // End of match standings
			{
				// Only score matters
				if (players[i].marescore > player->marescore)
					position++;
			}
			else
			{
				// I have less points than but the same bumpers as this player OR
				// I have less bumpers than this player
				if ((players[i].kartstuff[k_bumper] == player->kartstuff[k_bumper] && players[i].marescore > player->marescore)
					|| (players[i].kartstuff[k_bumper] > player->kartstuff[k_bumper]))
					position++;
			}
		}
	}

	if (leveltime < starttime || oldposition == 0)
		oldposition = position;

	if (oldposition != position) // Changed places?
		player->kartstuff[k_positiondelay] = 10; // Position number growth

	player->kartstuff[k_position] = position;
}

//
// K_StripItems
//
void K_StripItems(player_t *player)
{
	player->kartstuff[k_itemtype] = KITEM_NONE;
	player->kartstuff[k_itemamount] = 0;
	player->kartstuff[k_itemheld] = 0;

	player->kartstuff[k_rocketsneakertimer] = 0;

	if (!player->kartstuff[k_itemroulette] || player->kartstuff[k_roulettetype] != 2)
	{
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
	}
	player->kartstuff[k_eggmanheld] = 0;

	player->kartstuff[k_hyudorotimer] = 0;
	player->kartstuff[k_stealingtimer] = 0;
	player->kartstuff[k_stolentimer] = 0;

	player->kartstuff[k_curshield] = KSHIELD_NONE;
	player->kartstuff[k_bananadrag] = 0;

	player->kartstuff[k_sadtimer] = 0;

	K_UpdateHnextList(player, true);
}

void K_StripOther(player_t *player)
{
	player->kartstuff[k_itemroulette] = 0;
	player->kartstuff[k_roulettetype] = 0;

	player->kartstuff[k_invincibilitytimer] = 0;
	K_RemoveGrowShrink(player);

	if (player->kartstuff[k_eggmanexplode])
	{
		player->kartstuff[k_eggmanexplode] = 0;
		player->kartstuff[k_eggmanblame] = -1;
	}
}

static INT32 K_FlameShieldMax(player_t *player)
{
	UINT32 disttofinish = 0;
	UINT32 distv = DISTVAR;
	UINT8 numplayers = 0;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator)
			numplayers++;
		if (players[i].kartstuff[k_position] == 1)
			disttofinish = players[i].distancetofinish;
	}

	if (numplayers <= 1)
	{
		return 16; // max when alone, for testing
	}
	else if (player->kartstuff[k_position] == 1)
	{
		return 0; // minimum for first
	}

	disttofinish = player->distancetofinish - disttofinish;
	distv = FixedDiv(distv * FRACUNIT, mapobjectscale) / FRACUNIT;
	return min(16, 1 + (disttofinish / distv));
}

//
// K_MoveKartPlayer
//
void K_MoveKartPlayer(player_t *player, boolean onground)
{
	ticcmd_t *cmd = &player->cmd;
	boolean ATTACK_IS_DOWN = ((cmd->buttons & BT_ATTACK) && !(player->pflags & PF_ATTACKDOWN));
	boolean HOLDING_ITEM = (player->kartstuff[k_itemheld] || player->kartstuff[k_eggmanheld]);
	boolean NO_HYUDORO = (player->kartstuff[k_stolentimer] == 0 && player->kartstuff[k_stealingtimer] == 0);

	player->pflags &= ~PF_HITFINISHLINE;

	if (!player->exiting)
	{
		if (player->kartstuff[k_oldposition] < player->kartstuff[k_position]) // But first, if you lost a place,
		{
			player->kartstuff[k_oldposition] = player->kartstuff[k_position]; // then the other player taunts.
			K_RegularVoiceTimers(player); // and you can't for a bit
		}
		else if (player->kartstuff[k_oldposition] > player->kartstuff[k_position]) // Otherwise,
		{
			K_PlayOvertakeSound(player->mo); // Say "YOU'RE TOO SLOW!"
			player->kartstuff[k_oldposition] = player->kartstuff[k_position]; // Restore the old position,
		}
	}

	if (player->kartstuff[k_positiondelay])
		player->kartstuff[k_positiondelay]--;

	// Prevent ring misfire
	if (!(cmd->buttons & BT_ATTACK))
	{
		if (player->kartstuff[k_itemtype] == KITEM_NONE
			&& NO_HYUDORO && !(HOLDING_ITEM
			|| player->kartstuff[k_itemamount]
			|| player->kartstuff[k_itemroulette]
			|| player->kartstuff[k_rocketsneakertimer]
			|| player->kartstuff[k_eggmanexplode]))
			player->kartstuff[k_userings] = 1;
		else
			player->kartstuff[k_userings] = 0;
	}

	if ((player->pflags & PF_ATTACKDOWN) && !(cmd->buttons & BT_ATTACK))
		player->pflags &= ~PF_ATTACKDOWN;
	else if (cmd->buttons & BT_ATTACK)
		player->pflags |= PF_ATTACKDOWN;

	if (player && player->mo && player->mo->health > 0 && !player->spectator && !mapreset && leveltime > starttime
		&& player->kartstuff[k_spinouttimer] == 0 && player->kartstuff[k_squishedtimer] == 0 && (player->respawn.state == RESPAWNST_NONE))
	{
		// First, the really specific, finicky items that function without the item being directly in your item slot.
		// Karma item dropping
		if (player->kartstuff[k_comebackmode] && !player->kartstuff[k_comebacktimer])
		{
			if (ATTACK_IS_DOWN)
			{
				mobj_t *newitem;

				if (player->kartstuff[k_comebackmode] == 1)
				{
					newitem = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RANDOMITEM);
					newitem->threshold = 69; // selected "randomly".
				}
				else
				{
					newitem = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EGGMANITEM);
					if (player->kartstuff[k_eggmanblame] >= 0
					&& player->kartstuff[k_eggmanblame] < MAXPLAYERS
					&& playeringame[player->kartstuff[k_eggmanblame]]
					&& !players[player->kartstuff[k_eggmanblame]].spectator
					&& players[player->kartstuff[k_eggmanblame]].mo)
						P_SetTarget(&newitem->target, players[player->kartstuff[k_eggmanblame]].mo);
					player->kartstuff[k_eggmanblame] = -1;
				}

				newitem->flags2 = (player->mo->flags2 & MF2_OBJECTFLIP);
				newitem->fuse = 15*TICRATE; // selected randomly.

				player->kartstuff[k_comebackmode] = 0;
				player->kartstuff[k_comebacktimer] = comebacktime;
				S_StartSound(player->mo, sfx_s254);
			}
		}
		else
		{
			// Ring boosting
			if (player->kartstuff[k_userings])
			{
				if ((player->pflags & PF_ATTACKDOWN) && !player->kartstuff[k_ringdelay] && player->kartstuff[k_rings] > 0)
				{
					mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
					P_SetMobjState(ring, S_FASTRING1);
					ring->extravalue1 = 1; // Ring use animation timer
					ring->extravalue2 = 1; // Ring use animation flag
					ring->shadowscale = 0;
					P_SetTarget(&ring->target, player->mo); // user
					player->kartstuff[k_rings]--;
					player->kartstuff[k_ringdelay] = 3;
				}
			}
			// Other items
			else
			{
				// Eggman Monitor exploding
				if (player->kartstuff[k_eggmanexplode])
				{
					if (ATTACK_IS_DOWN && player->kartstuff[k_eggmanexplode] <= 3*TICRATE && player->kartstuff[k_eggmanexplode] > 1)
						player->kartstuff[k_eggmanexplode] = 1;
				}
				// Eggman Monitor throwing
				else if (player->kartstuff[k_eggmanheld])
				{
					if (ATTACK_IS_DOWN)
					{
						K_ThrowKartItem(player, false, MT_EGGMANITEM, -1, 0);
						K_PlayAttackTaunt(player->mo);
						player->kartstuff[k_eggmanheld] = 0;
						K_UpdateHnextList(player, true);
					}
				}
				// Rocket Sneaker usage
				else if (player->kartstuff[k_rocketsneakertimer] > 1)
				{
					if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
					{
						K_DoSneaker(player, 2);
						K_PlayBoostTaunt(player->mo);
						player->kartstuff[k_rocketsneakertimer] -= 2*TICRATE;
						if (player->kartstuff[k_rocketsneakertimer] < 1)
							player->kartstuff[k_rocketsneakertimer] = 1;
					}
				}
				else if (player->kartstuff[k_itemamount] <= 0)
				{
					player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
				}
				else
				{
					switch (player->kartstuff[k_itemtype])
					{
						case KITEM_SNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
							{
								K_DoSneaker(player, 1);
								K_PlayBoostTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_ROCKETSNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO
								&& player->kartstuff[k_rocketsneakertimer] == 0)
							{
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								K_PlayBoostTaunt(player->mo);
								//player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								//K_DoSneaker(player, 2);

								player->kartstuff[k_rocketsneakertimer] = (itemtime*3);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, true);

								for (moloop = 0; moloop < 2; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ROCKETSNEAKER);
									K_MatchGenericExtraFlags(mo, player->mo);
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = player->mo->angle;
									mo->threshold = 10;
									mo->movecount = moloop%2;
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							break;
						case KITEM_INVINCIBILITY:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO) // Doesn't hold your item slot hostage normally, so you're free to waste it if you have multiple
							{
								if (!player->kartstuff[k_invincibilitytimer])
								{
									mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INVULNFLASH);
									P_SetTarget(&overlay->target, player->mo);
									overlay->destscale = player->mo->scale;
									P_SetScale(overlay, player->mo->scale);
								}
								player->kartstuff[k_invincibilitytimer] = itemtime+(2*TICRATE); // 10 seconds
								if (P_IsDisplayPlayer(player))
									S_ChangeMusicSpecial("kinvnc");
								else
									S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmg : sfx_kinvnc));
								P_RestoreMusic(player);
								K_PlayPowerGloatSound(player->mo);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_BANANA:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 moloop;
								mobj_t *mo;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BANANA_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld]) // Banana x3 thrown
							{
								K_ThrowKartItem(player, false, MT_BANANA, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_EGGMAN:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_eggmanheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EGGMANITEM_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							break;
						case KITEM_ORBINAUT:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->kartstuff[k_itemamount]) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ORBINAUT_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = mo->lastlook = moloop+1;
									mo->color = player->skincolor;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld]) // Orbinaut x3 thrown
							{
								K_ThrowKartItem(player, true, MT_ORBINAUT, 1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_JAWZ:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->kartstuff[k_itemamount]) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_JAWZ_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && player->kartstuff[k_itemheld]) // Jawz thrown
							{
								if (player->kartstuff[k_throwdir] == 1 || player->kartstuff[k_throwdir] == 0)
									K_ThrowKartItem(player, true, MT_JAWZ, 1, 0);
								else if (player->kartstuff[k_throwdir] == -1) // Throwing backward gives you a dud that doesn't home in
									K_ThrowKartItem(player, true, MT_JAWZ_DUD, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_MINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SSMINE_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld])
							{
								K_ThrowKartItem(player, false, MT_SSMINE, 1, 1);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_itemheld] = 0;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_BALLHOG:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_ThrowKartItem(player, true, MT_BALLHOG, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_SPB:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_ThrowKartItem(player, true, MT_SPB, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_GROW:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								if (player->kartstuff[k_growshrinktimer] < 0) // If you're shrunk, then "grow" will just make you normal again.
									K_RemoveGrowShrink(player);
								else
								{
									K_PlayPowerGloatSound(player->mo);
									player->mo->scalespeed = mapobjectscale/TICRATE;
									player->mo->destscale = (3*mapobjectscale)/2;
									if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
										player->mo->destscale = (6*player->mo->destscale)/8;
									player->kartstuff[k_growshrinktimer] = itemtime+(4*TICRATE); // 12 seconds
									if (P_IsDisplayPlayer(player))
										S_ChangeMusicSpecial("kgrow");
									else
										S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmg : sfx_kgrow));
									P_RestoreMusic(player);
									S_StartSound(player->mo, sfx_kc5a);
								}
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_SHRINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoShrink(player);
								player->kartstuff[k_itemamount]--;
								K_PlayPowerGloatSound(player->mo);
							}
							break;
						case KITEM_THUNDERSHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_THUNDER)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THUNDERSHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k41);
								player->kartstuff[k_curshield] = KSHIELD_THUNDER;
							}

							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoThunderShield(player);
								player->kartstuff[k_itemamount]--;
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_BUBBLESHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_BUBBLE)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BUBBLESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3f);
								player->kartstuff[k_curshield] = KSHIELD_BUBBLE;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								if ((cmd->buttons & BT_ATTACK) && player->kartstuff[k_holdready])
								{
									if (player->kartstuff[k_bubbleblowup] == 0)
										S_StartSound(player->mo, sfx_s3k75);

									player->kartstuff[k_bubbleblowup]++;
									player->kartstuff[k_bubblecool] = player->kartstuff[k_bubbleblowup]*4;

									if (player->kartstuff[k_bubbleblowup] > bubbletime*2)
									{
										K_ThrowKartItem(player, (player->kartstuff[k_throwdir] > 0), MT_BUBBLESHIELDTRAP, -1, 0);
										K_PlayAttackTaunt(player->mo);
										player->kartstuff[k_bubbleblowup] = 0;
										player->kartstuff[k_bubblecool] = 0;
										player->kartstuff[k_holdready] = 0;
										player->kartstuff[k_itemamount]--;
									}
								}
								else
								{
									if (player->kartstuff[k_bubbleblowup] > bubbletime)
										player->kartstuff[k_bubbleblowup] = bubbletime;

									if (player->kartstuff[k_bubbleblowup])
										player->kartstuff[k_bubbleblowup]--;

									player->kartstuff[k_holdready] = (player->kartstuff[k_bubblecool] ? 0 : 1);
								}
							}
							break;
						case KITEM_FLAMESHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_FLAME)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_FLAMESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3e);
								player->kartstuff[k_curshield] = KSHIELD_FLAME;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 destlen = K_FlameShieldMax(player);
								INT32 flamemax = 0;

								if (player->kartstuff[k_flamelength] < destlen)
									player->kartstuff[k_flamelength]++; // Can always go up!

								flamemax = player->kartstuff[k_flamelength] * flameseg;
								if (flamemax > 0)
									flamemax += TICRATE; // leniency period

								if ((cmd->buttons & BT_ATTACK) && player->kartstuff[k_holdready])
								{
									if (player->kartstuff[k_flamemeter] < 0)
										player->kartstuff[k_flamemeter] = 0;

									if (player->kartstuff[k_flamedash] == 0)
									{
										S_StartSound(player->mo, sfx_s3k43);
										K_PlayBoostTaunt(player->mo);
									}

									player->kartstuff[k_flamedash] += 2;
									player->kartstuff[k_flamemeter] += 2;

									if (!onground)
									{
										P_Thrust(
											player->mo, R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy),
											FixedMul(player->mo->scale, K_GetKartGameSpeedScalar(gamespeed))
										);
									}

									if (player->kartstuff[k_flamemeter] > flamemax)
									{
										P_Thrust(
											player->mo, player->mo->angle,
											FixedMul((50*player->mo->scale), K_GetKartGameSpeedScalar(gamespeed))
										);

										player->kartstuff[k_flamemeter] = 0;
										player->kartstuff[k_flamelength] = 0;
										player->kartstuff[k_holdready] = 0;
										player->kartstuff[k_itemamount]--;
									}
								}
								else
								{
									player->kartstuff[k_holdready] = 1;

									if (player->kartstuff[k_flamemeter] > 0)
										player->kartstuff[k_flamemeter]--;

									if (player->kartstuff[k_flamelength] > destlen)
									{
										player->kartstuff[k_flamelength]--; // Can ONLY go down if you're not using it

										flamemax = player->kartstuff[k_flamelength] * flameseg;
										if (flamemax > 0)
											flamemax += TICRATE; // leniency period
									}

									if (player->kartstuff[k_flamemeter] > flamemax)
										player->kartstuff[k_flamemeter] = flamemax;
								}
							}
							break;
						case KITEM_HYUDORO:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_DoHyudoroSteal(player); // yes. yes they do.
							}
							break;
						case KITEM_POGOSPRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO
								&& !player->kartstuff[k_pogospring])
							{
								K_PlayBoostTaunt(player->mo);
								K_DoPogoSpring(player->mo, 32<<FRACBITS, 2);
								player->kartstuff[k_pogospring] = 1;
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_SUPERRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_superring] += (10*3);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_KITCHENSINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SINK_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && player->kartstuff[k_itemheld]) // Sink thrown
							{
								K_ThrowKartItem(player, false, MT_SINK, 1, 2);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_itemheld] = 0;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_SAD:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO
								&& !player->kartstuff[k_sadtimer])
							{
								player->kartstuff[k_sadtimer] = stealtime;
								player->kartstuff[k_itemamount]--;
							}
							break;
						default:
							break;
					}
				}
			}
		}

		// No more!
		if (!player->kartstuff[k_itemamount])
		{
			player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}

		if (K_GetShieldFromItem(player->kartstuff[k_itemtype]) == KSHIELD_NONE)
		{
			player->kartstuff[k_curshield] = KSHIELD_NONE; // RESET shield type
			player->kartstuff[k_bubbleblowup] = 0;
			player->kartstuff[k_bubblecool] = 0;
			player->kartstuff[k_flamelength] = 0;
			player->kartstuff[k_flamemeter] = 0;
		}

		if (spbplace == -1 || player->kartstuff[k_position] != spbplace)
			player->kartstuff[k_ringlock] = 0; // reset ring lock

		if (player->kartstuff[k_itemtype] == KITEM_SPB
			|| player->kartstuff[k_itemtype] == KITEM_SHRINK
			|| player->kartstuff[k_growshrinktimer] < 0)
			indirectitemcooldown = 20*TICRATE;

		if (player->kartstuff[k_hyudorotimer] > 0)
		{
			INT32 hyu = hyudorotime;

			if (G_RaceGametype())
				hyu *= 2; // double in race

			if (r_splitscreen)
			{
				if (leveltime & 1)
					player->mo->flags2 |= MF2_DONTDRAW;
				else
					player->mo->flags2 &= ~MF2_DONTDRAW;

				if (player->kartstuff[k_hyudorotimer] >= (TICRATE/2) && player->kartstuff[k_hyudorotimer] <= hyu-(TICRATE/2))
				{
					if (player == &players[displayplayers[1]])
						player->mo->eflags |= MFE_DRAWONLYFORP2;
					else if (player == &players[displayplayers[2]] && r_splitscreen > 1)
						player->mo->eflags |= MFE_DRAWONLYFORP3;
					else if (player == &players[displayplayers[3]] && r_splitscreen > 2)
						player->mo->eflags |= MFE_DRAWONLYFORP4;
					else if (player == &players[displayplayers[0]])
						player->mo->eflags |= MFE_DRAWONLYFORP1;
					else
						player->mo->flags2 |= MF2_DONTDRAW;
				}
				else
					player->mo->eflags &= ~(MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);
			}
			else
			{
				if (P_IsDisplayPlayer(player)
					|| (!P_IsDisplayPlayer(player) && (player->kartstuff[k_hyudorotimer] < (TICRATE/2) || player->kartstuff[k_hyudorotimer] > hyu-(TICRATE/2))))
				{
					if (leveltime & 1)
						player->mo->flags2 |= MF2_DONTDRAW;
					else
						player->mo->flags2 &= ~MF2_DONTDRAW;
				}
				else
					player->mo->flags2 |= MF2_DONTDRAW;
			}

			player->powers[pw_flashing] = player->kartstuff[k_hyudorotimer]; // We'll do this for now, let's people know about the invisible people through subtle hints
		}
		else if (player->kartstuff[k_hyudorotimer] == 0)
		{
			player->mo->flags2 &= ~MF2_DONTDRAW;
			player->mo->eflags &= ~(MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);
		}

		if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0) // dead in match? you da bomb
		{
			K_DropItems(player); //K_StripItems(player);
			K_StripOther(player);
			player->mo->flags2 |= MF2_SHADOW;
			player->powers[pw_flashing] = player->kartstuff[k_comebacktimer];
		}
		else if (G_RaceGametype() || player->kartstuff[k_bumper] > 0)
		{
			player->mo->flags2 &= ~MF2_SHADOW;
		}
	}

	if (onground)
	{
		fixed_t prevfriction = player->mo->friction;

		// Reduce friction after hitting a horizontal spring
		if (player->kartstuff[k_tiregrease])
			player->mo->friction += ((FRACUNIT - prevfriction) / greasetics) * player->kartstuff[k_tiregrease];

		// Friction
		if (!player->kartstuff[k_offroad])
		{
			if (player->speed > 0 && cmd->forwardmove == 0 && player->mo->friction == 59392)
				player->mo->friction += 4608;
		}

		if (player->speed > 0 && cmd->forwardmove < 0)	// change friction while braking no matter what, otherwise it's not any more effective than just letting go off accel
			player->mo->friction -= 2048;

		// Karma ice physics
		if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
			player->mo->friction += 1228;

		if (player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER))
			player->mo->friction += 614;

		// Wipeout slowdown
		if (player->kartstuff[k_spinouttimer] && player->kartstuff[k_wipeoutslow])
		{
			if (player->kartstuff[k_offroad])
				player->mo->friction -= 4912;
			if (player->kartstuff[k_wipeoutslow] == 1)
				player->mo->friction -= 9824;
		}

		// Friction was changed, so we must recalculate a bunch of stuff
		if (player->mo->friction != prevfriction)
		{
			if (player->mo->friction > FRACUNIT)
				player->mo->friction = FRACUNIT;
			if (player->mo->friction < 0)
				player->mo->friction = 0;

			player->mo->movefactor = FixedDiv(ORIG_FRICTION, player->mo->friction);

			if (player->mo->movefactor < FRACUNIT)
				player->mo->movefactor = 19*player->mo->movefactor - 18*FRACUNIT;
			else
				player->mo->movefactor = FRACUNIT;

			if (player->mo->movefactor < 32)
				player->mo->movefactor = 32;
		}
	}

	K_KartDrift(player, P_IsObjectOnGround(player->mo)); // Not using onground, since we don't want this affected by spring pads

	// Quick Turning
	// You can't turn your kart when you're not moving.
	// So now it's time to burn some rubber!
	if (player->speed < 2 && leveltime > starttime && cmd->buttons & BT_ACCELERATE && cmd->buttons & BT_BRAKE && cmd->driftturn != 0)
	{
		if (leveltime % 8 == 0)
			S_StartSound(player->mo, sfx_s224);
	}

	// Squishing
	// If a Grow player or a sector crushes you, get flattened instead of being killed.

	if (player->kartstuff[k_squishedtimer] > 0)
	{
		//player->mo->flags |= MF_NOCLIP;
		player->mo->momx = 0;
		player->mo->momy = 0;
	}

	// Play the starting countdown sounds
	if (player == &players[g_localplayers[0]]) // Don't play louder in splitscreen
	{
		if ((leveltime == starttime-(3*TICRATE)) || (leveltime == starttime-(2*TICRATE)) || (leveltime == starttime-TICRATE))
			S_StartSound(NULL, sfx_s3ka7);
		if (leveltime == starttime)
		{
			S_StartSound(NULL, sfx_s3kad);
			S_StopMusic(); // The GO! sound stops the level start ambience
		}
	}

	// Start charging once you're given the opportunity.
	if (leveltime >= starttime-(2*TICRATE) && leveltime <= starttime)
	{
		if (cmd->buttons & BT_ACCELERATE)
		{
			if (player->kartstuff[k_boostcharge] == 0)
				player->kartstuff[k_boostcharge] = cmd->latency;

			player->kartstuff[k_boostcharge]++;
		}
		else
			player->kartstuff[k_boostcharge] = 0;
	}

	// Increase your size while charging your engine.
	if (leveltime < starttime+10)
	{
		player->mo->scalespeed = mapobjectscale/12;
		player->mo->destscale = mapobjectscale + (player->kartstuff[k_boostcharge]*131);
		if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	// Determine the outcome of your charge.
	if (leveltime > starttime && player->kartstuff[k_boostcharge])
	{
		// Not even trying?
		if (player->kartstuff[k_boostcharge] < 35)
		{
			if (player->kartstuff[k_boostcharge] > 17)
				S_StartSound(player->mo, sfx_cdfm00); // chosen instead of a conventional skid because it's more engine-like
		}
		// Get an instant boost!
		else if (player->kartstuff[k_boostcharge] <= 50)
		{
			player->kartstuff[k_startboost] = (50-player->kartstuff[k_boostcharge])+20;

			if (player->kartstuff[k_boostcharge] <= 36)
			{
				player->kartstuff[k_startboost] = 0;
				K_DoSneaker(player, 0);
				player->kartstuff[k_sneakertimer] = 70; // PERFECT BOOST!!

				if (!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3) // Let everyone hear this one
					S_StartSound(player->mo, sfx_s25f);
			}
			else
			{
				K_SpawnDashDustRelease(player); // already handled for perfect boosts by K_DoSneaker
				if ((!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3) && P_IsDisplayPlayer(player))
				{
					if (player->kartstuff[k_boostcharge] <= 40)
						S_StartSound(player->mo, sfx_cdfm01); // You were almost there!
					else
						S_StartSound(player->mo, sfx_s23c); // Nope, better luck next time.
				}
			}
		}
		// You overcharged your engine? Those things are expensive!!!
		else if (player->kartstuff[k_boostcharge] > 50)
		{
			player->powers[pw_nocontrol] = 40;
			//S_StartSound(player->mo, sfx_kc34);
			S_StartSound(player->mo, sfx_s3k83);
			player->pflags |= PF_SKIDDOWN; // cheeky pflag reuse
		}

		player->kartstuff[k_boostcharge] = 0;
	}
}

void K_CheckSpectateStatus(void)
{
	UINT8 respawnlist[MAXPLAYERS];
	UINT8 i, j, numingame = 0, numjoiners = 0;

	// Maintain spectate wait timer
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		if (players[i].spectator && (players[i].pflags & PF_WANTSTOJOIN))
			players[i].kartstuff[k_spectatewait]++;
		else
			players[i].kartstuff[k_spectatewait] = 0;
	}

	// No one's allowed to join
	if (!cv_allowteamchange.value)
		return;

	// Get the number of players in game, and the players to be de-spectated.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (!players[i].spectator)
		{
			numingame++;
			if (cv_ingamecap.value && numingame >= cv_ingamecap.value) // DON'T allow if you've hit the in-game player cap
				return;
			if (gamestate != GS_LEVEL) // Allow if you're not in a level
				continue;
			if (players[i].exiting) // DON'T allow if anyone's exiting
				return;
			if (numingame < 2 || leveltime < starttime || mapreset) // Allow if the match hasn't started yet
				continue;
			if (leveltime > (starttime + 20*TICRATE)) // DON'T allow if the match is 20 seconds in
				return;
			if (G_RaceGametype() && players[i].laps >= 2) // DON'T allow if the race is at 2 laps
				return;
			continue;
		}
		else if (!(players[i].pflags & PF_WANTSTOJOIN))
			continue;

		respawnlist[numjoiners++] = i;
	}

	// literally zero point in going any further if nobody is joining
	if (!numjoiners)
		return;

	// Organize by spectate wait timer
	if (cv_ingamecap.value)
	{
		UINT8 oldrespawnlist[MAXPLAYERS];
		memcpy(oldrespawnlist, respawnlist, numjoiners);
		for (i = 0; i < numjoiners; i++)
		{
			UINT8 pos = 0;
			INT32 ispecwait = players[oldrespawnlist[i]].kartstuff[k_spectatewait];

			for (j = 0; j < numjoiners; j++)
			{
				INT32 jspecwait = players[oldrespawnlist[j]].kartstuff[k_spectatewait];
				if (j == i)
					continue;
				if (jspecwait > ispecwait)
					pos++;
				else if (jspecwait == ispecwait && j < i)
					pos++;
			}

			respawnlist[pos] = oldrespawnlist[i];
		}
	}

	// Finally, we can de-spectate everyone!
	for (i = 0; i < numjoiners; i++)
	{
		if (cv_ingamecap.value && numingame+i >= cv_ingamecap.value) // Hit the in-game player cap while adding people?
			break;
		P_SpectatorJoinGame(&players[respawnlist[i]]);
	}

	// Reset the match if you're in an empty server
	if (!mapreset && gamestate == GS_LEVEL && leveltime >= starttime && (numingame < 2 && numingame+i >= 2)) // use previous i value
	{
		S_ChangeMusicInternal("chalng", false); // COME ON
		mapreset = 3*TICRATE; // Even though only the server uses this for game logic, set for everyone for HUD
	}
}

//}

//{ SRB2kart HUD Code

#define NUMPOSNUMS 10
#define NUMPOSFRAMES 7 // White, three blues, three reds
#define NUMWINFRAMES 6 // Red, yellow, green, cyan, blue, purple

//{ 	Patch Definitions
static patch_t *kp_nodraw;

static patch_t *kp_timesticker;
static patch_t *kp_timestickerwide;
static patch_t *kp_lapsticker;
static patch_t *kp_lapstickerwide;
static patch_t *kp_lapstickernarrow;
static patch_t *kp_splitlapflag;
static patch_t *kp_bumpersticker;
static patch_t *kp_bumperstickerwide;
static patch_t *kp_capsulesticker;
static patch_t *kp_capsulestickerwide;
static patch_t *kp_karmasticker;
static patch_t *kp_splitkarmabomb;
static patch_t *kp_timeoutsticker;

static patch_t *kp_startcountdown[16];
static patch_t *kp_racefinish[6];

static patch_t *kp_positionnum[NUMPOSNUMS][NUMPOSFRAMES];
static patch_t *kp_winnernum[NUMPOSFRAMES];

static patch_t *kp_facenum[MAXPLAYERS+1];
static patch_t *kp_facehighlight[8];

static patch_t *kp_spbminimap;

static patch_t *kp_ringsticker[2];
static patch_t *kp_ringstickersplit[4];
static patch_t *kp_ring[6];
static patch_t *kp_smallring[6];
static patch_t *kp_ringdebtminus;
static patch_t *kp_ringdebtminussmall;
static patch_t *kp_ringspblock[16];
static patch_t *kp_ringspblocksmall[16];

static patch_t *kp_speedometersticker;
static patch_t *kp_speedometerlabel[4];

static patch_t *kp_rankbumper;
static patch_t *kp_tinybumper[2];
static patch_t *kp_ranknobumpers;
static patch_t *kp_rankcapsule;

static patch_t *kp_battlewin;
static patch_t *kp_battlecool;
static patch_t *kp_battlelose;
static patch_t *kp_battlewait;
static patch_t *kp_battleinfo;
static patch_t *kp_wanted;
static patch_t *kp_wantedsplit;
static patch_t *kp_wantedreticle;

static patch_t *kp_itembg[4];
static patch_t *kp_itemtimer[2];
static patch_t *kp_itemmulsticker[2];
static patch_t *kp_itemx;

static patch_t *kp_superring[2];
static patch_t *kp_sneaker[2];
static patch_t *kp_rocketsneaker[2];
static patch_t *kp_invincibility[13];
static patch_t *kp_banana[2];
static patch_t *kp_eggman[2];
static patch_t *kp_orbinaut[5];
static patch_t *kp_jawz[2];
static patch_t *kp_mine[2];
static patch_t *kp_ballhog[2];
static patch_t *kp_selfpropelledbomb[2];
static patch_t *kp_grow[2];
static patch_t *kp_shrink[2];
static patch_t *kp_thundershield[2];
static patch_t *kp_bubbleshield[2];
static patch_t *kp_flameshield[2];
static patch_t *kp_hyudoro[2];
static patch_t *kp_pogospring[2];
static patch_t *kp_kitchensink[2];
static patch_t *kp_sadface[2];

static patch_t *kp_check[6];

static patch_t *kp_eggnum[4];

static patch_t *kp_flameshieldmeter[104][2];
static patch_t *kp_flameshieldmeter_bg[16][2];

static patch_t *kp_fpview[3];
static patch_t *kp_inputwheel[5];

static patch_t *kp_challenger[25];

static patch_t *kp_lapanim_lap[7];
static patch_t *kp_lapanim_final[11];
static patch_t *kp_lapanim_number[10][3];
static patch_t *kp_lapanim_emblem[2];
static patch_t *kp_lapanim_hand[3];

static patch_t *kp_yougotem;
static patch_t *kp_itemminimap;

static patch_t *kp_alagles[10];
static patch_t *kp_blagles[6];

void K_LoadKartHUDGraphics(void)
{
	INT32 i, j;
	char buffer[9];

	// Null Stuff
	kp_nodraw = 				W_CachePatchName("K_TRNULL", PU_HUDGFX);

	// Stickers
	kp_timesticker = 			W_CachePatchName("K_STTIME", PU_HUDGFX);
	kp_timestickerwide = 		W_CachePatchName("K_STTIMW", PU_HUDGFX);
	kp_lapsticker = 			W_CachePatchName("K_STLAPS", PU_HUDGFX);
	kp_lapstickerwide = 		W_CachePatchName("K_STLAPW", PU_HUDGFX);
	kp_lapstickernarrow = 		W_CachePatchName("K_STLAPN", PU_HUDGFX);
	kp_splitlapflag = 			W_CachePatchName("K_SPTLAP", PU_HUDGFX);
	kp_bumpersticker = 			W_CachePatchName("K_STBALN", PU_HUDGFX);
	kp_bumperstickerwide = 		W_CachePatchName("K_STBALW", PU_HUDGFX);
	kp_capsulesticker = 		W_CachePatchName("K_STCAPN", PU_HUDGFX);
	kp_capsulestickerwide = 	W_CachePatchName("K_STCAPW", PU_HUDGFX);
	kp_karmasticker = 			W_CachePatchName("K_STKARM", PU_HUDGFX);
	kp_splitkarmabomb = 		W_CachePatchName("K_SPTKRM", PU_HUDGFX);
	kp_timeoutsticker = 		W_CachePatchName("K_STTOUT", PU_HUDGFX);

	// Starting countdown
	kp_startcountdown[0] = 		W_CachePatchName("K_CNT3A", PU_HUDGFX);
	kp_startcountdown[1] = 		W_CachePatchName("K_CNT2A", PU_HUDGFX);
	kp_startcountdown[2] = 		W_CachePatchName("K_CNT1A", PU_HUDGFX);
	kp_startcountdown[3] = 		W_CachePatchName("K_CNTGOA", PU_HUDGFX);
	kp_startcountdown[4] = 		W_CachePatchName("K_CNT3B", PU_HUDGFX);
	kp_startcountdown[5] = 		W_CachePatchName("K_CNT2B", PU_HUDGFX);
	kp_startcountdown[6] = 		W_CachePatchName("K_CNT1B", PU_HUDGFX);
	kp_startcountdown[7] = 		W_CachePatchName("K_CNTGOB", PU_HUDGFX);
	// Splitscreen
	kp_startcountdown[8] = 		W_CachePatchName("K_SMC3A", PU_HUDGFX);
	kp_startcountdown[9] = 		W_CachePatchName("K_SMC2A", PU_HUDGFX);
	kp_startcountdown[10] = 	W_CachePatchName("K_SMC1A", PU_HUDGFX);
	kp_startcountdown[11] = 	W_CachePatchName("K_SMCGOA", PU_HUDGFX);
	kp_startcountdown[12] = 	W_CachePatchName("K_SMC3B", PU_HUDGFX);
	kp_startcountdown[13] = 	W_CachePatchName("K_SMC2B", PU_HUDGFX);
	kp_startcountdown[14] = 	W_CachePatchName("K_SMC1B", PU_HUDGFX);
	kp_startcountdown[15] = 	W_CachePatchName("K_SMCGOB", PU_HUDGFX);

	// Finish
	kp_racefinish[0] = 			W_CachePatchName("K_FINA", PU_HUDGFX);
	kp_racefinish[1] = 			W_CachePatchName("K_FINB", PU_HUDGFX);
	// Splitscreen
	kp_racefinish[2] = 			W_CachePatchName("K_SMFINA", PU_HUDGFX);
	kp_racefinish[3] = 			W_CachePatchName("K_SMFINB", PU_HUDGFX);
	// 2P splitscreen
	kp_racefinish[4] = 			W_CachePatchName("K_2PFINA", PU_HUDGFX);
	kp_racefinish[5] = 			W_CachePatchName("K_2PFINB", PU_HUDGFX);

	// Position numbers
	sprintf(buffer, "K_POSNxx");
	for (i = 0; i < NUMPOSNUMS; i++)
	{
		buffer[6] = '0'+i;
		for (j = 0; j < NUMPOSFRAMES; j++)
		{
			//sprintf(buffer, "K_POSN%d%d", i, j);
			buffer[7] = '0'+j;
			kp_positionnum[i][j] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
		}
	}

	sprintf(buffer, "K_POSNWx");
	for (i = 0; i < NUMWINFRAMES; i++)
	{
		buffer[7] = '0'+i;
		kp_winnernum[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "OPPRNKxx");
	for (i = 0; i <= MAXPLAYERS; i++)
	{
		buffer[6] = '0'+(i/10);
		buffer[7] = '0'+(i%10);
		kp_facenum[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_CHILIx");
	for (i = 0; i < 8; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_facehighlight[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_spbminimap =				W_CachePatchName("SPBMMAP", PU_HUDGFX);

	// Rings & Lives
	kp_ringsticker[0] =			W_CachePatchName("RNGBACKA", PU_HUDGFX);
	kp_ringsticker[1] =			W_CachePatchName("RNGBACKB", PU_HUDGFX);

	sprintf(buffer, "K_RINGx");
	for (i = 0; i < 6; i++)
	{
		buffer[6] = '0'+(i+1);
		kp_ring[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_ringdebtminus =			W_CachePatchName("RDEBTMIN", PU_HUDGFX);

	sprintf(buffer, "SPBRNGxx");
	for (i = 0; i < 16; i++)
	{
		buffer[6] = '0'+((i+1) / 10);
		buffer[7] = '0'+((i+1) % 10);
		kp_ringspblock[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_ringstickersplit[0] =	W_CachePatchName("SMRNGBGA", PU_HUDGFX);
	kp_ringstickersplit[1] =	W_CachePatchName("SMRNGBGB", PU_HUDGFX);

	sprintf(buffer, "K_SRINGx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_smallring[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_ringdebtminussmall =		W_CachePatchName("SRDEBTMN", PU_HUDGFX);

	sprintf(buffer, "SPBRGSxx");
	for (i = 0; i < 16; i++)
	{
		buffer[6] = '0'+((i+1) / 10);
		buffer[7] = '0'+((i+1) % 10);
		kp_ringspblocksmall[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Speedometer
	kp_speedometersticker =		W_CachePatchName("K_SPDMBG", PU_HUDGFX);

	sprintf(buffer, "K_SPDMLx");
	for (i = 0; i < 4; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_speedometerlabel[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Extra ranking icons
	kp_rankbumper =				W_CachePatchName("K_BLNICO", PU_HUDGFX);
	kp_tinybumper[0] =			W_CachePatchName("K_BLNA", PU_HUDGFX);
	kp_tinybumper[1] =			W_CachePatchName("K_BLNB", PU_HUDGFX);
	kp_ranknobumpers =			W_CachePatchName("K_NOBLNS", PU_HUDGFX);
	kp_rankcapsule =			W_CachePatchName("K_CAPICO", PU_HUDGFX);

	// Battle graphics
	kp_battlewin = 				W_CachePatchName("K_BWIN", PU_HUDGFX);
	kp_battlecool = 			W_CachePatchName("K_BCOOL", PU_HUDGFX);
	kp_battlelose = 			W_CachePatchName("K_BLOSE", PU_HUDGFX);
	kp_battlewait = 			W_CachePatchName("K_BWAIT", PU_HUDGFX);
	kp_battleinfo = 			W_CachePatchName("K_BINFO", PU_HUDGFX);
	kp_wanted = 				W_CachePatchName("K_WANTED", PU_HUDGFX);
	kp_wantedsplit = 			W_CachePatchName("4PWANTED", PU_HUDGFX);
	kp_wantedreticle =			W_CachePatchName("MMAPWANT", PU_HUDGFX);

	// Kart Item Windows
	kp_itembg[0] = 				W_CachePatchName("K_ITBG", PU_HUDGFX);
	kp_itembg[1] = 				W_CachePatchName("K_ITBGD", PU_HUDGFX);
	kp_itemtimer[0] = 			W_CachePatchName("K_ITIMER", PU_HUDGFX);
	kp_itemmulsticker[0] = 		W_CachePatchName("K_ITMUL", PU_HUDGFX);
	kp_itemx = 					W_CachePatchName("K_ITX", PU_HUDGFX);

	kp_superring[0] =			W_CachePatchName("K_ITRING", PU_HUDGFX);
	kp_sneaker[0] =				W_CachePatchName("K_ITSHOE", PU_HUDGFX);
	kp_rocketsneaker[0] =		W_CachePatchName("K_ITRSHE", PU_HUDGFX);

	sprintf(buffer, "K_ITINVx");
	for (i = 0; i < 7; i++)
	{
		buffer[7] = '1'+i;
		kp_invincibility[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}
	kp_banana[0] =				W_CachePatchName("K_ITBANA", PU_HUDGFX);
	kp_eggman[0] =				W_CachePatchName("K_ITEGGM", PU_HUDGFX);
	sprintf(buffer, "K_ITORBx");
	for (i = 0; i < 4; i++)
	{
		buffer[7] = '1'+i;
		kp_orbinaut[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}
	kp_jawz[0] =				W_CachePatchName("K_ITJAWZ", PU_HUDGFX);
	kp_mine[0] =				W_CachePatchName("K_ITMINE", PU_HUDGFX);
	kp_ballhog[0] =				W_CachePatchName("K_ITBHOG", PU_HUDGFX);
	kp_selfpropelledbomb[0] =	W_CachePatchName("K_ITSPB", PU_HUDGFX);
	kp_grow[0] =				W_CachePatchName("K_ITGROW", PU_HUDGFX);
	kp_shrink[0] =				W_CachePatchName("K_ITSHRK", PU_HUDGFX);
	kp_thundershield[0] =		W_CachePatchName("K_ITTHNS", PU_HUDGFX);
	kp_bubbleshield[0] =		W_CachePatchName("K_ITBUBS", PU_HUDGFX);
	kp_flameshield[0] =			W_CachePatchName("K_ITFLMS", PU_HUDGFX);
	kp_hyudoro[0] = 			W_CachePatchName("K_ITHYUD", PU_HUDGFX);
	kp_pogospring[0] = 			W_CachePatchName("K_ITPOGO", PU_HUDGFX);
	kp_kitchensink[0] = 		W_CachePatchName("K_ITSINK", PU_HUDGFX);
	kp_sadface[0] = 			W_CachePatchName("K_ITSAD", PU_HUDGFX);

	sprintf(buffer, "FSMFGxxx");
	for (i = 0; i < 104; i++)
	{
		buffer[5] = '0'+((i+1)/100);
		buffer[6] = '0'+(((i+1)/10)%10);
		buffer[7] = '0'+((i+1)%10);
		kp_flameshieldmeter[i][0] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "FSMBG0xx");
	for (i = 0; i < 16; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_flameshieldmeter_bg[i][0] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Splitscreen
	kp_itembg[2] = 				W_CachePatchName("K_ISBG", PU_HUDGFX);
	kp_itembg[3] = 				W_CachePatchName("K_ISBGD", PU_HUDGFX);
	kp_itemtimer[1] = 			W_CachePatchName("K_ISIMER", PU_HUDGFX);
	kp_itemmulsticker[1] = 		W_CachePatchName("K_ISMUL", PU_HUDGFX);

	kp_superring[1] =			W_CachePatchName("K_ISRING", PU_HUDGFX);
	kp_sneaker[1] =				W_CachePatchName("K_ISSHOE", PU_HUDGFX);
	kp_rocketsneaker[1] =		W_CachePatchName("K_ISRSHE", PU_HUDGFX);
	sprintf(buffer, "K_ISINVx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '1'+i;
		kp_invincibility[i+7] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}
	kp_banana[1] =				W_CachePatchName("K_ISBANA", PU_HUDGFX);
	kp_eggman[1] =				W_CachePatchName("K_ISEGGM", PU_HUDGFX);
	kp_orbinaut[4] =			W_CachePatchName("K_ISORBN", PU_HUDGFX);
	kp_jawz[1] =				W_CachePatchName("K_ISJAWZ", PU_HUDGFX);
	kp_mine[1] =				W_CachePatchName("K_ISMINE", PU_HUDGFX);
	kp_ballhog[1] =				W_CachePatchName("K_ISBHOG", PU_HUDGFX);
	kp_selfpropelledbomb[1] =	W_CachePatchName("K_ISSPB", PU_HUDGFX);
	kp_grow[1] =				W_CachePatchName("K_ISGROW", PU_HUDGFX);
	kp_shrink[1] =				W_CachePatchName("K_ISSHRK", PU_HUDGFX);
	kp_thundershield[1] =		W_CachePatchName("K_ISTHNS", PU_HUDGFX);
	kp_bubbleshield[1] =		W_CachePatchName("K_ISBUBS", PU_HUDGFX);
	kp_flameshield[1] =			W_CachePatchName("K_ISFLMS", PU_HUDGFX);
	kp_hyudoro[1] = 			W_CachePatchName("K_ISHYUD", PU_HUDGFX);
	kp_pogospring[1] = 			W_CachePatchName("K_ISPOGO", PU_HUDGFX);
	kp_kitchensink[1] = 		W_CachePatchName("K_ISSINK", PU_HUDGFX);
	kp_sadface[1] = 			W_CachePatchName("K_ISSAD", PU_HUDGFX);

	sprintf(buffer, "FSMFSxxx");
	for (i = 0; i < 104; i++)
	{
		buffer[5] = '0'+((i+1)/100);
		buffer[6] = '0'+(((i+1)/10)%10);
		buffer[7] = '0'+((i+1)%10);
		kp_flameshieldmeter[i][1] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "FSMBS0xx");
	for (i = 0; i < 16; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_flameshieldmeter_bg[i][1] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// CHECK indicators
	sprintf(buffer, "K_CHECKx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '1'+i;
		kp_check[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Eggman warning numbers
	sprintf(buffer, "K_EGGNx");
	for (i = 0; i < 4; i++)
	{
		buffer[6] = '0'+i;
		kp_eggnum[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// First person mode
	kp_fpview[0] = 				W_CachePatchName("VIEWA0", PU_HUDGFX);
	kp_fpview[1] =				W_CachePatchName("VIEWB0D0", PU_HUDGFX);
	kp_fpview[2] = 				W_CachePatchName("VIEWC0E0", PU_HUDGFX);

	// Input UI Wheel
	sprintf(buffer, "K_WHEELx");
	for (i = 0; i < 5; i++)
	{
		buffer[7] = '0'+i;
		kp_inputwheel[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// HERE COMES A NEW CHALLENGER
	sprintf(buffer, "K_CHALxx");
	for (i = 0; i < 25; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_challenger[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Lap start animation
	sprintf(buffer, "K_LAP0x");
	for (i = 0; i < 7; i++)
	{
		buffer[6] = '0'+(i+1);
		kp_lapanim_lap[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPFxx");
	for (i = 0; i < 11; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_lapanim_final[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPNxx");
	for (i = 0; i < 10; i++)
	{
		buffer[6] = '0'+i;
		for (j = 0; j < 3; j++)
		{
			buffer[7] = '0'+(j+1);
			kp_lapanim_number[i][j] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
		}
	}

	sprintf(buffer, "K_LAPE0x");
	for (i = 0; i < 2; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_lapanim_emblem[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPH0x");
	for (i = 0; i < 3; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_lapanim_hand[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_yougotem = (patch_t *) W_CachePatchName("YOUGOTEM", PU_HUDGFX);
	kp_itemminimap = (patch_t *) W_CachePatchName("MMAPITEM", PU_HUDGFX);

	sprintf(buffer, "ALAGLESx");
	for (i = 0; i < 10; ++i)
	{
		buffer[7] = '0'+i;
		kp_alagles[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "BLAGLESx");
	for (i = 0; i < 6; ++i)
	{
		buffer[7] = '0'+i;
		kp_blagles[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}
}

// For the item toggle menu
const char *K_GetItemPatch(UINT8 item, boolean tiny)
{
	switch (item)
	{
		case KITEM_SNEAKER:
		case KRITEM_TRIPLESNEAKER:
			return (tiny ? "K_ISSHOE" : "K_ITSHOE");
		case KITEM_ROCKETSNEAKER:
			return (tiny ? "K_ISRSHE" : "K_ITRSHE");
		case KITEM_INVINCIBILITY:
			return (tiny ? "K_ISINV1" : "K_ITINV1");
		case KITEM_BANANA:
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TENFOLDBANANA:
			return (tiny ? "K_ISBANA" : "K_ITBANA");
		case KITEM_EGGMAN:
			return (tiny ? "K_ISEGGM" : "K_ITEGGM");
		case KITEM_ORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB1");
		case KITEM_JAWZ:
		case KRITEM_DUALJAWZ:
			return (tiny ? "K_ISJAWZ" : "K_ITJAWZ");
		case KITEM_MINE:
			return (tiny ? "K_ISMINE" : "K_ITMINE");
		case KITEM_BALLHOG:
			return (tiny ? "K_ISBHOG" : "K_ITBHOG");
		case KITEM_SPB:
			return (tiny ? "K_ISSPB" : "K_ITSPB");
		case KITEM_GROW:
			return (tiny ? "K_ISGROW" : "K_ITGROW");
		case KITEM_SHRINK:
			return (tiny ? "K_ISSHRK" : "K_ITSHRK");
		case KITEM_THUNDERSHIELD:
			return (tiny ? "K_ISTHNS" : "K_ITTHNS");
		case KITEM_BUBBLESHIELD:
			return (tiny ? "K_ISBUBS" : "K_ITBUBS");
		case KITEM_FLAMESHIELD:
			return (tiny ? "K_ISFLMS" : "K_ITFLMS");
		case KITEM_HYUDORO:
			return (tiny ? "K_ISHYUD" : "K_ITHYUD");
		case KITEM_POGOSPRING:
			return (tiny ? "K_ISPOGO" : "K_ITPOGO");
		case KITEM_SUPERRING:
			return (tiny ? "K_ISRING" : "K_ITRING");
		case KITEM_KITCHENSINK:
			return (tiny ? "K_ISSINK" : "K_ITSINK");
		case KRITEM_TRIPLEORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB3");
		case KRITEM_QUADORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB4");
		default:
			return (tiny ? "K_ISSAD" : "K_ITSAD");
	}
}

//}

INT32 ITEM_X, ITEM_Y;	// Item Window
INT32 TIME_X, TIME_Y;	// Time Sticker
INT32 LAPS_X, LAPS_Y;	// Lap Sticker
INT32 POSI_X, POSI_Y;	// Position Number
INT32 FACE_X, FACE_Y;	// Top-four Faces
INT32 STCD_X, STCD_Y;	// Starting countdown
INT32 CHEK_Y;			// CHECK graphic
INT32 MINI_X, MINI_Y;	// Minimap
INT32 WANT_X, WANT_Y;	// Battle WANTED poster

// This is for the P2 and P4 side of splitscreen. Then we'll flip P1's and P2's to the bottom with V_SPLITSCREEN.
INT32 ITEM2_X, ITEM2_Y;
INT32 LAPS2_X, LAPS2_Y;
INT32 POSI2_X, POSI2_Y;


static void K_initKartHUD(void)
{
	/*
		BASEVIDWIDTH  = 320
		BASEVIDHEIGHT = 200

		Item window graphic is 41 x 33

		Time Sticker graphic is 116 x 11
		Time Font is a solid block of (8 x [12) x 14], equal to 96 x 14
		Therefore, timestamp is 116 x 14 altogether

		Lap Sticker is 80 x 11
		Lap flag is 22 x 20
		Lap Font is a solid block of (3 x [12) x 14], equal to 36 x 14
		Therefore, lapstamp is 80 x 20 altogether

		Position numbers are 43 x 53

		Faces are 32 x 32
		Faces draw downscaled at 16 x 16
		Therefore, the allocated space for them is 16 x 67 altogether

		----

		ORIGINAL CZ64 SPLITSCREEN:

		Item window:
		if (!splitscreen) 	{ ICONX = 139; 				ICONY = 20; }
		else 				{ ICONX = BASEVIDWIDTH-315; ICONY = 60; }

		Time: 			   236, STRINGY(			   12)
		Lap:  BASEVIDWIDTH-304, STRINGY(BASEVIDHEIGHT-189)

	*/

	// Single Screen (defaults)
	// Item Window
	ITEM_X = 5;						//   5
	ITEM_Y = 5;						//   5
	// Level Timer
	TIME_X = BASEVIDWIDTH - 148;	// 172
	TIME_Y = 9;						//   9
	// Level Laps
	LAPS_X = 9;						//   9
	LAPS_Y = BASEVIDHEIGHT - 29;	// 171
	// Position Number
	POSI_X = BASEVIDWIDTH  - 9;		// 268
	POSI_Y = BASEVIDHEIGHT - 9;		// 138
	// Top-Four Faces
	FACE_X = 9;						//   9
	FACE_Y = 92;					//  92
	// Starting countdown
	STCD_X = BASEVIDWIDTH/2;		//   9
	STCD_Y = BASEVIDHEIGHT/2;		//  92
	// CHECK graphic
	CHEK_Y = BASEVIDHEIGHT;			// 200
	// Minimap
	MINI_X = BASEVIDWIDTH - 50;		// 270
	MINI_Y = (BASEVIDHEIGHT/2)-16; //  84
	// Battle WANTED poster
	WANT_X = BASEVIDWIDTH - 55;		// 270
	WANT_Y = BASEVIDHEIGHT- 71;		// 176

	if (r_splitscreen)	// Splitscreen
	{
		ITEM_X = 5;
		ITEM_Y = 3;

		LAPS_Y = (BASEVIDHEIGHT/2)-24;

		POSI_Y = (BASEVIDHEIGHT/2)- 2;

		STCD_Y = BASEVIDHEIGHT/4;

		MINI_Y = (BASEVIDHEIGHT/2);

		if (r_splitscreen > 1)	// 3P/4P Small Splitscreen
		{
			// 1P (top left)
			ITEM_X = -9;
			ITEM_Y = -8;

			LAPS_X = 3;
			LAPS_Y = (BASEVIDHEIGHT/2)-12;

			POSI_X = 24;
			POSI_Y = (BASEVIDHEIGHT/2)-26;

			// 2P (top right)
			ITEM2_X = BASEVIDWIDTH-39;
			ITEM2_Y = -8;

			LAPS2_X = BASEVIDWIDTH-43;
			LAPS2_Y = (BASEVIDHEIGHT/2)-12;

			POSI2_X = BASEVIDWIDTH -4;
			POSI2_Y = (BASEVIDHEIGHT/2)-26;

			// Reminder that 3P and 4P are just 1P and 2P splitscreen'd to the bottom.

			STCD_X = BASEVIDWIDTH/4;

			MINI_X = (3*BASEVIDWIDTH/4);
			MINI_Y = (3*BASEVIDHEIGHT/4);

			if (r_splitscreen > 2) // 4P-only
			{
				MINI_X = (BASEVIDWIDTH/2);
				MINI_Y = (BASEVIDHEIGHT/2);
			}
		}
	}

	if (timeinmap > 113)
		hudtrans = cv_translucenthud.value;
	else if (timeinmap > 105)
		hudtrans = ((((INT32)timeinmap) - 105)*cv_translucenthud.value)/(113-105);
	else
		hudtrans = 0;
}

INT32 K_calcSplitFlags(INT32 snapflags)
{
	INT32 splitflags = 0;

	if (r_splitscreen == 0)
		return snapflags;

	if (stplyr != &players[displayplayers[0]])
	{
		if (r_splitscreen == 1 && stplyr == &players[displayplayers[1]])
		{
			splitflags |= V_SPLITSCREEN;
		}
		else if (r_splitscreen > 1)
		{
			if (stplyr == &players[displayplayers[2]] || (r_splitscreen == 3 && stplyr == &players[displayplayers[3]]))
				splitflags |= V_SPLITSCREEN;
			if (stplyr == &players[displayplayers[1]] || (r_splitscreen == 3 && stplyr == &players[displayplayers[3]]))
				splitflags |= V_HORZSCREEN;
		}
	}

	if (splitflags & V_SPLITSCREEN)
		snapflags &= ~V_SNAPTOTOP;
	else
		snapflags &= ~V_SNAPTOBOTTOM;

	if (r_splitscreen > 1)
	{
		if (splitflags & V_HORZSCREEN)
			snapflags &= ~V_SNAPTOLEFT;
		else
			snapflags &= ~V_SNAPTORIGHT;
	}

	return (splitflags|snapflags);
}

static void K_drawKartItem(void)
{
	// ITEM_X = BASEVIDWIDTH-50;	// 270
	// ITEM_Y = 24;					//  24

	// Why write V_DrawScaledPatch calls over and over when they're all the same?
	// Set to 'no item' just in case.
	const UINT8 offset = ((r_splitscreen > 1) ? 1 : 0);
	patch_t *localpatch = kp_nodraw;
	patch_t *localbg = ((offset) ? kp_itembg[2] : kp_itembg[0]);
	patch_t *localinv = ((offset) ? kp_invincibility[((leveltime % (6*3)) / 3) + 7] : kp_invincibility[(leveltime % (7*3)) / 3]);
	INT32 fx = 0, fy = 0, fflags = 0;	// final coords for hud and flags...
	//INT32 splitflags = K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTOLEFT);
	const INT32 numberdisplaymin = ((!offset && stplyr->kartstuff[k_itemtype] == KITEM_ORBINAUT) ? 5 : 2);
	INT32 itembar = 0;
	INT32 maxl = 0; // itembar's normal highest value
	const INT32 barlength = (r_splitscreen > 1 ? 12 : 26);
	UINT8 localcolor = SKINCOLOR_NONE;
	SINT8 colormode = TC_RAINBOW;
	UINT8 *colmap = NULL;
	boolean flipamount = false;	// Used for 3P/4P splitscreen to flip item amount stuff

	if (stplyr->kartstuff[k_itemroulette])
	{
		if (stplyr->skincolor)
			localcolor = stplyr->skincolor;

		switch((stplyr->kartstuff[k_itemroulette] % (15*3)) / 3)
		{
			// Each case is handled in threes, to give three frames of in-game time to see the item on the roulette
			case 0: // Sneaker
				localpatch = kp_sneaker[offset];
				//localcolor = SKINCOLOR_RASPBERRY;
				break;
			case 1: // Banana
				localpatch = kp_banana[offset];
				//localcolor = SKINCOLOR_YELLOW;
				break;
			case 2: // Orbinaut
				localpatch = kp_orbinaut[3+offset];
				//localcolor = SKINCOLOR_STEEL;
				break;
			case 3: // Mine
				localpatch = kp_mine[offset];
				//localcolor = SKINCOLOR_JET;
				break;
			case 4: // Grow
				localpatch = kp_grow[offset];
				//localcolor = SKINCOLOR_TEAL;
				break;
			case 5: // Hyudoro
				localpatch = kp_hyudoro[offset];
				//localcolor = SKINCOLOR_STEEL;
				break;
			case 6: // Rocket Sneaker
				localpatch = kp_rocketsneaker[offset];
				//localcolor = SKINCOLOR_TANGERINE;
				break;
			case 7: // Jawz
				localpatch = kp_jawz[offset];
				//localcolor = SKINCOLOR_JAWZ;
				break;
			case 8: // Self-Propelled Bomb
				localpatch = kp_selfpropelledbomb[offset];
				//localcolor = SKINCOLOR_JET;
				break;
			case 9: // Shrink
				localpatch = kp_shrink[offset];
				//localcolor = SKINCOLOR_ORANGE;
				break;
			case 10: // Invincibility
				localpatch = localinv;
				//localcolor = SKINCOLOR_GREY;
				break;
			case 11: // Eggman Monitor
				localpatch = kp_eggman[offset];
				//localcolor = SKINCOLOR_ROSE;
				break;
			case 12: // Ballhog
				localpatch = kp_ballhog[offset];
				//localcolor = SKINCOLOR_LILAC;
				break;
			case 13: // Thunder Shield
				localpatch = kp_thundershield[offset];
				//localcolor = SKINCOLOR_CYAN;
				break;
			case 14: // Super Ring
				localpatch = kp_superring[offset];
				//localcolor = SKINCOLOR_GOLD;
				break;
			/*case 15: // Pogo Spring
				localpatch = kp_pogospring[offset];
				localcolor = SKINCOLOR_TANGERINE;
				break;
			case 16: // Kitchen Sink
				localpatch = kp_kitchensink[offset];
				localcolor = SKINCOLOR_STEEL;
				break;*/
			default:
				break;
		}
	}
	else
	{
		// I'm doing this a little weird and drawing mostly in reverse order
		// The only actual reason is to make sneakers line up this way in the code below
		// This shouldn't have any actual baring over how it functions
		// Hyudoro is first, because we're drawing it on top of the player's current item
		if (stplyr->kartstuff[k_stolentimer] > 0)
		{
			if (leveltime & 2)
				localpatch = kp_hyudoro[offset];
			else
				localpatch = kp_nodraw;
		}
		else if ((stplyr->kartstuff[k_stealingtimer] > 0) && (leveltime & 2))
		{
			localpatch = kp_hyudoro[offset];
		}
		else if (stplyr->kartstuff[k_eggmanexplode] > 1)
		{
			if (leveltime & 1)
				localpatch = kp_eggman[offset];
			else
				localpatch = kp_nodraw;
		}
		else if (stplyr->kartstuff[k_rocketsneakertimer] > 1)
		{
			itembar = stplyr->kartstuff[k_rocketsneakertimer];
			maxl = (itemtime*3) - barlength;

			if (leveltime & 1)
				localpatch = kp_rocketsneaker[offset];
			else
				localpatch = kp_nodraw;
		}
		else if (stplyr->kartstuff[k_sadtimer] > 0)
		{
			if (leveltime & 2)
				localpatch = kp_sadface[offset];
			else
				localpatch = kp_nodraw;
		}
		else
		{
			if (stplyr->kartstuff[k_itemamount] <= 0)
				return;

			switch(stplyr->kartstuff[k_itemtype])
			{
				case KITEM_SNEAKER:
					localpatch = kp_sneaker[offset];
					break;
				case KITEM_ROCKETSNEAKER:
					localpatch = kp_rocketsneaker[offset];
					break;
				case KITEM_INVINCIBILITY:
					localpatch = localinv;
					localbg = kp_itembg[offset+1];
					break;
				case KITEM_BANANA:
					localpatch = kp_banana[offset];
					break;
				case KITEM_EGGMAN:
					localpatch = kp_eggman[offset];
					break;
				case KITEM_ORBINAUT:
					localpatch = kp_orbinaut[(offset ? 4 : min(stplyr->kartstuff[k_itemamount]-1, 3))];
					break;
				case KITEM_JAWZ:
					localpatch = kp_jawz[offset];
					break;
				case KITEM_MINE:
					localpatch = kp_mine[offset];
					break;
				case KITEM_BALLHOG:
					localpatch = kp_ballhog[offset];
					break;
				case KITEM_SPB:
					localpatch = kp_selfpropelledbomb[offset];
					localbg = kp_itembg[offset+1];
					break;
				case KITEM_GROW:
					localpatch = kp_grow[offset];
					break;
				case KITEM_SHRINK:
					localpatch = kp_shrink[offset];
					break;
				case KITEM_THUNDERSHIELD:
					localpatch = kp_thundershield[offset];
					localbg = kp_itembg[offset+1];
					break;
				case KITEM_BUBBLESHIELD:
					localpatch = kp_bubbleshield[offset];
					localbg = kp_itembg[offset+1];
					break;
				case KITEM_FLAMESHIELD:
					localpatch = kp_flameshield[offset];
					localbg = kp_itembg[offset+1];
					break;
				case KITEM_HYUDORO:
					localpatch = kp_hyudoro[offset];
					break;
				case KITEM_POGOSPRING:
					localpatch = kp_pogospring[offset];
					break;
				case KITEM_SUPERRING:
					localpatch = kp_superring[offset];
					break;
				case KITEM_KITCHENSINK:
					localpatch = kp_kitchensink[offset];
					break;
				case KITEM_SAD:
					localpatch = kp_sadface[offset];
					break;
				default:
					return;
			}

			if (stplyr->kartstuff[k_itemheld] && !(leveltime & 1))
				localpatch = kp_nodraw;
		}

		if (stplyr->karthud[khud_itemblink] && (leveltime & 1))
		{
			colormode = TC_BLINK;

			switch (stplyr->karthud[khud_itemblinkmode])
			{
				case 2:
					localcolor = (UINT8)(1 + (leveltime % (MAXSKINCOLORS-1)));
					break;
				case 1:
					localcolor = SKINCOLOR_RED;
					break;
				default:
					localcolor = SKINCOLOR_WHITE;
					break;
			}
		}
	}

	// pain and suffering defined below
	if (r_splitscreen < 2) // don't change shit for THIS splitscreen.
	{
		fx = ITEM_X;
		fy = ITEM_Y;
		fflags = K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTOLEFT);
	}
	else // now we're having a fun game.
	{
		if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]]) // If we are P1 or P3...
		{
			fx = ITEM_X;
			fy = ITEM_Y;
			fflags = V_SNAPTOLEFT|((stplyr == &players[displayplayers[2]]) ? V_SPLITSCREEN : V_SNAPTOTOP); // flip P3 to the bottom.
		}
		else // else, that means we're P2 or P4.
		{
			fx = ITEM2_X;
			fy = ITEM2_Y;
			fflags = V_SNAPTORIGHT|((stplyr == &players[displayplayers[3]]) ? V_SPLITSCREEN : V_SNAPTOTOP); // flip P4 to the bottom
			flipamount = true;
		}
	}

	if (localcolor != SKINCOLOR_NONE)
		colmap = R_GetTranslationColormap(colormode, localcolor, GTC_CACHE);

	V_DrawScaledPatch(fx, fy, V_HUDTRANS|fflags, localbg);

	// Then, the numbers:
	if (stplyr->kartstuff[k_itemamount] >= numberdisplaymin && !stplyr->kartstuff[k_itemroulette])
	{
		V_DrawScaledPatch(fx + (flipamount ? 48 : 0), fy, V_HUDTRANS|fflags|(flipamount ? V_FLIP : 0), kp_itemmulsticker[offset]); // flip this graphic for p2 and p4 in split and shift it.
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, FRACUNIT, V_HUDTRANS|fflags, localpatch, colmap);
		if (offset)
			if (flipamount) // reminder that this is for 3/4p's right end of the screen.
				V_DrawString(fx+2, fy+31, V_ALLOWLOWERCASE|V_HUDTRANS|fflags, va("x%d", stplyr->kartstuff[k_itemamount]));
			else
				V_DrawString(fx+24, fy+31, V_ALLOWLOWERCASE|V_HUDTRANS|fflags, va("x%d", stplyr->kartstuff[k_itemamount]));
		else
		{
			V_DrawScaledPatch(fy+28, fy+41, V_HUDTRANS|fflags, kp_itemx);
			V_DrawKartString(fx+38, fy+36, V_HUDTRANS|fflags, va("%d", stplyr->kartstuff[k_itemamount]));
		}
	}
	else
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, FRACUNIT, V_HUDTRANS|fflags, localpatch, colmap);

	// Extensible meter, currently only used for rocket sneaker...
	if (itembar && hudtrans)
	{
		const INT32 fill = ((itembar*barlength)/maxl);
		const INT32 length = min(barlength, fill);
		const INT32 height = (offset ? 1 : 2);
		const INT32 x = (offset ? 17 : 11), y = (offset ? 27 : 35);

		V_DrawScaledPatch(fx+x, fy+y, V_HUDTRANS|fflags, kp_itemtimer[offset]);
		// The left dark "AA" edge
		V_DrawFill(fx+x+1, fy+y+1, (length == 2 ? 2 : 1), height, 12|fflags);
		// The bar itself
		if (length > 2)
		{
			V_DrawFill(fx+x+length, fy+y+1, 1, height, 12|fflags); // the right one
			if (height == 2)
				V_DrawFill(fx+x+2, fy+y+2, length-2, 1, 8|fflags); // the dulled underside
			V_DrawFill(fx+x+2, fy+y+1, length-2, 1, 0|fflags); // the shine
		}
	}

	// Quick Eggman numbers
	if (stplyr->kartstuff[k_eggmanexplode] > 1 /*&& stplyr->kartstuff[k_eggmanexplode] <= 3*TICRATE*/)
		V_DrawScaledPatch(fx+17, fy+13-offset, V_HUDTRANS|fflags, kp_eggnum[min(3, G_TicsToSeconds(stplyr->kartstuff[k_eggmanexplode]))]);

	if (stplyr->kartstuff[k_itemtype] == KITEM_FLAMESHIELD && stplyr->kartstuff[k_flamelength] > 0)
	{
		INT32 numframes = 104;
		INT32 absolutemax = 16 * flameseg;
		INT32 flamemax = stplyr->kartstuff[k_flamelength] * flameseg;
		INT32 flamemeter = min(stplyr->kartstuff[k_flamemeter], flamemax);

		INT32 bf = 16 - stplyr->kartstuff[k_flamelength];
		INT32 ff = numframes - ((flamemeter * numframes) / absolutemax);
		INT32 fmin = (8 * (bf-1));

		INT32 xo = 6, yo = 4;
		INT32 flip = 0;

		if (offset)
		{
			xo++;

			if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]]) // Flip for P1 and P3 (yes, that's correct)
			{
				xo -= 62;
				flip = V_FLIP;
			}
		}

		if (ff < fmin)
			ff = fmin;

		if (bf >= 0 && bf < 16)
			V_DrawScaledPatch(fx-xo, fy-yo, V_HUDTRANS|fflags|flip, kp_flameshieldmeter_bg[bf][offset]);

		if (ff >= 0 && ff < numframes && stplyr->kartstuff[k_flamemeter] > 0)
		{
			if ((stplyr->kartstuff[k_flamemeter] > flamemax) && (leveltime & 1))
			{
				UINT8 *fsflash = R_GetTranslationColormap(TC_BLINK, SKINCOLOR_WHITE, GTC_CACHE);
				V_DrawMappedPatch(fx-xo, fy-yo, V_HUDTRANS|fflags|flip, kp_flameshieldmeter[ff][offset], fsflash);
			}
			else
			{
				V_DrawScaledPatch(fx-xo, fy-yo, V_HUDTRANS|fflags|flip, kp_flameshieldmeter[ff][offset]);
			}
		}
	}
}

void K_drawKartTimestamp(tic_t drawtime, INT32 TX, INT32 TY, INT16 emblemmap, UINT8 mode)
{
	// TIME_X = BASEVIDWIDTH-124;	// 196
	// TIME_Y = 6;					//   6

	tic_t worktime;

	INT32 splitflags = 0;
	if (!mode)
	{
		splitflags = V_HUDTRANS|K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTORIGHT);
		if (cv_timelimit.value && timelimitintics > 0)
		{
			if (drawtime >= timelimitintics)
				drawtime = 0;
			else
				drawtime = timelimitintics - drawtime;
		}
	}

	V_DrawScaledPatch(TX, TY, splitflags, ((mode == 2) ? kp_lapstickerwide : kp_timestickerwide));

	TX += 33;

	worktime = drawtime/(60*TICRATE);

	if (mode && !drawtime)
		V_DrawKartString(TX, TY+3, splitflags, va("--'--\"--"));
	else if (worktime < 100) // 99:99:99 only
	{
		// zero minute
		if (worktime < 10)
		{
			V_DrawKartString(TX, TY+3, splitflags, va("0"));
			// minutes time       0 __ __
			V_DrawKartString(TX+12, TY+3, splitflags, va("%d", worktime));
		}
		// minutes time       0 __ __
		else
			V_DrawKartString(TX, TY+3, splitflags, va("%d", worktime));

		// apostrophe location     _'__ __
		V_DrawKartString(TX+24, TY+3, splitflags, va("'"));

		worktime = (drawtime/TICRATE % 60);

		// zero second        _ 0_ __
		if (worktime < 10)
		{
			V_DrawKartString(TX+36, TY+3, splitflags, va("0"));
		// seconds time       _ _0 __
			V_DrawKartString(TX+48, TY+3, splitflags, va("%d", worktime));
		}
		// zero second        _ 00 __
		else
			V_DrawKartString(TX+36, TY+3, splitflags, va("%d", worktime));

		// quotation mark location    _ __"__
		V_DrawKartString(TX+60, TY+3, splitflags, va("\""));

		worktime = G_TicsToCentiseconds(drawtime);

		// zero tick          _ __ 0_
		if (worktime < 10)
		{
			V_DrawKartString(TX+72, TY+3, splitflags, va("0"));
		// tics               _ __ _0
			V_DrawKartString(TX+84, TY+3, splitflags, va("%d", worktime));
		}
		// zero tick          _ __ 00
		else
			V_DrawKartString(TX+72, TY+3, splitflags, va("%d", worktime));
	}
	else if ((drawtime/TICRATE) & 1)
		V_DrawKartString(TX, TY+3, splitflags, va("99'59\"99"));

	if (emblemmap && (modeattacking || (mode == 1)) && !demo.playback) // emblem time!
	{
		INT32 workx = TX + 96, worky = TY+18;
		SINT8 curemb = 0;
		patch_t *emblempic[3] = {NULL, NULL, NULL};
		UINT8 *emblemcol[3] = {NULL, NULL, NULL};

		emblem_t *emblem = M_GetLevelEmblems(emblemmap);
		while (emblem)
		{
			char targettext[9];

			switch (emblem->type)
			{
				case ET_TIME:
					{
						static boolean canplaysound = true;
						tic_t timetoreach = emblem->var;

						if (emblem->collected)
						{
							emblempic[curemb] = W_CachePatchName(M_GetEmblemPatch(emblem), PU_CACHE);
							emblemcol[curemb] = R_GetTranslationColormap(TC_DEFAULT, M_GetEmblemColor(emblem), GTC_CACHE);
							if (++curemb == 3)
								break;
							goto bademblem;
						}

						snprintf(targettext, 9, "%i'%02i\"%02i",
							G_TicsToMinutes(timetoreach, false),
							G_TicsToSeconds(timetoreach),
							G_TicsToCentiseconds(timetoreach));

						if (!mode)
						{
							if (stplyr->realtime > timetoreach)
							{
								splitflags = (splitflags &~ V_HUDTRANS)|V_HUDTRANSHALF;
								if (canplaysound)
								{
									S_StartSound(NULL, sfx_s3k72); //sfx_s26d); -- you STOLE fizzy lifting drinks
									canplaysound = false;
								}
							}
							else if (!canplaysound)
								canplaysound = true;
						}

						targettext[8] = 0;
					}
					break;
				default:
					goto bademblem;
			}

			V_DrawRightAlignedString(workx, worky, splitflags, targettext);
			workx -= 67;
			V_DrawSmallScaledPatch(workx + 4, worky, splitflags, W_CachePatchName("NEEDIT", PU_CACHE));

			break;

			bademblem:
			emblem = M_GetLevelEmblems(-1);
		}

		if (!mode)
			splitflags = (splitflags &~ V_HUDTRANSHALF)|V_HUDTRANS;
		while (curemb--)
		{
			workx -= 12;
			V_DrawSmallMappedPatch(workx + 4, worky, splitflags, emblempic[curemb], emblemcol[curemb]);
		}
	}
}

static void K_DrawKartPositionNum(INT32 num)
{
	// POSI_X = BASEVIDWIDTH - 51;	// 269
	// POSI_Y = BASEVIDHEIGHT- 64;	// 136

	boolean win = (stplyr->exiting && num == 1);
	//INT32 X = POSI_X;
	INT32 W = SHORT(kp_positionnum[0][0]->width);
	fixed_t scale = FRACUNIT;
	patch_t *localpatch = kp_positionnum[0][0];
	//INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTORIGHT);
	INT32 fx = 0, fy = 0, fflags = 0;
	boolean flipdraw = false;	// flip the order we draw it in for MORE splitscreen bs. fun.
	boolean flipvdraw = false;	// used only for 2p splitscreen so overtaking doesn't make 1P's position fly off the screen.
	boolean overtake = false;

	if (stplyr->kartstuff[k_positiondelay] || stplyr->exiting)
	{
		scale *= 2;
		overtake = true;	// this is used for splitscreen stuff in conjunction with flipdraw.
	}
	if (r_splitscreen)
		scale /= 2;

	W = FixedMul(W<<FRACBITS, scale)>>FRACBITS;

	// pain and suffering defined below
	if (!r_splitscreen)
	{
		fx = POSI_X;
		fy = BASEVIDHEIGHT - 8;
		fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT;
	}
	else if (r_splitscreen == 1)	// for this splitscreen, we'll use case by case because it's a bit different.
	{
		fx = POSI_X;
		if (stplyr == &players[displayplayers[0]])	// for player 1: display this at the top right, above the minimap.
		{
			fy = 30;
			fflags = V_SNAPTOTOP|V_SNAPTORIGHT;
			if (overtake)
				flipvdraw = true;	// make sure overtaking doesn't explode us
		}
		else	// if we're not p1, that means we're p2. display this at the bottom right, below the minimap.
		{
			fy = BASEVIDHEIGHT - 8;
			fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT;
		}
	}
	else
	{
		if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]])	// If we are P1 or P3...
		{
			fx = POSI_X;
			fy = POSI_Y;
			fflags = V_SNAPTOLEFT|((stplyr == &players[displayplayers[2]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P3 to the bottom.
			flipdraw = true;
			if (num && num >= 10)
				fx += W;	// this seems dumb, but we need to do this in order for positions above 10 going off screen.
		}
		else // else, that means we're P2 or P4.
		{
			fx = POSI2_X;
			fy = POSI2_Y;
			fflags = V_SNAPTORIGHT|((stplyr == &players[displayplayers[3]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P4 to the bottom
		}
	}

	// Special case for 0
	if (!num)
	{
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, scale, V_HUDTRANSHALF|fflags, kp_positionnum[0][0], NULL);
		return;
	}

	I_Assert(num >= 0); // This function does not draw negative numbers

	// Draw the number
	while (num)
	{
		if (win) // 1st place winner? You get rainbows!!
			localpatch = kp_winnernum[(leveltime % (NUMWINFRAMES*3)) / 3];
		else if (stplyr->laps >= cv_numlaps.value || stplyr->exiting) // Check for the final lap, or won
		{
			// Alternate frame every three frames
			switch (leveltime % 9)
			{
				case 1: case 2: case 3:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][4];
					else
						localpatch = kp_positionnum[num % 10][1];
					break;
				case 4: case 5: case 6:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][5];
					else
						localpatch = kp_positionnum[num % 10][2];
					break;
				case 7: case 8: case 9:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][6];
					else
						localpatch = kp_positionnum[num % 10][3];
					break;
				default:
					localpatch = kp_positionnum[num % 10][0];
					break;
			}
		}
		else
			localpatch = kp_positionnum[num % 10][0];

		V_DrawFixedPatch((fx<<FRACBITS) + ((overtake && flipdraw) ? (SHORT(localpatch->width)*scale/2) : 0), (fy<<FRACBITS) + ((overtake && flipvdraw) ? (SHORT(localpatch->height)*scale/2) : 0), scale, V_HUDTRANSHALF|fflags, localpatch, NULL);
		// ^ if we overtake as p1 or p3 in splitscren, we shift it so that it doesn't go off screen.
		// ^ if we overtake as p1 in 2p splits, shift vertically so that this doesn't happen either.

		fx -= W;
		num /= 10;
	}
}

static boolean K_drawKartPositionFaces(void)
{
	// FACE_X = 15;				//  15
	// FACE_Y = 72;				//  72

	INT32 Y = FACE_Y+9; // +9 to offset where it's being drawn if there are more than one
	INT32 i, j, ranklines, strank = -1;
	boolean completed[MAXPLAYERS];
	INT32 rankplayer[MAXPLAYERS];
	INT32 bumperx, numplayersingame = 0;
	UINT8 *colormap;

	ranklines = 0;
	memset(completed, 0, sizeof (completed));
	memset(rankplayer, 0, sizeof (rankplayer));

	for (i = 0; i < MAXPLAYERS; i++)
	{
		rankplayer[i] = -1;

		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

		numplayersingame++;
	}

	if (numplayersingame <= 1)
		return true;

#ifdef HAVE_BLUA
	if (!LUA_HudEnabled(hud_minirankings))
		return false;	// Don't proceed but still return true for free play above if HUD is disabled.
#endif

	for (j = 0; j < numplayersingame; j++)
	{
		UINT8 lowestposition = MAXPLAYERS+1;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (completed[i] || !playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

			if (players[i].kartstuff[k_position] >= lowestposition)
				continue;

			rankplayer[ranklines] = i;
			lowestposition = players[i].kartstuff[k_position];
		}

		i = rankplayer[ranklines];

		completed[i] = true;

		if (players+i == stplyr)
			strank = ranklines;

		//if (ranklines == 5)
			//break; // Only draw the top 5 players -- we do this a different way now...

		ranklines++;
	}

	if (ranklines < 5)
		Y -= (9*ranklines);
	else
		Y -= (9*5);

	if (G_BattleGametype() || strank <= 2) // too close to the top, or playing battle, or a spectator? would have had (strank == -1) called out, but already caught by (strank <= 2)
	{
		i = 0;
		if (ranklines > 5) // could be both...
			ranklines = 5;
	}
	else if (strank+3 > ranklines) // too close to the bottom?
	{
		i = ranklines - 5;
		if (i < 0)
			i = 0;
	}
	else
	{
		i = strank-2;
		ranklines = strank+3;
	}

	for (; i < ranklines; i++)
	{
		if (!playeringame[rankplayer[i]]) continue;
		if (players[rankplayer[i]].spectator) continue;
		if (!players[rankplayer[i]].mo) continue;

		bumperx = FACE_X+19;

		if (players[rankplayer[i]].mo->color)
		{
			colormap = R_GetTranslationColormap(players[rankplayer[i]].skin, players[rankplayer[i]].mo->color, GTC_CACHE);
			if (players[rankplayer[i]].mo->colorized)
				colormap = R_GetTranslationColormap(TC_RAINBOW, players[rankplayer[i]].mo->color, GTC_CACHE);
			else
				colormap = R_GetTranslationColormap(players[rankplayer[i]].skin, players[rankplayer[i]].mo->color, GTC_CACHE);

			V_DrawMappedPatch(FACE_X, Y, V_HUDTRANS|V_SNAPTOLEFT, facerankprefix[players[rankplayer[i]].skin], colormap);

#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_battlebumpers))
			{
#endif
				if (G_BattleGametype() && players[rankplayer[i]].kartstuff[k_bumper] > 0)
				{
					V_DrawMappedPatch(bumperx-2, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_tinybumper[0], colormap);
					for (j = 1; j < players[rankplayer[i]].kartstuff[k_bumper]; j++)
					{
						bumperx += 5;
						V_DrawMappedPatch(bumperx, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_tinybumper[1], colormap);
					}
				}
#ifdef HAVE_BLUA
			}	// A new level of stupidity: checking if lua is enabled to close a bracket. :Fascinating:
#endif
		}

		if (i == strank)
			V_DrawScaledPatch(FACE_X, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_facehighlight[(leveltime / 4) % 8]);

		if (G_BattleGametype() && players[rankplayer[i]].kartstuff[k_bumper] <= 0)
			V_DrawScaledPatch(FACE_X-4, Y-3, V_HUDTRANS|V_SNAPTOLEFT, kp_ranknobumpers);
		else
		{
			INT32 pos = players[rankplayer[i]].kartstuff[k_position];
			if (pos < 0 || pos > MAXPLAYERS)
				pos = 0;
			// Draws the little number over the face
			V_DrawScaledPatch(FACE_X-5, Y+10, V_HUDTRANS|V_SNAPTOLEFT, kp_facenum[pos]);
		}

		Y += 18;
	}

	return false;
}

//
// HU_DrawTabRankings -- moved here to take advantage of kart stuff!
//
void HU_DrawTabRankings(INT32 x, INT32 y, playersort_t *tab, INT32 scorelines, INT32 whiteplayer, INT32 hilicol)
{
	static tic_t alagles_timer = 9;
	INT32 i, rightoffset = 240;
	const UINT8 *colormap;
	INT32 dupadjust = (vid.width/vid.dupx), duptweak = (dupadjust - BASEVIDWIDTH)/2;
	int y2;

	//this function is designed for 9 or less score lines only
	//I_Assert(scorelines <= 9); -- not today bitch, kart fixed it up

	V_DrawFill(1-duptweak, 26, dupadjust-2, 1, 0); // Draw a horizontal line because it looks nice!
	if (scorelines > 8)
	{
		V_DrawFill(160, 26, 1, 147, 0); // Draw a vertical line to separate the two sides.
		V_DrawFill(1-duptweak, 173, dupadjust-2, 1, 0); // And a horizontal line near the bottom.
		rightoffset = (BASEVIDWIDTH/2) - 4 - x;
	}

	for (i = 0; i < scorelines; i++)
	{
		char strtime[MAXPLAYERNAME+1];

		if (players[tab[i].num].spectator || !players[tab[i].num].mo)
			continue; //ignore them.

		if (netgame) // don't draw ping offline
		{
			if (players[tab[i].num].bot)
			{
				; // TODO: Put a graphic here to indicate this player is a bot!
			}
			else if (tab[i].num != serverplayer || !server_lagless)
			{
				HU_drawPing(x + ((i < 8) ? -17 : rightoffset + 11), y-4, playerpingtable[tab[i].num], 0);
			}
		}

		STRBUFCPY(strtime, tab[i].name);

		y2 = y;

		if (netgame && playerconsole[tab[i].num] == 0 && server_lagless && !players[tab[i].num].bot)
		{
			y2 = ( y - 4 );

			V_DrawScaledPatch(x + 20, y2, 0, kp_blagles[(leveltime / 3) % 6]);
			// every 70 tics
			if (( leveltime % 70 ) == 0)
			{
				alagles_timer = 9;
			}
			if (alagles_timer > 0)
			{
				V_DrawScaledPatch(x + 20, y2, 0, kp_alagles[alagles_timer]);
				if (( leveltime % 2 ) == 0)
					alagles_timer--;
			}
			else
				V_DrawScaledPatch(x + 20, y2, 0, kp_alagles[0]);

			y2 += SHORT (kp_alagles[0]->height) + 1;
		}

		if (scorelines > 8)
			V_DrawThinString(x + 20, y2, ((tab[i].num == whiteplayer) ? hilicol : 0)|V_ALLOWLOWERCASE|V_6WIDTHSPACE, strtime);
		else
			V_DrawString(x + 20, y2, ((tab[i].num == whiteplayer) ? hilicol : 0)|V_ALLOWLOWERCASE, strtime);

		if (players[tab[i].num].mo->color)
		{
			colormap = R_GetTranslationColormap(players[tab[i].num].skin, players[tab[i].num].mo->color, GTC_CACHE);
			if (players[tab[i].num].mo->colorized)
				colormap = R_GetTranslationColormap(TC_RAINBOW, players[tab[i].num].mo->color, GTC_CACHE);
			else
				colormap = R_GetTranslationColormap(players[tab[i].num].skin, players[tab[i].num].mo->color, GTC_CACHE);

			V_DrawMappedPatch(x, y-4, 0, facerankprefix[players[tab[i].num].skin], colormap);
			/*if (G_BattleGametype() && players[tab[i].num].kartstuff[k_bumper] > 0) -- not enough space for this
			{
				INT32 bumperx = x+19;
				V_DrawMappedPatch(bumperx-2, y-4, 0, kp_tinybumper[0], colormap);
				for (j = 1; j < players[tab[i].num].kartstuff[k_bumper]; j++)
				{
					bumperx += 5;
					V_DrawMappedPatch(bumperx, y-4, 0, kp_tinybumper[1], colormap);
				}
			}*/
		}

		if (tab[i].num == whiteplayer)
			V_DrawScaledPatch(x, y-4, 0, kp_facehighlight[(leveltime / 4) % 8]);

		if (G_BattleGametype() && players[tab[i].num].kartstuff[k_bumper] <= 0)
			V_DrawScaledPatch(x-4, y-7, 0, kp_ranknobumpers);
		else
		{
			INT32 pos = players[tab[i].num].kartstuff[k_position];
			if (pos < 0 || pos > MAXPLAYERS)
				pos = 0;
			// Draws the little number over the face
			V_DrawScaledPatch(x-5, y+6, 0, kp_facenum[pos]);
		}

		if (G_RaceGametype())
		{
#define timestring(time) va("%i'%02i\"%02i", G_TicsToMinutes(time, true), G_TicsToSeconds(time), G_TicsToCentiseconds(time))
			if (scorelines > 8)
			{
				if (players[tab[i].num].exiting)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, hilicol|V_6WIDTHSPACE, timestring(players[tab[i].num].realtime));
				else if (players[tab[i].num].pflags & PF_TIMEOVER)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, V_6WIDTHSPACE, "NO CONTEST.");
				else if (circuitmap)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, V_6WIDTHSPACE, va("Lap %d", tab[i].count));
			}
			else
			{
				if (players[tab[i].num].exiting)
					V_DrawRightAlignedString(x+rightoffset, y, hilicol, timestring(players[tab[i].num].realtime));
				else if (players[tab[i].num].pflags & PF_TIMEOVER)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, 0, "NO CONTEST.");
				else if (circuitmap)
					V_DrawRightAlignedString(x+rightoffset, y, 0, va("Lap %d", tab[i].count));
			}
#undef timestring
		}
		else
			V_DrawRightAlignedString(x+rightoffset, y, 0, va("%u", tab[i].count));

		y += 18;
		if (i == 7)
		{
			y = 33;
			x = (BASEVIDWIDTH/2) + 4;
		}
	}
}

#define RINGANIM_FLIPFRAME (RINGANIM_NUMFRAMES/2)

static void K_drawKartLapsAndRings(void)
{
	SINT8 ringanim_realframe = stplyr->karthud[khud_ringframe];
	INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);
	UINT8 rn[2];
	INT32 ringflip = 0;
	UINT8 *ringmap = NULL;
	boolean colorring = false;
	INT32 ringx = 0;

	rn[0] = ((abs(stplyr->kartstuff[k_rings]) / 10) % 10);
	rn[1] = (abs(stplyr->kartstuff[k_rings]) % 10);

	if (stplyr->kartstuff[k_rings] <= 0 && (leveltime/5 & 1)) // In debt
	{
		ringmap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_CRIMSON, GTC_CACHE);
		colorring = true;
	}
	else if (stplyr->kartstuff[k_rings] >= 20) // Maxed out
		ringmap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_YELLOW, GTC_CACHE);

	if (stplyr->karthud[khud_ringframe] > RINGANIM_FLIPFRAME)
	{
		ringflip = V_FLIP;
		ringanim_realframe = RINGANIM_NUMFRAMES-stplyr->karthud[khud_ringframe];
		ringx += SHORT((r_splitscreen > 1) ? kp_smallring[ringanim_realframe]->width : kp_ring[ringanim_realframe]->width);
	}

	if (r_splitscreen > 1)
	{
		INT32 fx = 0, fy = 0, fr = 0;
		INT32 flipflag = 0;

		// pain and suffering defined below
		if (r_splitscreen < 2)	// don't change shit for THIS splitscreen.
		{
			fx = LAPS_X;
			fy = LAPS_Y;
		}
		else
		{
			if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]])	// If we are P1 or P3...
			{
				fx = LAPS_X;
				fy = LAPS_Y;
				splitflags = V_SNAPTOLEFT|((stplyr == &players[displayplayers[2]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0); // flip P3 to the bottom.
			}
			else // else, that means we're P2 or P4.
			{
				fx = LAPS2_X;
				fy = LAPS2_Y;
				splitflags = V_SNAPTORIGHT|((stplyr == &players[displayplayers[3]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0); // flip P4 to the bottom
				flipflag = V_FLIP; // make the string right aligned and other shit
			}
		}

		fr = fx;

		// Laps
		V_DrawScaledPatch(fx-2 + (flipflag ? (SHORT(kp_ringstickersplit[1]->width) - 3) : 0), fy, V_HUDTRANS|splitflags|flipflag, kp_ringstickersplit[0]);

		V_DrawScaledPatch(fx, fy, V_HUDTRANS|splitflags, kp_splitlapflag);
		V_DrawScaledPatch(fx+22, fy, V_HUDTRANS|splitflags, frameslash);

		if (cv_numlaps.value >= 10)
		{
			UINT8 ln[2];
			ln[0] = ((stplyr->laps / 10) % 10);
			ln[1] = (stplyr->laps % 10);

			V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
			V_DrawScaledPatch(fx+17, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);

			ln[0] = ((abs(cv_numlaps.value) / 10) % 10);
			ln[1] = (abs(cv_numlaps.value) % 10);

			V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
			V_DrawScaledPatch(fx+31, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);
		}
		else
		{
			V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, kp_facenum[(stplyr->laps) % 10]);
			V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, kp_facenum[(cv_numlaps.value) % 10]);
		}

		// Rings
		if (netgame)
		{
			V_DrawScaledPatch(fx-2 + (flipflag ? (SHORT(kp_ringstickersplit[1]->width) - 3) : 0), fy-10, V_HUDTRANS|splitflags|flipflag, kp_ringstickersplit[1]);
			if (flipflag)
				fr += 15;
		}
		else
			V_DrawScaledPatch(fx-2 + (flipflag ? (SHORT(kp_ringstickersplit[0]->width) - 3) : 0), fy-10, V_HUDTRANS|splitflags|flipflag, kp_ringstickersplit[0]);

		V_DrawMappedPatch(fr+ringx, fy-13, V_HUDTRANS|splitflags|ringflip, kp_smallring[ringanim_realframe], (colorring ? ringmap : NULL));

		if (stplyr->kartstuff[k_rings] < 0) // Draw the minus for ring debt
			V_DrawMappedPatch(fr+7, fy-10, V_HUDTRANS|splitflags, kp_ringdebtminussmall, ringmap);

		V_DrawMappedPatch(fr+11, fy-10, V_HUDTRANS|splitflags, pingnum[rn[0]], ringmap);
		V_DrawMappedPatch(fr+15, fy-10, V_HUDTRANS|splitflags, pingnum[rn[1]], ringmap);

		// SPB ring lock
		if (stplyr->kartstuff[k_ringlock])
			V_DrawScaledPatch(fr-12, fy-23, V_HUDTRANS|splitflags, kp_ringspblocksmall[stplyr->karthud[khud_ringspblock]]);

		// Lives
		if (!netgame)
		{
			UINT8 *colormap = R_GetTranslationColormap(stplyr->skin, stplyr->skincolor, GTC_CACHE);
			V_DrawMappedPatch(fr+21, fy-13, V_HUDTRANS|splitflags, facemmapprefix[stplyr->skin], colormap);
			V_DrawScaledPatch(fr+34, fy-10, V_HUDTRANS|splitflags, pingnum[(stplyr->lives % 10)]); // make sure this doesn't overflow
		}
	}
	else
	{
		// Laps
		V_DrawScaledPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_lapsticker);

		if (stplyr->exiting)
			V_DrawKartString(LAPS_X+33, LAPS_Y+3, V_HUDTRANS|splitflags, "FIN");
		else
			V_DrawKartString(LAPS_X+33, LAPS_Y+3, V_HUDTRANS|splitflags, va("%d/%d", stplyr->laps, cv_numlaps.value));

		// Rings
		if (netgame)
			V_DrawScaledPatch(LAPS_X, LAPS_Y-11, V_HUDTRANS|splitflags, kp_ringsticker[1]);
		else
			V_DrawScaledPatch(LAPS_X, LAPS_Y-11, V_HUDTRANS|splitflags, kp_ringsticker[0]);

		V_DrawMappedPatch(LAPS_X+ringx+7, LAPS_Y-16, V_HUDTRANS|splitflags|ringflip, kp_ring[ringanim_realframe], (colorring ? ringmap : NULL));

		if (stplyr->kartstuff[k_rings] < 0) // Draw the minus for ring debt
		{
			V_DrawMappedPatch(LAPS_X+23, LAPS_Y-11, V_HUDTRANS|splitflags, kp_ringdebtminus, ringmap);
			V_DrawMappedPatch(LAPS_X+29, LAPS_Y-11, V_HUDTRANS|splitflags, kp_facenum[rn[0]], ringmap);
			V_DrawMappedPatch(LAPS_X+35, LAPS_Y-11, V_HUDTRANS|splitflags, kp_facenum[rn[1]], ringmap);
		}
		else
		{
			V_DrawMappedPatch(LAPS_X+23, LAPS_Y-11, V_HUDTRANS|splitflags, kp_facenum[rn[0]], ringmap);
			V_DrawMappedPatch(LAPS_X+29, LAPS_Y-11, V_HUDTRANS|splitflags, kp_facenum[rn[1]], ringmap);
		}

		// SPB ring lock
		if (stplyr->kartstuff[k_ringlock])
			V_DrawScaledPatch(LAPS_X-5, LAPS_Y-28, V_HUDTRANS|splitflags, kp_ringspblock[stplyr->karthud[khud_ringspblock]]);

		// Lives
		if (!netgame)
		{
			UINT8 *colormap = R_GetTranslationColormap(stplyr->skin, stplyr->skincolor, GTC_CACHE);
			V_DrawMappedPatch(LAPS_X+46, LAPS_Y-16, V_HUDTRANS|splitflags, facerankprefix[stplyr->skin], colormap);
			V_DrawScaledPatch(LAPS_X+63, LAPS_Y-11, V_HUDTRANS|splitflags, kp_facenum[(stplyr->lives % 10)]); // make sure this doesn't overflow
		}
	}
}

#undef RINGANIM_NUMFRAMES
#undef RINGANIM_FLIPFRAME

static void K_drawKartSpeedometer(void)
{
	static fixed_t convSpeed;
	UINT8 labeln = 0;
	UINT8 numbers[3];
	INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);
	UINT8 battleoffset = 0;

	if (!stplyr->exiting) // Keep the same speed value as when you crossed the finish line!
	{
		switch (cv_kartspeedometer.value)
		{
			case 1: // Sonic Drift 2 style percentage
			default:
				convSpeed = (((25*stplyr->speed)/24) * 100) / K_GetKartSpeed(stplyr, false); // Based on top speed! (cheats with the numbers due to some weird discrepancy)
				labeln = 0;
				break;
			case 2: // Kilometers
				convSpeed = FixedDiv(FixedMul(stplyr->speed, 142371), mapobjectscale)/FRACUNIT; // 2.172409058
				labeln = 1;
				break;
			case 3: // Miles
				convSpeed = FixedDiv(FixedMul(stplyr->speed, 88465), mapobjectscale)/FRACUNIT; // 1.349868774
				labeln = 2;
				break;
			case 4: // Fracunits
				convSpeed = FixedDiv(stplyr->speed, mapobjectscale)/FRACUNIT; // 1.0. duh.
				labeln = 3;
				break;
		}
	}

	// Don't overflow
	if (convSpeed > 999)
		convSpeed = 999;

	numbers[0] = ((convSpeed / 100) % 10);
	numbers[1] = ((convSpeed / 10) % 10);
	numbers[2] = (convSpeed % 10);

	if (G_BattleGametype())
		battleoffset = 8;

	V_DrawScaledPatch(LAPS_X, LAPS_Y-25 + battleoffset, V_HUDTRANS|splitflags, kp_speedometersticker);
	V_DrawScaledPatch(LAPS_X+7, LAPS_Y-25 + battleoffset, V_HUDTRANS|splitflags, kp_facenum[numbers[0]]);
	V_DrawScaledPatch(LAPS_X+13, LAPS_Y-25 + battleoffset, V_HUDTRANS|splitflags, kp_facenum[numbers[1]]);
	V_DrawScaledPatch(LAPS_X+19, LAPS_Y-25 + battleoffset, V_HUDTRANS|splitflags, kp_facenum[numbers[2]]);
	V_DrawScaledPatch(LAPS_X+29, LAPS_Y-25 + battleoffset, V_HUDTRANS|splitflags, kp_speedometerlabel[labeln]);
}

static void K_drawKartBumpersOrKarma(void)
{
	UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, stplyr->skincolor, GTC_CACHE);
	INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);

	if (r_splitscreen > 1)
	{
		INT32 fx = 0, fy = 0;
		INT32 flipflag = 0;

		// pain and suffering defined below
		if (r_splitscreen < 2)	// don't change shit for THIS splitscreen.
		{
			fx = LAPS_X;
			fy = LAPS_Y;
		}
		else
		{
			if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]])	// If we are P1 or P3...
			{
				fx = LAPS_X;
				fy = LAPS_Y;
				splitflags = V_SNAPTOLEFT|((stplyr == &players[displayplayers[2]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0); // flip P3 to the bottom.
			}
			else // else, that means we're P2 or P4.
			{
				fx = LAPS2_X;
				fy = LAPS2_Y;
				splitflags = V_SNAPTORIGHT|((stplyr == &players[displayplayers[3]]) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0); // flip P4 to the bottom
				flipflag = V_FLIP; // make the string right aligned and other shit
			}
		}

		V_DrawScaledPatch(fx-2 + (flipflag ? (SHORT(kp_ringstickersplit[1]->width) - 3) : 0), fy, V_HUDTRANS|splitflags|flipflag, kp_ringstickersplit[0]);
		V_DrawScaledPatch(fx+22, fy, V_HUDTRANS|splitflags, frameslash);

		if (battlecapsules)
		{
			V_DrawMappedPatch(fx+1, fy-2, V_HUDTRANS|splitflags, kp_rankcapsule, NULL);

			if (numtargets > 9 || maptargets > 9)
			{
				UINT8 ln[2];
				ln[0] = ((numtargets / 10) % 10);
				ln[1] = (numtargets % 10);

				V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
				V_DrawScaledPatch(fx+17, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);

				ln[0] = ((maptargets / 10) % 10);
				ln[1] = (maptargets % 10);

				V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
				V_DrawScaledPatch(fx+31, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);
			}
			else
			{
				V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, kp_facenum[numtargets % 10]);
				V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, kp_facenum[maptargets % 10]);
			}
		}
		else
		{
			if (stplyr->kartstuff[k_bumper] <= 0)
			{
				V_DrawMappedPatch(fx+1, fy-2, V_HUDTRANS|splitflags, kp_splitkarmabomb, colormap);
				V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, kp_facenum[(stplyr->kartstuff[k_comebackpoints]) % 10]);
				V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, kp_facenum[2]);
			}
			else
			{
				INT32 maxbumper = K_StartingBumperCount();
				V_DrawMappedPatch(fx+1, fy-2, V_HUDTRANS|splitflags, kp_rankbumper, colormap);

				if (stplyr->kartstuff[k_bumper] > 9 || maxbumper > 9)
				{
					UINT8 ln[2];
					ln[0] = ((abs(stplyr->kartstuff[k_bumper]) / 10) % 10);
					ln[1] = (abs(stplyr->kartstuff[k_bumper]) % 10);

					V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
					V_DrawScaledPatch(fx+17, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);

					ln[0] = ((abs(maxbumper) / 10) % 10);
					ln[1] = (abs(maxbumper) % 10);

					V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, pingnum[ln[0]]);
					V_DrawScaledPatch(fx+31, fy, V_HUDTRANS|splitflags, pingnum[ln[1]]);
				}
				else
				{
					V_DrawScaledPatch(fx+13, fy, V_HUDTRANS|splitflags, kp_facenum[(stplyr->kartstuff[k_bumper]) % 10]);
					V_DrawScaledPatch(fx+27, fy, V_HUDTRANS|splitflags, kp_facenum[(maxbumper) % 10]);
				}
			}
		}
	}
	else
	{
		if (battlecapsules)
		{
			if (numtargets > 9 && maptargets > 9)
				V_DrawMappedPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_capsulestickerwide, NULL);
			else
				V_DrawMappedPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_capsulesticker, NULL);
			V_DrawKartString(LAPS_X+47, LAPS_Y+3, V_HUDTRANS|splitflags, va("%d/%d", numtargets, maptargets));
		}
		else
		{
			if (stplyr->kartstuff[k_bumper] <= 0)
			{
				V_DrawMappedPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_karmasticker, colormap);
				V_DrawKartString(LAPS_X+47, LAPS_Y+3, V_HUDTRANS|splitflags, va("%d/2", stplyr->kartstuff[k_comebackpoints]));
			}
			else
			{
				INT32 maxbumper = K_StartingBumperCount();

				if (stplyr->kartstuff[k_bumper] > 9 && maxbumper > 9)
					V_DrawMappedPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_bumperstickerwide, colormap);
				else
					V_DrawMappedPatch(LAPS_X, LAPS_Y, V_HUDTRANS|splitflags, kp_bumpersticker, colormap);

				V_DrawKartString(LAPS_X+47, LAPS_Y+3, V_HUDTRANS|splitflags, va("%d/%d", stplyr->kartstuff[k_bumper], maxbumper));
			}
		}
	}
}

static fixed_t K_FindCheckX(fixed_t px, fixed_t py, angle_t ang, fixed_t mx, fixed_t my)
{
	fixed_t dist, x;
	fixed_t range = RING_DIST/3;
	angle_t diff;

	range *= gamespeed+1;

	dist = abs(R_PointToDist2(px, py, mx, my));
	if (dist > range)
		return -320;

	diff = R_PointToAngle2(px, py, mx, my) - ang;

	if (diff < ANGLE_90 || diff > ANGLE_270)
		return -320;
	else
		x = (FixedMul(FINETANGENT(((diff+ANGLE_90)>>ANGLETOFINESHIFT) & 4095), 160<<FRACBITS) + (160<<FRACBITS))>>FRACBITS;

	if (encoremode)
		x = 320-x;

	if (r_splitscreen > 1)
		x /= 2;

	return x;
}

static void K_drawKartWanted(void)
{
	UINT8 i, numwanted = 0;
	UINT8 *colormap = NULL;
	INT32 basex = 0, basey = 0;

	if (stplyr != &players[displayplayers[0]])
		return;

	for (i = 0; i < 4; i++)
	{
		if (battlewanted[i] == -1)
			break;
		numwanted++;
	}

	if (numwanted <= 0)
		return;

	// set X/Y coords depending on splitscreen.
	if (r_splitscreen < 3) // 1P and 2P use the same code.
	{
		basex = WANT_X;
		basey = WANT_Y;
		if (r_splitscreen == 2)
		{
			basey += 16; // slight adjust for 3P
			basex -= 6;
		}
	}
	else if (r_splitscreen == 3) // 4P splitscreen...
	{
		basex = BASEVIDWIDTH/2 - (SHORT(kp_wantedsplit->width)/2);	// center on screen
		basey = BASEVIDHEIGHT - 55;
		//basey2 = 4;
	}

	if (battlewanted[0] != -1)
		colormap = R_GetTranslationColormap(0, players[battlewanted[0]].skincolor, GTC_CACHE);
	V_DrawFixedPatch(basex<<FRACBITS, basey<<FRACBITS, FRACUNIT, V_HUDTRANS|(r_splitscreen < 3 ? V_SNAPTORIGHT : 0)|V_SNAPTOBOTTOM, (r_splitscreen > 1 ? kp_wantedsplit : kp_wanted), colormap);
	/*if (basey2)
		V_DrawFixedPatch(basex<<FRACBITS, basey2<<FRACBITS, FRACUNIT, V_HUDTRANS|V_SNAPTOTOP, (splitscreen == 3 ? kp_wantedsplit : kp_wanted), colormap);	// < used for 4p splits.*/

	for (i = 0; i < numwanted; i++)
	{
		INT32 x = basex+(r_splitscreen > 1 ? 13 : 8), y = basey+(r_splitscreen > 1 ? 16 : 21);
		fixed_t scale = FRACUNIT/2;
		player_t *p = &players[battlewanted[i]];

		if (battlewanted[i] == -1)
			break;

		if (numwanted == 1)
			scale = FRACUNIT;
		else
		{
			if (i & 1)
				x += 16;
			if (i > 1)
				y += 16;
		}

		if (players[battlewanted[i]].skincolor)
		{
			colormap = R_GetTranslationColormap(TC_RAINBOW, p->skincolor, GTC_CACHE);
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, V_HUDTRANS|(r_splitscreen < 3 ? V_SNAPTORIGHT : 0)|V_SNAPTOBOTTOM, (scale == FRACUNIT ? facewantprefix[p->skin] : facerankprefix[p->skin]), colormap);
			/*if (basey2)	// again with 4p stuff
				V_DrawFixedPatch(x<<FRACBITS, (y - (basey-basey2))<<FRACBITS, FRACUNIT, V_HUDTRANS|V_SNAPTOTOP, (scale == FRACUNIT ? facewantprefix[p->skin] : facerankprefix[p->skin]), colormap);*/
		}
	}
}

static void K_drawKartPlayerCheck(void)
{
	INT32 i;
	UINT8 *colormap;
	INT32 x;

	INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM);

	if (!stplyr->mo || stplyr->spectator)
		return;

	if (stplyr->awayviewtics)
		return;

	if (( stplyr->cmd.buttons & BT_LOOKBACK ))
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT8 pnum = 0;

		if (&players[i] == stplyr)
			continue;
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (!players[i].mo)
			continue;

		if ((players[i].kartstuff[k_invincibilitytimer] <= 0) && (leveltime & 2))
			pnum++; // white frames

		if (players[i].kartstuff[k_itemtype] == KITEM_GROW || players[i].kartstuff[k_growshrinktimer] > 0)
			pnum += 4;
		else if (players[i].kartstuff[k_itemtype] == KITEM_INVINCIBILITY || players[i].kartstuff[k_invincibilitytimer])
			pnum += 2;

		x = K_FindCheckX(stplyr->mo->x, stplyr->mo->y, stplyr->mo->angle, players[i].mo->x, players[i].mo->y);
		if (x <= 320 && x >= 0)
		{
			if (x < 14)
				x = 14;
			else if (x > 306)
				x = 306;

			colormap = R_GetTranslationColormap(TC_DEFAULT, players[i].mo->color, GTC_CACHE);
			V_DrawMappedPatch(x, CHEK_Y, V_HUDTRANS|splitflags, kp_check[pnum], colormap);
		}
	}
}

static void K_drawKartMinimapIcon(fixed_t objx, fixed_t objy, INT32 hudx, INT32 hudy, INT32 flags, patch_t *icon, UINT8 *colormap, patch_t *AutomapPic)
{
	// amnum xpos & ypos are the icon's speed around the HUD.
	// The number being divided by is for how fast it moves.
	// The higher the number, the slower it moves.

	// am xpos & ypos are the icon's starting position. Withouht
	// it, they wouldn't 'spawn' on the top-right side of the HUD.

	fixed_t amnumxpos, amnumypos;
	INT32 amxpos, amypos;

	node_t *bsp = &nodes[numnodes-1];
	fixed_t maxx, minx, maxy, miny;

	fixed_t mapwidth, mapheight;
	fixed_t xoffset, yoffset;
	fixed_t xscale, yscale, zoom;

	maxx = maxy = INT32_MAX;
	minx = miny = INT32_MIN;
	minx = bsp->bbox[0][BOXLEFT];
	maxx = bsp->bbox[0][BOXRIGHT];
	miny = bsp->bbox[0][BOXBOTTOM];
	maxy = bsp->bbox[0][BOXTOP];

	if (bsp->bbox[1][BOXLEFT] < minx)
		minx = bsp->bbox[1][BOXLEFT];
	if (bsp->bbox[1][BOXRIGHT] > maxx)
		maxx = bsp->bbox[1][BOXRIGHT];
	if (bsp->bbox[1][BOXBOTTOM] < miny)
		miny = bsp->bbox[1][BOXBOTTOM];
	if (bsp->bbox[1][BOXTOP] > maxy)
		maxy = bsp->bbox[1][BOXTOP];

	// You might be wondering why these are being bitshift here
	// it's because mapwidth and height would otherwise overflow for maps larger than half the size possible...
	// map boundaries and sizes will ALWAYS be whole numbers thankfully
	// later calculations take into consideration that these are actually not in terms of FRACUNIT though
	minx >>= FRACBITS;
	maxx >>= FRACBITS;
	miny >>= FRACBITS;
	maxy >>= FRACBITS;

	mapwidth = maxx - minx;
	mapheight = maxy - miny;

	// These should always be small enough to be bitshift back right now
	xoffset = (minx + mapwidth/2)<<FRACBITS;
	yoffset = (miny + mapheight/2)<<FRACBITS;

	xscale = FixedDiv(AutomapPic->width, mapwidth);
	yscale = FixedDiv(AutomapPic->height, mapheight);
	zoom = FixedMul(min(xscale, yscale), FRACUNIT-FRACUNIT/20);

	amnumxpos = (FixedMul(objx, zoom) - FixedMul(xoffset, zoom));
	amnumypos = -(FixedMul(objy, zoom) - FixedMul(yoffset, zoom));

	if (encoremode)
		amnumxpos = -amnumxpos;

	amxpos = amnumxpos + ((hudx + AutomapPic->width/2 - (icon->width/2))<<FRACBITS);
	amypos = amnumypos + ((hudy + AutomapPic->height/2 - (icon->height/2))<<FRACBITS);

	// do we want this? it feels unnecessary. easier to just modify the amnumxpos?
	/*if (encoremode)
	{
		flags |= V_FLIP;
		amxpos = -amnumxpos + ((hudx + AutomapPic->width/2 + (icon->width/2))<<FRACBITS);
	}*/

	V_DrawFixedPatch(amxpos, amypos, FRACUNIT, flags, icon, colormap);
}

static void K_drawKartMinimap(void)
{
	INT32 lumpnum;
	patch_t *AutomapPic;
	INT32 i = 0;
	INT32 x, y;
	INT32 minimaptrans, splitflags = (r_splitscreen == 3 ? 0 : V_SNAPTORIGHT); // flags should only be 0 when it's centered (4p split)
	UINT8 skin = 0;
	UINT8 *colormap = NULL;
	SINT8 localplayers[4];
	SINT8 numlocalplayers = 0;
	mobj_t *mobj, *next;	// for SPB drawing (or any other item(s) we may wanna draw, I dunno!)

	// Draw the HUD only when playing in a level.
	// hu_stuff needs this, unlike st_stuff.
	if (gamestate != GS_LEVEL)
		return;

	// Only draw for the first player
	// Maybe move this somewhere else where this won't be a concern?
	if (stplyr != &players[displayplayers[0]])
		return;

	lumpnum = W_CheckNumForName(va("%sR", G_BuildMapName(gamemap)));

	if (lumpnum != -1)
		AutomapPic = W_CachePatchName(va("%sR", G_BuildMapName(gamemap)), PU_HUDGFX);
	else
		return; // no pic, just get outta here

	x = MINI_X - (AutomapPic->width/2);
	y = MINI_Y - (AutomapPic->height/2);

	if (timeinmap > 105)
	{
		minimaptrans = cv_kartminimap.value;
		if (timeinmap <= 113)
			minimaptrans = ((((INT32)timeinmap) - 105)*minimaptrans)/(113-105);
		if (!minimaptrans)
			return;
	}
	else
		return;

	minimaptrans = ((10-minimaptrans)<<FF_TRANSSHIFT);
	splitflags |= minimaptrans;

	if (encoremode)
		V_DrawScaledPatch(x+(AutomapPic->width), y, splitflags|V_FLIP, AutomapPic);
	else
		V_DrawScaledPatch(x, y, splitflags, AutomapPic);

	if (!(r_splitscreen == 2))
	{
		splitflags &= ~minimaptrans;
		splitflags |= V_HUDTRANSHALF;
	}

	// let offsets transfer to the heads, too!
	if (encoremode)
		x += SHORT(AutomapPic->leftoffset);
	else
		x -= SHORT(AutomapPic->leftoffset);
	y -= SHORT(AutomapPic->topoffset);

	// Draw the super item in Battle
	if (G_BattleGametype() && battleovertime.enabled)
	{
		if (battleovertime.enabled >= 10*TICRATE || (battleovertime.enabled & 1))
		{
			const INT32 prevsplitflags = splitflags;
			splitflags &= ~V_HUDTRANSHALF;
			splitflags |= V_HUDTRANS;
			colormap = R_GetTranslationColormap(TC_RAINBOW, (UINT8)(1 + (leveltime % (MAXSKINCOLORS-1))), GTC_CACHE);
			K_drawKartMinimapIcon(battleovertime.x, battleovertime.y, x, y, splitflags, kp_itemminimap, colormap, AutomapPic);
			splitflags = prevsplitflags;
		}
	}

	// initialize
	for (i = 0; i < 4; i++)
		localplayers[i] = -1;

	// Player's tiny icons on the Automap. (drawn opposite direction so player 1 is drawn last in splitscreen)
	if (ghosts)
	{
		demoghost *g = ghosts;
		while (g)
		{
			if (g->mo->skin)
				skin = ((skin_t*)g->mo->skin)-skins;
			else
				skin = 0;
			if (g->mo->color)
			{
				if (g->mo->colorized)
					colormap = R_GetTranslationColormap(TC_RAINBOW, g->mo->color, GTC_CACHE);
				else
					colormap = R_GetTranslationColormap(skin, g->mo->color, GTC_CACHE);
			}
			else
				colormap = NULL;
			K_drawKartMinimapIcon(g->mo->x, g->mo->y, x, y, splitflags, facemmapprefix[skin], colormap, AutomapPic);
			g = g->next;
		}

		if (!stplyr->mo || stplyr->spectator || stplyr->exiting)
			return;

		localplayers[numlocalplayers] = stplyr-players;
		numlocalplayers++;
	}
	else
	{
		for (i = MAXPLAYERS-1; i >= 0; i--)
		{
			if (!playeringame[i])
				continue;
			if (!players[i].mo || players[i].spectator || players[i].exiting)
				continue;

			if (i != displayplayers[0] || r_splitscreen)
			{
				if (G_BattleGametype() && players[i].kartstuff[k_bumper] <= 0)
					continue;

				if (players[i].kartstuff[k_hyudorotimer] > 0)
				{
					if (!((players[i].kartstuff[k_hyudorotimer] < 1*TICRATE/2
						|| players[i].kartstuff[k_hyudorotimer] > hyudorotime-(1*TICRATE/2))
						&& !(leveltime & 1)))
						continue;
				}
			}

			if (i == displayplayers[0] || i == displayplayers[1] || i == displayplayers[2] || i == displayplayers[3])
			{
				// Draw display players on top of everything else
				localplayers[numlocalplayers] = i;
				numlocalplayers++;
				continue;
			}

			if (players[i].mo->skin)
				skin = ((skin_t*)players[i].mo->skin)-skins;
			else
				skin = 0;

			if (players[i].mo->color)
			{
				if (players[i].mo->colorized)
					colormap = R_GetTranslationColormap(TC_RAINBOW, players[i].mo->color, GTC_CACHE);
				else
					colormap = R_GetTranslationColormap(skin, players[i].mo->color, GTC_CACHE);
			}
			else
				colormap = NULL;

			K_drawKartMinimapIcon(players[i].mo->x, players[i].mo->y, x, y, splitflags, facemmapprefix[skin], colormap, AutomapPic);
			// Target reticule
			if ((G_RaceGametype() && players[i].kartstuff[k_position] == spbplace)
			|| (G_BattleGametype() && K_IsPlayerWanted(&players[i])))
				K_drawKartMinimapIcon(players[i].mo->x, players[i].mo->y, x, y, splitflags, kp_wantedreticle, NULL, AutomapPic);
		}
	}

	// draw SPB(s?)
	for (mobj = kitemcap; mobj; mobj = next)
	{
		next = mobj->itnext;
		if (mobj->type == MT_SPB)
		{
			colormap = NULL;

			if (mobj->target && !P_MobjWasRemoved(mobj->target))
			{
				if (mobj->player && mobj->player->skincolor)
					colormap = R_GetTranslationColormap(TC_RAINBOW, mobj->player->skincolor, GTC_CACHE);
				else if (mobj->color)
					colormap = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
			}

			K_drawKartMinimapIcon(mobj->x, mobj->y, x, y, splitflags, kp_spbminimap, colormap, AutomapPic);
		}
	}

	// draw our local players here, opaque.
	splitflags &= ~V_HUDTRANSHALF;
	splitflags |= V_HUDTRANS;

	for (i = 0; i < numlocalplayers; i++)
	{
		if (i == -1)
			continue; // this doesn't interest us

		if (players[localplayers[i]].mo->skin)
			skin = ((skin_t*)players[localplayers[i]].mo->skin)-skins;
		else
			skin = 0;

		if (players[localplayers[i]].mo->color)
		{
			if (players[localplayers[i]].mo->colorized)
				colormap = R_GetTranslationColormap(TC_RAINBOW, players[localplayers[i]].mo->color, GTC_CACHE);
			else
				colormap = R_GetTranslationColormap(skin, players[localplayers[i]].mo->color, GTC_CACHE);
		}
		else
			colormap = NULL;

		K_drawKartMinimapIcon(players[localplayers[i]].mo->x, players[localplayers[i]].mo->y, x, y, splitflags, facemmapprefix[skin], colormap, AutomapPic);

		// Target reticule
		if ((G_RaceGametype() && players[localplayers[i]].kartstuff[k_position] == spbplace)
		|| (G_BattleGametype() && K_IsPlayerWanted(&players[localplayers[i]])))
			K_drawKartMinimapIcon(players[localplayers[i]].mo->x, players[localplayers[i]].mo->y, x, y, splitflags, kp_wantedreticle, NULL, AutomapPic);
	}
}

static void K_drawKartStartCountdown(void)
{
	INT32 pnum = 0, splitflags = K_calcSplitFlags(0); // 3

	if (leveltime >= starttime-(2*TICRATE)) // 2
		pnum++;
	if (leveltime >= starttime-TICRATE) // 1
		pnum++;
	if (leveltime >= starttime) // GO!
		pnum++;
	if ((leveltime % (2*5)) / 5) // blink
		pnum += 4;
	if (r_splitscreen) // splitscreen
		pnum += 8;

	V_DrawScaledPatch(STCD_X - (SHORT(kp_startcountdown[pnum]->width)/2), STCD_Y - (SHORT(kp_startcountdown[pnum]->height)/2), splitflags, kp_startcountdown[pnum]);
}

static void K_drawKartFinish(void)
{
	INT32 pnum = 0, splitflags = K_calcSplitFlags(0);

	if (!stplyr->karthud[khud_cardanimation] || stplyr->karthud[khud_cardanimation] >= 2*TICRATE)
		return;

	if ((stplyr->karthud[khud_cardanimation] % (2*5)) / 5) // blink
		pnum = 1;

	if (r_splitscreen > 1) // 3/4p, stationary FIN
	{
		pnum += 2;
		V_DrawScaledPatch(STCD_X - (SHORT(kp_racefinish[pnum]->width)/2), STCD_Y - (SHORT(kp_racefinish[pnum]->height)/2), splitflags, kp_racefinish[pnum]);
		return;
	}

	//else -- 1/2p, scrolling FINISH
	{
		INT32 x, xval;

		if (r_splitscreen) // wide splitscreen
			pnum += 4;

		x = ((vid.width<<FRACBITS)/vid.dupx);
		xval = (SHORT(kp_racefinish[pnum]->width)<<FRACBITS);
		x = ((TICRATE - stplyr->karthud[khud_cardanimation])*(xval > x ? xval : x))/TICRATE;

		if (r_splitscreen && stplyr == &players[displayplayers[1]])
			x = -x;

		V_DrawFixedPatch(x + (STCD_X<<FRACBITS) - (xval>>1),
			(STCD_Y<<FRACBITS) - (SHORT(kp_racefinish[pnum]->height)<<(FRACBITS-1)),
			FRACUNIT,
			splitflags, kp_racefinish[pnum], NULL);
	}
}

static void K_drawBattleFullscreen(void)
{
	INT32 x = BASEVIDWIDTH/2;
	INT32 y = -64+(stplyr->karthud[khud_cardanimation]); // card animation goes from 0 to 164, 164 is the middle of the screen
	INT32 splitflags = V_SNAPTOTOP; // I don't feel like properly supporting non-green resolutions, so you can have a misuse of SNAPTO instead
	fixed_t scale = FRACUNIT;
	boolean drawcomebacktimer = true;	// lazy hack because it's cleaner in the long run.
#ifdef HAVE_BLUA
	if (!LUA_HudEnabled(hud_battlecomebacktimer))
		drawcomebacktimer = false;
#endif

	if (r_splitscreen)
	{
		if ((r_splitscreen == 1 && stplyr == &players[displayplayers[1]])
			|| (r_splitscreen > 1 && (stplyr == &players[displayplayers[2]]
			|| (stplyr == &players[displayplayers[3]] && r_splitscreen > 2))))
		{
			y = 232-(stplyr->karthud[khud_cardanimation]/2);
			splitflags = V_SNAPTOBOTTOM;
		}
		else
			y = -32+(stplyr->karthud[khud_cardanimation]/2);

		if (r_splitscreen > 1)
		{
			scale /= 2;

			if (stplyr == &players[displayplayers[1]]
				|| (stplyr == &players[displayplayers[3]] && r_splitscreen > 2))
				x = 3*BASEVIDWIDTH/4;
			else
				x = BASEVIDWIDTH/4;
		}
		else
		{
			if (stplyr->exiting)
			{
				if (stplyr == &players[displayplayers[1]])
					x = BASEVIDWIDTH-96;
				else
					x = 96;
			}
			else
				scale /= 2;
		}
	}

	if (stplyr->exiting)
	{
		if (stplyr == &players[displayplayers[0]])
			V_DrawFadeScreen(0xFF00, 16);
		if (stplyr->exiting < 6*TICRATE && !stplyr->spectator)
		{
			patch_t *p = kp_battlecool;

			if (K_IsPlayerLosing(stplyr))
				p = kp_battlelose;
			else if (stplyr->kartstuff[k_position] == 1)
				p = kp_battlewin;

			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, scale, splitflags, p, NULL);
		}
		else
			K_drawKartFinish();
	}
	else if (stplyr->kartstuff[k_bumper] <= 0 && stplyr->kartstuff[k_comebacktimer] && comeback && !stplyr->spectator && drawcomebacktimer)
	{
		UINT16 t = stplyr->kartstuff[k_comebacktimer]/(10*TICRATE);
		INT32 txoff, adjust = (r_splitscreen > 1) ? 4 : 6; // normal string is 8, kart string is 12, half of that for ease
		INT32 ty = (BASEVIDHEIGHT/2)+66;

		txoff = adjust;

		while (t)
		{
			txoff += adjust;
			t /= 10;
		}

		if (r_splitscreen)
		{
			if (r_splitscreen > 1)
				ty = (BASEVIDHEIGHT/4)+33;
			if ((r_splitscreen == 1 && stplyr == &players[displayplayers[1]])
				|| (stplyr == &players[displayplayers[2]] && r_splitscreen > 1)
				|| (stplyr == &players[displayplayers[3]] && r_splitscreen > 2))
				ty += (BASEVIDHEIGHT/2);
		}
		else
			V_DrawFadeScreen(0xFF00, 16);

		if (!comebackshowninfo)
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, scale, splitflags, kp_battleinfo, NULL);
		else
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, scale, splitflags, kp_battlewait, NULL);

		if (r_splitscreen > 1)
			V_DrawString(x-txoff, ty, 0, va("%d", stplyr->kartstuff[k_comebacktimer]/TICRATE));
		else
		{
			V_DrawFixedPatch(x<<FRACBITS, ty<<FRACBITS, scale, 0, kp_timeoutsticker, NULL);
			V_DrawKartString(x-txoff, ty, 0, va("%d", stplyr->kartstuff[k_comebacktimer]/TICRATE));
		}
	}

	if (netgame && !stplyr->spectator && timeinmap > 113) // FREE PLAY?
	{
		UINT8 i;

		// check to see if there's anyone else at all
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (i == displayplayers[0])
				continue;
			if (playeringame[i] && !stplyr->spectator)
				return;
		}

#ifdef HAVE_BLUA
		if (LUA_HudEnabled(hud_freeplay))
#endif
			K_drawKartFreePlay(leveltime);
	}
}

static void K_drawKartFirstPerson(void)
{
	static INT32 pnum[4], turn[4], drift[4];
	INT32 pn = 0, tn = 0, dr = 0;
	INT32 target = 0, splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM);
	INT32 x = BASEVIDWIDTH/2, y = BASEVIDHEIGHT;
	fixed_t scale;
	UINT8 *colmap = NULL;
	ticcmd_t *cmd = &stplyr->cmd;

	if (stplyr->spectator || !stplyr->mo || (stplyr->mo->flags2 & MF2_DONTDRAW))
		return;

	if (stplyr == &players[displayplayers[1]] && r_splitscreen)
		{ pn = pnum[1]; tn = turn[1]; dr = drift[1]; }
	else if (stplyr == &players[displayplayers[2]] && r_splitscreen > 1)
		{ pn = pnum[2]; tn = turn[2]; dr = drift[2]; }
	else if (stplyr == &players[displayplayers[3]] && r_splitscreen > 2)
		{ pn = pnum[3]; tn = turn[3]; dr = drift[3]; }
	else
		{ pn = pnum[0]; tn = turn[0]; dr = drift[0]; }

	if (r_splitscreen)
	{
		y >>= 1;
		if (r_splitscreen > 1)
			x >>= 1;
	}

	{
		if (stplyr->speed < (20*stplyr->mo->scale) && (leveltime & 1) && !r_splitscreen)
			y++;
		// the following isn't EXPLICITLY right, it just gets the result we want, but i'm too lazy to look up the right way to do it
		if (stplyr->mo->flags2 & MF2_SHADOW)
			splitflags |= FF_TRANS80;
		else if (stplyr->mo->frame & FF_TRANSMASK)
			splitflags |= (stplyr->mo->frame & FF_TRANSMASK);
	}

	if (cmd->driftturn > 400) // strong left turn
		target = 2;
	else if (cmd->driftturn < -400) // strong right turn
		target = -2;
	else if (cmd->driftturn > 0) // weak left turn
		target = 1;
	else if (cmd->driftturn < 0) // weak right turn
		target = -1;
	else // forward
		target = 0;

	if (encoremode)
		target = -target;

	if (pn < target)
		pn++;
	else if (pn > target)
		pn--;

	if (pn < 0)
		splitflags |= V_FLIP; // right turn

	target = abs(pn);
	if (target > 2)
		target = 2;

	x <<= FRACBITS;
	y <<= FRACBITS;

	if (tn != cmd->driftturn/50)
		tn -= (tn - (cmd->driftturn/50))/8;

	if (dr != stplyr->kartstuff[k_drift]*16)
		dr -= (dr - (stplyr->kartstuff[k_drift]*16))/8;

	if (r_splitscreen == 1)
	{
		scale = (2*FRACUNIT)/3;
		y += FRACUNIT/(vid.dupx < vid.dupy ? vid.dupx : vid.dupy); // correct a one-pixel gap on the screen view (not the basevid view)
	}
	else if (r_splitscreen)
		scale = FRACUNIT/2;
	else
		scale = FRACUNIT;

	if (stplyr->mo)
	{
		UINT8 driftcolor = K_DriftSparkColor(stplyr, stplyr->kartstuff[k_driftcharge]);
		const angle_t ang = R_PointToAngle2(0, 0, stplyr->rmomx, stplyr->rmomy) - stplyr->frameangle;
		// yes, the following is correct. no, you do not need to swap the x and y.
		fixed_t xoffs = -P_ReturnThrustY(stplyr->mo, ang, (BASEVIDWIDTH<<(FRACBITS-2))/2);
		fixed_t yoffs = -(P_ReturnThrustX(stplyr->mo, ang, 4*FRACUNIT) - 4*FRACUNIT);

		if (r_splitscreen)
			xoffs = FixedMul(xoffs, scale);

		xoffs -= (tn)*scale;
		xoffs -= (dr)*scale;

		if (stplyr->frameangle == stplyr->mo->angle)
		{
			const fixed_t mag = FixedDiv(stplyr->speed, 10*stplyr->mo->scale);

			if (mag < FRACUNIT)
			{
				xoffs = FixedMul(xoffs, mag);
				if (!r_splitscreen)
					yoffs = FixedMul(yoffs, mag);
			}
		}

		if (stplyr->mo->momz > 0) // TO-DO: Draw more of the kart so we can remove this if!
			yoffs += stplyr->mo->momz/3;

		if (encoremode)
			x -= xoffs;
		else
			x += xoffs;
		if (!r_splitscreen)
			y += yoffs;


		if ((leveltime & 1) && (driftcolor != SKINCOLOR_NONE)) // drift sparks!
			colmap = R_GetTranslationColormap(TC_RAINBOW, driftcolor, GTC_CACHE);
		else if (stplyr->mo->colorized && stplyr->mo->color) // invincibility/grow/shrink!
			colmap = R_GetTranslationColormap(TC_RAINBOW, stplyr->mo->color, GTC_CACHE);
	}

	V_DrawFixedPatch(x, y, scale, splitflags, kp_fpview[target], colmap);

	if (stplyr == &players[displayplayers[1]] && r_splitscreen)
		{ pnum[1] = pn; turn[1] = tn; drift[1] = dr; }
	else if (stplyr == &players[displayplayers[2]] && r_splitscreen > 1)
		{ pnum[2] = pn; turn[2] = tn; drift[2] = dr; }
	else if (stplyr == &players[displayplayers[3]] && r_splitscreen > 2)
		{ pnum[3] = pn; turn[3] = tn; drift[3] = dr; }
	else
		{ pnum[0] = pn; turn[0] = tn; drift[0] = dr; }
}

// doesn't need to ever support 4p
static void K_drawInput(void)
{
	static INT32 pn = 0;
	INT32 target = 0, splitflags = (V_SNAPTOBOTTOM|V_SNAPTORIGHT);
	INT32 x = BASEVIDWIDTH - 32, y = BASEVIDHEIGHT-24, offs, col;
	const INT32 accent1 = splitflags|colortranslations[stplyr->skincolor][5];
	const INT32 accent2 = splitflags|colortranslations[stplyr->skincolor][9];
	ticcmd_t *cmd = &stplyr->cmd;

	if (timeinmap <= 105)
		return;

	if (timeinmap < 113)
	{
		INT32 count = ((INT32)(timeinmap) - 105);
		offs = 64;
		while (count-- > 0)
			offs >>= 1;
		x += offs;
	}

#define BUTTW 8
#define BUTTH 11

#define drawbutt(xoffs, butt, symb)\
	if (stplyr->cmd.buttons & butt)\
	{\
		offs = 2;\
		col = accent1;\
	}\
	else\
	{\
		offs = 0;\
		col = accent2;\
		V_DrawFill(x+(xoffs), y+BUTTH, BUTTW-1, 2, splitflags|31);\
	}\
	V_DrawFill(x+(xoffs), y+offs, BUTTW-1, BUTTH, col);\
	V_DrawFixedPatch((x+1+(xoffs))<<FRACBITS, (y+offs+1)<<FRACBITS, FRACUNIT, splitflags, tny_font[symb-HU_FONTSTART], NULL)

	drawbutt(-2*BUTTW, BT_ACCELERATE, 'A');
	drawbutt(  -BUTTW, BT_BRAKE,      'B');
	drawbutt(       0, BT_DRIFT,      'D');
	drawbutt(   BUTTW, BT_ATTACK,     'I');

#undef drawbutt

#undef BUTTW
#undef BUTTH

	y -= 1;

	if (!cmd->driftturn) // no turn
		target = 0;
	else // turning of multiple strengths!
	{
		target = ((abs(cmd->driftturn) - 1)/125)+1;
		if (target > 4)
			target = 4;
		if (cmd->driftturn < 0)
			target = -target;
	}

	if (pn != target)
	{
		if (abs(pn - target) == 1)
			pn = target;
		else if (pn < target)
			pn += 2;
		else //if (pn > target)
			pn -= 2;
	}

	if (pn < 0)
	{
		splitflags |= V_FLIP; // right turn
		x--;
	}

	target = abs(pn);
	if (target > 4)
		target = 4;

	if (!stplyr->skincolor)
		V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, splitflags, kp_inputwheel[target], NULL);
	else
	{
		UINT8 *colormap;
		colormap = R_GetTranslationColormap(0, stplyr->skincolor, GTC_CACHE);
		V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, splitflags, kp_inputwheel[target], colormap);
	}
}

static void K_drawChallengerScreen(void)
{
	// This is an insanely complicated animation.
	static UINT8 anim[52] = {
		0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13, // frame 1-14, 2 tics: HERE COMES A NEW slides in
		14,14,14,14,14,14, // frame 15, 6 tics: pause on the W
		15,16,17,18, // frame 16-19, 1 tic: CHALLENGER approaches screen
		19,20,19,20,19,20,19,20,19,20, // frame 20-21, 1 tic, 5 alternating: all text vibrates from impact
		21,22,23,24 // frame 22-25, 1 tic: CHALLENGER turns gold
	};
	const UINT8 offset = min(52-1, (3*TICRATE)-mapreset);

	V_DrawFadeScreen(0xFF00, 16); // Fade out
	V_DrawScaledPatch(0, 0, 0, kp_challenger[anim[offset]]);
}

static void K_drawLapStartAnim(void)
{
	// This is an EVEN MORE insanely complicated animation.
	const UINT8 progress = 80-stplyr->karthud[khud_lapanimation];
	UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, stplyr->skincolor, GTC_CACHE);

	V_DrawFixedPatch((BASEVIDWIDTH/2 + (32*max(0, stplyr->karthud[khud_lapanimation]-76)))*FRACUNIT,
		(48 - (32*max(0, progress-76)))*FRACUNIT,
		FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
		(modeattacking ? kp_lapanim_emblem[1] : kp_lapanim_emblem[0]), colormap);

	if (stplyr->karthud[khud_laphand] >= 1 && stplyr->karthud[khud_laphand] <= 3)
	{
		V_DrawFixedPatch((BASEVIDWIDTH/2 + (32*max(0, stplyr->karthud[khud_lapanimation]-76)))*FRACUNIT,
			(48 - (32*max(0, progress-76))
				+ 4 - abs((signed)((leveltime % 8) - 4)))*FRACUNIT,
			FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
			kp_lapanim_hand[stplyr->karthud[khud_laphand]-1], NULL);
	}

	if (stplyr->laps == (UINT8)(cv_numlaps.value))
	{
		V_DrawFixedPatch((62 - (32*max(0, progress-76)))*FRACUNIT, // 27
			30*FRACUNIT, // 24
			FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
			kp_lapanim_final[min(progress/2, 10)], NULL);

		if (progress/2-12 >= 0)
		{
			V_DrawFixedPatch((188 + (32*max(0, progress-76)))*FRACUNIT, // 194
				30*FRACUNIT, // 24
				FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
				kp_lapanim_lap[min(progress/2-12, 6)], NULL);
		}
	}
	else
	{
		V_DrawFixedPatch((82 - (32*max(0, progress-76)))*FRACUNIT, // 61
			30*FRACUNIT, // 24
			FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
			kp_lapanim_lap[min(progress/2, 6)], NULL);

		if (progress/2-8 >= 0)
		{
			V_DrawFixedPatch((188 + (32*max(0, progress-76)))*FRACUNIT, // 194
				30*FRACUNIT, // 24
				FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
				kp_lapanim_number[(((UINT32)stplyr->laps) / 10)][min(progress/2-8, 2)], NULL);

			if (progress/2-10 >= 0)
			{
				V_DrawFixedPatch((208 + (32*max(0, progress-76)))*FRACUNIT, // 221
					30*FRACUNIT, // 24
					FRACUNIT, V_SNAPTOTOP|V_HUDTRANS,
					kp_lapanim_number[(((UINT32)stplyr->laps) % 10)][min(progress/2-10, 2)], NULL);
			}
		}
	}
}

void K_drawKartFreePlay(UINT32 flashtime)
{
	// no splitscreen support because it's not FREE PLAY if you have more than one player in-game
	// (you fool, you can take splitscreen online. :V)

	if ((flashtime % TICRATE) < TICRATE/2)
		return;

	V_DrawKartString((BASEVIDWIDTH - (LAPS_X+1)) - (12*9), // mirror the laps thingy
		LAPS_Y+3, V_HUDTRANS|V_SNAPTOBOTTOM|V_SNAPTORIGHT, "FREE PLAY");
}

static void
Draw_party_ping (int ss, INT32 snap)
{
	HU_drawMiniPing(0, 0, playerpingtable[displayplayers[ss]], V_HUDTRANS|snap);
}

static void
K_drawMiniPing (void)
{
	if (cv_showping.value)
	{
		switch (r_splitscreen)
		{
			case 3:
				Draw_party_ping(3, V_SNAPTORIGHT|V_SPLITSCREEN);
				/*FALLTHRU*/
			case 2:
				Draw_party_ping(2, V_SPLITSCREEN);
				Draw_party_ping(1, V_SNAPTORIGHT);
				Draw_party_ping(0, 0);
				break;
			case 1:
				Draw_party_ping(1, V_SNAPTORIGHT|V_SPLITSCREEN);
				Draw_party_ping(0, V_SNAPTORIGHT);
				break;
		}
	}
}

static void K_drawDistributionDebugger(void)
{
	patch_t *items[NUMKARTRESULTS] = {
		kp_sadface[1],
		kp_sneaker[1],
		kp_rocketsneaker[1],
		kp_invincibility[7],
		kp_banana[1],
		kp_eggman[1],
		kp_orbinaut[4],
		kp_jawz[1],
		kp_mine[1],
		kp_ballhog[1],
		kp_selfpropelledbomb[1],
		kp_grow[1],
		kp_shrink[1],
		kp_thundershield[1],
		kp_bubbleshield[1],
		kp_flameshield[1],
		kp_hyudoro[1],
		kp_pogospring[1],
		kp_superring[1],
		kp_kitchensink[1],

		kp_sneaker[1],
		kp_banana[1],
		kp_banana[1],
		kp_orbinaut[4],
		kp_orbinaut[4],
		kp_jawz[1]
	};
	UINT8 useodds = 0;
	UINT8 pingame = 0, bestbumper = 0;
	UINT32 pdis = 0;
	INT32 i;
	INT32 x = -9, y = -9;
	boolean spbrush = false;

	if (stplyr != &players[displayplayers[0]]) // only for p1
		return;

	// The only code duplication from the Kart, just to avoid the actual item function from calculating pingame twice
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		pingame++;
		if (players[i].kartstuff[k_bumper] > bestbumper)
			bestbumper = players[i].kartstuff[k_bumper];
	}

	// lovely double loop......
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator
			&& players[i].kartstuff[k_position] == 1)
		{
			// This player is first! Yay!
			pdis = stplyr->distancetofinish - players[i].distancetofinish;
			break;
		}
	}

	if (franticitems) // Frantic items make the distances between everyone artifically higher, for crazier items
		pdis = (15 * pdis) / 14;

	if (spbplace != -1 && stplyr->kartstuff[k_position] == spbplace+1) // SPB Rush Mode: It's 2nd place's job to catch-up items and make 1st place's job hell
	{
		pdis = (3 * pdis) / 2;
		spbrush = true;
	}

	pdis = ((28 + (8-pingame)) * pdis) / 28; // scale with player count

	useodds = K_FindUseodds(stplyr, 0, pdis, bestbumper, spbrush);

	for (i = 1; i < NUMKARTRESULTS; i++)
	{
		const INT32 itemodds = K_KartGetItemOdds(useodds, i, 0, spbrush, stplyr->bot);
		if (itemodds <= 0)
			continue;

		V_DrawScaledPatch(x, y, V_HUDTRANS|V_SNAPTOTOP, items[i]);
		V_DrawThinString(x+11, y+31, V_HUDTRANS|V_SNAPTOTOP, va("%d", itemodds));

		// Display amount for multi-items
		if (i >= NUMKARTITEMS)
		{
			INT32 amount;
			switch (i)
			{
				case KRITEM_TENFOLDBANANA:
					amount = 10;
					break;
				case KRITEM_QUADORBINAUT:
					amount = 4;
					break;
				case KRITEM_DUALJAWZ:
					amount = 2;
					break;
				default:
					amount = 3;
					break;
			}
			V_DrawString(x+24, y+31, V_ALLOWLOWERCASE|V_HUDTRANS|V_SNAPTOTOP, va("x%d", amount));
		}

		x += 32;
		if (x >= 297)
		{
			x = -9;
			y += 32;
		}
	}

	V_DrawString(0, 0, V_HUDTRANS|V_SNAPTOTOP, va("USEODDS %d", useodds));
}

static void K_drawCheckpointDebugger(void)
{
	if (stplyr != &players[displayplayers[0]]) // only for p1
		return;

	if (stplyr->starpostnum == numstarposts)
		V_DrawString(8, 184, 0, va("Checkpoint: %d / %d (Can finish)", stplyr->starpostnum, numstarposts));
	else
		V_DrawString(8, 184, 0, va("Checkpoint: %d / %d", stplyr->starpostnum, numstarposts));
}

static void K_DrawWaypointDebugger(void)
{
	if ((cv_kartdebugwaypoints.value != 0) && (stplyr == &players[displayplayers[0]]))
	{
		V_DrawString(8, 166, 0, va("'Best' Waypoint ID: %d", K_GetWaypointID(stplyr->nextwaypoint)));
		V_DrawString(8, 176, 0, va("Finishline Distance: %d", stplyr->distancetofinish));
	}
}

void K_drawKartHUD(void)
{
	boolean isfreeplay = false;
	boolean battlefullscreen = false;
	boolean freecam = demo.freecam;	//disable some hud elements w/ freecam
	UINT8 i;

	// Define the X and Y for each drawn object
	// This is handled by console/menu values
	K_initKartHUD();

	// Draw that fun first person HUD! Drawn ASAP so it looks more "real".
	for (i = 0; i <= r_splitscreen; i++)
	{
		if (stplyr == &players[displayplayers[i]] && !camera[i].chase && !freecam)
			K_drawKartFirstPerson();
	}

	// Draw full screen stuff that turns off the rest of the HUD
	if (mapreset && stplyr == &players[displayplayers[0]])
	{
		K_drawChallengerScreen();
		return;
	}

	battlefullscreen = ((G_BattleGametype())
		&& (stplyr->exiting
		|| (stplyr->kartstuff[k_bumper] <= 0
		&& stplyr->kartstuff[k_comebacktimer]
		&& comeback
		&& stplyr->playerstate == PST_LIVE)));

	if (!demo.title && (!battlefullscreen || r_splitscreen))
	{
		// Draw the CHECK indicator before the other items, so it's overlapped by everything else
#ifdef HAVE_BLUA
		if (LUA_HudEnabled(hud_check))	// delete lua when?
#endif
			if (cv_kartcheck.value && !splitscreen && !players[displayplayers[0]].exiting && !freecam)
				K_drawKartPlayerCheck();

		// Draw WANTED status
		if (G_BattleGametype())
		{
#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_wanted))
#endif
				K_drawKartWanted();
		}

		if (cv_kartminimap.value)
		{
#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_minimap))
#endif
				K_drawKartMinimap();
		}
	}

	if (battlefullscreen && !freecam)
	{
#ifdef HAVE_BLUA
		if (LUA_HudEnabled(hud_battlefullscreen))
#endif
			K_drawBattleFullscreen();
		return;
	}

	// Draw the item window
#ifdef HAVE_BLUA
	if (LUA_HudEnabled(hud_item) && !freecam)
#endif
		K_drawKartItem();

	// If not splitscreen, draw...
	if (!r_splitscreen && !demo.title)
	{
		// Draw the timestamp
#ifdef HAVE_BLUA
		if (LUA_HudEnabled(hud_time))
#endif
			K_drawKartTimestamp(stplyr->realtime, TIME_X, TIME_Y, gamemap, 0);

		if (!modeattacking)
		{
			// The top-four faces on the left
			/*#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_minirankings))
			#endif*/
				isfreeplay = K_drawKartPositionFaces();
		}
	}

	if (!stplyr->spectator && !demo.freecam) // Bottom of the screen elements, don't need in spectate mode
	{
		// Draw the speedometer
		if (cv_kartspeedometer.value && !r_splitscreen)
		{
#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_speedometer))
#endif
				K_drawKartSpeedometer();
		}

		if (demo.title) // Draw title logo instead in demo.titles
		{
			INT32 x = BASEVIDWIDTH - 32, y = 128, offs;

			if (r_splitscreen == 3)
			{
				x = BASEVIDWIDTH/2 + 10;
				y = BASEVIDHEIGHT/2 - 30;
			}

			if (timeinmap < 113)
			{
				INT32 count = ((INT32)(timeinmap) - 104);
				offs = 256;
				while (count-- > 0)
					offs >>= 1;
				x += offs;
			}

			V_DrawTinyScaledPatch(x-54, y, 0, W_CachePatchName("TTKBANNR", PU_CACHE));
			V_DrawTinyScaledPatch(x-54, y+25, 0, W_CachePatchName("TTKART", PU_CACHE));
		}
		else if (G_RaceGametype()) // Race-only elements
		{
			// Draw the lap counter
#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_gametypeinfo))
#endif
				K_drawKartLapsAndRings();

			if (isfreeplay)
				;
			else if (!modeattacking)
			{
				// Draw the numerical position
#ifdef HAVE_BLUA
				if (LUA_HudEnabled(hud_position))
#endif
					K_DrawKartPositionNum(stplyr->kartstuff[k_position]);
			}
			else //if (!(demo.playback && hu_showscores))
			{
				// Draw the input UI
#ifdef HAVE_BLUA
				if (LUA_HudEnabled(hud_position))
#endif
					K_drawInput();
			}
		}
		else if (G_BattleGametype()) // Battle-only
		{
			// Draw the hits left!
#ifdef HAVE_BLUA
			if (LUA_HudEnabled(hud_gametypeinfo))
#endif
				K_drawKartBumpersOrKarma();
		}
	}

	// Draw the countdowns after everything else.
	if (leveltime >= starttime-(3*TICRATE)
		&& leveltime < starttime+TICRATE)
		K_drawKartStartCountdown();
	else if (racecountdown && (!r_splitscreen || !stplyr->exiting))
	{
		char *countstr = va("%d", racecountdown/TICRATE);

		if (r_splitscreen > 1)
			V_DrawCenteredString(BASEVIDWIDTH/4, LAPS_Y+1, K_calcSplitFlags(0), countstr);
		else
		{
			INT32 karlen = strlen(countstr)*6; // half of 12
			V_DrawKartString((BASEVIDWIDTH/2)-karlen, LAPS_Y+3, K_calcSplitFlags(0), countstr);
		}
	}

	// Race overlays
	if (G_RaceGametype() && !freecam)
	{
		if (stplyr->exiting)
			K_drawKartFinish();
		else if (stplyr->karthud[khud_lapanimation] && !r_splitscreen)
			K_drawLapStartAnim();
	}

	if (modeattacking || freecam) // everything after here is MP and debug only
		return;

	if (G_BattleGametype() && !r_splitscreen && (stplyr->karthud[khud_yougotem] % 2)) // * YOU GOT EM *
		V_DrawScaledPatch(BASEVIDWIDTH/2 - (SHORT(kp_yougotem->width)/2), 32, V_HUDTRANS, kp_yougotem);

	// Draw FREE PLAY.
	if (isfreeplay && !stplyr->spectator && timeinmap > 113)
	{
#ifdef HAVE_BLUA
		if (LUA_HudEnabled(hud_freeplay))
#endif
			K_drawKartFreePlay(leveltime);
	}

	if (r_splitscreen == 0 && stplyr->kartstuff[k_wrongway] && ((leveltime / 8) & 1))
	{
		V_DrawCenteredString(BASEVIDWIDTH>>1, 176, V_REDMAP|V_SNAPTOBOTTOM, "WRONG WAY");
	}

	if (netgame && r_splitscreen)
	{
		K_drawMiniPing();
	}

	if (cv_kartdebugdistribution.value)
		K_drawDistributionDebugger();

	if (cv_kartdebugcheckpoint.value)
		K_drawCheckpointDebugger();

	if (cv_kartdebugnodes.value)
	{
		UINT8 p;
		for (p = 0; p < MAXPLAYERS; p++)
			V_DrawString(8, 64+(8*p), V_YELLOWMAP, va("%d - %d (%dl)", p, playernode[p], players[p].cmd.latency));
	}

	if (cv_kartdebugcolorize.value && stplyr->mo && stplyr->mo->skin)
	{
		INT32 x = 0, y = 0;
		UINT8 c;

		for (c = 1; c < MAXSKINCOLORS; c++)
		{
			UINT8 *cm = R_GetTranslationColormap(TC_RAINBOW, c, GTC_CACHE);
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT>>1, 0, facewantprefix[stplyr->skin], cm);

			x += 16;
			if (x > BASEVIDWIDTH-16)
			{
				x = 0;
				y += 16;
			}
		}
	}

	K_DrawWaypointDebugger();
}

//}
