#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "sceneStructs.h"
#include "utilities.h"

#define MIN_INTERSECT_DIST 0.0001f
#define MAX_INTERSECT_DIST 10000.0f

/**
 * Handy-dandy hash function that provides seeds for random number generation.
 */
__host__ __device__ inline unsigned int utilhash(unsigned int a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// CHECKITOUT
/**
 * Compute a point at parameter value `t` on ray `r`.
 * Falls slightly short so that it doesn't intersect the object it's hitting.
 */
__host__ __device__ glm::vec3 getPointOnRay(Ray r, float t) {
    return r.origin + t * glm::normalize(r.direction);
}

/**
 * Multiplies a mat4 and a vec4 and returns a vec3 clipped from the vec4.
 */
__host__ __device__ glm::vec3 multiplyMV(glm::mat4 m, glm::vec4 v) {
    return glm::vec3(m * v);
}

__host__ __device__ float squareplaneIntersectionTest(Geom& squareplane, Ray& r, glm::vec3& normal) {
    Ray q;
    q.origin = multiplyMV(squareplane.inverseTransform, glm::vec4(r.origin, 1.0f));
    q.direction = glm::normalize(multiplyMV(squareplane.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float t = glm::dot(glm::vec3(0.0f, 0.0f, 1.0f), (glm::vec3(0.5f, 0.5f, 0.0f) - q.origin)) / glm::dot(glm::vec3(0.0f, 0.0f, 1.0f), q.direction);
    glm::vec3 objspaceIntersection = getPointOnRay(q, t);

    if (t > 0.0001f && objspaceIntersection.x >= -0.5001f && objspaceIntersection.x <= 0.5001f && objspaceIntersection.y >= -0.5001f && objspaceIntersection.y <= 0.5001f) {
        glm::vec3 intersectionPoint = multiplyMV(squareplane.transform, glm::vec4(objspaceIntersection, 1.0f));
        normal = glm::normalize(multiplyMV(squareplane.invTranspose, glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }

    return MAX_INTERSECT_DIST;
}


// CHECKITOUT
/**
 * Test intersection between a ray and a transformed cube. Untransformed,
 * the cube ranges from -0.5 to 0.5 in each axis and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float boxIntersectionTest(Geom &box, Ray &r, glm::vec3 &normal) {
    Ray q;
    q.origin    =                multiplyMV(box.inverseTransform, glm::vec4(r.origin   , 1.0f));
    q.direction = glm::normalize(multiplyMV(box.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float tmin = -1e38f;
    float tmax = 1e38f;
    glm::vec3 tmin_n;
    glm::vec3 tmax_n;
    for (int xyz = 0; xyz < 3; ++xyz) {
        float qdxyz = q.direction[xyz];
        /*if (glm::abs(qdxyz) > 0.00001f)*/ {
            float t1 = (-0.5f - q.origin[xyz]) / qdxyz;
            float t2 = (+0.5f - q.origin[xyz]) / qdxyz;
            float ta = glm::min(t1, t2);
            float tb = glm::max(t1, t2);
            glm::vec3 n;
            n[xyz] = t2 < t1 ? +1.0f : -1.0f;
            if (ta > 0.0f && ta > tmin) {
                tmin = ta;
                tmin_n = n;
            }
            if (tb < tmax) {
                tmax = tb;
                tmax_n = n;
            }
        }
    }

    if (tmax >= tmin && tmax > 0.0f) {
        if (tmin <= 0.0f) {
            tmin = tmax;
            tmin_n = tmax_n;
        }
        glm::vec3 intersectionPoint = multiplyMV(box.transform, glm::vec4(getPointOnRay(q, tmin), 1.0f));
        normal = glm::normalize(multiplyMV(box.invTranspose, glm::vec4(tmin_n, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }
    return MAX_INTERSECT_DIST;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed sphere. Untransformed,
 * the sphere always has radius 0.5 and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float sphereIntersectionTest(Geom &sphere, Ray &r, glm::vec3 &normal) {
    float radius = 0.5f;

    glm::vec3 ro = multiplyMV(sphere.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(sphere.inverseTransform, glm::vec4(r.direction, 0.0f)));

    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    float vDotDirection = glm::dot(rt.origin, rt.direction);
    float radicand = vDotDirection * vDotDirection - (glm::dot(rt.origin, rt.origin) - powf(radius, 2.0f));
    if (radicand < 0.0f) {
        return MAX_INTERSECT_DIST;
    }

    float squareRoot = sqrt(radicand);
    float firstTerm = -vDotDirection;
    float t1 = firstTerm + squareRoot;
    float t2 = firstTerm - squareRoot;

    float t = 0.0f;
    if (t1 < 0.0f && t2 < 0.0f) {
        return MAX_INTERSECT_DIST;
    } else if (t1 > 0.0f && t2 > 0.0f) {
        t = min(t1, t2);
    } else {
        t = max(t1, t2);
    }

    glm::vec3 objspaceIntersection = getPointOnRay(rt, t);

    glm::vec3 intersectionPoint = multiplyMV(sphere.transform, glm::vec4(objspaceIntersection, 1.0f));
    normal = glm::normalize(multiplyMV(sphere.invTranspose, glm::vec4(objspaceIntersection, 0.0f)));

    return glm::length(r.origin - intersectionPoint);
}


// CHECKITOUT
/**
 * Test intersection between a ray and a triangle.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float triIntersectionTest(Tri &tri, Ray &r, glm::vec3& normal) {
    //glm::vec3 bary_coords;
    //t = glm::intersectRayTriangle(pathSegment.ray.origin, pathSegment.ray.direction, tri.p0, tri.p1, tri.p2, bary_coords);
        //1. Ray-plane intersection
    float t = glm::dot(tri.plane_normal, (tri.p0 - r.origin)) / glm::dot(tri.plane_normal, r.direction);
    if (t < 0.0f) return t;

    glm::vec3 P = r.origin + t * r.direction;
    //2. Barycentric test
    float S = 0.5f * glm::length(glm::cross(tri.p0 - tri.p1, tri.p0 - tri.p2));
    float s1 = 0.5f * glm::length(glm::cross(P - tri.p1, P - tri.p2)) / S;
    float s2 = 0.5f * glm::length(glm::cross(P - tri.p2, P - tri.p0)) / S;
    float s3 = 0.5f * glm::length(glm::cross(P - tri.p0, P - tri.p1)) / S;
    float sum = s1 + s2 + s3;

    if (s1 >= 0 && s1 <= 1 && s2 >= 0 && s2 <= 1 && s3 >= 0 && s3 <= 1 && sum == 1.0f) {
        normal = tri.plane_normal;
        return t;
    }
    return -1.0f;
}
