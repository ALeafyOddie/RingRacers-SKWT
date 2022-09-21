/* object-specific code */
#ifndef k_objects_H
#define k_objects_H

/* Hyudoro */
void Obj_HyudoroDeploy(mobj_t *master);
void Obj_HyudoroThink(mobj_t *actor);
void Obj_HyudoroCenterThink(mobj_t *actor);
void Obj_HyudoroCollide(mobj_t *special, mobj_t *toucher);

/* Shrink */
void Obj_PohbeeThinker(mobj_t *pohbee);
void Obj_PohbeeRemoved(mobj_t *pohbee);
void Obj_ShrinkGunRemoved(mobj_t *gun);
boolean Obj_ShrinkLaserCollide(mobj_t *gun, mobj_t *victim);
void Obj_CreateShrinkPohbees(player_t *owner);

/* Item Debris */
void Obj_SpawnItemDebrisEffects(mobj_t *collectible, mobj_t *collector);
void Obj_ItemDebrisThink(mobj_t *debris);
fixed_t Obj_ItemDebrisBounce(mobj_t *debris, fixed_t momz);

/* SPB */
void Obj_SPBThink(mobj_t *spb);

/* SPB Juicebox Rings */
void Obj_MantaRingThink(mobj_t *manta);
mobj_t *Obj_MantaRingCreate(mobj_t *spb, mobj_t *owner, mobj_t *chase);

#endif/*k_objects_H*/
