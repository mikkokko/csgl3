#include "stdafx.h"
#include "lightmap.h"
#include "memory.h"
#include "brush.h"
#include "texture.h"

// dump to disk so we can laugh at how inefficent we are
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb/stb_image_write.h"

namespace Render
{

// max width/height
constexpr int AtlasMaxDimension = 4096;

struct LightmapRect
{
    int surfaceIndex;
    int w, h, x, y;
};

static void CopyLightmapToAtlas(const gl3_fatsurface_t &surface, int style, Color32 *atlas, int atlasWidth)
{
    GL3_ASSERT(surface.styles[style] != NULL_LIGHTSTYLE);
    GL3_ASSERT(surface.lightmap_data);

    int width = surface.lightmap_width;
    int height = surface.lightmap_height;

    const Color24 *lightmap = &surface.lightmap_data[width * height * style];

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color24 temp = lightmap[y * width + x];
            atlas[y * atlasWidth + x] = { temp.r, temp.g, temp.b, 255 };
        }
    }
}

static void CopyLightmapsToAtlas(const gl3_fatsurface_t &surface, int x, int y, Color32 *atlas, int atlasWidth)
{
    int width = surface.lightmap_width;

    for (int i = 0; i < surface.style_count; i++)
    {
        CopyLightmapToAtlas(surface, i, &atlas[y * atlasWidth + x + (width * i)], atlasWidth);
    }
}

static bool PackRectsToSize(LightmapRect *rects, int rectCount, int atlasWidth, int atlasHeight)
{
    int x = 0;
    int y = 0;
    int maxHeight = 0;

    for (int i = 0; i < rectCount; i++)
    {
        LightmapRect &rect = rects[i];
        if ((x + rect.w) > atlasWidth)
        {
            y += maxHeight;
            maxHeight = 0;
            x = 0;
        }

        if ((y + rect.h) > atlasHeight)
        {
            return false;
        }

        rect.x = x;
        rect.y = y;

        x += rect.w;

        maxHeight = Q_max(maxHeight, rect.h);
    }

    return true;
}

static int ApproximateAtlasHeight(int pixels)
{
    int result = 1;

    int ideal = static_cast<int>(ceilf(sqrtf(static_cast<float>(pixels))));
    while (result < ideal)
    {
        result *= 2;
    }

    return result;
}

static bool PackRects(LightmapRect *rects, int rectCount, int pixelCount, int &atlasWidth, int &atlasHeight)
{
    // 2x1 sheets are tighter for most maps
    // we could try both but it's not worth it
    atlasHeight = ApproximateAtlasHeight(pixelCount);
    atlasWidth = atlasHeight;

    while (atlasWidth <= AtlasMaxDimension)
    {
        if (PackRectsToSize(rects, rectCount, atlasWidth, atlasHeight))
        {
            g_engfuncs.Con_Printf("Lightmaps packed to %dx%d\n", atlasWidth, atlasHeight);
            return true;
        }

        atlasWidth *= 2;
        atlasHeight *= 2;
    }

    g_engfuncs.Con_Printf("Lightmap packing failed, the map will have no lightmaps\n");
    return false;
}

static bool RectCompare(const LightmapRect &a, const LightmapRect &b)
{
    if (a.h > b.h)
    {
        return true;
    }

    if (a.h < b.h)
    {
        return false;
    }

    return a.w > b.w;
}

static void GetSortedLightmapRects(gl3_worldmodel_t *model, LightmapRect *rects, int &rectCount, int &pixelCount)
{
    rectCount = 0;
    pixelCount = 0;

    for (int i = 0; i < model->numsurfaces; i++)
    {
        gl3_fatsurface_t &surface = model->fatsurfaces[i];
        if (!surface.numverts)
        {
            continue;
        }

        if (!surface.style_count)
        {
            continue;
        }

        LightmapRect &rect = rects[rectCount++];
        rect.surfaceIndex = i;
        rect.w = surface.lightmap_width * surface.style_count;
        rect.h = surface.lightmap_height;

        pixelCount += (rect.w * rect.h);
    }

    std::sort(rects, rects + rectCount, RectCompare);
}

static GLuint CreateLightmapTexture(const Color32 *data, int width, int height)
{
    GLuint texture;
    textureGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    return texture;
}

GLuint lightmapCreateAtlas(gl3_worldmodel_t *model, gl3_brushvert_t *vertices)
{
    TempMemoryScope temp;

    int rectCount, pixelCount;
    LightmapRect *rects = temp.Alloc<LightmapRect>(model->numsurfaces);
    GetSortedLightmapRects(model, rects, rectCount, pixelCount);

    int atlasWidth, atlasHeight;
    if (!PackRects(rects, rectCount, pixelCount, atlasWidth, atlasHeight))
    {
        GL3_ASSERT(false);
        return 0;
    }

    model->lightmap_width = atlasWidth;
    model->lightmap_height = atlasHeight;

    Color32 *atlas = temp.Alloc<Color32>(atlasWidth * atlasHeight);

    for (int i = 0; i < rectCount; i++)
    {
        LightmapRect &rect = rects[i];
        gl3_fatsurface_t &surface = model->fatsurfaces[rect.surfaceIndex];

        surface.lightmap_x = rect.x;
        surface.lightmap_y = rect.y;

        CopyLightmapsToAtlas(surface, surface.lightmap_x, surface.lightmap_y, atlas, atlasWidth);

        float lightmap_width = (float)surface.lightmap_width / atlasWidth;

        for (int k = 0; k < surface.numverts; k++)
        {
            gl3_brushvert_t *vertex = &vertices[surface.firstvert + k];

            vertex->position.w = lightmap_width;

            vertex->lightmapTexCoord[0] = PACK_U16((float)(vertex->lightmapTexCoord[0] + (surface.lightmap_x * 16) + 8) / (atlasWidth * 16));
            vertex->lightmapTexCoord[1] = PACK_U16((float)(vertex->lightmapTexCoord[1] + (surface.lightmap_y * 16) + 8) / (atlasHeight * 16));
        }
    }

    return CreateLightmapTexture(atlas, atlasWidth, atlasHeight);
}

}
