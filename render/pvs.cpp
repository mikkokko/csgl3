#include "stdafx.h"
#include "pvs.h"
#include "brush.h"

namespace Render
{

int g_pvsFrame;

static gl3_leaf_t *LeafAtPoint(const Vector3 &point)
{
    gl3_node_t *node = g_worldmodel->nodes;

    while (node->contents >= 0)
    {
        float d = Dot(node->plane->normal, point) - node->plane->dist;
        if (d > 0)
        {
            node = node->children[0];
        }
        else
        {
            node = node->children[1];
        }
    }

    return reinterpret_cast<gl3_leaf_t *>(node);
}

static void MarkNodesVisible(gl3_leaf_t *leaf)
{
    gl3_node_t *node = reinterpret_cast<gl3_node_t *>(leaf);
    while (node && node->pvsframe != g_pvsFrame)
    {
        node->pvsframe = g_pvsFrame;
        node = node->parent;
    }
}

void pvsUpdate(const Vector3 &point)
{
    gl3_leaf_t *leaf = LeafAtPoint(g_state.viewOrigin);

    static gl3_leaf_t *lastLeaf;
    if (leaf == lastLeaf)
    {
        // no change
        return;
    }

    lastLeaf = leaf;
    g_pvsFrame++;

    byte *visdata = leaf->compressed_vis;

    if (leaf == g_worldmodel->leafs || !visdata)
    {
        // make all visible
        for (int i = 1; i <= g_worldmodel->numleafs; i++)
        {
            gl3_leaf_t *other = &g_worldmodel->leafs[i];
            if (other->has_visible_surfaces)
            {
                MarkNodesVisible(other);
            }
        }

        return;
    }

    for (int i = 1; i <= g_worldmodel->numleafs; visdata++)
    {
        if (!visdata[0])
        {
            i += 8 * visdata[1];
            visdata++;
            continue;
        }

        for (int j = 0; j < 8; j++, i++)
        {
            if (visdata[0] & (1 << j))
            {
                gl3_leaf_t *other = &g_worldmodel->leafs[i];
                if (other->has_visible_surfaces)
                {
                    MarkNodesVisible(other);
                }
            }
        }
    }
}

gl3_node_t *pvsNode(gl3_node_t *node, const Vector3 &mins, const Vector3 &maxs)
{
    if (node->pvsframe != g_pvsFrame)
    {
        return nullptr;
    }

    GL3_ASSERT(node->contents != CONTENTS_SOLID);

    if (node->contents < 0)
    {
        return node;
    }

    int bits = BoxOnPlaneSide(mins, maxs, *node->plane);
    if (bits & BopsFront)
    {
        gl3_node_t *next = pvsNode(node->children[0], mins, maxs);
        if (next)
        {
            return next;
        }
    }

    if (bits & BopsBack)
    {
        gl3_node_t *next = pvsNode(node->children[1], mins, maxs);
        if (next)
        {
            return next;
        }
    }

    return nullptr;
}

}
