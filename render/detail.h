#pragma once

namespace Render
{

void detailInit();

// called after map load
void detailInvalidate();

bool detailIsActive();

Vector2 detailSetTextureAndGetScale(int textureIndex);
void detailClearTexture();

}
