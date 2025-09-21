#ifndef STUDIO_MISC_H

namespace Render
{

bool studioFrustumCull(cl_entity_t *entity, studiohdr_t *header);
void studioDynamicLight(cl_entity_t *entity, alight_t *light);

studiohdr_t *studioTextureHeader(model_t *model, studiohdr_t *header);

}

#endif // STUDIO_MISC_H
