#ifndef PVS_H
#define PVS_H

namespace Render
{

struct gl3_leaf_t;
struct gl3_node_t;

extern int g_pvsFrame;

// called from brush the renderer
void pvsUpdate(const Vector3 &point);

// equivalent to engine PVSNode
gl3_node_t *pvsNode(gl3_node_t *node, const Vector3 &mins, const Vector3 &maxs);

}

#endif // PVS_H
