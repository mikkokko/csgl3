#ifndef PVS_H
#define PVS_H

namespace Render
{

struct gl3_leaf_t;
struct gl3_node_t;

// only for rendering: currently visible leaves that have visible geometry
extern int g_pvsLeafCount;
extern gl3_leaf_t *g_pvsLeaves[32768];

// called from brush the renderer
void pvsUpdate(const Vector3 &point);

// equivalent to engine PVSNode
gl3_node_t *pvsNode(gl3_node_t *node, const Vector3 &mins, const Vector3 &maxs);

}

#endif // PVS_H
