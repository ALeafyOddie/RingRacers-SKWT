#include "../r_main.h"
#include "../p_slopes.h"
#include "../p_local.h"
#include "../k_kart.h"
#include "../k_objects.h"

#define SNEAKERPANEL_RADIUS (64*FRACUNIT)

void Obj_SneakerPanelSpriteScale(mobj_t *mobj)
{
	statenum_t newState;
	fixed_t spriteScale;

	if (mobj->scale > FRACUNIT >> 1)
	{
		newState = S_SNEAKERPANEL;
		spriteScale = FRACUNIT;
	}
	else if (mobj->scale > FRACUNIT >> 2)
	{
		newState = S_SNEAKERPANEL_SMALL;
		spriteScale = FRACUNIT << 1;
	}
	else
	{
		newState = S_SNEAKERPANEL_TINY;
		spriteScale = FRACUNIT << 2;
	}

	P_SetMobjState(mobj, newState);
	mobj->spritexscale = mobj->spriteyscale = spriteScale;
}

void Obj_SneakerPanelSpawn(mobj_t *mobj)
{
	mobj->renderflags |= RF_OBJECTSLOPESPLAT | RF_NOSPLATBILLBOARD;
	Obj_SneakerPanelSpriteScale(mobj);
}

void Obj_SneakerPanelSetup(mobj_t *mobj, mapthing_t *mthing)
{
	if (mthing->options & MTF_OBJECTFLIP)
	{
		mobj->eflags |= MFE_VERTICALFLIP;
		mobj->flags2 |= MF2_OBJECTFLIP;
	}
	P_TryMove(mobj, mobj->x, mobj->y, true, NULL); // sets standingslope
	Obj_SneakerPanelSpriteScale(mobj);
}

void Obj_SneakerPanelCollide(mobj_t *panel, mobj_t *mo)
{
	pslope_t *slope = panel->standingslope;
	player_t *player = mo->player;
	fixed_t playerTop = mo->z + mo->height, playerBottom = mo->z;
	fixed_t panelTop, panelBottom, dist, x, y, radius;
	angle_t angle;

	// only players can boost!
	if (player == NULL)
		return;

	// these aren't aerial boosters, so you do need to be on the ground
	if (!P_IsObjectOnGround(mo))
		return;

	// player needs to have the same gravflip status as the panel
	if ((panel->eflags & MFE_VERTICALFLIP) != (mo->eflags & MFE_VERTICALFLIP))
		return;

	// find the x and y coordinates of the player relative to the booster's angle
	dist = R_PointToDist2(panel->x, panel->y, mo->x, mo->y);
	angle = R_PointToAngle2(panel->x, panel->y, mo->x, mo->y) - panel->angle;
	x = P_ReturnThrustX(NULL, angle, dist);
	y = P_ReturnThrustY(NULL, angle, dist);

	// check that these coordinates fall within the square panel
	radius = FixedMul(SNEAKERPANEL_RADIUS, panel->scale);

	if (x < -radius || x > radius || y < -radius || y > radius)
		return; // out of bounds

	// check that the player is within reasonable vertical bounds
	if (slope == NULL)
	{
		panelTop = panel->z + panel->height;
		panelBottom = panel->z;
	}
	else
	{
		x = P_ReturnThrustX(NULL, slope->xydirection, panel->radius);
		y = P_ReturnThrustY(NULL, slope->xydirection, panel->radius);
		panelTop = P_GetSlopeZAt(slope, panel->x + x, panel->y + y);
		panelBottom = P_GetSlopeZAt(slope, panel->x - x, panel->y - y);

		if (panelTop < panelBottom)
		{
			// variable swap
			panelTop = panelTop + panelBottom;
			panelBottom = panelTop - panelBottom;
			panelTop = panelTop - panelBottom;
		}
	}

	if ((playerBottom > panelTop) || (playerTop < panelBottom))
		return;

	// boost!
	if (player->floorboost == 0)
		player->floorboost = 3;
	else
		player->floorboost = 2;

	K_DoSneaker(player, 0);
}

void Obj_SneakerPanelSpawnerSpawn(mobj_t *mobj)
{
	mobj->fuse = mobj->reactiontime;
}

void Obj_SneakerPanelSpawnerSetup(mobj_t *mobj, mapthing_t *mthing)
{
	if (mthing->thing_args[0] != 0)
	{
		mobj->fuse = mobj->reactiontime = mthing->thing_args[0];
	}
}

void Obj_SneakerPanelSpawnerFuse(mobj_t *mobj)
{
	var1 = var2 = 0;
	A_SpawnSneakerPanel(mobj);
	mobj->fuse = mobj->reactiontime;
}
