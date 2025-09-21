#ifndef DECALCLIP_H
#define DECALCLIP_H

namespace Render
{

enum ClipEdge
{
    ClipEdgeLeft,
    ClipEdgeRight,
    ClipEdgeTop,
    ClipEdgeBottom
};

// Vertex must have Vector3 position and Vector2 texcoord
template<typename Vertex>
struct DecalClip
{
    static void Intersect(Vertex *result, const Vertex *v1, const Vertex *v2, ClipEdge edge)
    {
        bool xedge = (edge == ClipEdgeLeft || edge == ClipEdgeRight);
        float boundary = (edge == ClipEdgeLeft || edge == ClipEdgeTop) ? 0.0f : 1.0f;

        float v1coord = xedge ? v1->texcoord.x : v1->texcoord.y;
        float v2coord = xedge ? v2->texcoord.x : v2->texcoord.y;

        GL3_ASSERT(fabsf(v2coord - v1coord) > 1e-6f);
        float frac = (boundary - v1coord) / (v2coord - v1coord);
        GL3_ASSERT(frac >= 0.0f && frac <= 1.0f);

        result->texcoord.x = Lerp(v1->texcoord.x, v2->texcoord.x, frac);
        result->texcoord.y = Lerp(v1->texcoord.y, v2->texcoord.y, frac);
        result->position = VectorLerp(v1->position, v2->position, frac);
    }

    static bool Inside(const Vertex *vertex, ClipEdge edge)
    {
        switch (edge)
        {
        case ClipEdgeLeft:
            return (vertex->texcoord.x > 0);
        case ClipEdgeRight:
            return (vertex->texcoord.x < 1);
        case ClipEdgeTop:
            return (vertex->texcoord.y > 0);
        case ClipEdgeBottom:
            return (vertex->texcoord.y < 1);
        }

        GL3_ASSERT(0);
        return false;
    }

    static void Clip(const Vertex *vertices, int vertexCount, Vertex *results, int &resultCount, ClipEdge edge)
    {
        resultCount = 0;

        if (!vertexCount)
        {
            return;
        }

        const Vertex *previous = &vertices[vertexCount - 1];
        bool prevInside = Inside(previous, edge);

        for (int i = 0; i < vertexCount; i++)
        {
            const Vertex *current = &vertices[i];
            bool curInside = Inside(current, edge);
            if (curInside)
            {
                if (!prevInside)
                {
                    Intersect(&results[resultCount++], previous, current, edge);
                }

                results[resultCount++] = *current;
            }
            else if (prevInside)
            {
                Intersect(&results[resultCount++], previous, current, edge);
            }

            previous = current;
            prevInside = curInside;
        }
    }
};

}

#endif // DECALCLIP_H
