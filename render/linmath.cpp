#include "stdafx.h"
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
    __m128 a0 = _mm_load_ps(&a.m00);
    __m128 a1 = _mm_load_ps(&a.m10);
    __m128 a2 = _mm_load_ps(&a.m20);
    __m128 a3 = _mm_load_ps(&a.m30);

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
    _mm_store_ps(&result.m00, r0);
    _mm_store_ps(&result.m10, r1);
    _mm_store_ps(&result.m20, r2);
    _mm_store_ps(&result.m30, r3);
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

// absolute packed singles in your xmm0
static __m128 AbsPS(__m128 x)
{
    return _mm_andnot_ps(_mm_set1_ps(-0.0f), x);
}

void ViewFrustum::Set(const Matrix4 &m)
{
    __m128 r0 = _mm_load_ps(&m.m00);
    __m128 r1 = _mm_load_ps(&m.m10);
    __m128 r2 = _mm_load_ps(&m.m20);
    __m128 r3 = _mm_load_ps(&m.m30);

    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);

    __m128 plane0 = _mm_add_ps(r3, r0); // LEFT
    __m128 plane1 = _mm_sub_ps(r3, r0); // RIGHT
    __m128 plane2 = _mm_sub_ps(r3, r1); // TOP
    __m128 plane3 = _mm_add_ps(r3, r1); // BOTTOM

    _MM_TRANSPOSE4_PS(plane0, plane1, plane2, plane3);

    __m128 dot = _mm_mul_ps(plane0, plane0);
    dot = _mm_add_ps(dot, _mm_mul_ps(plane1, plane1));
    dot = _mm_add_ps(dot, _mm_mul_ps(plane2, plane2));

    __m128 ilength = _mm_rsqrt_ps(dot);

    m_nx = _mm_mul_ps(plane0, ilength);
    m_ny = _mm_mul_ps(plane1, ilength);
    m_nz = _mm_mul_ps(plane2, ilength);
    m_d = _mm_mul_ps(plane3, ilength);

    m_absNx = AbsPS(m_nx);
    m_absNy = AbsPS(m_ny);
    m_absNz = AbsPS(m_nz);
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
