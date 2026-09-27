#pragma once
#include "esp_entity.hpp"
#include <cmath>

static constexpr double PI_D = 3.14159265358979323846;
inline double DegToRadD(double d) { return d * (PI_D / 180.0); }
inline double RadToDegD(double r) { return r * (180.0 / PI_D); }

struct Vec2 { float x, y; };

bool WorldToScreen(const FVector& world, const FVector& cam_pos,
    double cam_yaw, double cam_pitch, float cam_fov,
    int sw, int sh, Vec2& out)
{
    double dx = world.x - cam_pos.x;
    double dy = world.y - cam_pos.y;
    double dz = world.z - cam_pos.z;

    double yr = DegToRadD(cam_yaw), pr = DegToRadD(-cam_pitch); // negate: Evrima positive = up, matrix expects positive = down
    double sy = sin(yr), cy = cos(yr), sp = sin(pr), cp = cos(pr);

    double rx = -sy,   ry = cy,   rz = 0.0;
    double ux = sp*cy, uy = sp*sy, uz = cp;
    double fx = cp*cy, fy = cp*sy, fz = -sp;

    double cam_x = dx*rx + dy*ry + dz*rz;
    double cam_y = dx*ux + dy*uy + dz*uz;
    double cam_z = dx*fx + dy*fy + dz*fz;

    if (cam_z <= 0.0) return false;

    double half_w = sw * 0.5;
    double half_h = sh * 0.5;
    // FIX: was half_h — UE cam_fov is HORIZONTAL, scale must use half_w
    // half_h produced 0.5625x vertical compression at 1080p → boxes barely moved on pitch
    double scale  = half_w / tan(DegToRadD(cam_fov * 0.5));

    out.x = (float)(half_w + (cam_x / cam_z) * scale);
    out.y = (float)(half_h - (cam_y / cam_z) * scale);
    return true;
}

inline double Dist3D(const FVector& a, const FVector& b) {
    double dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
    return sqrt(dx*dx+dy*dy+dz*dz);
}

struct Angle { double pitch, yaw; };

Angle CalcAngle(const FVector& src, const FVector& dst) {
    double dx=dst.x-src.x, dy=dst.y-src.y, dz=dst.z-src.z;
    double d2 = sqrt(dx*dx+dy*dy);
    return { RadToDegD(atan2(dz,d2)), RadToDegD(atan2(dy,dx)) }; // positive pitch = up, matches Evrima convention
}

double AngleDiff(double a, double b) {
    double d = a-b;
    while (d >  180.0) d-=360.0;
    while (d < -180.0) d+=360.0;
    return d;
}

double AngleDist(Angle a, Angle b) {
    double dp=AngleDiff(a.pitch,b.pitch), dy=AngleDiff(a.yaw,b.yaw);
    return sqrt(dp*dp+dy*dy);
}