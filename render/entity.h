#ifndef ENTITY_H
#define ENTITY_H

namespace Render
{

// sort translucent entities based on distance to viewer
void entitySortTranslucents(const Vector3 &cameraPosition);

// draw the world and solid brush entities
void entityDrawSolidBrushes();

// draw solid studio models and sprites
void entityDrawSolidEntities();

// draw all translucent models
void entityDrawTranslucentEntities(const Vector3 &origin, const Vector3 &forward);

// draw the viewmodel, done last
void entityDrawViewmodel(int drawFlags);

// also used by beams
int entityUpdateRenderAmt(cl_entity_t *entity, const Vector3 &origin, const Vector3 &forward);

// clears all entities marked to draw this frame
void entityClearBuckets();

// get beam entities added for this frame
cl_entity_t **entityGetBeams(int &count);

}

#endif
