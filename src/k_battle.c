/// \file  k_battle.c
/// \brief SRB2Kart Battle Mode specific code

#include "k_battle.h"
#include "k_kart.h"
#include "doomtype.h"
#include "doomdata.h"
#include "g_game.h"
#include "p_mobj.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_slopes.h" // P_GetZAt
#include "r_main.h"
#include "r_defs.h" // MAXFFLOORS
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "r_sky.h" // skyflatnum
#include "k_grandprix.h" // K_CanChangeRules
#include "k_boss.h" // bossinfo.valid
#include "p_spec.h"
#include "k_objects.h"
#include "k_rank.h"

// Battle overtime info
struct battleovertime battleovertime;
struct battleufo g_battleufo;

// Capsules mode enabled for this map?
boolean battleprisons = false;

// box respawning in battle mode
INT32 nummapboxes = 0;
INT32 numgotboxes = 0;

// Capsule counters
UINT8 maptargets = 0; // Capsules in map
UINT8 numtargets = 0; // Capsules busted

INT32 K_StartingBumperCount(void)
{
	if (battleprisons)
		return 0; // always 1 hit in Prison Break

	return cv_kartbumpers.value;
}

boolean K_IsPlayerWanted(player_t *player)
{
	UINT8 i = 0, nump = 0, numfirst = 0;
	for (; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		nump++;
		if (players[i].position > 1)
			continue;
		numfirst++;
	}
	return ((numfirst < nump) && !player->spectator && (player->position == 1));
}

void K_SpawnBattlePoints(player_t *source, player_t *victim, UINT8 amount)
{
	statenum_t st;
	mobj_t *pt;

	if (!source || !source->mo)
		return;

	if (amount == 1)
		st = S_BATTLEPOINT1A;
	else if (amount == 2)
		st = S_BATTLEPOINT2A;
	else if (amount == 3)
		st = S_BATTLEPOINT3A;
	else
		return; // NO STATE!

	pt = P_SpawnMobj(source->mo->x, source->mo->y, source->mo->z, MT_BATTLEPOINT);
	P_SetTarget(&pt->target, source->mo);
	P_SetMobjState(pt, st);
	if (victim && victim->skincolor)
		pt->color = victim->skincolor;
	else
		pt->color = source->skincolor;

	if (encoremode)
		pt->renderflags ^= RF_HORIZONTALFLIP;
}

void K_CheckBumpers(void)
{
	UINT8 i;
	UINT8 numingame = 0;
	UINT8 nobumpers = 0;
	UINT8 eliminated = 0;

	if (!(gametyperules & GTR_BUMPERS))
		return;

	if (gameaction == ga_completed)
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator) // not even in-game
			continue;

		if (players[i].exiting) // we're already exiting! stop!
			return;

		numingame++;

		if (!P_MobjWasRemoved(players[i].mo) && players[i].mo->health <= 0) // if you don't have any bumpers, you're probably not a winner
		{
			nobumpers++;
		}

		if (players[i].pflags & PF_ELIMINATED)
		{
			eliminated++;
		}
	}

	if (K_Cooperative())
	{
		if (nobumpers > 0 && nobumpers >= numingame)
		{
			P_DoAllPlayersExit(PF_NOCONTEST, false);
			return;
		}
	}
	else if (eliminated >= numingame - 1)
	{
		P_DoAllPlayersExit(0, false);
		return;
	}

	if (numingame <= 1)
	{
		if ((gametyperules & GTR_PRISONS) && (K_CanChangeRules(true) == true))
		{
			// Reset map to turn on battle capsules
			if (server)
				D_MapChange(gamemap, gametype, encoremode, true, 0, false, false);
		}

		return;
	}
}

void K_CheckEmeralds(player_t *player)
{
	if (!(gametyperules & GTR_POWERSTONES))
	{
		return;
	}

	if (!ALLCHAOSEMERALDS(player->emeralds))
	{
		return;
	}

	player->roundscore = 100; // lmao

	P_DoAllPlayersExit(0, false);
}

UINT16 K_GetChaosEmeraldColor(UINT32 emeraldType)
{
	switch (emeraldType)
	{
		case EMERALD_CHAOS1:
			return SKINCOLOR_CHAOSEMERALD1;
		case EMERALD_CHAOS2:
			return SKINCOLOR_CHAOSEMERALD2;
		case EMERALD_CHAOS3:
			return SKINCOLOR_CHAOSEMERALD3;
		case EMERALD_CHAOS4:
			return SKINCOLOR_CHAOSEMERALD4;
		case EMERALD_CHAOS5:
			return SKINCOLOR_CHAOSEMERALD5;
		case EMERALD_CHAOS6:
			return SKINCOLOR_CHAOSEMERALD6;
		case EMERALD_CHAOS7:
			return SKINCOLOR_CHAOSEMERALD7;
		default:
			return SKINCOLOR_NONE;
	}
}

mobj_t *K_SpawnChaosEmerald(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT32 emeraldType)
{
	boolean validEmerald = true;
	mobj_t *emerald = P_SpawnMobj(x, y, z, MT_EMERALD);
	mobj_t *overlay;

	P_Thrust(emerald,
		FixedAngle(P_RandomFixed(PR_ITEM_ROULETTE) * 180) + angle,
		36 * mapobjectscale);

	emerald->momz = flip * 36 * mapobjectscale;
	if (emerald->eflags & MFE_UNDERWATER)
		emerald->momz = (117 * emerald->momz) / 200;

	emerald->threshold = 10;

	switch (emeraldType)
	{
		case EMERALD_CHAOS1:
		case EMERALD_CHAOS2:
		case EMERALD_CHAOS3:
		case EMERALD_CHAOS4:
		case EMERALD_CHAOS5:
		case EMERALD_CHAOS6:
		case EMERALD_CHAOS7:
			emerald->color = K_GetChaosEmeraldColor(emeraldType);
			break;
		default:
			CONS_Printf("Invalid emerald type %d\n", emeraldType);
			validEmerald = false;
			break;
	}

	if (validEmerald == true)
	{
		emerald->extravalue1 = emeraldType;
	}

	overlay = P_SpawnMobjFromMobj(emerald, 0, 0, 0, MT_OVERLAY);
	P_SetTarget(&overlay->target, emerald);
	P_SetMobjState(overlay, S_CHAOSEMERALD_UNDER);
	overlay->color = emerald->color;

	if (gametyperules & GTR_CLOSERPLAYERS)
	{
		emerald->fuse = BATTLE_DESPAWN_TIME;
	}

	return emerald;
}

mobj_t *K_SpawnSphereBox(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 amount)
{
	mobj_t *drop = P_SpawnMobj(x, y, z, MT_SPHEREBOX);

	drop->angle = angle;
	P_Thrust(drop,
		FixedAngle(P_RandomFixed(PR_ITEM_ROULETTE) * 180) + angle,
		P_RandomRange(PR_ITEM_ROULETTE, 4, 12) * mapobjectscale);

	drop->momz = flip * 12 * mapobjectscale;
	if (drop->eflags & MFE_UNDERWATER)
		drop->momz = (117 * drop->momz) / 200;

	drop->flags &= ~(MF_NOGRAVITY|MF_NOCLIPHEIGHT);

	drop->extravalue2 = amount;

	return drop;
}

void K_DropEmeraldsFromPlayer(player_t *player, UINT32 emeraldType)
{
	UINT8 i;
	SINT8 flip = P_MobjFlip(player->mo);

	if (player->incontrol < TICRATE)
		return;

	for (i = 0; i < 14; i++)
	{
		UINT32 emeraldFlag = (1 << i);

		if ((player->emeralds & emeraldFlag) && (emeraldFlag & emeraldType))
		{
			K_SpawnChaosEmerald(player->mo->x, player->mo->y, player->mo->z, player->mo->angle - ANGLE_90, flip, emeraldFlag);

			player->emeralds &= ~emeraldFlag;
			break; // Drop only one emerald. Emerald wins are hard enough!
		}
	}
}

UINT8 K_NumEmeralds(player_t *player)
{
	UINT8 i;
	UINT8 num = 0;

	for (i = 0; i < 14; i++)
	{
		UINT32 emeraldFlag = (1 << i);

		if (player->emeralds & emeraldFlag)
		{
			num++;
		}
	}

	return num;
}

static inline boolean IsOnInterval(tic_t interval)
{
	return ((leveltime - starttime) % interval) == 0;
}

static UINT32 CountEmeraldsSpawned(const mobj_t *mo)
{
	switch (mo->type)
	{
		case MT_EMERALD:
			return mo->extravalue1;

		case MT_MONITOR:
			return Obj_MonitorGetEmerald(mo);

		default:
			return 0U;
	}
}

void K_RunPaperItemSpawners(void)
{
	const boolean overtime = (battleovertime.enabled >= 10*TICRATE);
	const tic_t interval = BATTLE_SPAWN_INTERVAL;

	const boolean canmakeemeralds = (gametyperules & GTR_POWERSTONES);

	UINT32 emeraldsSpawned = 0;
	UINT32 firstUnspawnedEmerald = 0;

	thinker_t *th;
	mobj_t *mo;

	UINT8 pcount = 0;
	INT16 i;

	if (battleprisons)
	{
		// Gametype uses paper items, but this specific expression doesn't
		return;
	}

	if (leveltime < starttime)
	{
		// Round hasn't started yet!
		return;
	}

	if (leveltime == g_battleufo.due)
	{
		Obj_SpawnBattleUFOFromSpawner();
	}

	if (!IsOnInterval(interval))
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		emeraldsSpawned |= players[i].emeralds;

		if ((players[i].exiting > 0 || (players[i].pflags & PF_ELIMINATED))
			|| ((gametyperules & GTR_BUMPERS) && !P_MobjWasRemoved(players[i].mo) && players[i].mo->health <= 0))
		{
			continue;
		}

		pcount++;
	}

	if (overtime == true)
	{
		SINT8 flip = 1;

		// Just find emeralds, no paper spots
		for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
		{
			if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
				continue;

			mo = (mobj_t *)th;

			emeraldsSpawned |= CountEmeraldsSpawned(mo);
		}

		if (canmakeemeralds)
		{
			for (i = 0; i < 7; i++)
			{
				UINT32 emeraldFlag = (1 << i);

				if (!(emeraldsSpawned & emeraldFlag))
				{
					firstUnspawnedEmerald = emeraldFlag;
					break;
				}
			}
		}

		if (firstUnspawnedEmerald != 0)
		{
			K_SpawnChaosEmerald(
				battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
				FixedAngle(P_RandomRange(PR_ITEM_ROULETTE, 0, 359) * FRACUNIT), flip,
				firstUnspawnedEmerald
			);
		}
		else
		{
			K_FlingPaperItem(
				battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
				FixedAngle(P_RandomRange(PR_ITEM_ROULETTE, 0, 359) * FRACUNIT), flip,
				0, 0
			);

			if (gametyperules & GTR_SPHERES)
			{
				K_SpawnSphereBox(
					battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
					FixedAngle(P_RandomRange(PR_ITEM_ROULETTE, 0, 359) * FRACUNIT), flip,
					10
				);
			}
		}
	}
	else
	{
		if (pcount > 0)
		{
#define MAXITEM 64
			mobj_t *spotList[MAXITEM];
			UINT8 spotMap[MAXITEM];
			UINT8 spotCount = 0, spotBackup = 0, spotAvailable = 0;
			UINT8 monitorsSpawned = 0;

			for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
			{
				if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
					continue;

				mo = (mobj_t *)th;

				emeraldsSpawned |= CountEmeraldsSpawned(mo);

				if (mo->type != MT_PAPERITEMSPOT)
					continue;

				if (spotCount >= MAXITEM)
					continue;

				if (Obj_ItemSpotIsAvailable(mo))
				{
					// spotMap first only includes spots
					// where a monitor doesn't exist
					spotMap[spotAvailable] = spotCount;
					spotAvailable++;
				}
				else
				{
					monitorsSpawned++;
				}

				spotList[spotCount] = mo;
				spotCount++;
			}

			if (spotCount <= 0)
			{
				return;
			}

			if (canmakeemeralds)
			{
				for (i = 0; i < 7; i++)
				{
					UINT32 emeraldFlag = (1 << i);

					if (!(emeraldsSpawned & emeraldFlag))
					{
						firstUnspawnedEmerald = emeraldFlag;
						break;
					}
				}
			}

			//CONS_Printf("leveltime = %d ", leveltime);

			if (spotAvailable > 0 && monitorsSpawned < BATTLE_MONITOR_SPAWN_LIMIT)
			{
				const UINT8 r = spotMap[P_RandomKey(PR_ITEM_ROULETTE, spotAvailable)];

				Obj_ItemSpotAssignMonitor(spotList[r], Obj_SpawnMonitor(
							spotList[r], 3, firstUnspawnedEmerald));
			}

			for (i = 0; i < spotCount; ++i)
			{
				// now spotMap includes every spot
				spotMap[i] = i;
			}

			if ((gametyperules & GTR_SPHERES) && IsOnInterval(16 * interval))
			{
				spotBackup = spotCount;
				for (i = 0; i < pcount; i++)
				{
					UINT8 r = 0, key = 0;
					mobj_t *drop = NULL;
					SINT8 flip = 1;

					if (spotCount == 0)
					{
						// all are accessible again
						spotCount = spotBackup;
					}

					if (spotCount == 1)
					{
						key = 0;
					}
					else
					{
						key = P_RandomKey(PR_ITEM_ROULETTE, spotCount);
					}

					r = spotMap[key];

					//CONS_Printf("[%d %d %d] ", i, key, r);

					flip = P_MobjFlip(spotList[r]);

					drop = K_SpawnSphereBox(
						spotList[r]->x, spotList[r]->y, spotList[r]->z + (128 * mapobjectscale * flip),
							FixedAngle(P_RandomRange(PR_ITEM_ROULETTE, 0, 359) * FRACUNIT), flip,
							10
					);

					K_FlipFromObject(drop, spotList[r]);

					spotCount--;
					if (key != spotCount)
					{
						// So the core theory of what's going on is that we keep every
						// available option at the front of the array, so we don't have
						// to skip over any gaps or do recursion to avoid doubles.
						// But because spotCount can be reset in the case of a low
						// quanitity of item spawnpoints in a map, we still need every
						// entry in the array, even outside of the "visible" range.
						// A series of swaps allows us to adhere to both constraints.
						// -toast 22/03/22 (semipalindromic!)
						spotMap[key] = spotMap[spotCount];
						spotMap[spotCount] = r; // was set to spotMap[key] previously
					}
				}
			}
			//CONS_Printf("\n");
		}
	}
}

static void K_SpawnOvertimeLaser(fixed_t x, fixed_t y, fixed_t scale)
{
	const fixed_t heightPadding = 346 * scale;

	UINT8 i, j;

	for (i = 0; i <= r_splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];
		fixed_t zpos;
		SINT8 flip;

		if (player == NULL || player->mo == NULL || P_MobjWasRemoved(player->mo) == true)
		{
			continue;
		}

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			zpos = player->mo->z + player->mo->height;
			zpos = min(zpos + heightPadding, player->mo->ceilingz);
		}
		else
		{
			zpos = player->mo->z;
			zpos = max(zpos - heightPadding, player->mo->floorz);
		}

		flip = P_MobjFlip(player->mo);

		for (j = 0; j < 3; j++)
		{
			mobj_t *mo = P_SpawnMobj(x, y, zpos, MT_OVERTIME_PARTICLE);

			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				mo->flags2 |= MF2_OBJECTFLIP;
				mo->eflags |= MFE_VERTICALFLIP;
			}

			mo->angle = R_PointToAngle2(mo->x, mo->y, battleovertime.x, battleovertime.y) + ANGLE_90;
			mo->renderflags |= (RF_DONTDRAW & ~(K_GetPlayerDontDrawFlag(player)));

			P_SetScale(mo, scale);

			switch (j)
			{
				case 0:
					P_SetMobjState(mo, S_OVERTIME_BULB1);

					if (leveltime & 1)
						mo->frame += 1;

					//P_SetScale(mo, mapobjectscale);
					zpos += 35 * mo->scale * flip;
					break;
				case 1:
					P_SetMobjState(mo, S_OVERTIME_LASER);

					if (leveltime & 1)
						mo->frame += 3;
					else
						mo->frame += (leveltime / 2) % 3;

					//P_SetScale(mo, scale);
					zpos += 346 * mo->scale * flip;

					if (battleovertime.enabled < 10*TICRATE)
						mo->renderflags |= RF_TRANS50;
					break;
				case 2:
					P_SetMobjState(mo, S_OVERTIME_BULB2);

					if (leveltime & 1)
						mo->frame += 1;

					//P_SetScale(mo, mapobjectscale);
					break;
				default:
					I_Error("Bruh moment has occured\n");
					return;
			}
		}
	}
}

void K_RunBattleOvertime(void)
{
	if (battleovertime.enabled < 10*TICRATE)
	{
		battleovertime.enabled++;
		if (battleovertime.enabled == TICRATE)
			S_StartSound(NULL, sfx_bhurry);
		if (battleovertime.enabled == 10*TICRATE)
			S_StartSound(NULL, sfx_kc40);
	}
	else if (battleovertime.radius > 0)
	{
		const fixed_t minradius = 768 * mapobjectscale;

		if (battleovertime.radius > minradius)
			battleovertime.radius -= (battleovertime.initial_radius / (30*TICRATE));

		if (battleovertime.radius < minradius)
			battleovertime.radius = minradius;

		// Subtract the 10 second grace period of the barrier
		if (battleovertime.enabled < 25*TICRATE)
		{
			battleovertime.enabled++;
			Obj_PointPlayersToXY(battleovertime.x, battleovertime.y);
		}
	}

	if (battleovertime.radius > 0)
	{
		const INT32 orbs = 32;
		const angle_t angoff = ANGLE_MAX / orbs;
		const UINT8 spriteSpacing = 128;

		fixed_t circumference = FixedMul(M_PI_FIXED, battleovertime.radius * 2);
		fixed_t scale = max(circumference / spriteSpacing / orbs, mapobjectscale);

		fixed_t size = FixedMul(mobjinfo[MT_OVERTIME_PARTICLE].radius, scale);
		fixed_t posOffset = max(battleovertime.radius - size, 0);

		INT32 i;

		for (i = 0; i < orbs; i++)
		{
			angle_t ang = (i * angoff) + FixedAngle((leveltime * FRACUNIT) / 4);

			fixed_t x = battleovertime.x + P_ReturnThrustX(NULL, ang, posOffset);
			fixed_t y = battleovertime.y + P_ReturnThrustY(NULL, ang, posOffset);

			K_SpawnOvertimeLaser(x, y, scale);
		}
	}
}

void K_SetupMovingCapsule(mapthing_t *mt, mobj_t *mobj)
{
	UINT8 sequence = mt->args[0] - 1;
	fixed_t speed = (FRACUNIT >> 3) * mt->args[1];
	boolean backandforth = (mt->args[2] & TMBCF_BACKANDFORTH);
	boolean reverse = (mt->args[2] & TMBCF_REVERSE);
	mobj_t *target = NULL;

	// Find the inital target
	if (reverse)
	{
		target = P_GetLastTubeWaypoint(sequence);
	}
	else
	{
		target = P_GetFirstTubeWaypoint(sequence);
	}

	if (!target)
	{
		CONS_Alert(CONS_WARNING, "No target waypoint found for moving capsule (seq: #%d)\n", sequence);
		return;
	}

	P_SetTarget(&mobj->target, target);
	mobj->lastlook = sequence;
	mobj->movecount = target->health;
	mobj->movefactor = speed;

	if (backandforth) {
		mobj->flags2 |= MF2_AMBUSH;
	} else {
		mobj->flags2 &= ~MF2_AMBUSH;
	}

	if (reverse) {
		mobj->cvmem = -1;
	} else {
		mobj->cvmem = 1;
	}
}

void K_SpawnPlayerBattleBumpers(player_t *p)
{
	const UINT8 bumpers = K_Bumpers(p);

	if (bumpers <= 0)
	{
		return;
	}

	{
		INT32 i;
		angle_t diff = FixedAngle(360*FRACUNIT / bumpers);
		angle_t newangle = p->mo->angle;
		mobj_t *bump;

		for (i = 0; i < bumpers; i++)
		{
			bump = P_SpawnMobjFromMobj(p->mo,
				P_ReturnThrustX(p->mo, newangle + ANGLE_180, 64*FRACUNIT),
				P_ReturnThrustY(p->mo, newangle + ANGLE_180, 64*FRACUNIT),
				0, MT_BATTLEBUMPER);
			bump->threshold = i;
			P_SetTarget(&bump->target, p->mo);
			bump->angle = newangle;
			bump->color = p->mo->color;
			if (p->mo->renderflags & RF_DONTDRAW)
				bump->renderflags |= RF_DONTDRAW;
			else
				bump->renderflags &= ~RF_DONTDRAW;
			newangle += diff;
		}
	}
}

void K_BattleInit(boolean singleplayercontext)
{
	size_t i;

	if ((gametyperules & GTR_PRISONS) && singleplayercontext && !battleprisons && !cv_battletest.value)
	{
		mapthing_t *mt = mapthings;
		for (i = 0; i < nummapthings; i++, mt++)
		{
			if (mt->type == mobjinfo[MT_BATTLECAPSULE].doomednum)
				P_SpawnMapThing(mt);
			else if (mt->type == mobjinfo[MT_CDUFO].doomednum)
				maptargets++;
		}

		battleprisons = true;
	}

	if (gametyperules & GTR_BUMPERS)
	{
		const INT32 startingHealth = K_BumpersToHealth(K_StartingBumperCount());

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;

			if (players[i].mo)
			{
				players[i].mo->health = startingHealth;
			}

			K_SpawnPlayerBattleBumpers(players+i);
		}
	}

	g_battleufo.due = starttime;
	g_battleufo.previousId = Obj_GetFirstBattleUFOSpawnerID();
}

UINT8 K_Bumpers(player_t *player)
{
	if ((gametyperules & GTR_BUMPERS) == 0)
	{
		return 0;
	}

	if (P_MobjWasRemoved(player->mo))
	{
		return 0;
	}

	if (player->mo->health < 1)
	{
		return 0;
	}

	if (player->mo->health > UINT8_MAX)
	{
		return UINT8_MAX;
	}

	return (player->mo->health - 1);
}

INT32 K_BumpersToHealth(UINT8 bumpers)
{
	return (bumpers + 1);
}

boolean K_BattleOvertimeKiller(mobj_t *mobj)
{
	if (battleovertime.enabled < 10*TICRATE)
	{
		return false;
	}

	fixed_t distance = R_PointToDist2(mobj->x, mobj->y, battleovertime.x, battleovertime.y);

	if (distance <= battleovertime.radius)
	{
		return false;
	}

	P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);

	return true;
}
