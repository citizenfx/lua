/*
** $Id: bindings.hpp $
**
** Template system designed to relate the various glm types (e.g., scalars, vec,
** quaternions, and matrices) and their functions to Lua operations. This file
** is designed to be compatible with g++, clang++, and MSVC (prior to
** https://docs.microsoft.com/en-us/cpp/preprocessor/preprocessor-experimental-overview?view=vs-2019)
**
** For example, to execute glm::axisAngleMatrix:
**  (1) Ensure the top two elements on a Lua stack are compatible with vec<3>
**      and a numeric type;
**  (2) Retrieve those values from the Lua stack and convert them into the
**      corresponding GLM types;
**  (3) Invoke glm::axisAngleMatrix() for those converted values;
**  (4) Convert the result back into something that is compatible with Lua;
**  (5) Push that value onto the Lua stack.
**
** See Copyright Notice in lua.h
*/
#ifndef __BINDING_LUA_BINDINGS_HPP__
#define __BINDING_LUA_BINDINGS_HPP__
#if !defined(LUA_API_LINKAGE)
  #define LUA_API_LINKAGE "C++"
#endif

#include <string>

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>
#include <glm/gtx/type_trait.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/integer.hpp>

#include <lua.hpp>
#include <lglm.hpp>
extern LUA_API_LINKAGE {
  #include "lgrit_lib.h"
  #include "lapi.h"
  #include "lobject.h"
  #include "lgc.h"
  #include "lstate.h"
  #include "lvm.h"
  #include "lglm_core.h"
}

#if defined(LUA_GLM_GEOM_EXTENSIONS)
  #include "allocator.hpp"
  #include "geom/aabb.hpp"
  #include "geom/line.hpp"
  #include "geom/linesegment.hpp"
  #include "geom/ray.hpp"
  #include "geom/triangle.hpp"
  #include "geom/sphere.hpp"
  #include "geom/plane.hpp"
  #include "geom/polygon.hpp"
#endif

/* Lua Definitions */
#define GLM_NAME(F) glm_##F
#define GLM_NAMESPACE(NAME) glm::NAME

/* Invalid glm structure configurations */
#define GLM_INVALID_VECTOR_TYPE ("invalid " LABEL_VECTOR " type")
#define GLM_INVALID_VECTOR_STRUCTURE ("invalid " LABEL_VECTOR " structure")
#define GLM_INVALID_VECTOR_DIMENSIONS ("invalid " LABEL_VECTOR " dimensions")
#define GLM_INVALID_QUAT_STRUCTURE ("invalid " LABEL_QUATERN " structure")
#define GLM_INVALID_MAT_STRUCTURE ("invalid " LABEL_MATRIX " structure")
#define GLM_INVALID_MAT_DIMENSIONS ("invalid " LABEL_MATRIX " dimensions")
#define GLM_INVALID_MAT_ROW ("unexpected " LABEL_MATRIX " row")
#define GLM_INVALID_MAT_COLUMN ("unexpected " LABEL_MATRIX " column")

/* Metatable name for polygon userdata. */
#define LUA_GLM_POLYGON_META "GLM_POLYGON"

/*
** Matrices in GLM are stored in a column-major format. These macros exists for
** the almost-zero percent change that matrices ever have the option to be
** represented by Lua as a row-major format.
*/
#define lua_matrix_cols(size, secondary) size
#define lua_matrix_rows(size, secondary) secondary
#define gm_cols(M) lua_matrix_cols(mvalue(M).size, mvalue(M).secondary)
#define gm_rows(M) lua_matrix_rows(mvalue(M).size, mvalue(M).secondary)

/* Macro for implicitly handling floating point drift where possible */
#if defined(LUA_GLM_DRIFT)
  #define gm_drift(x) glm::normalize((x))
#else
  #define gm_drift(x) x
#endif

/* lua_gettop() macro */
#if !defined(_gettop)
#define _gettop(L) cast_int((L)->top.p - ((L)->ci->func.p + 1))
#define _isvalid(L, o) (!ttisnil(o) || o != &G(L)->nilvalue)
#endif

/* TValue -> glmVector */
#if !defined(glm_vecvalue)
#define glm_mvalue(o) mvalue(o)
#define glm_vvalue(o) vvalue(o)
#define glm_vecvalue(o) vecvalue(o)
#define glm_quatvalue(o) quatvalue(o)
#endif

/*
** index2value ported from lapi.c. Simplified to only operate on positive stack
** indices; see related function index2stack.
*/
static LUA_INLINE const TValue *glm_i2v(lua_State *L, int idx) {
  const StkId o = L->ci->func.p + idx;
  api_check(L, idx <= L->ci->top.p - (L->ci->func.p + 1), "invalid index");
  return (o >= L->top.p) ? &G(L)->nilvalue : s2v(o);
}

/*
** {==================================================================
** Traits
** ===================================================================
*/

/*
** Common header for Lua Traits.
**
** @NOTE: inlining this declaration will come at a (significant) cost to the
**  size of the shared-object/static-library.
*/
#define LUA_TRAIT_QUALIFIER static GLM_INLINE

/* Trait declaration for non-boolean integral types. */
#define LUA_TRAIT_INT(Name, R) template<typename T> \
LUA_TRAIT_QUALIFIER typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, R>::type Name

/* Trait declaration for floating-point types. */
#define LUA_TRAIT_FLOAT(Name, R) template<typename T> \
LUA_TRAIT_QUALIFIER typename std::enable_if<std::is_floating_point<T>::value, R>::type Name

/// <summary>
/// Forward declare (function) parameter trait.
/// </summary>
template<typename T>
struct gLuaTrait;

/// <summary>
/// A structure that interfaces with an active Lua state.
///
/// This structure serves two purposes:
/// (1) A simple stack iterator state.
/// (2) Uses SFINAE for (static) "Push" and "Pull" operations:
///     Pull: Converts the Lua value(s) at, or starting at, the given Lua stack
///       index to the C/GLM type.
///
///     Push: Pushes a value (or values) on top of the Lua stack that represent
///       the GLM object; returning the number of values placed onto the stack.
///
/// A benefit to this approach is to allow the quick creation of geometric
/// structures that does not require additional userdata/metatable definitions.
///
/// @TODO:
///   1. An interface for generating random numbers to replace std::rand(). This
///     allows gLuaBase to invoke math_random instead of having to maintain
///     multiple random states.
///
///   2. Consider: https://en.cppreference.com/w/cpp/numeric/random
/// </summary>
struct gLuaBase {
  lua_State *L;  // Current lua state.
  int idx;  // Iteration pointer.
  int ltop;  // Number of function parameters (lazily cached)

  gLuaBase(lua_State *baseL, int baseIdx = 1)
    : L(baseL), idx(baseIdx), ltop(0) {
  }

  /// <summary>
  /// Lazy catching lua_gettop
  /// </summary>
  GLM_INLINE int top() {
    return (ltop == 0) ? ((ltop = _gettop(L))) : ltop;
  }

  /// <summary>
  /// Reset the iterator
  /// </summary>
  GLM_INLINE gLuaBase &reset() {
    idx = 1;
    return *this;
  }

  /// <summary>
  /// Invalid the pointer index, i.e., set it greater than top.
  /// </summary>
  GLM_INLINE void invalidate() {
    idx = top() + 1;
  }

  /// <summary>
  /// Temporary math.random() hook
  ///
  /// @TODO: Remove this function when math.random/std:rand functions are unified
  /// and have proper bindings.
  /// </summary>
  lua_Number rand() {
    const int t = top();  // Get cached top;
    lua_checkstack(L, 3);
    if (lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE) == LUA_TTABLE) {  // [..., load_tab]
      if (lua_getfield(L, -1, LUA_MATHLIBNAME) == LUA_TTABLE) {  // [..., load_tab, math_tab]
        if (lua_getfield(L, -1, "random") == LUA_TFUNCTION) {  // [..., load_tab, math_tab, rand_func]
          lua_call(L, 0, 1);
          lua_Number result = lua_tonumber(L, -1);  // [..., load_tab, math_tab, result]
          lua_pop(L, 3);
          return result;
        }
      }
    }

    lua_pop(L, _gettop(L) - t);  // Fallback to std::rand() if lmathlib has not been loaded
    return cast_num(std::rand()) / cast_num((RAND_MAX));
  }

  /// <summary>
  /// Return true if the current iteration pointer references a valid, and
  /// recyclable, data structure.
  /// </summary>
  GLM_INLINE bool can_recycle() {
#if defined(LUA_GLM_RECYCLE)
    return (idx < 0 || idx <= top());
#else
    return false;
#endif
  }

  /// <summary>
  /// Push(gLuaBase) wrapper
  /// </summary>
  template<typename T>
  LUA_TRAIT_QUALIFIER int Push(lua_State *L, const T &v) {
    gLuaBase _LB(L, _gettop(L) + 1);
    return gLuaBase::Push(_LB, v);
  }

  /// <summary>
  /// Pull(gLuaBase) wrapper
  /// </summary>
  template<typename T>
  LUA_TRAIT_QUALIFIER int Pull(lua_State *L, int idx_, T &v) {
    gLuaBase _LB(L, _gettop(L) + 1);
    return gLuaBase::Pull(_LB, idx_, v);
  }

  /* scalar types */

  /// <summary>
  /// Pushes a nil value onto the stack.
  /// </summary>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB) {
    luaL_pushfail(LB.L);
    return 1;
  }

  /* Boolean */

  /// <summary>
  /// Returns true if the value at the given index is a boolean, and 0 otherwise.
  /// </summary>
  template<typename T>
  LUA_TRAIT_QUALIFIER typename std::enable_if<std::is_same<T, bool>::value, bool>::type Is(const gLuaBase &LB, int idx_) {
    return lua_isboolean(LB.L, idx_);
  }

  /// <summary>
  /// Converts the Lua value at the given index to a C boolean value (0 or 1).
  /// </summary>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, bool &v) {
    v = static_cast<bool>(lua_toboolean(LB.L, idx_));
    return 1;
  }

  /// <summary>
  /// Pushes a boolean value with value b onto the stack.
  /// </summary>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, bool b) {
    lua_pushboolean(LB.L, b);
    return 1;
  }

  /* Integer */

  /// <summary>
  /// lua_tointeger with additional rules for casting booleans.
  /// </summary>
  template<typename T>
  static int tointegerx(lua_State *L_, int idx_, T &v) {
    const TValue *o = glm_i2v(L_, idx_);
    switch (ttypetag(o)) {
      case LUA_VTRUE: v = static_cast<T>(1); break;
      case LUA_VFALSE: v = static_cast<T>(0); break;
      case LUA_VNUMINT: v = static_cast<T>(ivalue(o)); break;
      case LUA_VNUMFLT: v = static_cast<T>(fltvalue(o)); break;
      default: {
        v = static_cast<T>(luaL_checkinteger(L_, idx_));
        break;
      }
    }
    return 1;
  }

  /// <summary>
  /// Returns true if the value at the given index is an integer (i.e., a number
  /// and is represented as an integer); false otherwise.
  /// </summary>
  LUA_TRAIT_INT(Is, bool)(const gLuaBase &LB, int idx_) {
    return lua_isinteger(LB.L, idx_);
  }

  /// <summary>
  /// Converts the value at the given index to a lua_Integer. Afterwards, that
  /// value is casted into the integer declaration.
  /// </summary>
  LUA_TRAIT_INT(Pull, int)(const gLuaBase &LB, int idx_, T &v) {
    return tointegerx<T>(LB.L, idx_, v);
  }

  /// <summary>
  /// Pushes integer-type with value 'v' (casted to a lua_Integer) onto the stack.
  /// </summary>
  LUA_TRAIT_INT(Push, int)(const gLuaBase &LB, T v) {
    lua_pushinteger(LB.L, static_cast<lua_Integer>(v));
    return 1;
  }

  /* Float */

  /// <summary>
  /// lua_tonumber with additional rules for casting booleans
  /// </summary>
  template<typename T>
  static int tonumberx(lua_State *L_, int idx_, T &v) {
    const TValue *o = glm_i2v(L_, idx_);
    switch (ttypetag(o)) {
      case LUA_VTRUE: v = static_cast<T>(1); break;
      case LUA_VFALSE: v = static_cast<T>(0); break;
      case LUA_VNUMINT: v = static_cast<T>(ivalue(o)); break;
      case LUA_VNUMFLT: v = static_cast<T>(fltvalue(o)); break;
      // string coercion must exist for this binding to be a superset of lmathlib.
      // As much of the luaL_checknumber logic is redundant, this should be optimized.
      // However, luaV_tonumber_ is not an exported function.
      default: {
        v = static_cast<T>(luaL_checknumber(L_, idx_));
        break;
      }
    }
    return 1;
  }

  /// <summary>
  /// Returns true if the value at the given index is a number, or a string
  /// convertible to a number; false otherwise.
  /// </summary>
  LUA_TRAIT_FLOAT(Is, bool)(const gLuaBase &LB, int idx_) {
    return lua_isnumber(LB.L, idx_);
  }

  /// <summary>
  /// Converts the value at the given index to a lua_Number. Afterwards, that
  /// value is casted into the float declaration.
  /// </summary>
  LUA_TRAIT_FLOAT(Pull, int)(const gLuaBase &LB, int idx_, T &v) {
    return tonumberx<T>(LB.L, idx_, v);
  }

  /// <summary>
  /// Pushes floating-point-type with value 'v' (casted to a lua_Number) onto the stack.
  /// </summary>
  LUA_TRAIT_FLOAT(Push, int)(const gLuaBase &LB, T v) {
    lua_pushnumber(LB.L, static_cast<lua_Number>(v));
    return 1;
  }

  /// <summary>
  /// Attempt to push the number as an integer; falling back to number otherwise
  /// </summary>
  LUA_TRAIT_QUALIFIER int PushNumInt(const gLuaBase &LB, lua_Number d) {
    lua_Integer n;
    if (lua_numbertointeger(d, &n)) /* does 'd' fit in an integer? */
      lua_pushinteger(LB.L, n); /* result is integer */
    else
      lua_pushnumber(LB.L, d); /* result is float */
    return 1;
  }

  /// <summary>
  /// Attempt to push the vector as an glm::ivec; falling back to glm::vec otherwise.
  ///
  /// This function exists for future-proofing.
  /// </summary>
  template<glm::length_t L, typename T>
  LUA_TRAIT_QUALIFIER int PushNumInt(const gLuaBase &LB, const glm::vec<L, T> &v) {
    return Push(LB, v);
  }

  /* String */

  /// <summary>
  /// Returns true if the value at the given index is a string; false otherwise.
  ///
  /// @NOTE: lua_isstring will also return true for "cvt2str", which is not
  /// desired for this API.
  /// </summary>
  template<typename T>
  LUA_TRAIT_QUALIFIER typename std::enable_if<std::is_same<T, const char *>::value, bool>::type Is(const gLuaBase &LB, int idx_) {
    return lua_type(LB.L, idx_) == LUA_TSTRING;
  }

  /// <summary>
  /// Converts the Lua value at the given index to a C string. If len is not
  /// NULL, this function will also set *len with the strings length.
  /// </summary>
  /// <returns></returns>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, const char *&v, size_t *len = GLM_NULLPTR) {
    v = lua_tolstring(LB.L, idx_, len);
    return 1;
  }

  /// <summary>
  /// Pushes a zero-terminated string pointed to by s onto the stack.
  /// </summary>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, const char *&s) {
    lua_pushstring(LB.L, s);
    return 1;
  }

  /* vec<, float, > */

  LUA_TRAIT_FLOAT(Pull, int)(const gLuaBase &LB, int idx_, glm::vec<1, T> &v) {
    v.x = static_cast<T>(luaL_checknumber(LB.L, idx_));
    return 1;
  }

  LUA_TRAIT_INT(Pull, int)(const gLuaBase &LB, int idx_, glm::vec<1, T> &v) {
    v.x = static_cast<T>(luaL_checkinteger(LB.L, idx_));
    return 1;
  }

  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<1, bool> &v) {
    v.x = static_cast<bool>(lua_toboolean(LB.L, idx_));
    return 1;
  }

  LUA_TRAIT_FLOAT(Push, int)(const gLuaBase &LB, const glm::vec<1, T> &v) {
    lua_pushnumber(LB.L, static_cast<lua_Number>(v.x));
    return 1;
  }

  LUA_TRAIT_INT(Push, int)(const gLuaBase &LB, const glm::vec<1, T> &v) {
    lua_pushinteger(LB.L, static_cast<lua_Integer>(v.x));
    return 1;
  }

  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, const glm::vec<1, bool> &v) {
    return gLuaBase::Push(LB, v.x);
  }

  /// <summary>
  /// Convert one-or-more Lua values starting at idx into a suitable glm::vec<>
  /// structure; returning the number of stack values consumed to populate the
  /// vector.
  /// </summary>

  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<2, glm_Float> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector2(o))) {
      v = glm_vecvalue(o).v2;
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR2);
  }

  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<3, glm_Float> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector3(o))) {
      v = glm_vecvalue(o).v3;
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR3);
  }

  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<4, glm_Float> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector4(o))) {
      v = glm_vecvalue(o).v4;
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR4);
  }

  template<typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<2, T> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector2(o))) {
      const glm::vec<2, glm_Float> &_v = glm_vecvalue(o).v2;
      v = cast_vec2(_v, T);
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR2);
  }

  template<typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<3, T> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector3(o))) {
      const glm::vec<3, glm_Float> &_v = glm_vecvalue(o).v3;
      v = cast_vec3(_v, T);
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR3);
  }

  template<typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::vec<4, T> &v) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisvector4(o))) {
      const glm::vec<4, glm_Float> &_v = glm_vecvalue(o).v4;
      v = cast_vec4(_v, T);
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_VECTOR4);
  }

  template<glm::length_t L, typename T>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, const glm::vec<L, T> &v) {
    return glm_pushvec(LB.L, glmVector(v), L);
  }

  /* qua<?> */

  /// <summary>
  /// Convert one-or-more Lua values starting at idx into a suitable glm::qua<>
  /// structure.
  /// </summary>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase LB, int idx_, glm::qua<glm_Float> &q) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttisquat(o))) {
      q = gm_drift(glm_quatvalue(o).q);
      return 1;
    }
    return luaL_typeerror(LB.L, idx_, LABEL_QUATERN);
  }

  /// <summary>
  /// Convert the provided glm::qua into a Lua suitable value(s).
  /// </summary>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, const glm::qua<glm_Float> &q) {
    return glm_pushquat(LB.L, gm_drift(q));
  }

  /* mat<C, ?> */

  template<glm::length_t C, glm::length_t R>
  static int Pull(const gLuaBase &LB, int idx_, glm::mat<C, R, glm_Float> &m) {
    const TValue *o = glm_i2v(LB.L, idx_);
    if (l_likely(ttismatrix(o))) {
      const glmMatrix &mat = glm_mvalue(o);
      if (lua_matrix_cols(mat.size, mat.secondary) == C && lua_matrix_rows(mat.size, mat.secondary) == R)
        return mat.Get(m);
    }
    return luaL_error(LB.L, GLM_INVALID_MAT_STRUCTURE);
  }

  template<glm::length_t C, glm::length_t R>
  static int Push(gLuaBase &LB, const glm::mat<C, R, glm_Float> &m) {
    if (LB.can_recycle()) {
      lua_State *L_ = LB.L;

      lua_lock(L_);
      const TValue *o = glm_i2v(L_, LB.idx);
      if (ttismatrix(o)) {
        LB.idx++;

        glm_mvalue(o) = m;
        setobj2s(L_, L_->top.p, o); // lua_pushvalue
        api_incr_top(L_);
        lua_unlock(L_);
        return 1;
      }
      lua_unlock(L_);
    }

#if defined(LUA_GLM_FORCED_RECYCLE)
    /* This library allocating memory is verboten! */
    return luaL_error(LB.L, "library configured to not allocate additional memory; use recycling mechanisms")
#else
    return glm_pushmat(LB.L, glmMatrix(m));
#endif
  }

#if defined(LUA_GLM_GEOM_EXTENSIONS)
  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::AABB<D, T> &v) {
    Pull(LB, idx_, v.minPoint);
    Pull(LB, idx_ + 1, v.maxPoint);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::AABB<D, T> &v) {
    Push(LB, v.minPoint);
    Push(LB, v.maxPoint);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Line<D, T> &l) {
    Pull(LB, idx_, l.pos);
    Pull(LB, idx_ + 1, l.dir);
#if defined(LUA_GLM_DRIFT)
    l.dir = gm_drift(l.dir);
#endif
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::Line<D, T> &l) {
    Push(LB, l.pos);
    Push(LB, gm_drift(l.dir));
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::LineSegment<D, T> &l) {
    Pull(LB, idx_, l.a);
    Pull(LB, idx_ + 1, l.b);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::LineSegment<D, T> &l) {
    Push(LB, l.a);
    Push(LB, l.b);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Ray<D, T> &r) {
    Pull(LB, idx_, r.pos);
    Pull(LB, idx_ + 1, r.dir);
#if defined(LUA_GLM_DRIFT)
    r.dir = gm_drift(r.dir);
#endif
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::Ray<D, T> &r) {
    Push(LB, r.pos);
    Push(LB, gm_drift(r.dir));
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Triangle<D, T> &t) {
    Pull(LB, idx_, t.a);
    Pull(LB, idx_ + 1, t.b);
    Pull(LB, idx_ + 2, t.c);
    return 3;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::Triangle<D, T> &t) {
    Push(LB, t.a);
    Push(LB, t.b);
    Push(LB, t.c);
    return 3;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Sphere<D, T> &s) {
    Pull(LB, idx_, s.pos);
    Pull(LB, idx_ + 1, s.r);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::Sphere<D, T> &s) {
    Push(LB, s.pos);
    Push(LB, s.r);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Plane<D, T> &p) {
    Pull(LB, idx_, p.normal);
    Pull(LB, idx_ + 1, p.d);
    return 2;
  }

  template<glm::length_t D, typename T>
  LUA_TRAIT_QUALIFIER int Push(gLuaBase &LB, const glm::Plane<D, T> &p) {
    Push(LB, p.normal);
    Push(LB, p.d);
    return 2;
  }

  template<typename T>
  LUA_TRAIT_QUALIFIER int Pull(const gLuaBase &LB, int idx_, glm::Polygon<3, T> &p) {
    void *ptr = GLM_NULLPTR;
    if (idx_ <= 0)
      return luaL_error(LB.L, "Invalid PolygonPull operation; incorrect API usage");
    else if ((ptr = luaL_checkudata(LB.L, idx_, LUA_GLM_POLYGON_META)) == GLM_NULLPTR)
      return luaL_error(LB.L, "Invalid PolygonPull operation; not userdata");

    p = *(reinterpret_cast<glm::Polygon<3, T> *>(ptr));
    p.stack_idx = idx_;
    return 1;
  }

  /// <summary>
  /// All mutable operations mutate the referenced Polygon userdata; simply
  /// push that userdata back onto the Lua stack.
  /// </summary>
  template<typename T>
  LUA_TRAIT_QUALIFIER int Push(const gLuaBase &LB, const glm::Polygon<3, T> &p) {
    if (p.stack_idx >= 1) {
      lua_pushvalue(LB.L, p.stack_idx);
      return 1;
    }

    // This code-path is not implemented for the time being. All polygons must
    // already exist on the Lua stack. Otherwise polygon_new will need to be
    // duplicated here.
    return luaL_error(LB.L, "not implemented");
  }
#endif
};

/* }================================================================== */

/*
** {==================================================================
** Specializations
** ===================================================================
*/

#define AS_TYPE(...) VA_NARGS_CALL_OVERLOAD(AS_TYPE, __VA_ARGS__)
#define AS_TYPE1(Tr) Tr::as_type<>
#define AS_TYPE2(Tr, Type) Tr::as_type<Type>

template<typename V, typename T = V>
struct gTraitCommon;

template<typename T>
struct gLuaTrait;

/// <summary>
/// Shared functions for parsing types from the Lua stack.
/// </summary>
template<typename V, typename T>
struct gLuaSharedTrait : glm::type<T> {
  using type = T;
  using value_type = V;

  /// <summary>
  /// Empty-initialized
  /// </summary>
  LUA_TRAIT_QUALIFIER GLM_CONSTEXPR T zero() {
    return T(0);
  }

  /// <summary>
  /// Return true if the value starting at "idx" on the Lua stack corresponds
  /// to this type.
  /// </summary>
  LUA_TRAIT_QUALIFIER bool Is(const gLuaBase &LB, int idx) {
    return gLuaBase::Is<T>(LB, idx);
  }

  /// <summary>
  /// Return a descriptive parameter literal for debugging/error messaging.
  /// </summary>
  static GLM_CONSTEXPR const char *Label() {
    GLM_IF_CONSTEXPR (std::is_same<T, bool>::value) return "bool";
    else GLM_IF_CONSTEXPR (std::is_same<T, const char *>::value) return "string";
    else GLM_IF_CONSTEXPR (std::is_integral<T>::value) return LABEL_INTEGER;
    else GLM_IF_CONSTEXPR (std::is_floating_point<T>::value) return LABEL_NUMBER;
    return "Unknown_Type";
  }

  /// <summary>
  /// Given a current stack state, create a GLM object corresponding to the
  /// "type" this trait is capturing.
  ///
  /// @NOTE: This function will be invoked for each GLM bound parameter.
  ///   Force inlining by default (the compiler should be making this decision
  ///   anyway) will significantly increase the size of the shared object.
  ///   Instead, this function is beholden to GLM_FORCE_INLINE which is a
  ///   compile-time toggle.
  /// </summary>
  static GLM_INLINE const T Next(gLuaBase &LB) {
    T v = gLuaTrait<T>::zero();
    LB.idx += gLuaBase::Pull(LB, LB.idx, v);
    return v;
  }
};

template<typename T>
struct gLuaTrait : gLuaSharedTrait<T, T> {
  template<typename Type = T> using as_type = gLuaTrait<Type>;
};

template<glm::length_t D, typename T>
struct gLuaTrait<glm::vec<D, T>> : gLuaSharedTrait<T, glm::vec<D, T>> {
  template<typename Type = T>
  using as_type = gLuaTrait<glm::vec<D, Type>>;

  template<glm::length_t R>
  using mul_type = gLuaTrait<glm::mat<R, D, T>>;
  using row_type = gLuaTrait<glm::vec<D, T>>;

  LUA_TRAIT_QUALIFIER GLM_CONSTEXPR glm::vec<D, T> zero() { return glm::vec<D, T>(T(0)); }
  LUA_TRAIT_QUALIFIER bool Is(const gLuaBase &LB, int idx) { return glm_vector_length(LB.L, idx) == D; }
  static GLM_CONSTEXPR const char *Label() {
    switch (D) {
      case 1: return LABEL_VECTOR1;
      case 2: return LABEL_VECTOR2;
      case 3: return LABEL_VECTOR3;
      case 4: return LABEL_VECTOR4;
      default:
        return LABEL_VECTOR;
    }
  }
};

template<typename T>
struct gLuaTrait<glm::qua<T>> : gLuaSharedTrait<T, glm::qua<T>> {
  LUA_TRAIT_QUALIFIER bool Is(const gLuaBase &LB, int idx) { return glm_isquat(LB.L, idx); }
  LUA_TRAIT_QUALIFIER GLM_CONSTEXPR glm::qua<T> zero() { return glm::quat_identity<glm_Float, glm::defaultp>(); }
  static GLM_CONSTEXPR const char *Label() { return LABEL_QUATERN; }
};

template<glm::length_t C, glm::length_t R, typename T>
struct gLuaTrait<glm::mat<C, R, T>> : gLuaSharedTrait<T, glm::mat<C, R, T>> {
  template<glm::length_t RNext>
  using mul_type = gLuaTrait<glm::mat<RNext, C, T>>;
  using col_type = gLuaTrait<glm::vec<R, T>>;
  using row_type = gLuaTrait<glm::vec<C, T>>;

  LUA_TRAIT_QUALIFIER GLM_CONSTEXPR glm::mat<C, R, T> zero() { return glm::mat<C, R, T>(T(0)); }
  LUA_TRAIT_QUALIFIER bool Is(const gLuaBase &LB, int idx) {
    glm::length_t size = 0, secondary = 0;
    return glm_ismatrix(LB.L, idx, size, secondary)
           && lua_matrix_cols(size, secondary) == C
           && lua_matrix_rows(size, secondary) == R;
  }

  static GLM_CONSTEXPR const char *Label() {
    switch (lua_matrix_cols(C, R)) {
      case 2: {
        switch (lua_matrix_rows(C, R)) {
          case 2: return LABEL_MATRIX "2x2";
          case 3: return LABEL_MATRIX "2x3";
          case 4: return LABEL_MATRIX "2x4";
          default:
            break;
        }
        break;
      }
      case 3: {
        switch (lua_matrix_rows(C, R)) {
          case 2: return LABEL_MATRIX "3x2";
          case 3: return LABEL_MATRIX "3x3";
          case 4: return LABEL_MATRIX "3x4";
          default:
            break;
        }
        break;
      }
      case 4: {
        switch (lua_matrix_rows(C, R)) {
          case 2: return LABEL_MATRIX "4x2";
          case 3: return LABEL_MATRIX "4x3";
          case 4: return LABEL_MATRIX "4x4";
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
    return LABEL_MATRIX;
  }
};

using gLuaFloat = gLuaTrait<glm_Float>;
using gLuaNumber = gLuaTrait<glm_Number>;
using gLuaInteger = gLuaTrait<lua_Integer>;

template<typename T = glm_Float> using gLuaVec1 = gLuaTrait<glm::vec<1, T>>;
template<typename T = glm_Float> using gLuaVec2 = gLuaTrait<glm::vec<2, T>>;
template<typename T = glm_Float> using gLuaVec3 = gLuaTrait<glm::vec<3, T>>;
template<typename T = glm_Float> using gLuaVec4 = gLuaTrait<glm::vec<4, T>>;
template<typename T = glm_Float> using gLuaQuat = gLuaTrait<glm::qua<T>>;

template<typename T = glm_Float> using gLuaMat2x2 = gLuaTrait<glm::mat<2, 2, T>>;
template<typename T = glm_Float> using gLuaMat2x3 = gLuaTrait<glm::mat<2, 3, T>>;
template<typename T = glm_Float> using gLuaMat2x4 = gLuaTrait<glm::mat<2, 4, T>>;
template<typename T = glm_Float> using gLuaMat3x2 = gLuaTrait<glm::mat<3, 2, T>>;
template<typename T = glm_Float> using gLuaMat3x3 = gLuaTrait<glm::mat<3, 3, T>>;
template<typename T = glm_Float> using gLuaMat3x4 = gLuaTrait<glm::mat<3, 4, T>>;
template<typename T = glm_Float> using gLuaMat4x2 = gLuaTrait<glm::mat<4, 2, T>>;
template<typename T = glm_Float> using gLuaMat4x3 = gLuaTrait<glm::mat<4, 3, T>>;
template<typename T = glm_Float> using gLuaMat4x4 = gLuaTrait<glm::mat<4, 4, T>>;

/// <summary>
/// Specialization for implicitly normalizing direction vectors/quaternions.
/// </summary>
#if defined(LUA_GLM_DRIFT)
template<glm::length_t L, typename T = glm_Float>
struct gLuaDir : gLuaTrait<glm::vec<L, T>> {
  LUA_TRAIT_QUALIFIER glm::vec<L, T> Next(gLuaBase &LB) {
    return gm_drift(gLuaTrait<glm::vec<L, T>>::Next(LB));
  }
};
template<typename T = glm_Float> using gLuaDir2 = gLuaDir<2, T>;
template<typename T = glm_Float> using gLuaDir3 = gLuaDir<3, T>;
#else
template<typename T = glm_Float> using gLuaDir2 = gLuaTrait<glm::vec<2, T>>;
template<typename T = glm_Float> using gLuaDir3 = gLuaTrait<glm::vec<3, T>>;
#endif

/// <summary>
/// epsilon specialization for floating point types.
/// </summary>
template<typename T = glm_Float>
struct gLuaEps : gLuaTrait<T> {
  static GLM_CONSTEXPR const char *Label() { return "epsilon"; }
  LUA_TRAIT_QUALIFIER bool Is(const gLuaBase &LB, int idx) { return lua_isnoneornil(LB.L, idx) || gLuaTrait<T>::Is(LB, idx); }
  LUA_TRAIT_QUALIFIER T Next(gLuaBase &LB) {
    if (lua_isnoneornil(LB.L, LB.idx)) {
      LB.idx++;  // Skip the argument
      return glm::epsilon<T>();
    }
    return gLuaTrait<T>::Next(LB);
  }
};

/* }================================================================== */

/*
** {==================================================================
** Traits Functions.
** ===================================================================
*/

/*
** Generic Traits Operation:
**  (1) Extract data from Lua stack and convert into some glm suitable structure
**  (2) Execute the bound glm function: "R = F(...)"
**  (3) Convert the function result back into something suitable for Lua and it
**      onto the Lua stack: "gLuaBase::Push(LB, ...)"
**
** @NOTE: Must consider the evaluation of function arguments.
*/

#define _VA_NARGS_GLUE(x, y) x y
#define _VA_NARGS_RETURN_COUNT(_1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10_, count, ...) count
#define _VA_NARGS_EXPAND(args) _VA_NARGS_RETURN_COUNT args
#define _VA_NARGS_COUNT_MAX(...) _VA_NARGS_EXPAND((__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define _VA_NARGS_COUNT 10

#define _VA_NARGS_OVERLOAD_MACRO2(name, count) name##count
#define _VA_NARGS_OVERLOAD_MACRO1(name, count) _VA_NARGS_OVERLOAD_MACRO2(name, count)
#define _VA_NARGS_OVERLOAD_MACRO(name, count) _VA_NARGS_OVERLOAD_MACRO1(name, count)
#define VA_NARGS_CALL_OVERLOAD(name, ...) _VA_NARGS_GLUE(_VA_NARGS_OVERLOAD_MACRO(name, _VA_NARGS_COUNT_MAX(__VA_ARGS__)), (__VA_ARGS__))

/* Mapping Lua stack values to function parameters */
#define TRAITS_FUNC(...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, __VA_ARGS__)

/* fail */
#define TRAITS_FUNC1(LB, F) \
  return gLuaBase::Push(LB)

/* F() */
#define TRAITS_FUNC2(LB, F) \
  return gLuaBase::Push(LB, F())

/* F(a) */
#define TRAITS_FUNC3(LB, F, A)               \
  LUA_MLM_BEGIN                              \
  return gLuaBase::Push(LB, F(A::Next(LB))); \
  LUA_MLM_END

/* F(a, b) */
#define TRAITS_FUNC4(LB, F, A, B)         \
  LUA_MLM_BEGIN                           \
  const A::type __a = A::Next(LB);        \
  const B::type __b = B::Next(LB);        \
  return gLuaBase::Push(LB, F(__a, __b)); \
  LUA_MLM_END

/* F(a, b, c)) */
#define TRAITS_FUNC5(LB, F, A, B, C)           \
  LUA_MLM_BEGIN                                \
  const A::type __a = A::Next(LB);             \
  const B::type __b = B::Next(LB);             \
  const C::type __c = C::Next(LB);             \
  return gLuaBase::Push(LB, F(__a, __b, __c)); \
  LUA_MLM_END

/* F(a, b, c, d) */
#define TRAITS_FUNC6(LB, F, A, B, C, D) \
  LUA_MLM_BEGIN                         \
  const A::type __a = A::Next(LB);      \
  const B::type __b = B::Next(LB);      \
  const C::type __c = C::Next(LB);      \
  const D::type __d = D::Next(LB);      \
  return gLuaBase::Push(LB, F(          \
    __a, __b, __c, __d                  \
  ));                                   \
  LUA_MLM_END

/* F(a, b, c, d, e) */
#define TRAITS_FUNC7(LB, F, A, B, C, D, E) \
  LUA_MLM_BEGIN                            \
  const A::type __a = A::Next(LB);         \
  const B::type __b = B::Next(LB);         \
  const C::type __c = C::Next(LB);         \
  const D::type __d = D::Next(LB);         \
  const E::type __e = E::Next(LB);         \
  return gLuaBase::Push(LB, F(             \
    __a, __b, __c, __d, __e                \
  ));                                      \
  LUA_MLM_END

/* F(a, b, c, d, e, g) */
#define TRAITS_FUNC8(LB, F, A, B, C, D, E, G) \
  LUA_MLM_BEGIN                               \
  const A::type __a = A::Next(LB);            \
  const B::type __b = B::Next(LB);            \
  const C::type __c = C::Next(LB);            \
  const D::type __d = D::Next(LB);            \
  const E::type __e = E::Next(LB);            \
  const G::type __g = G::Next(LB);            \
  return gLuaBase::Push(LB, F(                \
    __a, __b, __c, __d, __e, __g              \
  ));                                         \
  LUA_MLM_END

/* F(a, b, c, d, e, g, h) */
#define TRAITS_FUNC9(LB, F, A, B, C, D, E, G, H) \
  LUA_MLM_BEGIN                                  \
  const A::type __a = A::Next(LB);               \
  const B::type __b = B::Next(LB);               \
  const C::type __c = C::Next(LB);               \
  const D::type __d = D::Next(LB);               \
  const E::type __e = E::Next(LB);               \
  const G::type __g = G::Next(LB);               \
  const H::type __h = H::Next(LB);               \
  return gLuaBase::Push(LB, F(                   \
    __a, __b, __c, __d, __e, __g, __h            \
  ));                                            \
  LUA_MLM_END

/* F(a, b, c, d, e, g, h, i) */
#define TRAITS_FUNC10(LB, F, A, B, C, D, E, G, H, I) \
  LUA_MLM_BEGIN                                      \
  const A::type __a = A::Next(LB);                   \
  const B::type __b = B::Next(LB);                   \
  const C::type __c = C::Next(LB);                   \
  const D::type __d = D::Next(LB);                   \
  const E::type __e = E::Next(LB);                   \
  const G::type __g = G::Next(LB);                   \
  const H::type __h = H::Next(LB);                   \
  const I::type __i = I::Next(LB);                   \
  return gLuaBase::Push(LB, F(                       \
    __a, __b, __c, __d, __e, __g, __h, __i           \
  ));                                                \
  LUA_MLM_END

/*
** Place values onto the Lua stack in an order-of-evaluation independent
** fashion; returning the number of values placed onto the Lua stack.
*/
#define TRAITS_PUSH(...) VA_NARGS_CALL_OVERLOAD(TRAITS_PUSH, __VA_ARGS__)

#define TRAITS_PUSH1(LB) \
  return gLuaBase::Push(LB)

#define TRAITS_PUSH2(LB, A) \
  return gLuaBase::Push(LB, (A))

#define TRAITS_PUSH3(LB, A, B)             \
  LUA_MLM_BEGIN                            \
  const int __a = gLuaBase::Push(LB, (A)); \
  const int __b = gLuaBase::Push(LB, (B)); \
  return __a + __b;                        \
  LUA_MLM_END

#define TRAITS_PUSH4(LB, A, B, C)          \
  LUA_MLM_BEGIN                            \
  const int __a = gLuaBase::Push(LB, (A)); \
  const int __b = gLuaBase::Push(LB, (B)); \
  const int __c = gLuaBase::Push(LB, (C)); \
  return __a + __b + __c;                  \
  LUA_MLM_END

#define TRAITS_PUSH5(LB, A, B, C, D)       \
  LUA_MLM_BEGIN                            \
  const int __a = gLuaBase::Push(LB, (A)); \
  const int __b = gLuaBase::Push(LB, (B)); \
  const int __c = gLuaBase::Push(LB, (C)); \
  const int __d = gLuaBase::Push(LB, (D)); \
  return __a + __b + __c + __d;            \
  LUA_MLM_END

#define TRAITS_PUSH6(LB, A, B, C, D, E)    \
  LUA_MLM_BEGIN                            \
  const int __a = gLuaBase::Push(LB, (A)); \
  const int __b = gLuaBase::Push(LB, (B)); \
  const int __c = gLuaBase::Push(LB, (C)); \
  const int __d = gLuaBase::Push(LB, (D)); \
  const int __e = gLuaBase::Push(LB, (E)); \
  return __a + __b + __c + __d + __e;      \
  LUA_MLM_END

/* }================================================================== */

/*
** {==================================================================
** Argument Layouts
** ===================================================================
*/

/*
** Argument Layout: In most cases, the first argument to a glm::function is
** sufficient in template argument deduction. Moreover, that argument, or that
** trait, is often repeated. For example, consider a binary operation glm::dot.
** in which the vector/quaternion trait/type is repeated once.
**
** LAYOUT_*(F, Traits, ...) are defined by:
**    F - the glm::function being wrapped.
**    Tr - the first/deducing argument trait.
**    ... - Any trailing traits (types) consistent across all templates of the
**          same glm::function.
*/

/* Trait repetition */
#define LAYOUT_UNARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, ##__VA_ARGS__)
#define LAYOUT_BINARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, ##__VA_ARGS__)
#define LAYOUT_TERNARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, ##__VA_ARGS__)
#define LAYOUT_QUATERNARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, Tr, ##__VA_ARGS__)
#define LAYOUT_QUINARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, Tr, Tr, ##__VA_ARGS__)
#define LAYOUT_SENARY(LB, F, Tr, ...) VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, Tr, Tr, Tr, ##__VA_ARGS__)

/* trait + eps op */
#define LAYOUT_BINARY_EPS(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, gLuaEps<Tr::value_type>, ##__VA_ARGS__)

/* trait + trait::primitive op */
#define LAYOUT_BINARY_SCALAR(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__)

/* trait + trait + eps op */
#define LAYOUT_TERNARY_EPS(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, gLuaEps<Tr::value_type>, ##__VA_ARGS__)

/* trait + trait + trait::primitive op */
#define LAYOUT_TERNARY_SCALAR(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__)

/* trait + trait + trait + trait + trait::primitive op */
#define LAYOUT_QUINARY_SCALAR(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__)

/* trait + trait<int> op */
#define LAYOUT_VECTOR_INT(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr::as_type<int>, ##__VA_ARGS__)

/* trait + trait + trait + trait::primitive + trait::primitive op */
#define LAYOUT_BARYCENTRIC(LB, F, Tr, ...) \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, gLuaTrait<Tr::value_type>, gLuaTrait<Tr::value_type>, ##__VA_ARGS__)

/* unary or binary operator depending on the state of the Lua stack */
#define LAYOUT_UNARY_OR_BINARY(LB, F, Tr, ...)                       \
  LUA_MLM_BEGIN                                                      \
  if (lua_isnoneornil((LB).L, (LB).idx + 1))                         \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, ##__VA_ARGS__);   \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, ##__VA_ARGS__); \
  LUA_MLM_END

/* trait + {nil || trait::primitive} op */
#define LAYOUT_UNARY_OPTIONAL(LB, F, Tr, ...)                                               \
  LUA_MLM_BEGIN                                                                             \
  if (lua_isnoneornil((LB).L, (LB).idx + 1))                                                \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, ##__VA_ARGS__);                          \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__); \
  LUA_MLM_END

/* unary or ternary operator depending on state of Lua stack */
#define LAYOUT_UNARY_OR_TERNARY(LB, F, Tr, ...)                          \
  LUA_MLM_BEGIN                                                          \
  if (lua_isnoneornil((LB).L, (LB).idx + 1))                             \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, ##__VA_ARGS__);       \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, ##__VA_ARGS__); \
  LUA_MLM_END

/* trait + {trait || trait::primitive} op */
#define LAYOUT_BINARY_OPTIONAL(LB, F, Tr, ...)                                                \
  LUA_MLM_BEGIN                                                                               \
  if (gLuaTrait<Tr::value_type>::Is(LB, (LB).idx + 1))                                        \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__); \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, ##__VA_ARGS__);                          \
  LUA_MLM_END

/* trait + trait + {trait || trait::primitive} op */
#define LAYOUT_TERNARY_OPTIONAL(LB, F, Tr, ...)                                                   \
  LUA_MLM_BEGIN                                                                                   \
  if (gLuaTrait<Tr::value_type>::Is(LB, (LB).idx + 2))                                            \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, gLuaTrait<Tr::value_type>, ##__VA_ARGS__); \
  VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, Tr, Tr, Tr, ##__VA_ARGS__);                          \
  LUA_MLM_END

/* */
#define LAYOUT_UNARY_NUMINT(LB, F, Tr, ...) \
  return gLuaBase::PushNumInt(LB, F(Tr::Next(LB)));

/* A binary integer layout that sanitizes the second argument (division/modulo zero) */
#define LAYOUT_BINARY_INTEGER(LB, F, Tr, ...)                           \
  LUA_MLM_BEGIN                                                         \
  if (Tr::Is(LB, (LB).idx + 1)) {                                       \
    const Tr::type __a = Tr::Next(LB);                                  \
    const Tr::type __b = Tr::Next(LB);                                  \
    if ((lua_Unsigned)__b + 1u <= 1u) { /* special cases: -1 or 0 */    \
      luaL_argcheck((LB).L, __b != 0, (LB).idx - 1, "zero");            \
      return gLuaBase::Push(LB, 0); /* overflow with 0x80000... / -1 */ \
    }                                                                   \
    return gLuaBase::Push(LB, F(__a, __b));                             \
  }                                                                     \
  /* Second argument is a number, fall-back to <number, number> */      \
  else if (gLuaNumber::Is(LB, (LB).idx + 1))                            \
    LAYOUT_BINARY(LB, F, gLuaNumber, ##__VA_ARGS__);                    \
  return luaL_typeerror((LB).L, (LB).idx, Tr::Label());                 \
  LUA_MLM_END

/* }================================================================== */

/*
** {==================================================================
** Common Argument Parsers
** ===================================================================
*/

/* A GLM function where the first argument (Tr) is sufficient in template argument deduction */
#define PARSE_UNARY(LB, F, ArgLayout, Tr, ...) \
  ArgLayout(LB, F, Tr, ##__VA_ARGS__);

/* Vector definition where the lua_Number operation takes priority */
#define PARSE_NUMBER_VECTOR(LB, F, ArgLayout, ...)                           \
  LUA_MLM_BEGIN                                                              \
  switch (ttypetag(glm_i2v((LB).L, (LB).idx))) {                             \
    case LUA_VFALSE: case LUA_VTRUE:                                         \
    case LUA_VSHRSTR: case LUA_VLNGSTR: /* string coercion */                \
    case LUA_VNUMINT: /* integer to number */                                \
    case LUA_VNUMFLT: ArgLayout(LB, F, gLuaNumber, ##__VA_ARGS__); break;    \
    case LUA_VVECTOR2: ArgLayout(LB, F, gLuaVec2<>, ##__VA_ARGS__); break;   \
    case LUA_VVECTOR3: ArgLayout(LB, F, gLuaVec3<>, ##__VA_ARGS__); break;   \
    case LUA_VVECTOR4: ArgLayout(LB, F, gLuaVec4<>, ##__VA_ARGS__); break;   \
    default:                                                                 \
      break;                                                                 \
  }                                                                          \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_NUMBER " or " LABEL_VECTOR); \
  LUA_MLM_END

/* Vector definition where lua_Integer and lua_Number operations takes priority */
#define PARSE_INTEGER_NUMBER_VECTOR(LB, F, ILayout, FLayout, VLayout, ...)   \
  LUA_MLM_BEGIN                                                              \
  switch (ttypetag(glm_i2v((LB).L, (LB).idx))) {                             \
    case LUA_VFALSE: case LUA_VTRUE:                                         \
    case LUA_VNUMINT: ILayout(LB, F, gLuaInteger, ##__VA_ARGS__); break;     \
    case LUA_VSHRSTR: case LUA_VLNGSTR: /* string coercion */                \
    case LUA_VNUMFLT: FLayout(LB, F, gLuaNumber, ##__VA_ARGS__); break;      \
    case LUA_VVECTOR2: VLayout(LB, F, gLuaVec2<>, ##__VA_ARGS__); break;     \
    case LUA_VVECTOR3: VLayout(LB, F, gLuaVec3<>, ##__VA_ARGS__); break;     \
    case LUA_VVECTOR4: VLayout(LB, F, gLuaVec4<>, ##__VA_ARGS__); break;     \
    default:                                                                 \
      break;                                                                 \
  }                                                                          \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_NUMBER " or " LABEL_VECTOR); \
  LUA_MLM_END

/* glm::function defined over the vector & quaternion space: vec1, vec2, vec3, vec4, quat */
#define PARSE_NUMBER_VECTOR_QUAT(LB, F, FLayout, VLayout, QLayout, ...)       \
  LUA_MLM_BEGIN                                                               \
  switch (ttypetag(glm_i2v((LB).L, (LB).idx))) {                              \
    case LUA_VFALSE: case LUA_VTRUE:                                          \
    case LUA_VSHRSTR: case LUA_VLNGSTR: /* string coercion */                 \
    case LUA_VNUMINT: /* integer to number */                                 \
    case LUA_VNUMFLT: FLayout(LB, F, gLuaNumber, ##__VA_ARGS__); break;       \
    case LUA_VVECTOR2: VLayout(LB, F, gLuaVec2<>, ##__VA_ARGS__); break;      \
    case LUA_VVECTOR3: VLayout(LB, F, gLuaVec3<>, ##__VA_ARGS__); break;      \
    case LUA_VVECTOR4: VLayout(LB, F, gLuaVec4<>, ##__VA_ARGS__); break;      \
    case LUA_VQUAT: QLayout(LB, F, gLuaQuat<>, ##__VA_ARGS__); break;         \
    default:                                                                  \
      break;                                                                  \
  }                                                                           \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_VECTOR " or " LABEL_QUATERN); \
  LUA_MLM_END

/*
** A glm::function that defined over any NxM matrix
**
** @NOTE: This parser does not throw an error if the Value is not a known matrix
**  value. This is to simplify the EQUAL/HASH operations.
*/
#define PARSE_MATRIX(LB, Value, F, ArgLayout, ...)                    \
  LUA_MLM_BEGIN                                                       \
  switch (gm_cols(Value)) {                                           \
    case 2: {                                                         \
      switch (gm_rows(Value)) {                                       \
        case 2: ArgLayout(LB, F, gLuaMat2x2<>, ##__VA_ARGS__); break; \
        case 3: ArgLayout(LB, F, gLuaMat2x3<>, ##__VA_ARGS__); break; \
        case 4: ArgLayout(LB, F, gLuaMat2x4<>, ##__VA_ARGS__); break; \
        default:                                                      \
          break;                                                      \
      }                                                               \
      break;                                                          \
    }                                                                 \
    case 3: {                                                         \
      switch (gm_rows(Value)) {                                       \
        case 2: ArgLayout(LB, F, gLuaMat3x2<>, ##__VA_ARGS__); break; \
        case 3: ArgLayout(LB, F, gLuaMat3x3<>, ##__VA_ARGS__); break; \
        case 4: ArgLayout(LB, F, gLuaMat3x4<>, ##__VA_ARGS__); break; \
        default:                                                      \
          break;                                                      \
      }                                                               \
      break;                                                          \
    }                                                                 \
    case 4: {                                                         \
      switch (gm_rows(Value)) {                                       \
        case 2: ArgLayout(LB, F, gLuaMat4x2<>, ##__VA_ARGS__); break; \
        case 3: ArgLayout(LB, F, gLuaMat4x3<>, ##__VA_ARGS__); break; \
        case 4: ArgLayout(LB, F, gLuaMat4x4<>, ##__VA_ARGS__); break; \
        default:                                                      \
          break;                                                      \
      }                                                               \
      break;                                                          \
    }                                                                 \
    default:                                                          \
      break;                                                          \
  }                                                                   \
  LUA_MLM_END

/* A glm::function that operates only on NxN matrices */
#define PARSE_SYMMETRIC_MATRIX(LB, F, ArgLayout, ...)                        \
  LUA_MLM_BEGIN                                                              \
  const TValue *_tv = glm_i2v((LB).L, (LB).idx);                             \
  if (l_likely(ttismatrix(_tv) && gm_cols(_tv) == gm_rows(_tv))) {           \
    switch (gm_cols(_tv)) {                                                  \
      case 2: ArgLayout(LB, F, gLuaMat2x2<>, ##__VA_ARGS__); break;          \
      case 3: ArgLayout(LB, F, gLuaMat3x3<>, ##__VA_ARGS__); break;          \
      case 4: ArgLayout(LB, F, gLuaMat4x4<>, ##__VA_ARGS__); break;          \
      default:                                                               \
        return luaL_typeerror((LB).L, (LB).idx, GLM_INVALID_MAT_DIMENSIONS); \
    }                                                                        \
  }                                                                          \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_SYMMETRIC_MATRIX);           \
  LUA_MLM_END

/*
** a glm::function that operates on rotation matrices
**
** @NOTE: All geometric objects must support multiplication operations for
**  quaternions, mat3x3, mat3x4, mat4x3, and mat4x4.
*/
#define PARSE_ROTATION_MATRIX(LB, F, ArgLayout, ...)                          \
  LUA_MLM_BEGIN                                                               \
  const TValue *_tv = glm_i2v((LB).L, (LB).idx);                              \
  switch (ttypetag(_tv)) {                                                    \
    case LUA_VQUAT: ArgLayout(LB, F, gLuaQuat<>, ##__VA_ARGS__); break;       \
    case LUA_VMATRIX: {                                                       \
      const glm::length_t size = gm_cols(_tv);                                \
      const glm::length_t secondary_size = gm_rows(_tv);                      \
      if (size == 3 && secondary_size == 3) ArgLayout(LB, F, gLuaMat3x3<>, ##__VA_ARGS__); \
      if (size == 3 && secondary_size == 4) ArgLayout(LB, F, gLuaMat3x4<>, ##__VA_ARGS__); \
      if (size == 4 && secondary_size == 3) ArgLayout(LB, F, gLuaMat4x3<>, ##__VA_ARGS__); \
      if (size == 4 && secondary_size == 4) ArgLayout(LB, F, gLuaMat4x4<>, ##__VA_ARGS__); \
      return luaL_typeerror((LB).L, (LB).idx, GLM_INVALID_MAT_DIMENSIONS);    \
      break;                                                                  \
    }                                                                         \
    default:                                                                  \
      break;                                                                  \
  }                                                                           \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_QUATERN " or " LABEL_MATRIX); \
  LUA_MLM_END

/*
** Generalized int16_t, int32_t, etc. definition
**
** @NOTE: Due to the nature of storing most/all data as floating point types,
**  bitfield operations on vectors may be inconsistent with float -> int -> float
**  casting.
**
**  Therefore, all INTEGER_VECTOR definitions are considered unsafe when the
**  function isn't explicitly operating on lua_Integer types.
*/
#define PARSE_VECTOR_TYPE(LB, F, ArgLayout, IType, ...)                         \
  LUA_MLM_BEGIN                                                                 \
  switch (ttypetag(glm_i2v((LB).L, (LB).idx))) {                                \
    case LUA_VFALSE: case LUA_VTRUE:                                            \
    case LUA_VSHRSTR: case LUA_VLNGSTR: /* string coercion */                   \
    case LUA_VNUMINT: /* integer to number */                                   \
    case LUA_VNUMFLT: ArgLayout(LB, F, gLuaTrait<IType>, ##__VA_ARGS__); break; \
    case LUA_VVECTOR2: ArgLayout(LB, F, gLuaVec2<IType>, ##__VA_ARGS__); break; \
    case LUA_VVECTOR3: ArgLayout(LB, F, gLuaVec3<IType>, ##__VA_ARGS__); break; \
    case LUA_VVECTOR4: ArgLayout(LB, F, gLuaVec4<IType>, ##__VA_ARGS__); break; \
    default:                                                                    \
      break;                                                                    \
  }                                                                             \
  return luaL_typeerror((LB).L, (LB).idx, LABEL_NUMBER " or " LABEL_VECTOR);    \
  LUA_MLM_END

/* }================================================================== */

/*
** {==================================================================
** Generic Function API
** ===================================================================
*/

/* Common function declaration for all Lua-binded GLM functions. */
#define GLM_BINDING_QUALIFIER(NAME) static int GLM_NAME(NAME)(lua_State *L)

/*
** Exception handling wrapper for generic API functions.
**
** Generic try/catch blocks are used to avoid any lingering std::logic_error and
** std::runtime_error exceptions that could be thrown by GLM.
**
** @NOTE: This toggle isn't required for versions of Lua compiled with C++ so
**    long as LUAI_TRY handles exceptions via try/catch. Although, the flags
**    LUA_USE_LONGJMP & LUA_CPP_EXCEPTIONS can change that functionality.
*/
#if defined(LUA_GLM_SAFE)
  #define GLM_BINDING_BEGIN         \
    gLuaBase LB(L);                 \
    /* Ensure LB.top() is cached */ \
    const int __top = LB.top();     \
    try {

  #define GLM_BINDING_END                 \
    }                                     \
    catch (const std::exception &e) {     \
      lua_settop(L, __top);               \
      lua_pushstring(L, e.what());        \
    }                                     \
    catch (...) {                         \
      lua_settop(L, __top);               \
      lua_pushstring(L, "GLM exception"); \
    }                                     \
    return lua_error(L);
#else
  #define GLM_BINDING_BEGIN gLuaBase LB(L);
  #define GLM_BINDING_END
#endif

/* GLM function that corresponds to one unique set of function parameters */
#define TRAITS_DEFN(Name, F, ...)                              \
  GLM_BINDING_QUALIFIER(Name) {                                \
    GLM_BINDING_BEGIN                                          \
    VA_NARGS_CALL_OVERLOAD(TRAITS_FUNC, LB, F, ##__VA_ARGS__); \
    GLM_BINDING_END                                            \
  }

/* A GLM function where the first argument (Tr) is sufficient in template argument deduction; */
#define TRAITS_LAYOUT_DEFN(Name, F, ArgLayout, Tr, ...) \
  GLM_BINDING_QUALIFIER(Name) {                         \
    GLM_BINDING_BEGIN                                   \
    ArgLayout(LB, F, Tr, ##__VA_ARGS__);                \
    GLM_BINDING_END                                     \
  }

/* A operation defined for two traits; often 2D/3D or 3D/4D vectors. */
#define TRAITS_BINARY_LAYOUT_DEFN(Name, F, ArgLayout, A, B, ...)            \
  GLM_BINDING_QUALIFIER(Name) {                                             \
    GLM_BINDING_BEGIN                                                       \
    if (A::Is(LB, (LB).idx)) ArgLayout(LB, F, A, ##__VA_ARGS__);            \
    if (B::Is(LB, (LB).idx)) ArgLayout(LB, F, B, ##__VA_ARGS__);            \
    return luaL_error((LB).L, "%s or %s expected", A::Label(), B::Label()); \
    GLM_BINDING_END                                                         \
  }

/* Vector definition where the lua_Number operation takes priority */
#define NUMBER_VECTOR_DEFN(Name, F, ArgLayout, ...)       \
  GLM_BINDING_QUALIFIER(Name) {                           \
    GLM_BINDING_BEGIN                                     \
    PARSE_NUMBER_VECTOR(LB, F, ArgLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                       \
  }

/* Vector definition where lua_Integer and lua_Number operations takes priority */
#define INTEGER_NUMBER_VECTOR_DEFN(Name, F, ArgLayout, ...)                             \
  GLM_BINDING_QUALIFIER(Name) {                                                         \
    GLM_BINDING_BEGIN                                                                   \
    PARSE_INTEGER_NUMBER_VECTOR(LB, F, ArgLayout, ArgLayout, ArgLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                                                     \
  }

#define INTEGER_NUMBER_VECTOR_DEFNS(Name, F, ILayout, FLayout, VLayout, ...)      \
  GLM_BINDING_QUALIFIER(Name) {                                                   \
    GLM_BINDING_BEGIN                                                             \
    PARSE_INTEGER_NUMBER_VECTOR(LB, F, ILayout, FLayout, VLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                                               \
  }

/* glm::function defined over the vector & quaternion space: vec1, vec2, vec3, vec4, quat */
#define NUMBER_VECTOR_QUAT_DEFN(Name, F, ArgLayout, ...)                             \
  GLM_BINDING_QUALIFIER(Name) {                                                      \
    GLM_BINDING_BEGIN                                                                \
    PARSE_NUMBER_VECTOR_QUAT(LB, F, ArgLayout, ArgLayout, ArgLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                                                  \
  }

#define NUMBER_VECTOR_QUAT_DEFNS(Name, F, FLayout, VLayout, QLayout, ...)      \
  GLM_BINDING_QUALIFIER(Name) {                                                \
    GLM_BINDING_BEGIN                                                          \
    PARSE_NUMBER_VECTOR_QUAT(LB, F, FLayout, VLayout, QLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                                            \
  }

/* A glm::function that defined over any quaternions */
#define QUAT_DEFN(Name, F, ArgLayout, ...)                    \
  GLM_BINDING_QUALIFIER(Name) {                               \
    GLM_BINDING_BEGIN                                         \
    PARSE_UNARY(LB, F, ArgLayout, gLuaQuat<>, ##__VA_ARGS__); \
    GLM_BINDING_END                                           \
  }

/* A glm::function that defined over any NxM matrix */
#define MATRIX_DEFN(Name, F, ArgLayout, ...)               \
  GLM_BINDING_QUALIFIER(Name) {                            \
    GLM_BINDING_BEGIN                                      \
    const TValue *_m = glm_i2v((LB).L, (LB).idx);          \
    if (l_likely(ttismatrix(_m)))                          \
      PARSE_MATRIX(LB, _m, F, ArgLayout, ##__VA_ARGS__);   \
    return luaL_typeerror((LB).L, (LB).idx, LABEL_MATRIX); \
    GLM_BINDING_END                                        \
  }

/* A glm::function that operates only on NxN matrices */
#define SYMMETRIC_MATRIX_DEFN(Name, F, ArgLayout, ...)       \
  GLM_BINDING_QUALIFIER(Name) {                              \
    GLM_BINDING_BEGIN                                        \
    PARSE_SYMMETRIC_MATRIX(LB, F, ArgLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                          \
  }

/* a glm::function that operates on rotation matrices */
#define ROTATION_MATRIX_DEFN(Name, F, ArgLayout, ...)       \
  GLM_BINDING_QUALIFIER(Name) {                             \
    GLM_BINDING_BEGIN                                       \
    PARSE_ROTATION_MATRIX(LB, F, ArgLayout, ##__VA_ARGS__); \
    GLM_BINDING_END                                         \
  }

/* Generalized int16_t, int32_t, etc. function definition */
#define INTEGER_VECTOR_DEFN(Name, F, ArgLayout, IType, ...)    \
  GLM_BINDING_QUALIFIER(Name) {                                \
    GLM_BINDING_BEGIN                                          \
    PARSE_VECTOR_TYPE(LB, F, ArgLayout, IType, ##__VA_ARGS__); \
    GLM_BINDING_END                                            \
  }

/* }================================================================== */

/*
** {==================================================================
** Function Bindings
** ===================================================================
*/

/* glm/gtx/hash.hpp */
#define STD_HASH(LB, F, Traits, ...)          \
  LUA_MLM_BEGIN                               \
  F<Traits::type> hash;                       \
  gLuaBase::Push(LB, hash(Traits::Next(LB))); \
  LUA_MLM_END

/* Generic equals layout */
#define _EQUAL(LB, F, Tr, Tr_Row)                                              \
  LUA_MLM_BEGIN                                                                \
  const Tr::type __a = Tr::Next(LB);                                           \
  const Tr::type __b = Tr::Next(LB);                                           \
  const TValue *_tv3 = glm_i2v((LB).L, (LB).idx);                              \
  if (!_isvalid((LB).L, _tv3)) /* <Tr, Tr> */                                  \
    return gLuaBase::Push(LB, F(__a, __b));                                    \
  else if (ttisfloat(_tv3)) /* <Tr, Tr, eps> */                                \
    return gLuaBase::Push(LB, F(__a, __b, gLuaEps<Tr::value_type>::Next(LB))); \
  else if (ttisinteger(_tv3)) /* <Tr, Tr, ULPs> */                             \
    return gLuaBase::Push(LB, F(__a, __b, gLuaTrait<int>::Next(LB)));          \
  else if (Tr_Row::Is(LB, (LB).idx)) /* <Tr, Tr, vec> */                       \
    return gLuaBase::Push(LB, F(__a, __b, Tr_Row::Next(LB)));                  \
  return luaL_typeerror((LB).L, (LB).idx, "expected none, " LABEL_NUMBER " or " LABEL_VECTOR); \
  LUA_MLM_END

/*
** Accumulation for min/max functions, where arguments can be the Trait or a primitive
**
** @TODO: Potentially handle the case of LB.idx not changing after an iteration
*/
#define MIN_MAX(LB, F, A, ...)                               \
  LUA_MLM_BEGIN                                              \
  A::type base = A::Next(LB);                                \
  while ((LB).idx <= (LB).top()) {                           \
    if (A::Is(LB, (LB).idx))                                 \
      base = F(base, A::Next(LB));                           \
    else if (gLuaTrait<A::value_type>::Is(LB, (LB).idx))     \
      base = F(base, gLuaTrait<A::value_type>::Next(LB));    \
    else                                                     \
      return luaL_error((LB).L, "%s or %s expected",         \
             A::Label(), gLuaTrait<A::value_type>::Label()); \
  }                                                          \
  return gLuaBase::Push(LB, base);                           \
  LUA_MLM_END

/* glm::qr_decompose */
#define QRDECOMPOSE(LB, F, Tr, ...) \
  LUA_MLM_BEGIN                     \
  Tr::type q, r;                    \
  F(Tr::Next(LB), q, r);            \
  TRAITS_PUSH(LB, q, r);            \
  LUA_MLM_END

/* glm::clamp */
#define CLAMP(LB, F, Tr, ...)                                                                                  \
  LUA_MLM_BEGIN                                                                                                \
  if (lua_isnoneornil((LB).L, (LB).idx + 1) && lua_isnoneornil((LB).L, (LB).idx + 2)) /* <vec, 0, 1> */        \
    TRAITS_FUNC(LB, F, Tr);                                                                                    \
  else if (gLuaTrait<Tr::value_type>::Is(LB, (LB).idx + 1) && gLuaTrait<Tr::value_type>::Is(LB, (LB).idx + 2)) \
    TRAITS_FUNC(LB, F, Tr, gLuaTrait<Tr::value_type>, gLuaTrait<Tr::value_type>); /* <vec, minVal, maxVal> */  \
  else                                                                                                         \
    LAYOUT_TERNARY(LB, F, Tr); /* <vector, minVector, maxVector> */                                            \
  LUA_MLM_END

/* glm::intersectLineSphere */
#define INTERSECT_LINE_SPHERE(LB, F, Tr, ...)                    \
  LUA_MLM_BEGIN                                                  \
  Tr::type v5, v6, v7, v8;                                       \
  const Tr::type v1 = Tr::Next(LB);                              \
  const Tr::type v2 = Tr::Next(LB);                              \
  const Tr::type v3 = Tr::Next(LB);                              \
  const Tr::value_type v4 = gLuaTrait<Tr::value_type>::Next(LB); \
  if (glm::intersectLineSphere(v1, v2, v3, v4, v5, v6, v7, v8))  \
    TRAITS_PUSH(LB, v5, v6, v7, v8);                             \
  return gLuaBase::Push(LB);                                     \
  LUA_MLM_END

/* glm::intersectRayPlane */
#define INTERSECT_RAY_PLANE(LB, F, Tr, ...)       \
  LUA_MLM_BEGIN                                   \
  Tr::value_type v5;                              \
  const Tr::type v1 = Tr::Next(LB);               \
  const Tr::type v2 = gm_drift(Tr::Next(LB));     \
  const Tr::type v3 = Tr::Next(LB);               \
  const Tr::type v4 = Tr::Next(LB);               \
  if (glm::intersectRayPlane(v1, v2, v3, v4, v5)) \
    return gLuaBase::Push(LB, v5);                \
  return gLuaBase::Push(LB);                      \
  LUA_MLM_END

/* glm::intersectRaySphere */
#define INTERSECT_RAY_SPHERE(LB, F, Tr, ...)                     \
  LUA_MLM_BEGIN                                                  \
  Tr::type v5, v6;                                               \
  const Tr::type v1 = Tr::Next(LB);                              \
  const Tr::type v2 = gm_drift(Tr::Next(LB));                    \
  const Tr::type v3 = Tr::Next(LB);                              \
  const Tr::value_type v4 = gLuaTrait<Tr::value_type>::Next(LB); \
  if (glm::intersectRaySphere(v1, v2, v3, v4, v5, v6))           \
    TRAITS_PUSH(LB, v5, v6);                                     \
  return gLuaBase::Push(LB);                                     \
  LUA_MLM_END

/* LUA_VECTOR_EXTENSIONS: glm::smoothDamp */
#define SMOOTH_DAMP(LB, F, Tr, ...)                              \
  LUA_MLM_BEGIN                                                  \
  const Tr::type c = Tr::Next(LB);                               \
  const Tr::type t = Tr::Next(LB);                               \
  Tr::type cv = Tr::Next(LB);                                    \
  const Tr::value_type st = gLuaTrait<Tr::value_type>::Next(LB); \
  const Tr::value_type ms = gLuaTrait<Tr::value_type>::Next(LB); \
  const Tr::value_type dt = gLuaTrait<Tr::value_type>::Next(LB); \
  const Tr::type result = F(c, t, cv, st, ms, dt);               \
  TRAITS_PUSH(LB, result, cv);                                   \
  LUA_MLM_END

/* }================================================================== */

#endif
