// ============================================================================
//  Passive — GLM-free 3D math (vec3, mat4, world-to-screen projection)
// ============================================================================
#pragma once
#include <cmath>
#include <cstring>

namespace tyr {

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s)       const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float len() const { return std::sqrt(dot(*this)); }
    float len2() const { return dot(*this); }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    Vec3 normalized() const {
        const float l = len();
        return l > 1e-6f ? *this * (1.f / l) : Vec3(0, 1, 0);
    }
};

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}
};

// Column-major 4x4, same convention as GL/ImGui's ProjectionMatrix.
struct Mat4 {
    float m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
        return r;
    }

    // Minecraft's own perspective projection (matches GameRenderer.getBasicProjectionMatrix).
    // fovRad: vertical fov; aspect: w/h; near/far in blocks.
    static Mat4 perspective(float fovRad, float aspect, float nearZ, float farZ) {
        Mat4 r;
        const float h = 1.0f / std::tan(fovRad * 0.5f);
        r.m[0] = h / aspect;
        r.m[5] = h;
        r.m[10] = (farZ + nearZ) / (nearZ - farZ);
        r.m[11] = -1.f;
        r.m[14] = 2.f * farZ * nearZ / (nearZ - farZ);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        const Vec3 f = (center - eye);            // forward
        const float fl = f.len();
        Vec3 fn = fl > 1e-6f ? f * (1.f / fl) : Vec3(0, 0, -1);
        const Vec3 s = fn.cross(up).normalized();
        const Vec3 u = s.cross(fn);
        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -fn.x;r.m[6] = -fn.y;r.m[10] = -fn.z;
        r.m[12] = -s.dot(eye);
        r.m[13] = -u.dot(eye);
        r.m[14] =  fn.dot(eye);
        r.m[15] = 1.f;
        return r;
    }

    Vec3 transformPoint(const Vec3& p, float* outW = nullptr) const {
        const float x = p.x, y = p.y, z = p.z;
        Vec3 o{
            m[0] * x + m[4] * y + m[8]  * z + m[12],
            m[1] * x + m[5] * y + m[9]  * z + m[13],
            m[2] * x + m[6] * y + m[10] * z + m[14]
        };
        const float w = m[3] * x + m[7] * y + m[11] * z + m[15];
        if (outW) *outW = w;
        return o;
    }
};

// Smooth (exponential) interpolation toward a target; frame-rate independent.
inline float expLerp(float current, float target, float speed, float dt) {
    if (speed <= 0.f) return target;
    const float t = 1.f - std::exp(-speed * dt);
    return current + (target - current) * t;
}

inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

} // namespace tyr
