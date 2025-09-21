#ifndef STDAFX_H
#define STDAFX_H

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>

#ifndef NDEBUG
#define SCHIZO_DEBUG
#endif

#ifdef SCHIZO_DEBUG
#define GL3_ASSERT(exp) \
    do \
    { \
        if (!(exp)) \
        { \
            platformError("Assertion failed: %s:%d: %s", __FILE__, __LINE__, #exp); \
        } \
    } while (0)
#else
#define GL3_ASSERT(exp) (void)0
#endif

/************************************************/
/* cdll_int.dll begin
 */

// hack so we can use our math types with the sdk
#include "linmath.h"
#define vec3_t Render::Vector3

typedef float vec_t;
typedef int (*pfnUserMsgHook)(const char *pszName, int iSize, void *pbuf);

typedef struct rect_s
{
    int left, right, top, bottom;
} wrect_t;

#include "enums.h" // netsrc_s
#include "cdll_int.h"

/*
 * cdll_int.dll end
 ************************************************/

// other sdk includes
#include "cvardef.h"
#include "r_studioint.h"
#include "com_model.h"
#include "studio.h"
#include "event_api.h"
#include "entity_types.h"
#include "pm_defs.h"
#include "triangleapi.h"
#include "pm_movevars.h"
#include "screenfade.h"
#include "shake.h"

#define PRODUCT_NAME "csgl3"

// not defined in the sdk
#define MAX_LIGHTSTYLES 64
#define MAX_DLIGHTS 32
#define MAX_ELIGHTS 64

// used for schizo checks
#define NULL_LIGHTSTYLE (MAX_LIGHTSTYLES - 1)

// these are in the sdk but in the utils folder, so define them here
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2
#define PLANE_ANYX 3
#define PLANE_ANYY 4
#define PLANE_ANYZ 5

// manually define to avoid conflicts with windows.h on msvc
#if defined(_MSC_VER)
#define WINAPI __stdcall
#define APIENTRY WINAPI
#endif
#include <glad/glad.h>

#include "../shaders/shader_common.h"

#include "render_interface.h"

#include "platform.h"
#include "shader.h"
#include "utility.h"
#include "vertexformat.h"

#include "render.h"

#ifdef SCHIZO_DEBUG
#define GL_ERROR_CHECK
#endif

#ifndef GL_ERROR_CHECK
#define GL_ERRORS() (void)0
#define GL_ERRORS_QUIET() (void)0
#else
inline void GLErrors(const char *file, int line, bool message)
{
    int error = 0;

    while ((error = glGetError()))
    {
        if (message)
        {
            Render::g_engfuncs.Con_Printf("OpenGL error 0x%x (%s:%d)\n", error, file, line);
        }
    }
}

#define GL_ERRORS() GLErrors(__FILE__, __LINE__, true)
#define GL_ERRORS_QUIET() GLErrors(__FILE__, __LINE__, false)
#endif

#endif
