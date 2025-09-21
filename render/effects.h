#ifndef EFFECTS_H
#define EFFECTS_H

namespace Render
{

// for the cstrike nvgs dlight hack
void effectsBeginDlightCapture();
void effectsEndDlightCapture();

void effectsInit(efx_api_t *efx, engine_studio_api_t *studio);

}

#endif
