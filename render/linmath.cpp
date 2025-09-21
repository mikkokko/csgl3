#include "stdafx.h"
#include <xmmintrin.h>
#include "brush.h" // gl3_plane_t

namespace Render
{

Matrix4 ViewMatrix(const Vector3 &origin, const Vector3 &forward, const Vector3 &right, const Vector3 &up)
{
    Matrix4 result;

    result.m00 = right.x;
    result.m01 = up.x;
    result.m02 = -forward.x;
    result.m03 = 0;

    result.m10 = right.y;
    result.m11 = up.y;
    result.m12 = -forward.y;
    result.m13 = 0;

    result.m20 = right.z;
    result.m21 = up.z;
    result.m22 = -forward.z;
    result.m23 = 0;

    result.m30 = -Dot(right, origin);
    result.m31 = -Dot(up, origin);
    result.m32 = Dot(forward, origin);
    result.m33 = 1;

    return result;
}

Matrix4 ProjectionMatrix(float fovy, float aspect, float znear, float zfar)
{
    Matrix4 result;

    float f = 1 / tanf(fovy * 0.5f);
    float fn = 1 / (znear - zfar);

    result.m00 = f / aspect;
    result.m01 = 0;
    result.m02 = 0;
    result.m03 = 0;

    result.m10 = 0;
    result.m11 = f;
    result.m12 = 0;
    result.m13 = 0;

    result.m20 = 0;
    result.m21 = 0;
    result.m22 = (znear + zfar) * fn;
    result.m23 = -1;

    result.m30 = 0;
    result.m31 = 0;
    result.m32 = 2 * znear * zfar * fn;
    result.m33 = 0;

    return result;
}

Matrix4 operator*(const Matrix4 &a, const Matrix4 &b)
{
    __m128 a0 = _mm_loadu_ps(&a.m00);
    __m128 a1 = _mm_loadu_ps(&a.m10);
    __m128 a2 = _mm_loadu_ps(&a.m20);
    __m128 a3 = _mm_loadu_ps(&a.m30);

    __m128 r0 = _mm_mul_ps(a0, _mm_set1_ps(b.m00));
    r0 = _mm_add_ps(r0, _mm_mul_ps(a1, _mm_set1_ps(b.m01)));
    r0 = _mm_add_ps(r0, _mm_mul_ps(a2, _mm_set1_ps(b.m02)));
    r0 = _mm_add_ps(r0, _mm_mul_ps(a3, _mm_set1_ps(b.m03)));

    __m128 r1 = _mm_mul_ps(a0, _mm_set1_ps(b.m10));
    r1 = _mm_add_ps(r1, _mm_mul_ps(a1, _mm_set1_ps(b.m11)));
    r1 = _mm_add_ps(r1, _mm_mul_ps(a2, _mm_set1_ps(b.m12)));
    r1 = _mm_add_ps(r1, _mm_mul_ps(a3, _mm_set1_ps(b.m13)));

    __m128 r2 = _mm_mul_ps(a0, _mm_set1_ps(b.m20));
    r2 = _mm_add_ps(r2, _mm_mul_ps(a1, _mm_set1_ps(b.m21)));
    r2 = _mm_add_ps(r2, _mm_mul_ps(a2, _mm_set1_ps(b.m22)));
    r2 = _mm_add_ps(r2, _mm_mul_ps(a3, _mm_set1_ps(b.m23)));

    __m128 r3 = _mm_mul_ps(a0, _mm_set1_ps(b.m30));
    r3 = _mm_add_ps(r3, _mm_mul_ps(a1, _mm_set1_ps(b.m31)));
    r3 = _mm_add_ps(r3, _mm_mul_ps(a2, _mm_set1_ps(b.m32)));
    r3 = _mm_add_ps(r3, _mm_mul_ps(a3, _mm_set1_ps(b.m33)));

    Matrix4 result;
    _mm_storeu_ps(&result.m00, r0);
    _mm_storeu_ps(&result.m10, r1);
    _mm_storeu_ps(&result.m20, r2);
    _mm_storeu_ps(&result.m30, r3);
    return result;
}

void AngleVectors(const Vector3 &angles, Vector3 *front, Vector3 *right, Vector3 *up)
{
    float sp, cp, sy, cy, sr, cr;
    SinCos(Radians(angles.x), sp, cp);
    SinCos(Radians(angles.y), sy, cy);
    SinCos(Radians(angles.z), sr, cr);

    if (front)
    {
        front->x = cp * cy;
        front->y = cp * sy;
        front->z = -sp;
    }

    if (right)
    {
        right->x = cr * sy - cy * sp * sr;
        right->y = -cr * cy - sp * sr * sy;
        right->z = -cp * sr;
    }

    if (up)
    {
        up->x = cr * cy * sp + sr * sy;
        up->y = cr * sp * sy - cy * sr;
        up->z = cp * cr;
    }
}

Matrix3x4 ModelMatrix3x4(const Vector3 &origin, const Vector3 &angles)
{
    float sp, cp, sy, cy, sr, cr;

    SinCos(Radians(angles.x), sp, cp);
    SinCos(Radians(angles.y), sy, cy);
    SinCos(Radians(angles.z), sr, cr);

    Matrix3x4 result;
    result.m00 = cp * cy;
    result.m10 = cp * sy;
    result.m20 = -sp;

    result.m01 = sr * sp * cy - cr * sy;
    result.m11 = sr * sp * sy + cr * cy;
    result.m21 = sr * cp;

    result.m02 = cr * sp * cy + sr * sy;
    result.m12 = cr * sp * sy - sr * cy;
    result.m22 = cr * cp;

    result.m03 = origin.x;
    result.m13 = origin.y;
    result.m23 = origin.z;
    return result;
}

Matrix3x4 DiagonalMatrix3x4(float f)
{
    Matrix3x4 result{};
    result.m00 = f;
    result.m11 = f;
    result.m22 = f;
    return result;
}

void ViewFrustum::Set(const Matrix4 &m)
{
    // LEFT RIGHT TOP BOTTOM
    m_normals_x[0] = m.m03 + m.m00;
    m_normals_y[0] = m.m13 + m.m10;
    m_normals_z[0] = m.m23 + m.m20;
    m_dists[0] = m.m33 + m.m30;

    m_normals_x[1] = m.m03 - m.m00;
    m_normals_y[1] = m.m13 - m.m10;
    m_normals_z[1] = m.m23 - m.m20;
    m_dists[1] = m.m33 - m.m30;

    m_normals_x[2] = m.m03 - m.m01;
    m_normals_y[2] = m.m13 - m.m11;
    m_normals_z[2] = m.m23 - m.m21;
    m_dists[2] = m.m33 - m.m31;

    m_normals_x[3] = m.m03 + m.m01;
    m_normals_y[3] = m.m13 + m.m11;
    m_normals_z[3] = m.m23 + m.m21;
    m_dists[3] = m.m33 + m.m31;

    for (int i = 0; i < 4; i++)
    {
        Vector3 normal{ m_normals_x[i], m_normals_y[i], m_normals_z[i] };
        float scale = 1.0f / VectorLength(normal);
        m_normals_x[i] *= scale;
        m_normals_y[i] *= scale;
        m_normals_z[i] *= scale;
        m_dists[i] *= -scale; // technically a bruh moment
    }
}

bool ViewFrustum::CullAABB(const Vector3 &min, const Vector3 &max)
{
    for (int i = 0; i < 4; i++)
    {
        Vector3 normal{ m_normals_x[i], m_normals_y[i], m_normals_z[i] };
        float dist = m_dists[i];

        Vector3 point{
            (normal.x < 0) ? min.x : max.x,
            (normal.y < 0) ? min.y : max.y,
            (normal.z < 0) ? min.z : max.z
        };

        if (Dot(normal, point) < dist)
        {
            return true;
        }
    }

    return false;
}

bool ViewFrustum::CullSphere(const Vector3 &origin, float radius)
{
    for (int i = 0; i < 4; i++)
    {
        Vector3 normal{ m_normals_x[i], m_normals_y[i], m_normals_z[i] };
        float dist = m_dists[i];

        if (Dot(origin, normal) - dist <= -radius)
        {
            return true;
        }
    }

    return false;
}

bool ViewFrustum::CullPolygon(const Vector3 (&vertices)[4])
{
    for (int i = 0; i < 4; ++i)
    {
        Vector3 normal{ m_normals_x[i], m_normals_y[i], m_normals_z[i] };
        float dist = m_dists[i];

        if (Dot(normal, vertices[0]) < dist
            && Dot(normal, vertices[1]) < dist
            && Dot(normal, vertices[2]) < dist
            && Dot(normal, vertices[3]) < dist)
        {
            return true;
        }
    }

    return false;
}

int BoxOnPlaneSide(const Vector3 &mins, const Vector3 &maxs, const gl3_plane_t &plane)
{
    if (plane.type < 3)
    {
        if (plane.dist <= mins.Get(plane.type))
        {
            return 1;
        }

        if (plane.dist >= maxs.Get(plane.type))
        {
            return 2;
        }

        return 3;
    }

    Vector3 p1, p2;

    for (int i = 0; i < 3; i++)
    {
        if (plane.normal.Get(i) < 0)
        {
            p1.Get(i) = mins.Get(i);
            p2.Get(i) = maxs.Get(i);
        }
        else
        {
            p1.Get(i) = maxs.Get(i);
            p2.Get(i) = mins.Get(i);
        }
    }

    float d1 = Dot(plane.normal, p1) - plane.dist;
    float d2 = Dot(plane.normal, p2) - plane.dist;

    int bits = 0;

    if (d1 >= 0)
    {
        bits |= BopsFront;
    }

    if (d2 < 0)
    {
        bits |= BopsBack;
    }

    return bits;
}

}
