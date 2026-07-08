/// <summary>
/// See Copyright Notice in setup.hpp
///
/// @TODO: Cleanup template definitions and introduce Triangle2D support.
/// </summary>
#ifndef __EXT_GEOM_TRIANGLE_HPP__
#define __EXT_GEOM_TRIANGLE_HPP__

#include "setup.hpp"
#include "line.hpp"
#include "linesegment.hpp"
#include "ray.hpp"

namespace glm {
  /// <summary>
  /// A triangle defined by points in 3D Space.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  struct Triangle {
    using Point = vec<L, T, Q>;

    Point a;
    Point b;
    Point c;

    Triangle() = default;

    Triangle(T scalar)
      : a(scalar), b(scalar), c(scalar) {
    }

    Triangle(const Triangle<L, T, Q> &tri)
      : a(tri.a), b(tri.b), c(tri.c) {
    }

    Triangle(const vec<L, T, Q> &a_, const vec<L, T, Q> &b_, const vec<L, T, Q> &c_)
      : a(a_), b(b_), c(c_) {
    }

    Triangle<L, T, Q> &operator=(const Triangle<L, T, Q> &t) {
      a = t.a;
      b = t.b;
      c = t.c;
      return *this;
    }
  };

  // operators

  template<length_t L, typename T, qualifier Q>
  static Triangle<L, T, Q> operator-(const Triangle<L, T, Q> &t) {
    return Triangle<L, T, Q>(t.a, t.c, t.b);
  }

  template<length_t L, typename T, qualifier Q>
  static bool operator==(const Triangle<L, T, Q> &t1, const Triangle<L, T, Q> &t2) {
    return t1.a == t1.a && t1.b == t2.b && t1.c == t2.c;
  }

  template<length_t L, typename T, qualifier Q>
  static Triangle<L, T, Q> operator+(const Triangle<L, T, Q> &t, const vec<L, T, Q> &offset) {
    return Triangle<L, T, Q>(t.a + offset, t.b + offset, t.c + offset);
  }

  template<length_t L, typename T, qualifier Q>
  static Triangle<L, T, Q> operator-(const Triangle<L, T, Q> &t, const vec<L, T, Q> &offset) {
    return Triangle<L, T, Q>(t.a - offset, t.b - offset, t.c - offset);
  }

  template<typename T, qualifier Q>
  static Triangle<3, T, Q> operator*(const mat<3, 3, T, Q> &m, const Triangle<3, T, Q> &t) {
    return Triangle<3, T, Q>(m * t.a, m * t.b, m * t.c);
  }

  template<typename T, qualifier Q>
  static Triangle<3, T, Q> operator*(const mat<3, 4, T, Q> &m, const Triangle<3, T, Q> &t) {
    return Triangle<3, T, Q>(m * t.a, m * t.b, m * t.c);
  }

  template<typename T, qualifier Q>
  static Triangle<3, T, Q> operator*(const mat<4, 3, T, Q> &m, const Triangle<3, T, Q> &t) {
    return Triangle<3, T, Q>(transformPos(m, t.a), transformPos(m, t.b), transformPos(m, t.c));
  }

  template<typename T, qualifier Q>
  static Triangle<3, T, Q> operator*(const mat<4, 4, T, Q> &m, const Triangle<3, T, Q> &t) {
    return Triangle<3, T, Q>(transformPos(m, t.a), transformPos(m, t.b), transformPos(m, t.c));
  }

  template<typename T, qualifier Q>
  static Triangle<3, T, Q> operator*(const qua<T, Q> &q, const Triangle<3, T, Q> &t) {
    return Triangle<3, T, Q>(q * t.a, q * t.b, q * t.c);
  }

  // all+equal

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool equal(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, T eps = epsilon<T>()) {
    return all_equal(x.a, y.a, eps) && all_equal(x.b, y.b, eps) && all_equal(x.c, y.c, eps);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool equal(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, vec<L, T, Q> const &eps) {
    return all_equal(x.a, y.a, eps) && all_equal(x.b, y.b, eps) && all_equal(x.c, y.c, eps);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool equal(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, int MaxULPs) {
    return all_equal(x.a, y.a, MaxULPs) && all_equal(x.b, y.b, MaxULPs) && all_equal(x.c, y.c, MaxULPs);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool equal(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, vec<L, int, Q> const &MaxULPs) {
    return all_equal(x.a, y.a, MaxULPs) && all_equal(x.b, y.b, MaxULPs) && all_equal(x.c, y.c, MaxULPs);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool notEqual(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, T eps = epsilon<T>()) {
    return any_notequal(x.a, y.a, eps) || any_notequal(x.b, y.b, eps) || any_notequal(x.c, y.c, eps);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool notEqual(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, vec<L, T, Q> const &eps) {
    return any_notequal(x.a, y.a, eps) || any_notequal(x.b, y.b, eps) || any_notequal(x.c, y.c, eps);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool notEqual(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, int MaxULPs) {
    return any_notequal(x.a, y.a, MaxULPs) || any_notequal(x.b, y.b, MaxULPs) || any_notequal(x.c, y.c, MaxULPs);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool notEqual(Triangle<L, T, Q> const &x, Triangle<L, T, Q> const &y, vec<L, int, Q> const &MaxULPs) {
    return any_notequal(x.a, y.a, MaxULPs) || any_notequal(x.b, y.b, MaxULPs) || any_notequal(x.c, y.c, MaxULPs);
  }

  // Utilities

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool isinf(const Triangle<L, T, Q> &t) {
    return any_isinf(t.a) || any_isinf(t.b) || any_isinf(t.c);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool isnan(const Triangle<L, T, Q> &t) {
    return any_isnan(t.a) || any_isnan(t.b) || any_isnan(t.c);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool isfinite(const Triangle<L, T, Q> &t) {
    return all(isfinite(t.a)) && all(isfinite(t.b)) && all(isfinite(t.c));
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER GLM_CONSTEXPR bool isDegenerate(Triangle<L, T, Q> const &t, T Epsilon = epsilon<T>()) {
    return all_equal(t.a, t.b, Epsilon) || all_equal(t.a, t.c, Epsilon) || all_equal(t.b, t.c, Epsilon);
  }

  /// <summary>
  /// Return the center of the triangle.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> centroid(const Triangle<L, T, Q> &t) {
    return (t.a + t.b + t.c) * (T(1) / T(3));
  }

  /// <summary>
  /// Compute the surface area of the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T area(const Triangle<3, T, Q> &t) {
    return T(0.5) * length(cross(t.b - t.a, t.c - t.a));
  }

  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T area(const Triangle<2, T, Q> &t) {
    return (t.a.x - t.b.x) * (t.b.y - t.c.y) - (t.b.x - t.c.x) * (t.a.y - t.b.y);
  }

  /// <summary>
  /// Compute the Barycentric U-coordinate of the given point on the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T signedArea(const Triangle<3, T, Q> &t, const vec<3, T, Q> &pt) {
    return dot(cross(t.b - pt, t.c - pt), normalize(cross(t.b - t.a, t.c - t.a)));
  }

  /// <summary>
  /// Compute the total edge length of the triangle.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T perimeter(const Triangle<L, T, Q> &t) {
    return distance(t.a, t.b) + distance(t.b, t.c) + distance(t.c, t.a);
  }

  /// <summary>
  /// Return an edge of the triangle.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER LineSegment<L, T, Q> edge(const Triangle<L, T, Q> &t, int i) {
    switch (i) {
      case 1:
        return LineSegment<L, T, Q>(t.b, t.c);
      case 2:
        return LineSegment<L, T, Q>(t.c, t.a);
      default:
        return LineSegment<L, T, Q>(t.a, t.b);
    }
  }

  /// <summary>
  /// Return a vertex of the Triangle.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> vertex(const Triangle<L, T, Q> &t, int i) {
    switch (i) {
      case 1:
        return t.b;
      case 2:
        return t.c;
      default:
        return t.a;
    }
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> cornerPoint(const Triangle<L, T, Q> &t, int i) {
    return vertex(t, i);
  }

  /// <summary>
  /// Compute all face normals of the triangle.
  /// @TODO @LuaGLM Commented out until binding implementation is complete.
  /// </summary>
  // template<length_t L, typename T, qualifier Q>
  // GLM_GEOM_QUALIFIER void faceNormals(const Triangle<L, T, Q> &t, vec<L, T, Q> (&normals)[4]) {
  //   normals[0] = cross(t.b - t.a, t.c - t.a);
  //   normals[1] = cross(normals[0], t.b - t.a);
  //   normals[2] = cross(normals[0], t.c - t.a);
  //   normals[3] = cross(normals[0], t.c - t.b);
  // }

  /// <summary>
  /// Compute the directions of each edge.
  /// @TODO @LuaGLM Commented out until binding implementation is complete.
  /// </summary>
  // template<length_t L, typename T, qualifier Q>
  // GLM_GEOM_QUALIFIER void faceNormals(const Triangle<L, T, Q> &t, vec<L, T, Q> (&directions)[3]) {
  //   directions[0] = normalize(t.b - t.a);
  //   directions[1] = normalize(t.c - t.a);
  //   directions[2] = normalize(t.c - t.b);
  // }

  /// <summary>
  /// Project the triangle onto the provided axis.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER void projectToAxis(const Triangle<L, T, Q> &t, const vec<L, T, Q> &axis, T &dMin, T &dMax) {
    dMin = dMax = dot(axis, t.a);

    const T db = dot(axis, t.b);
    dMin = min(dMin, db);
    dMax = max(dMax, db);

    const T dc = dot(axis, t.c);
    dMin = min(dMin, dc);
    dMax = max(dMax, dc);
  }

  /// <summary>
  /// Computes an extreme point (further point) in the given direction.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> extremePoint(const Triangle<L, T, Q> &t, const vec<L, T, Q> &direction) {
    vec<L, T, Q> extremePt(T(0));
    T extremeDist = std::numeric_limits<T>::min();

    for (int i = 0; i < 3; ++i) {
      const vec<L, T, Q> &pt = vertex(t, i);
      const T d = dot(direction, pt);
      if (d > extremeDist) {
        extremeDist = d;
        extremePt = pt;
      }
    }
    return extremePt;
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> extremePoint(const Triangle<3, T, Q> &t, const vec<3, T, Q> &direction, T &distance) {
    const vec<L, T, Q> extremePt = extremePoint(t, direction);
    distance = dot(extremePt, direction);
    return extremePt;
  }

  /// <summary>
  /// Return the minimal AABB that encloses the triangle
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER AABB<L, T, Q> boundingAABB(const Triangle<L, T, Q> &t) {
    return AABB<L, T, Q>(min(t.a, t.b, t.c), max(t.a, t.b, t.c));
  }

  /// <summary>
  /// Return true if the given barycentric coordinates lie inside a triangle,
  /// i.e., each uvw value is between 0.0 and 1.0 and whose total sum is 1.0
  /// </summary>
  template<typename T>
  GLM_GEOM_QUALIFIER bool barycentricInsideTriangle(T u, T v, T w, T eps = epsilon<T>()) {
    return u >= T(0) && v >= T(0) && w >= T(0) && equal(u + v + w, T(1), eps);
  }

  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool barycentricInsideTriangle(const vec<3, T, Q> &uvw, T eps = epsilon<T>()) {
    return barycentricInsideTriangle(uvw.x, uvw.y, uvw.z, eps);
  }

  /// <summary>
  /// Helper
  /// </summary>
  template<typename T>
  static GLM_INLINE T triangleArea2D(T x1, T y1, T x2, T y2, T x3, T y3) {
    return (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
  }

  /// <summary>
  /// Express the given point in terms of barycentric u, v, w coordinates. To
  /// map to <u, v> coordinates use: <v, w>.
  ///
  /// Note, "point" should lie on the plane formed by the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> barycentricUVW(const Triangle<3, T, Q> &t, const vec<3, T, Q> &point) {
    const vec<3, T, Q> m = cross(t.b - t.a, t.c - t.a);  // Unnormalized triangle normal.
    const vec<3, T, Q> m_abs = abs(m);

    T nu, nv, d;
    if (m_abs.x >= m_abs.y && m_abs.x >= m_abs.z) {  // YZ plane projection
      nu = triangleArea2D(point.y, point.z, t.b.y, t.b.z, t.c.y, t.c.z);  // P-B-C Area
      nv = triangleArea2D(point.y, point.z, t.c.y, t.c.z, t.a.y, t.a.z);  // P-C-A Area
      d = T(1) / m.x;
    }
    else if (m_abs.y >= m_abs.z) {  // XZ plane projection
      nu = triangleArea2D(point.x, point.z, t.b.x, t.b.z, t.c.x, t.c.z);
      nv = triangleArea2D(point.x, point.z, t.c.x, t.c.z, t.a.x, t.a.z);
      d = T(1) / -m.y;
    }
    else {  // XY plane projection
      nu = triangleArea2D(point.x, point.y, t.b.x, t.b.y, t.c.x, t.c.y);
      nv = triangleArea2D(point.x, point.y, t.c.x, t.c.y, t.a.x, t.a.y);
      d = T(1) / m.z;
    }

    const T u = nu * d;
    const T v = nv * d;
    return vec<3, T, Q>(u, v, T(1) - u - v);
  }

  /// <summary>
  /// Express the given point in terms of barycentric u, v coordinates. To map to
  /// <u, v, w> coordinates use: <1 - u - v, u, v>.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<2, T, Q> barycentricUV(const Triangle<3, T, Q> &t, const vec<3, T, Q> &point) {
    const vec<3, T, Q> uvw = barycentricUVW(t, point);
    return vec<2, T, Q>(uvw.y, uvw.z);
  }

  /// <summary>
  /// Return the point at the given barycentric (UV) coordinates.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> barycentricPoint(const Triangle<3, T, Q> &t, T u, T v) {
    return t.a + ((t.b - t.a) * u + (t.c - t.a) * v);
  }

  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> barycentricPoint(const Triangle<3, T, Q> &t, const vec<2, T, Q> &point) {
    return barycentricPoint(t, point.x, point.y);
  }

  /// <summary>
  /// Return the point at the given barycentric (UVW) coordinates.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> barycentricPoint(const Triangle<3, T, Q> &t, T u, T v, T w) {
    return u * t.a + v * t.b + w * t.c;
  }

  //template<typename T, qualifier Q>
  //GLM_GEOM_QUALIFIER vec<3, T, Q> barycentricPoint(const Triangle<3, T, Q> &t, const vec<3, T, Q> &point) {
  //  return barycentricPoint(t, point.x, point.y, point.z);
  //}

  /// <summary>
  /// Return the plane this triangle lies with counter-clockwise orientation.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER Plane<3, T, Q> planeCCW(const Triangle<3, T, Q> &t) {
    return planeFrom(t.a, t.b, t.c);
  }

  /// <summary>
  /// Return an unnormalized counter-clockwise oriented normal vector for the given vector.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> unnormalizedNormalCCW(const Triangle<3, T, Q> &t) {
    return cross(t.b - t.a, t.c - t.a);
  }

  /// <summary>
  /// Return a counter-clockwise oriented normal vector for the given vector.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> normalCCW(const Triangle<3, T, Q> &t) {
    return normalize(unnormalizedNormalCCW(t));
  }

  /// <summary>
  /// Return the plane this triangle lies with clockwise orientation.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER Plane<3, T, Q> planeCW(const Triangle<3, T, Q> &t) {
    return planeFrom(t.a, t.c, t.b);
  }

  /// <summary>
  /// Return an unnormalized clockwise oriented normal vector for the given vector.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> unnormalizedNormalCW(const Triangle<3, T, Q> &t) {
    return cross(t.c - t.a, t.b - t.a);
  }

  /// <summary>
  /// Return a clockwise oriented normal vector for the given vector.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<3, T, Q> normalCW(const Triangle<3, T, Q> &t) {
    return normalize(unnormalizedNormalCW(t));
  }

  /// <summary>
  /// Return true if the given point is contained within the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool contains(const Triangle<3, T, Q> &t, const vec<3, T, Q> &point, T sqThickness = epsilon<T>()) {
    const vec<3, T, Q> normal = cross(t.b - t.a, t.c - t.a);
    const T d = dot(normal, t.b - point);
    if (d * d <= sqThickness * length2(normal)) {
      const vec<3, T, Q> br = barycentricUVW(t, point);
      return br.x >= -epsilon<T>() && br.y >= -epsilon<T>() && br.z >= -epsilon<T>();
    }
    return false;
  }

  /// <summary>
  /// Return true if the given line-segment is fully contained within the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool contains(const Triangle<3, T, Q> &t, const LineSegment<3, T, Q> &segment, T sqThickness = epsilon<T>()) {
    return contains(t, segment.a, sqThickness) && contains(t, segment.b, sqThickness);
  }

  /// <summary>
  /// Return true if the given triangle is fully contained within the triangle.
  /// </summary>
  template<typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool contains(const Triangle<3, T, Q> &t, const Triangle<3, T, Q> &other, T sqThickness = epsilon<T>()) {
    return contains(t, other.a, sqThickness)
           && contains(t, other.b, sqThickness)
           && contains(t, other.c, sqThickness);
  }

  /// <summary>
  /// Calculate the line/triangle intersection.
  ///
  /// Returning the distance along the 'line' of intersection (infinity otherwise)
  /// and the barycentric coordinates of intersection.
  ///
  /// @NOTE: Moller ray triangle test.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER_NOINLINE T intersectTriangleLine(const Triangle<L, T, Q> &t, const Line<L, T, Q> &line, T &u, T &v) {
    const T eps = epsilon<T>();
    const vec<L, T, Q> e1 = t.b - t.a;
    const vec<L, T, Q> e2 = t.c - t.a;
    const vec<L, T, Q> vt = line.pos - t.a;
    const vec<L, T, Q> vp = cross(line.dir, e2);
    const vec<L, T, Q> vq = cross(vt, e1);

    const T det = dot(e1, vp);
    if (abs(det) <= eps) {  // Determinant zero: line lies on triangles plane
      return std::numeric_limits<T>::infinity();
    }

    // Compute barycentric coordinates.
    const T invDet = T(1) / det;
    u = dot(vt, vp) * invDet;
    v = dot(line.dir, vq) * invDet;

    if (u < -eps || u > (T(1) + eps))
      return std::numeric_limits<T>::infinity();
    if (v < -eps || (u + v) > (T(1) + eps))
      return std::numeric_limits<T>::infinity();

    return dot(e2, vq) * invDet;
  }

  /// <summary>
  /// Compute the closest point between the triangle (along an edge) and provided
  /// point. Also returning the point in Barycentric <u, v, w>.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER_NOINLINE vec<L, T, Q> closestPointTriangle(const Triangle<L, T, Q> &t, const vec<L, T, Q> &p, T &u, T &v, T &w) {
    const vec<L, T, Q> ba = t.b - t.a;
    const vec<L, T, Q> ca = t.c - t.a;
    const vec<L, T, Q> pa = p - t.a;
    const vec<L, T, Q> bp = p - t.b;
    const vec<L, T, Q> cp = p - t.c;

    const T d1 = dot(ba, pa);
    const T d2 = dot(ca, pa);
    const T d3 = dot(ba, bp);
    const T d4 = dot(ca, bp);
    const T d5 = dot(ba, cp);
    const T d6 = dot(ca, cp);

    const T vc = d1 * d4 - d3 * d2;
    const T vb = d5 * d2 - d1 * d6;
    const T va = d3 * d6 - d5 * d4;

    const T zero = T(0);
    if (d1 <= zero && d2 <= zero) {  // P is in the vertex region outside A.
      v = w = zero;
      u = T(1);
      return t.a;  // (1, 0, 0)
    }
    else if (d3 >= zero && d4 <= d3) {  // P is in the vertex region outside B.
      u = w = zero;
      v = T(1);
      return t.b;  // (0, 1, 0)
    }
    else if (vc <= zero && d1 >= zero && d3 <= zero) {  // P is in edge region of AB
      w = zero;
      v = d1 / (d1 - d3);
      u = T(1) - v;
      return t.a + v * ba;  // (1 - v, v, 0)
    }
    else if (d6 >= zero && d5 <= d6) {  // P is in the vertex region outside C.
      u = v = zero;
      w = T(1);
      return t.c;  // (0, 0, 1)
    }
    else if (vb <= zero && d2 >= zero && d6 <= zero) {  // P is in edge region of AC
      v = zero;
      w = d2 / (d2 - d6);
      u = 1 - w;
      return t.a + v * ca;  // (1 - w, 0, w)
    }
    else if (va <= zero && d4 - d3 >= zero && d5 - d6 >= zero) {  // P is in edge region of BC
      u = zero;
      w = (d4 - d3) / (d4 - d3 + d5 - d6);
      v = 1 - w;
      return t.b + v * (t.c - t.b);  // (0, 1 - w, w)
    }

    const T denom = T(1) / (va + vb + vc);  // P must be inside the face
    v = vb * denom;
    w = vc * denom;
    u = T(1) - v - w;  // va * denom
    return t.a + ba * v + ca * w;
  }

  /// <summary>
  /// Compute the closest point between the triangle (along an edge) and provided
  /// line. Returning a point on the triangles edge 'closest' to the line, its
  /// barycentric coordinate (outU, outV), and distance along the line (outD).
  /// </summary>
  template<length_t L, typename T, qualifier Q, typename Line>
  GLM_GEOM_QUALIFIER_NOINLINE vec<L, T, Q> closestPointTriangleLine(const Triangle<L, T, Q> &t, const Line &line, T &outU, T &outV, T &outD) {
    T d1, d2, d3, d_unused;

    const vec<L, T, Q> pt1 = closestPoint(edge(t, 0), line, d_unused, d1);
    const vec<L, T, Q> pt2 = closestPoint(edge(t, 1), line, d_unused, d2);
    const vec<L, T, Q> pt3 = closestPoint(edge(t, 2), line, d_unused, d3);
    const T dist1 = distance2(pt1, getPoint(line, d1));
    const T dist2 = distance2(pt2, getPoint(line, d2));
    const T dist3 = distance2(pt3, getPoint(line, d3));

    T resultDist = d3;
    vec<L, T, Q> result = pt3;
    if (dist1 <= dist2 && dist1 <= dist3) {
      result = pt1;
      resultDist = d1;
    }
    else if (dist2 <= dist3) {
      result = pt2;
      resultDist = d2;
    }

    outD = resultDist;
    outU = barycentricUV(t, result).x;
    outV = barycentricUV(t, result).y;
    return result;
  }

  /// <summary>
  /// Compute the closest point between the triangle (along an edge) and provided
  /// segment. Returning a point on the triangles edge 'closest' to the segment,
  /// its barycentric coordinate (outU, outV), and distance along the segment (outD).
  /// </summary>
  template<length_t L, typename T, qualifier Q, typename Line>
  GLM_GEOM_QUALIFIER_NOINLINE vec<L, T, Q> closestPointTriangleSegment(const Triangle<L, T, Q> &t, const LineSegment<L, T, Q> &line, T &outU, T &outV, T &outD) {
    outD = intersectTriangleLine(t, toLine(line), outU, outV);
    if (outD >= T(0.0) && outD <= T(1.0)) {
      return barycentricPoint(t, outU, outV);
    }

    T d1(0);
    const vec<L, T, Q> pt1 = closestPointTriangleLine<L, T, Q, LineSegment<L, T, Q>>(t, line, outU, outV, d1);
    const vec<L, T, Q> pt2 = closestPoint(t, line.a);
    const vec<L, T, Q> pt3 = closestPoint(t, line.b);
    const T l1 = distance2(pt1, getPoint(line, d1));
    const T l2 = distance2(pt2, line.a);
    const T l3 = distance2(pt3, line.b);

    if (l1 <= l2 && l1 <= l3) {
      outD = d1;
      return pt1;
    }
    else if (l2 <= l3) {
      outD = T(0);
      return pt2;
    }
    else {
      outD = T(1);
      return pt3;
    }
  }

  /// <summary>
  /// Compute the closest point on the triangle to the given point.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER_NOINLINE vec<L, T, Q> closestPoint(const Triangle<L, T, Q> &t, const vec<L, T, Q> &p) {
    T u, v, w;
    return closestPointTriangle(t, p, u, v, w);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> closestPoint(const Triangle<L, T, Q> &t, const LineSegment<L, T, Q> &line, vec<L, T, Q> &linePt) {
    T u, v, d(0);
    const vec<L, T, Q> result = closestPointTriangleSegment<L, T, Q, LineSegment<L, T, Q>>(t, line, u, v, d);
    linePt = getPoint(line, d);
    return result;
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER vec<L, T, Q> closestPoint(const Triangle<L, T, Q> &t, const Line<L, T, Q> &line, vec<L, T, Q> &linePt) {
    T u, v, d = intersectTriangleLine(t, line, u, v);
    if (d != std::numeric_limits<T>::infinity()) {
      linePt = barycentricPoint(t, u, v);
      return linePt;
    }

    const vec<3, T, Q> result = closestPointTriangleLine<3, T, Q, Line<3, T, Q>>(t, line, u, v, d);
    linePt = getPoint(line, d);
    return result;
  }

  /// <summary>
  /// @TODO Compute the closest point between two triangles.
  /// </summary>
  // template<typename T, qualifier Q>
  // GLM_GEOM_QUALIFIER vec<3, T, Q> closestPoint(const Triangle<3, T, Q> &t, const Triangle<3, T, Q> &other, vec<L, T, Q> &otherPt) {
  //   otherPt = vec<3, T, Q>(T(0));
  //   return vec<3, T, Q>(T(0));
  // }

  /// <summary>
  /// Test whether the triangle and given line segment intersect. On success,
  /// also return the intersection barycentric coordinates and distance along
  /// the segment where the triangle intersects.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const LineSegment<L, T, Q> &line, T &u, T &v, T &d) {
    d = intersectTriangleLine(t, toLine(line), u, v);
    return d >= T(0.0) && d <= T(1.0);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Line<L, T, Q> &line, T &u, T &v, T &d) {
    d = intersectTriangleLine(t, line, u, v);
    return d != std::numeric_limits<T>::infinity();
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Ray<L, T, Q> &ray, T &u, T &v, T &d) {
    d = intersectTriangleLine(t, toLine(ray), u, v);
    return d != std::numeric_limits<T>::infinity() && d >= T(0);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Plane<L, T, Q> &plane) {
    return intersects(plane, t);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Sphere<L, T, Q> &sphere, vec<L, T, Q> &intersection) {
    intersection = closestPoint(t, sphere.pos);
    return distance2(intersection, sphere.pos) <= sphere.r * sphere.r;
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const LineSegment<L, T, Q> &line) {
    T u, v;
    const T d = intersectTriangleLine(t, toLine(line), u, v);
    return d >= T(0.0) && d <= T(1.0);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Line<L, T, Q> &line) {
    T u, v;
    const T d = intersectTriangleLine(t, line, u, v);
    return d != std::numeric_limits<T>::infinity();
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Ray<L, T, Q> &ray) {
    T u, v;
    const T d = intersectTriangleLine(t, toLine(ray), u, v);
    return d != std::numeric_limits<T>::infinity() && d >= T(0);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Sphere<L, T, Q> &sphere) {
    vec<L, T, Q> unused;
    return intersects(t, sphere, unused);
  }

  /// <summary>
  /// @TODO
  /// </summary>
  // template<length_t L, typename T, qualifier Q>
  // GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const Triangle<L, T, Q> &other, LineSegment<L, T, Q> &intersection) {
  //   UNUSED(t); UNUSED(other); UNUSED(intersect);
  //   return false;
  // }

  /// <summary>
  /// @TODO
  /// </summary>
  // template<length_t L, typename T, qualifier Q>
  // GLM_GEOM_QUALIFIER bool intersects(const Triangle<L, T, Q> &t, const AABB<L, T, Q> &aabb) {
  //   UNUSED(t); UNUSED(aabb);
  //   return false;
  // }

  /// <summary>
  /// Return the distance between the triangle and given point.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T distance(const Triangle<L, T, Q> &t, const vec<L, T, Q> &p) {
    return distance(closestPoint(t, p), p);
  }

  /// <summary>
  /// Return the distance between the triangle and given sphere.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_GEOM_QUALIFIER T distance(const Triangle<L, T, Q> &t, const Sphere<L, T, Q> &s) {
    return max(T(0), distance(t, s.pos) - s.r);
  }

  namespace detail {
    template<glm::length_t L, typename T, qualifier Q>
    struct compute_to_string<Triangle<L, T, Q>> {
      GLM_GEOM_QUALIFIER std::string call(const Triangle<L, T, Q> &t) {
        return detail::format("triangle(%s, %s, %s)",
        glm::to_string(t.a).c_str(),
        glm::to_string(t.b).c_str(),
        glm::to_string(t.c).c_str());
      }
    };
  }
}
#endif
