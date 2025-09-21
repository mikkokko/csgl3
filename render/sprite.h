#ifndef SPRITE_H
#define SPRITE_H

namespace Render
{

void spriteInit();

void spriteBegin(bool alphaBlend);
void spriteDraw(cl_entity_t *entity, float blend);
void spriteEnd();

}

#endif
