/*
** $Id: lglm.h $
** Library & API definitions for LuaGLM.
**
** All functions defined in this header are expected to have C++ linkage
** and are to be isolated from the Lua core.
**
** See Copyright Notice in lua.h
*/
#ifndef lglm_h
#define lglm_h
#define LUA_GLM_API
#if !defined(GLM_ENABLE_EXPERIMENTAL)
  #define GLM_ENABLE_EXPERIMENTAL
#endif

#include "lua.hpp"

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

/*
** LuaGLM offers vectors as a primitive type in the runtime and the default
** primitive to each vector/quaternion is float. This increases the minimum size
** to a Value/TaggedValue to 16-bytes (or 4 x float).
**
** By enabling "LUA_GLM_NUMBER_TYPE", the primitive type of each vector becomes
** lua_Number.
*/
#if defined(LUA_GLM_NUMBER_TYPE) && LUA_FLOAT_TYPE != LUA_FLOAT_LONGDOUBLE
  #define GLM_FLOAT_TYPE LUA_NUMBER
  #define GLM_INT_TYPE LUA_INTEGER
#else
  #define GLM_FLOAT_TYPE float
  #define GLM_INT_TYPE int
#endif

/* Floating point glm-operation type */
typedef GLM_FLOAT_TYPE glm_Float;

/* Integer glm-operation type */
typedef GLM_INT_TYPE glm_Integer;

/*
** Specific lua_Number definition to avoid the usage of long doubles within the
** GLM API, avoiding "no matching constructor for initialization of
** 'const detail::float_t<long double>'" compilation errors.
*/
#if LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE
  typedef double glm_Number;
#else
  typedef lua_Number glm_Number;
#endif

/* Additional type cases (see: llimits.h) */
#define cast_glmfloat(i) static_cast<glm_Float>((i))
#define cast_glmint(i) static_cast<glm_Integer>((i))

/* Index-type casting; */
#define i_luaint(i) static_cast<lua_Integer>((i))
#define i_glmlen(l) static_cast<glm::length_t>((l))

/*
** Utility macros for casting vectors as integer/boolean vectors are not
** supported for this iteration of LuaGLM.
*/
#define cast_vec1(V, T) glm::vec<1, T>(static_cast<T>((V).x))
#define cast_vec2(V, T) glm::vec<2, T>(static_cast<T>((V).x), static_cast<T>((V).y))
#define cast_vec3(V, T) glm::vec<3, T>(static_cast<T>((V).x), static_cast<T>((V).y), static_cast<T>((V).z))
#define cast_vec4(V, T) glm::vec<4, T>(static_cast<T>((V).x), static_cast<T>((V).y), static_cast<T>((V).z), static_cast<T>((V).w))
#define cast_quat(Q, T) glm::qua<T>(static_cast<T>((Q).w), static_cast<T>((Q).x), static_cast<T>((Q).y), static_cast<T>((Q).z))

/* @NOTE equal objects must have equal hashes; use with caution. */
#if defined(LUA_GLM_EPS_EQUAL)
  #define _glmeq(a, b) (glm::all(glm::equal((a), (b), glm::epsilon<glm_Float>())))
#else
  #define _glmeq(a, b) ((a) == (b))
#endif

/* lib:LuaGLM requirements */
#define LABEL_INTEGER "integer"
#define LABEL_NUMBER "number"
#define LABEL_VECTOR "vector"
#define LABEL_VECTOR1 "vector1"
#define LABEL_VECTOR2 "vector2"
#define LABEL_VECTOR3 "vector3"
#define LABEL_VECTOR4 "vector4"
#define LABEL_QUATERN "quat"
#define LABEL_MATRIX "matrix"
#define LABEL_SYMMETRIC_MATRIX "symmetric " LABEL_MATRIX

/*
** {==================================================================
** GLM Interface
** ===================================================================
*/

/*
** Return true if the element at the given index is a vector, setting "size" to
** the dimensionality of the vector.
*/
LUA_API bool glm_isvector(lua_State *L, int idx, glm::length_t &size);

/* Return true if the element at the given index is a quaternion. */
LUA_API bool glm_isquat(lua_State *L, int idx);

/*
** Return true if the element at the given index is a matrix, setting "size" to
** the number of column vectors and "secondary" as the size of each column
** component.
*/
LUA_API bool glm_ismatrix(lua_State *L, int idx, glm::length_t &size, glm::length_t &secondary);

/* Push a vector/quaternion onto the Lua stack. */
LUA_API int glm_pushvec1(lua_State *L, const glm::vec<1, glm_Float> &v);
LUA_API int glm_pushvec2(lua_State *L, const glm::vec<2, glm_Float> &v);
LUA_API int glm_pushvec3(lua_State *L, const glm::vec<3, glm_Float> &v);
LUA_API int glm_pushvec4(lua_State *L, const glm::vec<4, glm_Float> &v);
LUA_API int glm_pushquat(lua_State *L, const glm::qua<glm_Float> &q);

/*
** Convert the element at the given index into a vector/quaternion if
** permissible, i.e., the dimensions of element is greater-than-or-equal to the
** dimensions of the conversion.
*/

LUA_API glm::vec<1, glm_Float> glm_tovec1(lua_State *L, int idx);
LUA_API glm::vec<2, glm_Float> glm_tovec2(lua_State *L, int idx);
LUA_API glm::vec<3, glm_Float> glm_tovec3(lua_State *L, int idx);
LUA_API glm::vec<4, glm_Float> glm_tovec4(lua_State *L, int idx);
LUA_API glm::qua<glm_Float> glm_toquat(lua_State *L, int idx);

/* Push a matrix onto the Lua stack. */
LUA_API int glm_pushmat2x2(lua_State *L, const glm::mat<2, 2, glm_Float> &m);
LUA_API int glm_pushmat2x3(lua_State *L, const glm::mat<2, 3, glm_Float> &m);
LUA_API int glm_pushmat2x4(lua_State *L, const glm::mat<2, 4, glm_Float> &m);
LUA_API int glm_pushmat3x2(lua_State *L, const glm::mat<3, 2, glm_Float> &m);
LUA_API int glm_pushmat3x3(lua_State *L, const glm::mat<3, 3, glm_Float> &m);
LUA_API int glm_pushmat3x4(lua_State *L, const glm::mat<3, 4, glm_Float> &m);
LUA_API int glm_pushmat4x2(lua_State *L, const glm::mat<4, 2, glm_Float> &m);
LUA_API int glm_pushmat4x3(lua_State *L, const glm::mat<4, 3, glm_Float> &m);
LUA_API int glm_pushmat4x4(lua_State *L, const glm::mat<4, 4, glm_Float> &m);

/*
** Convert the element at the given index into a matrix if permissible, i.e.,
** the number of columns to the element is greater-than-or-equal to the
** dimensions of the conversion.
*/

LUA_API glm::mat<2, 2, glm_Float> glm_tomat2x2(lua_State *L, int idx);
LUA_API glm::mat<2, 3, glm_Float> glm_tomat2x3(lua_State *L, int idx);
LUA_API glm::mat<2, 4, glm_Float> glm_tomat2x4(lua_State *L, int idx);
LUA_API glm::mat<3, 2, glm_Float> glm_tomat3x2(lua_State *L, int idx);
LUA_API glm::mat<3, 3, glm_Float> glm_tomat3x3(lua_State *L, int idx);
LUA_API glm::mat<3, 4, glm_Float> glm_tomat3x4(lua_State *L, int idx);
LUA_API glm::mat<4, 2, glm_Float> glm_tomat4x2(lua_State *L, int idx);
LUA_API glm::mat<4, 3, glm_Float> glm_tomat4x3(lua_State *L, int idx);
LUA_API glm::mat<4, 4, glm_Float> glm_tomat4x4(lua_State *L, int idx);

/*
** Return the dimensionality of the vector at the given index; zero on failure.
** This function is simply syntactic sugar for glm_isvector;
*/
static LUA_INLINE glm::length_t glm_vector_length(lua_State *L, int idx) {
  glm::length_t size = 0;
  return glm_isvector(L, idx, size) ? size : 0;
}

/*
** Return the dimensionality of the matrix (i.e., the number of component
** vectors) of the Matrix at (or starting at) the given index; zero on failure.
*/
static LUA_INLINE glm::length_t glm_matrix_length(lua_State *L, int idx, glm::length_t &secondary) {
  glm::length_t size = 0;
  return glm_ismatrix(L, idx, size, secondary) ? size : 0;
}

/* }================================================================== */

/*
** {==================================================================
**  GLM Object & Internal Definitions
** ===================================================================
*/

/// <summary>
/// Internal vector definition
/// </summary>
union glmVector {
  glm::vec<1, glm_Float> v1;
  glm::vec<2, glm_Float> v2;
  glm::vec<3, glm_Float> v3;
  glm::vec<4, glm_Float> v4;
  glm::qua<glm_Float> q;

  glmVector() GLM_DEFAULT_CTOR;
  glmVector(const glm::vec<1, glm_Float> &_v) : v1(_v) { }
  glmVector(const glm::vec<2, glm_Float> &_v) : v2(_v) { }
  glmVector(const glm::vec<3, glm_Float> &_v) : v3(_v) { }
  glmVector(const glm::vec<4, glm_Float> &_v) : v4(_v) { }
  glmVector(const glm::qua<glm_Float> &_q) : q(_q) { }

  template<class T> glmVector(const glm::vec<1, T> &_v) : v1(cast_vec1(_v, glm_Float)) { }
  template<class T> glmVector(const glm::vec<2, T> &_v) : v2(cast_vec2(_v, glm_Float)) { }
  template<class T> glmVector(const glm::vec<3, T> &_v) : v3(cast_vec3(_v, glm_Float)) { }
  template<class T> glmVector(const glm::vec<4, T> &_v) : v4(cast_vec4(_v, glm_Float)) { }
  template<class T> glmVector(const glm::qua<T> &_q) : q(cast_quat(_q, glm_Float)) { }

  // Assignment Operators

  inline void operator=(const glm::vec<1, glm_Float> &_v) { v1 = _v; }
  inline void operator=(const glm::vec<2, glm_Float> &_v) { v2 = _v; }
  inline void operator=(const glm::vec<3, glm_Float> &_v) { v3 = _v; }
  inline void operator=(const glm::vec<4, glm_Float> &_v) { v4 = _v; }
  inline void operator=(const glm::qua<glm_Float> &_q) { q = _q; }

  template <typename T> inline void operator=(const glm::vec<1, T> &_v) { v1 = cast_vec1(_v, glm_Float); }
  template <typename T> inline void operator=(const glm::vec<2, T> &_v) { v2 = cast_vec2(_v, glm_Float); }
  template <typename T> inline void operator=(const glm::vec<3, T> &_v) { v3 = cast_vec3(_v, glm_Float); }
  template <typename T> inline void operator=(const glm::vec<4, T> &_v) { v4 = cast_vec4(_v, glm_Float); }
  template <typename T> inline void operator=(const glm::qua<T> &_q) { q = cast_quat(_q, glm_Float); }

  // Reassignment; glm::vec = glmVector.

  inline int Get(glm::vec<1, glm_Float> &_v) const { _v = v1; return 1; }
  inline int Get(glm::vec<2, glm_Float> &_v) const { _v = v2; return 1; }
  inline int Get(glm::vec<3, glm_Float> &_v) const { _v = v3; return 1; }
  inline int Get(glm::vec<4, glm_Float> &_v) const { _v = v4; return 1; }
  inline int Get(glm::qua<glm_Float> &_q) const { _q = q; return 1; }

  template <typename T> inline int Get(glm::vec<1, T> &_v) const { _v = cast_vec1(v1, T); return 1; }
  template <typename T> inline int Get(glm::vec<2, T> &_v) const { _v = cast_vec2(v2, T); return 1; }
  template <typename T> inline int Get(glm::vec<3, T> &_v) const { _v = cast_vec3(v3, T); return 1; }
  template <typename T> inline int Get(glm::vec<4, T> &_v) const { _v = cast_vec4(v4, T); return 1; }
  template <typename T> inline int Get(glm::qua<T> &_q) const { _q = cast_quat(q, T); return 1; }
};

/// <summary>
/// Internal matrix definition.
/// </summary>
LUA_GLM_ALIGNED_TYPE(struct, glmMatrix) {
  union {
    glm::mat<2, 2, glm_Float> m22;
    glm::mat<2, 3, glm_Float> m23;
    glm::mat<2, 4, glm_Float> m24;
    glm::mat<3, 2, glm_Float> m32;
    glm::mat<3, 3, glm_Float> m33;
    glm::mat<3, 4, glm_Float> m34;
    glm::mat<4, 2, glm_Float> m42;
    glm::mat<4, 3, glm_Float> m43;
    glm::mat<4, 4, glm_Float> m44;
  };
  glm::length_t size;
  glm::length_t secondary;

  glmMatrix() GLM_DEFAULT_CTOR;
  glmMatrix(const glm::mat<2, 2, glm_Float> &_m) : m22(_m), size(2), secondary(2) { }
  glmMatrix(const glm::mat<2, 3, glm_Float> &_m) : m23(_m), size(2), secondary(3) { }
  glmMatrix(const glm::mat<2, 4, glm_Float> &_m) : m24(_m), size(2), secondary(4) { }
  glmMatrix(const glm::mat<3, 2, glm_Float> &_m) : m32(_m), size(3), secondary(2) { }
  glmMatrix(const glm::mat<3, 3, glm_Float> &_m) : m33(_m), size(3), secondary(3) { }
  glmMatrix(const glm::mat<3, 4, glm_Float> &_m) : m34(_m), size(3), secondary(4) { }
  glmMatrix(const glm::mat<4, 2, glm_Float> &_m) : m42(_m), size(4), secondary(2) { }
  glmMatrix(const glm::mat<4, 3, glm_Float> &_m) : m43(_m), size(4), secondary(3) { }
  glmMatrix(const glm::mat<4, 4, glm_Float> &_m) : m44(_m), size(4), secondary(4) { }

  // Assignment Operators

  inline void operator=(const glm::mat<2, 2, glm_Float> &_m) { size = 2; secondary = 2; m22 = _m; }
  inline void operator=(const glm::mat<2, 3, glm_Float> &_m) { size = 2; secondary = 3; m23 = _m; }
  inline void operator=(const glm::mat<2, 4, glm_Float> &_m) { size = 2; secondary = 4; m24 = _m; }
  inline void operator=(const glm::mat<3, 2, glm_Float> &_m) { size = 3; secondary = 2; m32 = _m; }
  inline void operator=(const glm::mat<3, 3, glm_Float> &_m) { size = 3; secondary = 3; m33 = _m; }
  inline void operator=(const glm::mat<3, 4, glm_Float> &_m) { size = 3; secondary = 4; m34 = _m; }
  inline void operator=(const glm::mat<4, 2, glm_Float> &_m) { size = 4; secondary = 2; m42 = _m; }
  inline void operator=(const glm::mat<4, 3, glm_Float> &_m) { size = 4; secondary = 3; m43 = _m; }
  inline void operator=(const glm::mat<4, 4, glm_Float> &_m) { size = 4; secondary = 4; m44 = _m; }

  // Reassignment; glm::mat = glmMatrix.

  inline int Get(glm::mat<2, 2, glm_Float> &_m) const { _m = m22; return 1; }
  inline int Get(glm::mat<2, 3, glm_Float> &_m) const { _m = m23; return 1; }
  inline int Get(glm::mat<2, 4, glm_Float> &_m) const { _m = m24; return 1; }
  inline int Get(glm::mat<3, 2, glm_Float> &_m) const { _m = m32; return 1; }
  inline int Get(glm::mat<3, 3, glm_Float> &_m) const { _m = m33; return 1; }
  inline int Get(glm::mat<3, 4, glm_Float> &_m) const { _m = m34; return 1; }
  inline int Get(glm::mat<4, 2, glm_Float> &_m) const { _m = m42; return 1; }
  inline int Get(glm::mat<4, 3, glm_Float> &_m) const { _m = m43; return 1; }
  inline int Get(glm::mat<4, 4, glm_Float> &_m) const { _m = m44; return 1; }
};

/*
** Pushes a vector of dimensionality of 'd' represented by 'v' onto the stack.
** Returning one on success (i.e., valid dimension argument), zero otherwise.
**
** @NOTE If Lua is compiled with LUA_USE_APICHECK, a runtime error will be
**  thrown instead of returning zero.
*/
LUA_API int glm_pushvec(lua_State *L, const glmVector &v, glm::length_t d);

/*
** Pushes a quaternion represented by 'q' onto the stack. Returning one on
** success, zero otherwise.
**
** @NOTE This function is identical to glm_pushquat, but without the (implicit)
**  conversion.
*/
LUA_API int glm_pushvec_quat(lua_State *L, const glmVector &q);

/*
** Creates a new Matrix object, represented by 'm', and places it onto the stack.
** Returning one on success, zero otherwise (invalid glmMatrix dimensionality).
**
** @NOTE If Lua is compiled with LUA_USE_APICHECK, a runtime error with be
**  thrown instead of returning zero.
*/
LUA_API int glm_pushmat(lua_State *L, const glmMatrix &m);

/* }================================================================== */

/*
** {==================================================================
**  C/CPP boundary-interfacing structures
** ===================================================================
*/
#if defined(LUA_GRIT_API)
#if GLM_HAS_STATIC_ASSERT

  // Additional defensive checks around the aliasing and alignment of lua_Mat4
  // and glmMatrix.

  GLM_STATIC_ASSERT(true
    && sizeof(lua_Float4) == sizeof(glmVector)
    && offsetof(lua_Float4, x) == offsetof(glmVector, v4.x)
    && offsetof(lua_Float4, y) == offsetof(glmVector, v4.y)
    && offsetof(lua_Float4, z) == offsetof(glmVector, v4.z)
    && offsetof(lua_Float4, w) == offsetof(glmVector, v4.w)
    && offsetof(glmVector, v2.x) == offsetof(glmVector, v4.x)
    && offsetof(glmVector, v3.x) == offsetof(glmVector, v4.x), "Inconsistent Structures: lua_Float4 / glm::vec<4, glm_Float>"
  );

  GLM_STATIC_ASSERT(true
    && sizeof(grit_length_t) == sizeof(glm::length_t)
    && sizeof(lua_Mat4) == sizeof(glmMatrix)
    // @TODO: Name the anonymous union in glmMatrix similar to lua_Mat4.
    && sizeof(lua_Mat4::Columns::m2) == sizeof(glmMatrix::m24)
    && sizeof(lua_Mat4::Columns::m3) == sizeof(glmMatrix::m34)
    && sizeof(lua_Mat4::Columns::m4) == sizeof(glmMatrix::m44)
    && offsetof(lua_Mat4, size) == offsetof(glmMatrix, size)
    && offsetof(lua_Mat4, secondary) == offsetof(glmMatrix, secondary)
    && offsetof(lua_Mat4, m.m2) == offsetof(glmMatrix, m24)
    && offsetof(lua_Mat4, m.m3) == offsetof(glmMatrix, m34)
    && offsetof(lua_Mat4, m.m4) == offsetof(glmMatrix, m44), "Inconsistent Structures: lua_Mat4 / glmMatrix"
  );
#endif

#endif
/* }================================================================== */

#endif
