#ifndef LINMATH_H
#define LINMATH_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Render
{

// FIXME: extremely cringe, make this generic?
struct gl3_plane_t;

constexpr float Radians(float degrees)
{
    return degrees * static_cast<float>(M_PI / 180.0);
}

constexpr float Degrees(float radians)
{
    return radians * static_cast<float>(180.0 / M_PI);
}

inline void SinCos(float x, float &sine, float &cosine)
{
    sine = sinf(x);
    cosine = cosf(x);
}

constexpr float Lerp(float a, float b, float f)
{
    return a + (b - a) * f;
}

struct Vector2
{
    float x, y;
};

struct Vector3
{
    Vector3() = default;
    Vector3(float x, float y, float z);
    Vector3(const float *p); // goldsrc support

    // legacy trauma
    float &Get(int component) { return (&x)[component]; }
    const float &Get(int component) const { return (&x)[component]; }

    float x, y, z;
};

void AngleVectors(const Vector3 &angles, Vector3 *front, Vector3 *right, Vector3 *up);

inline Vector3::Vector3(float _x, float _y, float _z)
{
    x = _x;
    y = _y;
    z = _z;
}

inline Vector3::Vector3(const float *p)
{
    x = p[0];
    y = p[1];
    z = p[2];
}

inline bool VectorIsZero(const Vector3 &a)
{
    return !a.x && !a.y && !a.z;
}

inline float VectorLengthSquared(const Vector3 &a)
{
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

inline float VectorLength(const Vector3 &a)
{
    return sqrtf(VectorLengthSquared(a));
}

inline void VectorNormalize(Vector3 &a)
{
    float s = VectorLengthSquared(a);
    if (s != 0.0f)
    {
        s = 1.0f / sqrtf(s);
        a.x *= s;
        a.y *= s;
        a.z *= s;
    }
}

inline float Dot(const Vector3 &a, const Vector3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double DotDouble(const Vector3 &a, const Vector3 &b)
{
    double a0 = a.x;
    double a1 = a.y;
    double a2 = a.z;

    double b0 = b.x;
    double b1 = b.y;
    double b2 = b.z;

    return a0 * b0 + a1 * b1 + a2 * b2;
}

inline Vector3 Cross(const Vector3 &a, const Vector3 &b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vector3 operator+(const Vector3 &a, const Vector3 &b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector3 operator-(const Vector3 &a, const Vector3 &b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3 operator*(const Vector3 &a, float b)
{
    return { a.x * b, a.y * b, a.z * b };
}

inline void operator+=(Vector3 &a, const Vector3 &b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}

inline void operator-=(Vector3 &a, const Vector3 &b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
}

inline void operator*=(Vector3 &a, float b)
{
    a.x *= b;
    a.y *= b;
    a.z *= b;
}

inline Vector3 operator-(const Vector3 &a)
{
    return { -a.x, -a.y, -a.z };
}

inline bool operator==(const Vector3 &a, const Vector3 &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline Vector3 VectorLerp(const Vector3 &a, const Vector3 &b, float f)
{
    return a + (b - a) * f;
}

struct Vector4
{
    Vector4() = default;
    Vector4(float x, float y, float z, float w);
    Vector4(const Vector3 &xyz, float w);

    float x, y, z, w;
};

inline Vector4::Vector4(float _x, float _y, float _z, float _w)
{
    x = _x;
    y = _y;
    z = _z;
    w = _w;
}

inline Vector4::Vector4(const Vector3 &_xyz, float _w)
{
    x = _xyz.x;
    y = _xyz.y;
    z = _xyz.z;
    w = _w;
}

inline Vector4 operator-(const Vector4 &a)
{
    return { -a.x, -a.y, -a.z, -a.w };
}

struct alignas(16) Matrix4
{
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
};

Matrix4 ViewMatrix(const Vector3 &origin, const Vector3 &forward, const Vector3 &right, const Vector3 &up);
Matrix4 ProjectionMatrix(float fovy, float aspect, float znear, float zfar);

Matrix4 operator*(const Matrix4 &a, const Matrix4 &b);

struct Matrix3x4
{
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
};

Matrix3x4 ModelMatrix3x4(const Vector3 &origin, const Vector3 &angles);
Matrix3x4 DiagonalMatrix3x4(float f);

class alignas(16) ViewFrustum
{
public:
    void Set(const Matrix4 &viewProjectionMatrix);

    bool CullBox(const Vector3 &center, const Vector3 &extents)
    {
        __m128 cx = _mm_set1_ps(center.x);
        __m128 cy = _mm_set1_ps(center.y);
        __m128 cz = _mm_set1_ps(center.z);

        __m128 ex = _mm_set1_ps(extents.x);
        __m128 ey = _mm_set1_ps(extents.y);
        __m128 ez = _mm_set1_ps(extents.z);

        __m128 dist = _mm_mul_ps(cx, m_nx);
        __m128 radius = _mm_mul_ps(ex, m_absNx);

        dist = _mm_add_ps(dist, _mm_mul_ps(cy, m_ny));
        radius = _mm_add_ps(radius, _mm_mul_ps(ey, m_absNy));

        dist = _mm_add_ps(dist, _mm_mul_ps(cz, m_nz));
        radius = _mm_add_ps(radius, _mm_mul_ps(ez, m_absNz));

        dist = _mm_add_ps(dist, m_d);

        __m128 backMask = _mm_cmplt_ps(_mm_add_ps(dist, radius), _mm_setzero_ps());

        return (_mm_movemask_ps(backMask) != 0);
    }

    bool CullSphere(const Vector3 &center, float radius)
    {
        __m128 cx = _mm_set1_ps(center.x);
        __m128 cy = _mm_set1_ps(center.y);
        __m128 cz = _mm_set1_ps(center.z);
        __m128 r = _mm_set1_ps(radius);

        __m128 dist = _mm_mul_ps(cx, m_nx);
        dist = _mm_add_ps(dist, _mm_mul_ps(cy, m_ny));
        dist = _mm_add_ps(dist, _mm_mul_ps(cz, m_nz));
        dist = _mm_add_ps(dist, m_d);

        __m128 backMask = _mm_cmplt_ps(_mm_add_ps(dist, r), _mm_setzero_ps());

        return (_mm_movemask_ps(backMask) != 0);
    }

    __m128 m_nx;
    __m128 m_ny;
    __m128 m_nz;
    __m128 m_d;

    __m128 m_absNx;
    __m128 m_absNy;
    __m128 m_absNz;
};

// quake function for classifying an aabb against plane
enum
{
    BopsFront = 1,
    BopsBack = 2,
    BopsStraddle = (BopsFront | BopsBack)
};

int BoxOnPlaneSide(const Vector3 &mins, const Vector3 &maxs, const gl3_plane_t &plane);

}

#endif
