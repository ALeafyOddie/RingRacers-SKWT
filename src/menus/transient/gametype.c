/// \file  menus/transient/gametype.c
/// \brief Gametype selection

#include "../../k_menu.h"

INT16 menugametype = GT_RACE;

void M_NextMenuGametype(UINT32 forbidden)
{
	const INT16 currentmenugametype = menugametype;
	do
	{
		menugametype++;
		if (menugametype >= numgametypes)
			menugametype = 0;

		if (!(gametypes[menugametype]->rules & forbidden))
			break;
	} while (menugametype != currentmenugametype);
}

void M_PrevMenuGametype(UINT32 forbidden)
{
	const INT16 currentmenugametype = menugametype;
	do
	{
		if (menugametype == 0)
			menugametype = numgametypes;
		menugametype--;

		if (!(gametypes[menugametype]->rules & forbidden))
			break;
	} while (menugametype != currentmenugametype);
}
