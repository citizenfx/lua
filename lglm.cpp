/*
** $Id: lglm.cpp $
** Vector and Matrix object definitions.
** See Copyright Notice in lua.h
*/

#define lglm_cpp
#define LUA_CORE

/* @TEMP Disallow 820a2c0e625f26000c688d841836bb10483be34d */
#if defined(GLM_FORCE_QUAT_DATA_XYZW)
  #error "glm broke compatbility, please compile without GLM_FORCE_QUAT_DATA_XYZW"
#endif

/*
@@ LUA_API_LINKAGE is a mark for the linkage specification to core API functions
*/
#define LUA_API_LINKAGE "C++"

extern LUA_API_LINKAGE {
#include "luaconf.h"
#include "lua.h"
#include "ldebug.h"
#include "lfunc.h"
#include "lgc.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lgrit_lib.h"
#include "lglm_core.h"
}

#include "lglm.hpp"
#include "lglm_string.hpp"

#include <string>

#include <glm/glm.hpp>
#include <glm/fwd.hpp>
#include <glm/ext.hpp>

#include <glm/detail/type_quat.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/exterior_product.hpp>
#include <glm/ext/matrix_relational.hpp>
#include <glm/ext/matrix_transform.hpp>

#define INVALID_VECTOR_TYPE "invalid " LABEL_VECTOR " type"
#define INVALID_VECTOR_STRUCTURE "invalid " LABEL_VECTOR " structure"
#define INVALID_VECTOR_DIMENSIONS "invalid " LABEL_VECTOR " dimensions"
#define INVALID_MATRIX_STRUCTURE "invalid " LABEL_MATRIX " structure"
#define INVALID_MATRIX_DIMENSIONS "invalid " LABEL_MATRIX " dimension"

/* return helpers */
#define glm_runerror(L, M) (luaG_runerror((L), (M)), 0)
#define glm_typeError(L, O, M) (luaG_typeerror((L), (O), (M)), 0)
#define glm_finishset(L, T, K, V) (luaV_finishset((L), (T), (K), (V), GLM_NULLPTR), 1)

#define _gettop(L) (cast_int((L)->top.p - ((L)->ci->func.p + 1)))
#define _isvalid(L, o) (!ttisnil(o) || o != &G(L)->nilvalue)
#define _ispseudo(i) ((i) <= LUA_REGISTRYINDEX)

#define svalue(o) getstr(tsvalue(o))
#define vslen(o) tsslen(tsvalue(o))

/* index2value copied from lapi.c */
static TValue *glm_index2value(lua_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, idx <= L->ci->top.p - (ci->func.p + 1), "unacceptable index");
    return (o >= L->top.p) ? &G(L)->nilvalue : s2v(o);
  }
  else if (!_ispseudo(idx)) { /* negative index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1), "invalid index");
    return s2v(L->top.p + idx);
  }
  else if (idx == LUA_REGISTRYINDEX)
    return &G(L)->l_registry;
  else { /* upvalues */
    idx = LUA_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) { /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx - 1] : &G(L)->nilvalue;
    }
    else { /* light C function or Lua function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C function");
      return &G(L)->nilvalue; /* no upvalues */
    }
  }
}

/// <summary>
/// Parse the given number object as a vector/matrix accessible index.
/// </summary>
static LUA_INLINE lua_Integer glm_flttointeger(const TValue *obj) {
  const lua_Number n = l_floor(fltvalue(obj));
  if (n >= cast_num(LUA_MININTEGER) && n < -cast_num(LUA_MININTEGER))
    return static_cast<lua_Integer>(n);

  return 0;
}

/* trybinTM where the first element is predefined */
static int num_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);
static int vec_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);
static int quat_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);
static int mat_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);

/*
** {==================================================================
** Object Conversion
** ===================================================================
*/

/* nvalue */
#define glm_toflt(obj) cast_glmfloat(nvalue(obj))

/* convert an object to an integer (without string coercion) */
#define glm_tointeger(o) (ttisinteger(o) ? ivalue(o) : glm_flttointeger(o))

/* Future-proof for when/if quaternions have their own type tag */
#define glm_vvalue(o) vvalue(o)
#define glm_vecvalue(o) vecvalue(o)
#define glm_quatvalue(o) quatvalue(o)
#define glm_setvvalue2s(s, x, o)        \
  LUA_MLM_BEGIN                         \
  TValue *io = s2v(s);                  \
  vvalue_(io) = (x);                    \
  settt_(io, (o));                      \
  LUA_MLM_END

#define glm_mvalue(o) mvalue(o)
#define glm_setmvalue2s(L, o, x) glm_setmvalue(L, s2v(o), x)
#define glm_setmvalue(L, obj, x) \
  LUA_MLM_BEGIN                  \
  TValue *io = (obj);            \
  GCMatrix *x_ = (x);            \
  val_(io).gc = obj2gco(x_);     \
  settt_(io, ctb(LUA_VMATRIX));  \
  checkliveness(L, io);          \
  LUA_MLM_END

/* }================================================================== */

/*
** {==================================================================
** Vector Object API
** ===================================================================
*/

/// <summary>
/// The vector-type equivalent to luaV_finishget. The 'angle' and 'axis' fields
/// are gritLua compatibility fields for quaternion types.
///
/// @NOTE If the quaternion type has a metatable then the 'angle' and 'axis'
///   fields are no longer parsed. Ideally all compatibility bloat will be
///   removed from this codebase. It is already ugly enough as-is.
/// </summary>
static void vec_finishget(lua_State *L, const TValue *obj, TValue *key, StkId res) {
  const TValue *tm = luaT_gettmbyobj(L, obj, TM_INDEX);
  if (notm(tm)) {
    if (!ttisstring(key))
      luaG_typeerror(L, obj, "index");
    else if (ttisquat(obj)) {
      if (strcmp(svalue(key), "angle") == 0) {
        setfltvalue(s2v(res), glm::degrees(cast_num(glm::angle(glm_quatvalue(obj).q))));
      }
      else if (strcmp(svalue(key), "axis") == 0) {
        glmVector out(glm::axis(glm_quatvalue(obj).q));
        glm_setvvalue2s(res, out, LUA_VVECTOR3);
      }
      else {
#if defined(LUA_GLM_APICHECK)
        luaG_runerror(L, "invalid " LABEL_QUATERN " field: '%s'", svalue(key));
#else
        setnilvalue(s2v(res));
#endif
      }
    }
    else {
#if defined(LUA_GLM_APICHECK)
      luaG_runerror(L, "invalid " LABEL_VECTOR " field: '%s'", svalue(key));
#else
      setnilvalue(s2v(res));
#endif
    }
  }
  else {  // Finish the vector access and try the metamethod
    luaV_finishget(L, obj, key, res, NULL);
  }
}

/// <summary>
/// Runtime swizzle operation.
///
/// Returning the number of copied vector fields on success, zero on failure.
/// </summary>
template<glm::length_t L, typename T, glm::qualifier Q>
static glm::length_t swizzle(const glm::vec<L, T, Q> &v, const char *key, glm::vec<4, T, Q> &out) {
  glm::length_t i = 0;
  for (; i < 4 && key[i] != '\0'; ++i) {
    switch (key[i]) {
      case 'x': GLM_IF_CONSTEXPR (L < 1) { return 0; } out[i] = static_cast<T>(v[0]); break;
      case 'y': GLM_IF_CONSTEXPR (L < 2) { return 0; } out[i] = static_cast<T>(v[1]); break;
      case 'z': GLM_IF_CONSTEXPR (L < 3) { return 0; } out[i] = static_cast<T>(v[2]); break;
      case 'w': GLM_IF_CONSTEXPR (L < 4) { return 0; } out[i] = static_cast<T>(v[3]); break;
      default:
        return 0;
    }
  }
  return i;
}

/// <summary>
/// Runtime swizzle operation for quaternions. This implementation is
/// GLM_FORCE_QUAT_DATA_XYZW agnostic.
///
/// Returning the number of copied quaternion fields on success, zero on failure.
/// </summary>
template<typename T, glm::qualifier Q>
static LUA_INLINE glm::length_t swizzle(const glm::qua<T, Q> &q, const char *key, glm::vec<4, T, Q> &out) {
  const glm::vec<4, T, Q> v(q.x, q.y, q.z, q.w);
  return swizzle(v, key, out);
}

static LUA_INLINE int glm_finishrawget(lua_State *L, int result) {
  if (result == LUA_TNONE) {
    setnilvalue(s2v(L->top.p));
    result = LUA_TNIL;
  }
  api_incr_top(L);
  lua_unlock(L);
  return novariant(result);
}

int glmVec_rawgeti(lua_State *L, const TValue *obj, lua_Integer n) {
  return glm_finishrawget(L, vecgeti(obj, n, L->top.p));
}

int glmVec_get1(lua_State *L, const TValue *obj, const char *k) {
  int result = strlen(k) == 1 ? vecgets(obj, k, L->top.p) : LUA_TNONE;
  if (result != LUA_TNONE) {
    api_incr_top(L);
    lua_unlock(L);
  }
  return result;
}

int glmVec_rawget(lua_State* L, const TValue *obj) {
  TValue* key = s2v(L->top.p - 1);
  L->top.p--; /* remove key */

  int result = LUA_TNONE;
  switch (ttype(key)) {
    case LUA_TNUMBER: {
      result = vecgeti(obj, glm_tointeger(key), L->top.p);
      break;
    }
    case LUA_TSTRING: {
      // The 'dim', 'axis', and 'angle' fields viewed as metafields. To simplify
      // logic the 'n' (shorthand dimensionality) field will be exposed by this
      // function.
      if (vslen(key) == 1)
        result = vecgets(obj, svalue(key), L->top.p);
      break;
    }
    default:
      break;
  }
  return glm_finishrawget(L, result);
}

void glmVec_geti(lua_State *L, const TValue *obj, lua_Integer c, StkId res) {
  if (vecgeti(obj, c, res) == LUA_TNONE) {  // Attempt metatable access
    TValue key;
    setivalue(&key, c);
    luaV_finishget(L, obj, &key, res, NULL);
  }
}

void glmVec_get(lua_State *L, const TValue *obj, TValue *key, StkId res) {
  if (ttisnumber(key)) {
    if (vecgeti(obj, glm_tointeger(key), res) != LUA_TNONE)
      return;
  }
  else if (ttisstring(key)) {
    const char *str = svalue(key);
    const size_t str_len = vslen(key);
    if (str_len == 1) {  // hot-path single character access
      if (vecgets(obj, str, res) != LUA_TNONE)
        return;
    }
    // Allow runtime swizzle operations prior to metamethod access.
    else if (str_len <= 4) {
      const glmVector &v = glm_vvalue(obj);

      glmVector out;
      glm::length_t count = 0;
      switch (ttypetag(obj)) {
        case LUA_VVECTOR2: count = swizzle(v.v2, str, out.v4); break;
        case LUA_VVECTOR3: count = swizzle(v.v3, str, out.v4); break;
        case LUA_VVECTOR4: count = swizzle(v.v4, str, out.v4); break;
        case LUA_VQUAT: count = swizzle(v.q, str, out.v4); break;
        default:
          break;
      }

      switch (count) {
        case 1: setfltvalue(s2v(res), cast_num(out.v4.x)); return;
        case 2: setvvalue(s2v(res), out, LUA_VVECTOR2); return;
        case 3: setvvalue(s2v(res), out, LUA_VVECTOR3); return;
        case 4: {
          // Quaternion was swizzled and resultant vector is still normalized.
          // Keep quaternion semantics.
          if (ttisquat(obj) && glm::isNormalized(out.v4, glm::epsilon<glm_Float>())) {
#if !defined(GLM_FORCE_QUAT_DATA_XYZW)  // quaternion has WXYZ layout
            out.q = glm::qua<glm_Float>(out.v4.w, out.v4.x, out.v4.y, out.v4.z);
#endif
            setvvalue(s2v(res), out, LUA_VQUAT);
          }
          else
            setvvalue(s2v(res), out, LUA_VVECTOR4);
          return;
        }
        default: {
          // gritLua compatibility: dimension field takes priority over tag methods
          if (strcmp(str, "dim") == 0) {
            setivalue(s2v(res), i_luaint(glm_dimensions(ttypetag(obj))));
            return;
          }
          break;
        }
      }
    }
  }

  vec_finishget(L, obj, key, res);  // Metatable Access
}

void glmVec_objlen(const TValue *obj, StkId res) {
  const glmVector &v = glm_vvalue(obj);
  switch (ttypetag(obj)) {
    case LUA_VVECTOR2: setfltvalue(s2v(res), cast_num(glm::length(v.v2))); break;
    case LUA_VVECTOR3: setfltvalue(s2v(res), cast_num(glm::length(v.v3))); break;
    case LUA_VVECTOR4: setfltvalue(s2v(res), cast_num(glm::length(v.v4))); break;
    case LUA_VQUAT: setfltvalue(s2v(res), cast_num(glm::length(v.q))); break;
    default:
      setfltvalue(s2v(res), cast_num(0));
      break;
  }
}

int glmVec_equalObj(lua_State *L, const TValue *o1, const TValue *o2, int rtt) {
  bool result = false;
  const glmVector &v = glm_vvalue(o1);
  const glmVector &other_v = glm_vvalue(o2);
  switch (rtt) {
    case LUA_VVECTOR2: result = _glmeq(v.v2, other_v.v2); break;
    case LUA_VVECTOR3: result = _glmeq(v.v3, other_v.v3); break;
    case LUA_VVECTOR4: result = _glmeq(v.v4, other_v.v4); break;
    case LUA_VQUAT: result = _glmeq(v.q, other_v.q); break;
    default:
      break;
  }

  // @TODO: Document the specifics of this tag method and how glm::equal(V, V2)
  // takes priority over any custom method for the vector type. The intent is to
  // still allow custom __eq declarations to include desired epsilon or ULPS.
  if (result == false && L != GLM_NULLPTR) {
    const TValue *tm = luaT_gettmbyobj(L, o1, TM_EQ);
    if (!notm(tm)) {
      luaT_callTMres(L, tm, o1, o2, L->top.p);  /* call TM */
      result = !l_isfalse(s2v(L->top.p));
    }
  }
  return result;
}

int glmVec_concat(const TValue *obj, const TValue *value, StkId res) {
  const glmVector &v = glm_vvalue(obj);

  glmVector result = v;  // Create a copy of the vector
  glm::length_t dims = glm_dimensions(ttypetag(obj));  // Current dimensionality
  if (ttisinteger(value) && dims < 4)
    result.v4[dims++] = cast_glmfloat(ivalue(value));
  else if (ttisfloat(value) && dims < 4)
    result.v4[dims++] = cast_glmfloat(fltvalue(value));
  else if (ttisboolean(value) && dims < 4)
    result.v4[dims++] = cast_glmfloat(!l_isfalse(value));
  else if (ttisvector(value)) {
    const glm::length_t v_dims = glm_dimensions(ttypetag(value));
    if ((dims + v_dims) > 4) // Outside valid dimensionality
      return 0;

    for (glm::length_t i = 0; i < v_dims; ++i)
      result.v4[dims++] = glm_vecvalue(value).v4[i];
  }
  else {
    return 0;
  }

  glm_setvvalue2s(res, result, glm_variant(dims));
  return 1;
}

int glmVec_tostr(const TValue *obj, char *buff, size_t len) {
  int copy = 0;
  const glmVector &v = glm_vvalue(obj);
  switch (ttypetag(obj)) {
    case LUA_VVECTOR1: copy = glm::detail::format_type(buff, len, v.v1); break;
    case LUA_VVECTOR2: copy = glm::detail::format_type(buff, len, v.v2); break;
    case LUA_VVECTOR3: copy = glm::detail::format_type(buff, len, v.v3); break;
    case LUA_VVECTOR4: copy = glm::detail::format_type(buff, len, v.v4); break;
    case LUA_VQUAT: copy = glm::detail::format_type(buff, len, v.q); break;
    default:
      break;
  }
  lua_assert(copy >= 0);
  return copy;
}

int glmVec_equalKey(const TValue *k1, const Node *n2, int rtt) {
  // @NOTE: Ideally _glmeq would be used. However, that would put the table in
  // an invalid state: mainposition != equalkey.

  switch (withvariant(rtt)) {
    case LUA_VVECTOR2: return glm_vecvalue(k1).v2 == vvalue_raw(keyval(n2)).v2;
    case LUA_VVECTOR3: return glm_vecvalue(k1).v3 == vvalue_raw(keyval(n2)).v3;
    case LUA_VVECTOR4: return glm_vecvalue(k1).v4 == vvalue_raw(keyval(n2)).v4;
    case LUA_VQUAT: return glm_quatvalue(k1).q == vvalue_raw(keyval(n2)).q;
    default:
      return 0;
  }
}

size_t glmVec_hash(const TValue *obj) {
  // Uses a custom glm::hash implementation without the dependency on std::hash
  switch (ttypetag(obj)) {
    case LUA_VVECTOR2: return glm::hash::hash(glm_vecvalue(obj).v2);
    case LUA_VVECTOR3: return glm::hash::hash(glm_vecvalue(obj).v3);
    case LUA_VVECTOR4: return glm::hash::hash(glm_vecvalue(obj).v4);
    case LUA_VQUAT: return glm::hash::hash(glm_vecvalue(obj).q);
    default:
      return 0xDEAD;  // C0D3
  }
}

namespace glm {
  /// <summary>
  /// Return true if each component of the vector is finite.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_FUNC_DECL bool __isfinite(vec<L, T, Q> const &v);
}

int glmVec_isfinite(const TValue *obj) {
  switch (ttypetag(obj)) {
    case LUA_VVECTOR2: return glm::__isfinite(glm_vecvalue(obj).v2);
    case LUA_VVECTOR3: return glm::__isfinite(glm_vecvalue(obj).v3);
    case LUA_VVECTOR4: return glm::__isfinite(glm_vecvalue(obj).v4);
    case LUA_VQUAT: return glm::__isfinite(glm_quatvalue(obj).v4); // @HACK
    default:
      break;
  }
  return 0;
}

int glmVec_next(const TValue *obj, StkId key) {
  TValue *key_obj = s2v(key);
  if (ttisnil(key_obj)) {
    setivalue(key_obj, 1);
    if (vecgeti(obj, 1, key + 1) == LUA_TNONE)
      setnilvalue(s2v(key + 1));
    return 1;
  }
  else if (ttisnumber(key_obj)) {
    const lua_Integer l_nextIdx = glm_tointeger(key_obj) + 1;
    const glm::length_t nextIdx = i_glmlen(l_nextIdx);
    if (nextIdx >= 1 && nextIdx <= glm_dimensions(ttypetag(obj))) {
      setivalue(key_obj, l_nextIdx);  // Iterator values are 1-based
      if (vecgeti(obj, l_nextIdx, key + 1) == LUA_TNONE)
        setnilvalue(s2v(key + 1));
      return 1;
    }
  }
  return 0;
}

int glm_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  switch (ttype(p1)) {
    case LUA_TNUMBER: return num_trybinTM(L, p1, p2, res, event);
    case LUA_TMATRIX: return mat_trybinTM(L, p1, p2, res, event);
    case LUA_TVECTOR: {
      if (ttisquat(p1))  // quaternion-specific implementation
        return quat_trybinTM(L, p1, p2, res, event);
      return vec_trybinTM(L, p1, p2, res, event);
    }
    default:
      break;
  }
  return 0;
}

LUA_API int glm_unpack_vector(lua_State *L, int idx) {
  luaL_checkstack(L, 4, "vector fields"); // Ensure stack-space
  const TValue *o = glm_index2value(L, idx);
  switch (ttypetag(o)) {
    case LUA_VVECTOR2:
      lua_pushnumber(L, cast_num(vecvalue(o).v2.x));
      lua_pushnumber(L, cast_num(vecvalue(o).v2.y));
      return 2;
    case LUA_VVECTOR3:
      lua_pushnumber(L, cast_num(vecvalue(o).v3.x));
      lua_pushnumber(L, cast_num(vecvalue(o).v3.y));
      lua_pushnumber(L, cast_num(vecvalue(o).v3.z));
      return 3;
    case LUA_VVECTOR4:
      lua_pushnumber(L, cast_num(vecvalue(o).v4.x));
      lua_pushnumber(L, cast_num(vecvalue(o).v4.y));
      lua_pushnumber(L, cast_num(vecvalue(o).v4.z));
      lua_pushnumber(L, cast_num(vecvalue(o).v4.w));
      return 4;
    case LUA_VQUAT:
      lua_pushnumber(L, cast_num(quatvalue(o).q.w));
      lua_pushnumber(L, cast_num(quatvalue(o).q.x));
      lua_pushnumber(L, cast_num(quatvalue(o).q.y));
      lua_pushnumber(L, cast_num(quatvalue(o).q.z));
      return 4;
    default: {
      lua_pushvalue(L, idx);
      return 1;
    }
  }
}

/* }================================================================== */

/*
** {==================================================================
** Matrix Object API
** ===================================================================
*/

/// <summary>
/// If "raw" is true (denoting 'rawset'), the function will throw Lua runtime
/// errors when attempting to operate on invalid keys/fields. Otherwise, this
/// function (like non 'raw' functions) will attempt to reference a metatable.
/// </summary>
static int glmMat_auxset(lua_State *L, const TValue *obj, TValue *key, TValue *val, bool raw) {
  if (!ttisnumber(key)) {  // Invalid index for matrix
    return raw ? glm_typeError(L, key, "index")
               : glm_finishset(L, obj, key, val);
  }

  glmMatrix &m = mvalue(obj);
  const glm::length_t dim = i_glmlen(glm_tointeger(key));
  if (ttisvector(val)) {
    const bool expanding = dim <= 4 && (dim == (m.size + 1));
    if (glm_dimensions(ttypetag(val)) != m.secondary)  // Invalid vector being appended
      return raw ? glm_runerror(L, INVALID_MATRIX_DIMENSIONS) : glm_finishset(L, obj, key, val);
    else if (dim <= 0 || (dim > m.size && !expanding))  // Index out of bounds.
      return raw ? glm_runerror(L, INVALID_MATRIX_DIMENSIONS) : glm_finishset(L, obj, key, val);

    switch (m.secondary) {
      case 2: m.m42[dim - 1] = glm_vecvalue(val).v2; break;
      case 3: m.m43[dim - 1] = glm_vecvalue(val).v3; break;
      case 4: {
#if !defined(GLM_FORCE_QUAT_DATA_XYZW)  // quaternion has WXYZ layout
        if (ttisquat(val)) {
          const glm::qua<glm_Float> &q = glm_quatvalue(val).q;
          m.m44[dim - 1] = glm::vec<4, glm_Float>(q.x, q.y, q.z, q.w);
        }
        else
#endif
        {
          m.m44[dim - 1] = glm_vecvalue(val).v4;
        }
        break;
      }
      default: {
        return raw ? glm_runerror(L, INVALID_MATRIX_DIMENSIONS)
                   : glm_finishset(L, obj, key, val);
      }
    }

    m.size += (expanding ? 1 : 0);
    return 1;
  }
  else if (ttisnil(val)) {  // Attempt to shrink the dimension of the matrix
    if (dim == m.size && dim > 2) {  // Matrices must have at least two columns; >= 2x2
      m.size--;
      return 1;
    }
    return raw ? glm_runerror(L, LABEL_MATRIX " must have at least two columns")
               : glm_finishset(L, obj, key, val);
  }
  return raw ? glm_runerror(L, "attempt to set a " LABEL_MATRIX " value with an incorrect index")
             : glm_finishset(L, obj, key, val);
}

GCMatrix *glmMat_new(lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_VMATRIX, sizeof(GCMatrix));
  GCMatrix *mat = gco2mat(o);
  mat->mat4 = glm::identity<glm::mat<4, 4, glm_Float>>();
  return mat;
}

int glmMat_rawgeti(lua_State *L, const TValue *obj, lua_Integer n) {
  return glm_finishrawget(L, matgeti(obj, n, L->top.p));
}

int glmMat_rawget(lua_State *L, const TValue *obj) {
  TValue *key = s2v(L->top.p - 1);
  L->top.p--; /* remove key */
  return glm_finishrawget(L, ttisnumber(key) ? glmMat_rawgeti(L, obj, glm_tointeger(key)) : LUA_TNONE);
}

void glmMat_rawset(lua_State *L, const TValue *obj, TValue *key, TValue *val) {
  glmMat_auxset(L, obj, key, val, true);
}

void glmMat_get(lua_State *L, const TValue *obj, TValue *key, StkId res) {
  if (!ttisnumber(key) || matgeti(obj, glm_tointeger(key), res) == LUA_TNONE)
    luaV_finishget(L, obj, key, res, NULL);
}

void glmMat_geti(lua_State *L, const TValue *obj, lua_Integer c, StkId res) {
  if (matgeti(obj, c, res) == LUA_TNONE) {
    TValue key;
    setivalue(&key, c);
    luaV_finishget(L, obj, &key, res, NULL);
  }
}

void glmMat_set(lua_State *L, const TValue *obj, TValue *key, TValue *val) {
  glmMat_auxset(L, obj, key, val, false);
}

void glmMat_seti(lua_State *L, const TValue *obj, lua_Integer c, TValue *val) {
  TValue key;
  setivalue(&key, c);
  glmMat_auxset(L, obj, &key, val, false);
}

void glmMat_objlen(const TValue *obj, StkId res) {
  setivalue(s2v(res), static_cast<lua_Integer>(mvalue(obj).size));
}

int glmMat_tostr(const TValue *obj, char *buff, size_t len) {
  int copy = 0;
  const glmMatrix &m = glm_mvalue(obj);

  // Use a custom glm::to_string implementation that avoids using std::string
  // and CRT allocator use. In addition, gtx/string_cast is broken for GCC when
  // GLM_FORCE_INLINE is enabled.
  switch (m.size) {
    case 2: {
      switch (m.secondary) {
        case 2: copy = glm::detail::format_type(buff, len, m.m22); break;
        case 3: copy = glm::detail::format_type(buff, len, m.m23); break;
        case 4: copy = glm::detail::format_type(buff, len, m.m24); break;
        default:
          break;
      }
      break;
    }
    case 3: {
      switch (m.secondary) {
        case 2: copy = glm::detail::format_type(buff, len, m.m32); break;
        case 3: copy = glm::detail::format_type(buff, len, m.m33); break;
        case 4: copy = glm::detail::format_type(buff, len, m.m34); break;
        default:
          break;
      }
      break;
    }
    case 4: {
      switch (m.secondary) {
        case 2: copy = glm::detail::format_type(buff, len, m.m42); break;
        case 3: copy = glm::detail::format_type(buff, len, m.m43); break;
        case 4: copy = glm::detail::format_type(buff, len, m.m44); break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  lua_assert(copy >= 0);
  return copy;
}

int glmMat_next(const TValue *obj, StkId key) {
  TValue *key_value = s2v(key);
  if (ttisnil(key_value)) {
    setivalue(key_value, 1);
    matgeti(obj, 1, key + 1);
    return 1;
  }
  else if (ttisnumber(key_value)) {
    const lua_Integer l_nextIdx = glm_tointeger(key_value) + 1;
    const glm::length_t nextIdx = i_glmlen(l_nextIdx);
    if (nextIdx >= 1 && nextIdx <= mvalue(obj).size) {
      setivalue(key_value, l_nextIdx);  // Iterator values are 1-based
      matgeti(obj, l_nextIdx, key + 1);
      return 1;
    }
  }
  return 0;
}

int glmMat_equalObj(lua_State *L, const TValue *o1, const TValue *o2) {
  bool result = false;
  const glmMatrix &m = glm_mvalue(o1);
  const glmMatrix &other_m = glm_mvalue(o2);
  if (m.size == other_m.size && m.secondary == other_m.secondary) {
    switch (m.size) {
      case 2: {
        switch (m.secondary) {
          case 2: result = _glmeq(m.m22, other_m.m22); break;
          case 3: result = _glmeq(m.m23, other_m.m23); break;
          case 4: result = _glmeq(m.m24, other_m.m24); break;
          default:
            break;
        }
        break;
      }
      case 3: {
        switch (m.secondary) {
          case 2: result = _glmeq(m.m32, other_m.m32); break;
          case 3: result = _glmeq(m.m33, other_m.m33); break;
          case 4: result = _glmeq(m.m34, other_m.m34); break;
          default:
            break;
        }
        break;
      }
      case 4: {
        switch (m.secondary) {
          case 2: result = _glmeq(m.m42, other_m.m42); break;
          case 3: result = _glmeq(m.m43, other_m.m43); break;
          case 4: result = _glmeq(m.m44, other_m.m44); break;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
  }

  // @TODO: Document the specifics of this tag method and how glm::equal(M, M2)
  // takes priority over any custom method for the matrix type.
  if (!result && L != GLM_NULLPTR) {
    const TValue *tm = luaT_gettmbyobj(L, o1, TM_EQ);
    if (!notm(tm)) {
      luaT_callTMres(L, tm, o1, o2, L->top.p);  /* call TM */
      result = !l_isfalse(s2v(L->top.p));
    }
  }

  return result;
}

LUA_API int glm_unpack_matrix(lua_State *L, int idx) {
  luaL_checkstack(L, 4, "matrix unpack");

  const TValue *obj = glm_index2value(L, idx);
  if (ttismatrix(obj)) {
    const glmMatrix &m = glm_mvalue(obj);
    for (glm::length_t i = 0; i < m.size; ++i) {
      switch (m.secondary) {
        case 2: glm_pushvec2(L, m.m42[i]); break;
        case 3: glm_pushvec3(L, m.m43[i]); break;
        case 4: glm_pushvec4(L, m.m44[i]); break;
        default: {
          lua_pushnil(L);
          break;
        }
      }
    }
    return cast_int(m.size);
  }
  return 0;
}

/* }================================================================== */

/*
** {==================================================================
** GLM Interface
** ===================================================================
*/

/// <summary>
/// Generalized TValue to glm::vec conversion; using glmVector.Get to implicitly
/// handle type conversions.
/// </summary>
template <glm::length_t D, typename T>
static glm::vec<D, T> glm_tovec(lua_State *L, int idx) {
  glm::vec<D, glm_Float> result(0);

  const TValue *o = glm_index2value(L, idx);
  if (ttisvector(o) && glm_dimensions(ttypetag(o)) >= D)
    glm_vvalue(o).Get(result);
  return result;
}

/// <summary>
/// Generalized TValue to glm::mat conversion; using glmMatrix.Get to implicitly
/// handle type conversions.
/// </summary>
template <glm::length_t C, glm::length_t R, typename T>
static glm::mat<C, R, T> glm_tomat(lua_State *L, int idx) {
  glm::mat<C, R, T> result = glm::identity<glm::mat<C, R, glm_Float>>();

  const TValue *o = glm_index2value(L, idx);
  if (ttismatrix(o)) {
    const glmMatrix &m = glm_mvalue(o);
    if (m.size >= C && m.secondary == R)
      m.Get(result);
  }
  return result;
}

LUA_API int glm_pushvec(lua_State *L, const glmVector &v, glm::length_t dimensions) {
  const lu_byte variant = glm_variant(dimensions);
  if (variant == LUA_VVECTOR1)
    lua_pushnumber(L, cast_num(v.v1.x));
  else if (novariant(variant) == LUA_TVECTOR) {
    lua_lock(L);
    glm_setvvalue2s(L->top.p, v, variant);
    api_incr_top(L);
    lua_unlock(L);
  }
  else {
#if defined(LUA_USE_APICHECK)
    luaG_runerror(L, INVALID_VECTOR_TYPE);
#endif
    return 0;
  }
  return 1;
}

LUA_API int glm_pushvec_quat(lua_State *L, const glmVector &q) {
  lua_lock(L);
  glm_setvvalue2s(L->top.p, q, LUA_VQUAT);
  api_incr_top(L);
  lua_unlock(L);
  return 1;
}

LUA_API int glm_pushmat(lua_State *L, const glmMatrix &m) {
  GCMatrix *mat = GLM_NULLPTR;
  if (m.size < 2 || m.size > 4 || m.secondary < 2 || m.secondary > 4) {
#if defined(LUA_USE_APICHECK)
    luaG_runerror(L, INVALID_MATRIX_DIMENSIONS);
#endif
    return 0;
  }

  lua_lock(L);
  mat = glmMat_new(L);
  mat->mat4 = m;
  glm_setmvalue2s(L, L->top.p, mat);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return 1;
};

LUA_API bool glm_isvector(lua_State *L, int idx, glm::length_t &size) {
  bool result = true;
  const TValue *o = glm_index2value(L, idx);
  if (ttisvector(o) && !ttisquat(o))
    size = glm_dimensions(ttypetag(o));
  else if (ttisnumber(o))
    size = 1;
  else {
    result = false;
  }
  return result;
}

LUA_API bool glm_isquat(lua_State *L, int idx) {
  const TValue *o = glm_index2value(L, idx);
  return ttisquat(o);
}

LUA_API bool glm_ismatrix(lua_State *L, int idx, glm::length_t &size, glm::length_t &secondary) {
  const TValue *o = glm_index2value(L, idx);
  if (ttismatrix(o)) {
    size = mvalue(o).size;
    secondary = mvalue(o).secondary;
    return true;
  }
  return false;
}

LUA_API int glm_pushvec1(lua_State *L, const glm::vec<1, glm_Float> &v) { lua_pushnumber(L, cast_num(v.x)); return 1; }
LUA_API int glm_pushvec2(lua_State *L, const glm::vec<2, glm_Float> &v) { return glm_pushvec(L, glmVector(v), 2); }
LUA_API int glm_pushvec3(lua_State *L, const glm::vec<3, glm_Float> &v) { return glm_pushvec(L, glmVector(v), 3); }
LUA_API int glm_pushvec4(lua_State *L, const glm::vec<4, glm_Float> &v) { return glm_pushvec(L, glmVector(v), 4); }
LUA_API int glm_pushquat(lua_State *L, const glm::qua<glm_Float> &q) { return glm_pushvec_quat(L, glmVector(q)); }

LUA_API glm::vec<1, glm_Float> glm_tovec1(lua_State *L, int idx) { return glm::vec<1, glm_Float>(cast_glmfloat(lua_tonumber(L, idx))); }
LUA_API glm::vec<2, glm_Float> glm_tovec2(lua_State *L, int idx) { return glm_tovec<2, glm_Float>(L, idx); }
LUA_API glm::vec<3, glm_Float> glm_tovec3(lua_State *L, int idx) { return glm_tovec<3, glm_Float>(L, idx); }
LUA_API glm::vec<4, glm_Float> glm_tovec4(lua_State *L, int idx) { return glm_tovec<4, glm_Float>(L, idx); }
LUA_API glm::qua<glm_Float> glm_toquat(lua_State *L, int idx) {
  const TValue *o = glm_index2value(L, idx);
  return ttisquat(o) ? glm_quatvalue(o).q : glm::quat_identity<glm_Float, glm::defaultp>();
}

LUA_API int glm_pushmat2x2(lua_State *L, const glm::mat<2, 2, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat2x3(lua_State *L, const glm::mat<2, 3, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat2x4(lua_State *L, const glm::mat<2, 4, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat3x2(lua_State *L, const glm::mat<3, 2, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat3x3(lua_State *L, const glm::mat<3, 3, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat3x4(lua_State *L, const glm::mat<3, 4, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat4x2(lua_State *L, const glm::mat<4, 2, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat4x3(lua_State *L, const glm::mat<4, 3, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }
LUA_API int glm_pushmat4x4(lua_State *L, const glm::mat<4, 4, glm_Float> &m) { return glm_pushmat(L, glmMatrix(m)); }

LUA_API glm::mat<2, 2, glm_Float> glm_tomat2x2(lua_State *L, int idx) { return glm_tomat<2, 2, glm_Float>(L, idx); }
LUA_API glm::mat<2, 3, glm_Float> glm_tomat2x3(lua_State *L, int idx) { return glm_tomat<2, 3, glm_Float>(L, idx); }
LUA_API glm::mat<2, 4, glm_Float> glm_tomat2x4(lua_State *L, int idx) { return glm_tomat<2, 4, glm_Float>(L, idx); }
LUA_API glm::mat<3, 2, glm_Float> glm_tomat3x2(lua_State *L, int idx) { return glm_tomat<3, 2, glm_Float>(L, idx); }
LUA_API glm::mat<3, 3, glm_Float> glm_tomat3x3(lua_State *L, int idx) { return glm_tomat<3, 3, glm_Float>(L, idx); }
LUA_API glm::mat<3, 4, glm_Float> glm_tomat3x4(lua_State *L, int idx) { return glm_tomat<3, 4, glm_Float>(L, idx); }
LUA_API glm::mat<4, 2, glm_Float> glm_tomat4x2(lua_State *L, int idx) { return glm_tomat<4, 2, glm_Float>(L, idx); }
LUA_API glm::mat<4, 3, glm_Float> glm_tomat4x3(lua_State *L, int idx) { return glm_tomat<4, 3, glm_Float>(L, idx); }
LUA_API glm::mat<4, 4, glm_Float> glm_tomat4x4(lua_State *L, int idx) { return glm_tomat<4, 4, glm_Float>(L, idx); }

LUA_API const char *glm_typename(lua_State *L, int idx) {
  const TValue *o = glm_index2value(L, idx);
  switch (ttypetag(o)) {
    case LUA_VNUMFLT: return LABEL_NUMBER;
    case LUA_VNUMINT: return LABEL_INTEGER;
    case LUA_VVECTOR2: return LABEL_VECTOR2;
    case LUA_VVECTOR3: return LABEL_VECTOR3;
    case LUA_VVECTOR4: return LABEL_VECTOR4;
    case LUA_VQUAT: return LABEL_QUATERN;
    case LUA_VMATRIX: return LABEL_MATRIX;
    default:
      return "Unknown GLM Type";
  }
}

LUA_API const char *glm_pushstring(lua_State *L, int idx) {
  const TValue *o = glm_index2value(L, idx);
  if (ttisinteger(o))
    return lua_pushfstring(L, LUA_INTEGER_FMT, ivalue(o));
  else if (ttisfloat(o))
    return lua_pushfstring(L, LUA_NUMBER_FMT, static_cast<LUAI_UACNUMBER>(lua_tonumber(L, idx)));
  else if (ttisvector(o)) {
    char buff[GLM_STRING_BUFFER];
    const int len = glmVec_tostr(o, buff, GLM_STRING_BUFFER);
    return lua_pushlstring(L, buff, cast_sizet(len < 0 ? 0 : len));
  }
  else if (ttismatrix(o)) {
    char buff[GLM_STRING_BUFFER];
    const int len = glmMat_tostr(o, buff, GLM_STRING_BUFFER);
    return lua_pushlstring(L, buff, cast_sizet(len < 0 ? 0 : len));
  }
  return lua_pushliteral(L, "nil");
}

/* }================================================================== */

/*
** {==================================================================
** Deprecated gritLua API
** ===================================================================
*/

namespace glm {
  /// <summary>
  /// Return true if all components of the vector are finite.
  ///
  /// @NOTE: -ffast-math will break this condition.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_FUNC_QUALIFIER bool __isfinite(vec<L, T, Q> const &v) {
    GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'isnan' only accept floating-point inputs");

    bool result = true;
    for (length_t l = 0; l < L; ++l)
      result &= glm::isfinite(v[l]);
    return result;
  }

  /// <summary>
  /// Improved slerp implementation for generalized vectors.
  /// </summary>
  template<length_t L, typename T, qualifier Q>
  GLM_FUNC_QUALIFIER vec<L, T, Q> __objslerp(vec<L, T, Q> const &x, vec<L, T, Q> const &y, T const &a) {
    const T CosAlpha = dot(x, y);

    // Perform a linear interpolation when CosAlpha is close to 1 to avoid side
    // effect of sin(angle) becoming a zero denominator
    if (CosAlpha > static_cast<T>(1) - epsilon<T>())
      return mix(x, y, a);
    else {
      const T Alpha = acos(CosAlpha);  // get angle (0 -> pi)
      const T SinAlpha = sin(Alpha);  // get sine of angle between vectors (0 -> 1)
      const T t1 = sin((static_cast<T>(1) - a) * Alpha) / SinAlpha;
      const T t2 = sin(a * Alpha) / SinAlpha;
      return x * t1 + y * t2;
    }
  }

  template<typename T, typename T2>
  GLM_FUNC_QUALIFIER T __objFloorDivide(const T &x, const T2 &y) {
    return floor(x / y);
  }

  template<length_t L, typename T, qualifier Q>
  GLM_FUNC_QUALIFIER vec<L, T, Q> __objPow(const vec<L, T, Q> &v, const T exp) {
    return pow(v, vec<L, T, Q>(exp));
  }
}

lua_Integer luaO_HashString(const char *string, size_t length, int ignore_case) {
  unsigned int hash = 0;
  for (size_t i = 0; i < length; ++i) {
    hash += (ignore_case ? string[i] : tolower(string[i]));
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

#if defined(GRIT_POWER_UHASH)
  return l_castU2S(hash);
#else
  // @TODO: Eventually avoid sign-extension issues. If ever...
  return (lua_Integer)(int)hash;
#endif
}

/* gritLua functions stored in lbaselib; considered deprecated */

/// <summary>
/// Parse the provided table object as a vector type, i.e., check the table for
/// numeric (and consecutive) 'x', 'y', 'z', and 'w' fields. With 'v' as an
/// optional vector pointer that is populated with the contents from the table.
///
/// This function returns the variant tag (not number of dimensions) of the
/// resultant vector... and LUA_TNIL on failure.
///
/// @NOTE: Function considered deprecated. The previous idea that tables can be
/// implicit vector types does not "mesh" well with the glm binding library.
/// </summary>
static lu_byte glmH_tovector(lua_State *L, const TValue *o, glmVector *v) {
  static const char *const dims[] = { "x", "y", "z", "w" };

  glm::length_t count = 0;
  for (int i = 0; i < 4; ++i) {
    TString *key = luaS_newlstr(L, dims[i], 1);  // luaS_newliteral
    const TValue *slot = luaH_getstr(hvalue(o), key);
    if (ttisnumber(slot)) {
      if (v != GLM_NULLPTR)
        v->v4[i] = cast_glmfloat(nvalue(slot));

      count++;
    }
  }
  return (count > 0) ? glm_variant(count) : LUA_TNIL;
}

LUA_API lua_Integer glm_tohash(lua_State *L, int idx, int ignore_case) {
  const TValue *o = glm_index2value(L, idx);
  if (ttisstring(o))
    return luaO_HashString(svalue(o), vslen(o), ignore_case);
  else if (ttisboolean(o))
    return ttistrue(o) ? 1 : 0;
  else if (ttisnumber(o)) {
    lua_Integer res = 0;
    return tointeger(o, &res) ? res : 0;
  }
  return 0;
}

LUA_API int glmVec_dot(lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  const TValue *y = glm_index2value(L, 2);
  if (ttisinteger(x) && ttisinteger(y))
    lua_pushinteger(L, ivalue(x) * ivalue(y));
  else if (ttisnumber(x) && ttisnumber(y))
    lua_pushnumber(L, nvalue(x) * nvalue(y));
  else if (ttisquat(x) && ttisquat(y))
    lua_pushnumber(L, cast_num(glm::dot(glm_quatvalue(x).q, glm_quatvalue(y).q)));
  else if (ttisvector(x) && ttypetag(x) == ttypetag(y)) {
    switch (ttypetag(x)) {
      case LUA_VVECTOR2: lua_pushnumber(L, cast_num(glm::dot(glm_vecvalue(x).v2, glm_vecvalue(y).v2))); break;
      case LUA_VVECTOR3: lua_pushnumber(L, cast_num(glm::dot(glm_vecvalue(x).v3, glm_vecvalue(y).v3))); break;
      case LUA_VVECTOR4: lua_pushnumber(L, cast_num(glm::dot(glm_vecvalue(x).v4, glm_vecvalue(y).v4))); break;
      default:
        lua_pushnumber(L, 0);
        break;
    }
  }
  else {
    return luaL_typeerror(L, 1, LABEL_NUMBER " or " LABEL_VECTOR " type");
  }
  return 1;
}

LUA_API int glmVec_cross(lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  const TValue *y = glm_index2value(L, 2);
  switch (ttypetag(x)) {
    case LUA_VVECTOR2: {
      if (ttypetag(y) == LUA_VVECTOR2) {
        lua_pushnumber(L, cast_num(glm::cross(glm_vecvalue(x).v2, glm_vecvalue(y).v2)));
        return 1;
      }
      return luaL_typeerror(L, 2, LABEL_VECTOR2);
    }
    case LUA_VVECTOR3: {
      if (ttypetag(y) == LUA_VQUAT)
        return glm_pushvec3(L, glm::cross(glm_vecvalue(x).v3, glm_quatvalue(y).q));
      if (ttypetag(y) == LUA_VVECTOR3)
        return glm_pushvec3(L, glm::cross(glm_vecvalue(x).v3, glm_vecvalue(y).v3));
      return luaL_typeerror(L, 2, LABEL_VECTOR3 " or " LABEL_QUATERN);
    }
    case LUA_VQUAT: {
      if (ttypetag(y) == LUA_VQUAT)
        return glm_pushquat(L, glm::cross(glm_quatvalue(x).q, glm_quatvalue(y).q));
      if (ttypetag(y) == LUA_VVECTOR3)
        return glm_pushvec3(L, glm::cross(glm_quatvalue(x).q, glm_vecvalue(y).v3));
      return luaL_typeerror(L, 2, LABEL_VECTOR3 " or " LABEL_QUATERN);
    }
    default:
      break;
  }
  return luaL_typeerror(L, 1, LABEL_VECTOR2 ", " LABEL_VECTOR3 ", or " LABEL_QUATERN);
}

LUA_API int glmVec_inverse(lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  if (ttisquat(x))
    return glm_pushquat(L, glm::inverse(glm_quatvalue(x).q));
  else if (ttismatrix(x)) {
    const glmMatrix &m = glm_mvalue(x);
    if (m.size == m.secondary) {
      switch (m.size) {
        case 2: return glm_pushmat2x2(L, glm::inverse(m.m22));
        case 3: return glm_pushmat3x3(L, glm::inverse(m.m33));
        case 4: return glm_pushmat4x4(L, glm::inverse(m.m44));
        default:
          break;
      }
    }
  }
  return luaL_typeerror(L, 1, LABEL_QUATERN " or " LABEL_SYMMETRIC_MATRIX);
}

LUA_API int glmVec_normalize(lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  switch (ttypetag(x)) {
    case LUA_VVECTOR2: return glm_pushvec2(L, glm::normalize(glm_vecvalue(x).v2));
    case LUA_VVECTOR3: return glm_pushvec3(L, glm::normalize(glm_vecvalue(x).v3));
    case LUA_VVECTOR4: return glm_pushvec4(L, glm::normalize(glm_vecvalue(x).v4));
    case LUA_VQUAT: return glm_pushquat(L, glm::normalize(glm_quatvalue(x).q));
    default:
      break;
  }
  return luaL_typeerror(L, 1, LABEL_VECTOR " or " LABEL_QUATERN);
}

LUA_API int glmVec_slerp(lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  const TValue *y = glm_index2value(L, 2);
  const TValue *a = glm_index2value(L, 3);
  if (ttypetag(x) == ttypetag(y) && ttype(a) == LUA_TNUMBER) {
    const glm_Float t = cast_glmfloat(nvalue(a));
    switch (ttypetag(x)) {
      case LUA_VVECTOR2: return glm_pushvec2(L, glm::__objslerp(glm_vecvalue(x).v2, glm_vecvalue(y).v2, t));
      case LUA_VVECTOR3: return glm_pushvec3(L, glm::__objslerp(glm_vecvalue(x).v3, glm_vecvalue(y).v3, t));
      case LUA_VVECTOR4: return glm_pushvec4(L, glm::__objslerp(glm_vecvalue(x).v4, glm_vecvalue(y).v4, t));
      case LUA_VQUAT: return glm_pushquat(L, glm::slerp(glm_quatvalue(x).q, glm_quatvalue(y).q, t));
      default:
        break;
    }
  }
  return luaL_error(L, "slerp(v, v, a) or slerp(q, q, a) expected");
}

LUA_API int glmVec_clamp (lua_State *L) {
  const TValue *x = glm_index2value(L, 1);
  const TValue *y = glm_index2value(L, 2);
  const TValue *z = glm_index2value(L, 3);
  if (ttypetag(x) == ttypetag(y) && ttypetag(y) == ttypetag(z)) {
    switch (ttypetag(x)) {
      case LUA_VNUMINT: /* grit-lua implementation will cast integers */
      case LUA_VNUMFLT: lua_pushnumber(L, glm::clamp(nvalue(x), nvalue(y), nvalue(z))); return 1;
      case LUA_VVECTOR2: return glm_pushvec2(L, glm::clamp(glm_vecvalue(x).v2, glm_vecvalue(y).v2, glm_vecvalue(z).v2));
      case LUA_VVECTOR3: return glm_pushvec3(L, glm::clamp(glm_vecvalue(x).v3, glm_vecvalue(y).v3, glm_vecvalue(z).v3));
      case LUA_VVECTOR4: return glm_pushvec4(L, glm::clamp(glm_vecvalue(x).v4, glm_vecvalue(y).v4, glm_vecvalue(z).v4));
      default:
        break;
    }
  }
  return luaL_error(L, LABEL_NUMBER " or " LABEL_VECTOR " expected");
}

LUA_API lua_Integer lua_ToHash (lua_State *L, int idx, int ignore_case) {
  return glm_tohash(L, idx, ignore_case);
}

LUA_API int luaVec_dimensions (int rtt) {
  return (rtt == LUA_VVECTOR1) ? 1 : cast_int(glm_dimensions(rtt));
}

LUA_API int lua_isvector (lua_State *L, int idx, int flags) {
  const TValue *o = glm_index2value(L, idx);

  lu_byte variant = LUA_TNIL;
  if (ttisvector(o))
    variant = ttypetag(o);
  else if (ttisnumber(o) && (flags & V_NONUMBER) == 0)
    variant = LUA_VVECTOR1;
  else if (ttistable(o) && (flags & V_PARSETABLE) != 0) {
    lua_lock(L);
    variant = glmH_tovector(L, o, GLM_NULLPTR);
    lua_unlock(L);
  }
  return variant;
}

LUA_API int lua_tovector (lua_State *L, int idx, int flags, lua_Float4 *f4) {
  const TValue *o = glm_index2value(L, idx);

  glmVector v(glm::vec<4, glm_Float>(0));
  lu_byte variant = LUA_TNIL;
  if (ttisvector(o)) {
    v = glm_vvalue(o);
    variant = ttypetag(o);
  }
  else if ((flags & V_PARSETABLE) != 0 && ttistable(o)) {
    lua_lock(L);
    variant = glmH_tovector(L, o, &v);
    lua_unlock(L);
  }
  else if ((flags & V_NONUMBER) == 0 && ttisnumber(o)) {
    lua_Number n = 0;  // @TODO Warn of potential down-casting.
    if (tonumberns(o, n)) {
      v.v4.x = cast_glmfloat(n);
      variant = LUA_VVECTOR1;
    }
  }

  if (f4 != GLM_NULLPTR && variant != LUA_TNIL) {
    if (variant == LUA_VVECTOR1)
      f4->x = v.v4.x;

    // @TODO: Use GLM_FORCE_QUAT_DATA_XYZW preprocessor directive to avoid the
    // runtime ternary operations. However, this API is deprecated and will
    // receive little attention.
    else if (novariant(variant) == LUA_TVECTOR) {
      f4->x = ((variant == LUA_VQUAT) ? v.q.x : v.v4.x);
      f4->y = ((variant == LUA_VQUAT) ? v.q.y : v.v4.y);
      f4->z = ((variant == LUA_VQUAT) ? v.q.z : v.v4.z);
      f4->w = ((variant == LUA_VQUAT) ? v.q.w : v.v4.w);
    }
  }
  return variant;
}

LUA_API void lua_pushvector (lua_State *L, lua_Float4 f4, int variant) {
  const lu_byte b_variant = cast_byte(withvariant(variant));
  if (novariant(b_variant) != LUA_TVECTOR) {
#if defined(LUA_USE_APICHECK)
    luaG_runerror(L, INVALID_VECTOR_TYPE);
#else
    lua_pushnil(L);
#endif
  }
  else if (b_variant == LUA_VVECTOR1)
    lua_pushnumber(L, cast_num(f4.x));
  else if (b_variant == LUA_VQUAT) {
#if defined(GLM_FORCE_QUAT_DATA_XYZW)
    glm_pushvec_quat(L, glmVector(glm::qua<glm_Float>(f4.x, f4.y, f4.z, f4.w)));
#else
    glm_pushvec_quat(L, glmVector(glm::qua<glm_Float>(f4.w, f4.x, f4.y, f4.z)));
#endif
  }
  else
    glm_pushvec(L, glmVector(glm::vec<4, glm_Float>(f4.x, f4.y, f4.z, f4.w)), glm_dimensions(b_variant));
}

LUA_API int lua_ismatrix (lua_State *L, int idx, int *size, int *secondary) {
  const TValue *o = glm_index2value(L, idx);
  if (ttismatrix(o)) {
    if (size != GLM_NULLPTR) *size = cast_int(mvalue(o).size);
    if (secondary != GLM_NULLPTR) *secondary = cast_int(mvalue(o).secondary);
    return 1;
  }
  return 0;
}

LUA_API int lua_tomatrix (lua_State *L, int idx, lua_Mat4 *matrix) {
  const TValue *o = glm_index2value(L, idx);
  if (ttismatrix(o) && matrix != GLM_NULLPTR) {
    *matrix = *reinterpret_cast<const lua_Mat4*>(mvalue_ref(o));
    return 1;
  }
  return 0;
}

LUA_API int lua_pushmatrix (lua_State *L, lua_Mat4 *matrix) {
  if (matrix == GLM_NULLPTR
      || matrix->size < 2 || matrix->size > 4
      || matrix->secondary < 2 || matrix->secondary > 4) {
#if defined(LUA_USE_APICHECK)
    luaG_runerror(L, INVALID_MATRIX_DIMENSIONS);
#endif
    return 0;
  }
  return glm_pushmat(L, *reinterpret_cast<const glmMatrix*>(matrix));
}

/* }================================================================== */

/*
** {==================================================================
** Constructors
** ===================================================================
*/

/// <summary>
/// Statically cast a tagged value to the specified type parameter. Returning
/// true on success, false otherwise.
/// </summary>
template<typename T>
static bool glm_castvalue(const TValue *value, T &out) {
  switch (ttypetag(value)) {
    case LUA_VTRUE: out = static_cast<T>(1); break;
    case LUA_VFALSE: out = static_cast<T>(0); break;
    case LUA_VNUMINT: out = static_cast<T>(ivalue(value)); break;
    case LUA_VNUMFLT: out = static_cast<T>(fltvalue(value)); break;
    default: {
      out = static_cast<T>(0);
      return false;
    }
  }
  return true;
}

/// <summary>
/// Unpack a tagged value into a vector "v" starting at offset "v_idx"
/// </summary>
template<typename T>
static glm::length_t PopulateVectorObject(lua_State *L, int idx, glm::vec<4, T> &vec, glm::length_t v_idx, glm::length_t v_desired, const TValue *value) {
  if (glm_castvalue(value, vec[v_idx]))  // Primitive type: cast & store it.
    return 1;
  else if (ttisvector(value)) {  // Vector: concatenate components values.

    // To handle (not) 'GLM_FORCE_QUAT_DATA_XYZW' it is much easier to force an
    // explicit length rule for quaternion types. For other vector variants,
    // copy the vector or a subset to satisfy 'v_desired'
    const glmVector &v = glm_vvalue(value);
    if (ttisquat(value)) {
      if ((v_idx + 4) > v_desired)
        return luaL_argerror(L, idx, INVALID_VECTOR_DIMENSIONS);

      vec[v_idx++] = static_cast<T>(v.q.x);
      vec[v_idx++] = static_cast<T>(v.q.y);
      vec[v_idx++] = static_cast<T>(v.q.z);
      vec[v_idx++] = static_cast<T>(v.q.w);
      return 4;
    }
    else {
      const glm::length_t dims = glm_dimensions(ttypetag(value));
      const glm::length_t length = std::min(dims, v_desired - v_idx);
      for (glm::length_t j = 0; j < length; ++j)
        vec[v_idx++] = static_cast<T>(v.v4[j]);
      return length;
    }
  }
  else if (ttistable(value)) {  // Array: concatenate values.
    const glm::length_t dims = i_glmlen(luaH_getn(hvalue(value)));
    const glm::length_t length = glm::min(dims, v_desired - v_idx);
    for (glm::length_t j = 1; j <= length; ++j) {
      const TValue *t_val = luaH_getint(hvalue(value), i_luaint(j));
      if (!glm_castvalue(t_val, vec[v_idx++]))  // Primitive type: cast & store it.
        return luaL_argerror(L, idx, INVALID_VECTOR_TYPE);
    }
    return length;
  }

  return luaL_argerror(L, idx, INVALID_VECTOR_TYPE);
}

/// <summary>
/// Generic matrix population/construction function. Iterate over the current
/// Lua stack and produce a matrix type according to the rules:
///   1. If the first and only object is a number: populate the diagonal of a
///     matrix.
///   2. If the first and only object is a quaternion: cast it to the
///     arbitrarily sized matrix. This logic follows glm::toMat3 and glm::toMat4
///     and uses constructors to down/up-cast the matrix.
///   3. If the first object is a matrix: down/up-cast it.
///   4. Otherwise, expect a column vector for each dimension of the matrix.
///
/// A "desired" or "expected" dimensionality may be specified within 'm'.
/// Otherwise, this function will infer the dimensions of matrix according to
/// supplied columns vectors and their dimensionality.
/// </summary>
static bool PopulateMatrix(lua_State *L, int idx, bool fixed_size, glmMatrix &m, glm::length_t &outSize, glm::length_t &outSecondary) {
  glm::length_t size = 0, secondary = 0;

  // Maximum number of stack values to parse from the starting "idx"
  // idx = lua_absindex(L, idx); @NOTE: Assume 'idx' is positive.
  const int stack_count = (_gettop(L) - idx + 1);
  const TValue *o = glm_index2value(L, idx);

  if (stack_count == 1 && ttisnumber(o)) {
    size = m.size;
    secondary = m.secondary;
    m.m44 = glm::mat<4, 4, glm_Float>(cast_glmfloat(nvalue(o)));
  }
  else if (stack_count == 1 && ttisquat(o)) {
    size = m.size;
    secondary = m.secondary;
    m.m44 = glm::mat4_cast<glm_Float, glm::defaultp>(glm_quatvalue(o).q);
  }
  else if (stack_count == 1 && ttismatrix(o)) {
    const glmMatrix &_m = glm_mvalue(o);

    size = fixed_size ? m.size : _m.size;
    secondary = fixed_size ? m.secondary : _m.secondary;
    switch (_m.size) {
      case 2: {
        switch (_m.secondary) {
          case 2: m.m44 = glm::mat<4, 4, glm_Float>(_m.m22); break;
          case 3: m.m44 = glm::mat<4, 4, glm_Float>(_m.m23); break;
          case 4: m.m44 = glm::mat<4, 4, glm_Float>(_m.m24); break;
          default:
            return false;
        }
        break;
      }
      case 3: {
        switch (_m.secondary) {
          case 2: m.m44 = glm::mat<4, 4, glm_Float>(_m.m32); break;
          case 3: m.m44 = glm::mat<4, 4, glm_Float>(_m.m33); break;
          case 4: m.m44 = glm::mat<4, 4, glm_Float>(_m.m34); break;
          default:
            return false;
        }
        break;
      }
      case 4: {
        switch (_m.secondary) {
          case 2: m.m44 = glm::mat<4, 4, glm_Float>(_m.m42); break;
          case 3: m.m44 = glm::mat<4, 4, glm_Float>(_m.m43); break;
          case 4: m.m44 = _m.m44; break;
          default:
            return false;
        }
        break;
      }
      default:
        return false;
    }
  }
  // Otherwise, parse column vectors
  else {
    // If there is only one element to be parsed and it is a table, assume the
    // matrix is packed within an array; otherwise, use the elements on the stack.
    const bool as_table = stack_count == 1 && ttistable(o);
    const glm::length_t column_limit = as_table ? m.size : glm::min(m.size, i_glmlen(stack_count));
    if (fixed_size && column_limit < m.size)
      return false;

    secondary = fixed_size ? m.secondary : secondary;
    for (; size < column_limit; ++size) {
      glm::length_t v_size = 0;
      if (as_table) {  // An array contains all of the elements of a matrix.
        const TValue *value = luaH_getint(hvalue(o), i_luaint(size) + 1);
        v_size = ttisnil(value) ? 0 : PopulateVectorObject(L, idx, m.m44[size], 0, m.secondary, value);
      }
      else {
        const TValue *value = glm_index2value(L, idx);
        v_size = PopulateVectorObject(L, idx++, m.m44[size], 0, m.secondary, value);
      }

      if (v_size > 1) {  // Parse the column/row vector.
        if (secondary > 0 && secondary != v_size)  // Inconsistent dimensions
          return false;

        secondary = v_size;
      }
      else if (secondary == 0)
        return false;  // No/not-enough columns have been parsed
      else
        break;  // At least one column has been parsed.
    }
  }

  outSize = size;
  outSecondary = secondary;
  return (size >= 2 && size <= 4 && secondary >= 2 && secondary <= 4);
}

/// <summary>
/// Generic vector population/construction function.
///
/// This function will iterate over the current Lua stack and unpack its values;
/// returning the number of components of the vector populated and zero on
/// failure (e.g., exceeding row size or attempting to populate the vector with
/// an invalid type).
///
/// Unpacking Rules:
///   (1) A primitive type (float, int, bool) will be stored at v[X];
///   (2) A vector (of N dimensions) will have its contents stored at v[X],
///       v[X + 1], ..., v[X + N]; following x, y, z, w ordering (same applies
///       to quaternions);
///   (3) An array (of length N) will have contents started at v[X], v[X + 1], etc.
///   (4) Otherwise, a lua_error is thrown.
/// </summary>
template<typename T>
static int glm_createVector(lua_State *L, glm::length_t desiredSize = 0) {
  glm::vec<4, T> v(T(0));
  glm::length_t v_len = 0;

  // If the vector is of a fixed/desired size and only one non-table argument has been supplied
  const int top = _gettop(L);
  const TValue *o_f = glm_index2value(L, 1);
  if (desiredSize > 0 && top == 1 && glm_castvalue(o_f, v.x))
    return glm_pushvec(L, glmVector(glm::vec<4, T>(v.x)), desiredSize);

  // Maximum number of stack values to parse, starting from "idx"
  const glm::length_t v_max = desiredSize == 0 ? 4 : desiredSize;
  for (int i = 1; i <= top; ++i) {
    if (v_len >= v_max)  // Ensure at least one element can be packed into the vector;
      return luaL_argerror(L, i, INVALID_VECTOR_DIMENSIONS);

    v_len += PopulateVectorObject(L, i, v, v_len, v_max, glm_index2value(L, i));
  }

  if (desiredSize == 0 && v_len == 0)
    return luaL_error(L, LABEL_VECTOR " requires 1 to 4 values");
  else if (desiredSize != 0 && v_len != desiredSize)
    return luaL_error(L, LABEL_VECTOR "%d requires 1 or %d values", cast_int(desiredSize), cast_int(desiredSize));
  else if (v_len == 1) {
    GLM_IF_CONSTEXPR(std::is_same<T, bool>::value)
      lua_pushboolean(L, cast_int(v.x));
    else GLM_IF_CONSTEXPR(std::is_integral<T>::value)
      lua_pushinteger(L, i_luaint(v.x));
    else
      lua_pushnumber(L, cast_num(v.x));
    return 1;
  }

  return glm_pushvec(L, glmVector(v), v_len);
}

/// <summary>
/// Generalized create matrix.
/// </summary>
template<typename T>
static int glm_createMatrix(lua_State *L, glm::length_t C, glm::length_t R) {
  const int top = _gettop(L);

  glmMatrix result;
  result.size = (C == 0) ? 4 : C;
  result.secondary = (R == 0) ? 4 : R;

  if (top == 0) {  // If there are no elements, return the identity matrix
    switch (result.secondary) {
      case 2: result.m42 = glm::identity<glm::mat<4, 2, T>>(); break;
      case 3: result.m43 = glm::identity<glm::mat<4, 3, T>>(); break;
      case 4: result.m44 = glm::identity<glm::mat<4, 4, T>>(); break;
      default:
        break;
    }
    return glm_pushmat(L, result);
  }
  else {  // Parse the contents of the stack and populate 'result'
    const TValue *o = glm_index2value(L, 1);
    const bool recycle = top > 1 && ttismatrix(o);
    if (PopulateMatrix(L, recycle ? 2 : 1, (C != 0 || R != 0), result, result.size, result.secondary)) {
      // Realign column-vectors, ensuring the matrix can be faithfully
      // represented by its m.mCR union value.
      switch (result.secondary) {
        case 2: result.m42 = glm::mat<4, 2, glm_Float>(result.m44); break;
        case 3: result.m43 = glm::mat<4, 3, glm_Float>(result.m44); break;
        case 4: /* result.m44 = glm::mat<4, 4, glm_Float>(result.m44); */ break;
        default:
          break;
      }

      // The first argument was a 'matrix' intended to be recycled.
      if (recycle) {
        const TValue *orec = GLM_NULLPTR;

        lua_pushvalue(L, 1);
        lua_lock(L);
        orec = glm_index2value(L, -1);
        mvalue(orec) = result;
        lua_unlock(L);
        return 1;
      }
      return glm_pushmat(L, result);
    }
  }
  return luaL_error(L, INVALID_MATRIX_STRUCTURE);
}

LUA_API int glmVec_vec(lua_State *L) { return glm_createVector<glm_Float>(L); }
LUA_API int glmVec_vec1(lua_State *L) { return glm_createVector<glm_Float>(L, 1); }
LUA_API int glmVec_vec2(lua_State *L) { return glm_createVector<glm_Float>(L, 2); }
LUA_API int glmVec_vec3(lua_State *L) { return glm_createVector<glm_Float>(L, 3); }
LUA_API int glmVec_vec4(lua_State *L) { return glm_createVector<glm_Float>(L, 4); }

LUA_API int glmVec_ivec(lua_State *L) { return glm_createVector<glm_Integer>(L); }
LUA_API int glmVec_ivec1(lua_State *L) { return glm_createVector<glm_Integer>(L, 1); }
LUA_API int glmVec_ivec2(lua_State *L) { return glm_createVector<glm_Integer>(L, 2); }
LUA_API int glmVec_ivec3(lua_State *L) { return glm_createVector<glm_Integer>(L, 3); }
LUA_API int glmVec_ivec4(lua_State *L) { return glm_createVector<glm_Integer>(L, 4); }

LUA_API int glmVec_bvec(lua_State *L) { return glm_createVector<bool>(L); }
LUA_API int glmVec_bvec1(lua_State *L) { return glm_createVector<bool>(L, 1); }
LUA_API int glmVec_bvec2(lua_State *L) { return glm_createVector<bool>(L, 2); }
LUA_API int glmVec_bvec3(lua_State *L) { return glm_createVector<bool>(L, 3); }
LUA_API int glmVec_bvec4(lua_State *L) { return glm_createVector<bool>(L, 4); }

LUA_API int glmMat_mat2x2(lua_State *L) { return glm_createMatrix<glm_Float>(L, 2, 2); }
LUA_API int glmMat_mat2x3(lua_State *L) { return glm_createMatrix<glm_Float>(L, 2, 3); }
LUA_API int glmMat_mat2x4(lua_State *L) { return glm_createMatrix<glm_Float>(L, 2, 4); }
LUA_API int glmMat_mat3x2(lua_State *L) { return glm_createMatrix<glm_Float>(L, 3, 2); }
LUA_API int glmMat_mat3x3(lua_State *L) { return glm_createMatrix<glm_Float>(L, 3, 3); }
LUA_API int glmMat_mat3x4(lua_State *L) { return glm_createMatrix<glm_Float>(L, 3, 4); }
LUA_API int glmMat_mat4x2(lua_State *L) { return glm_createMatrix<glm_Float>(L, 4, 2); }
LUA_API int glmMat_mat4x3(lua_State *L) { return glm_createMatrix<glm_Float>(L, 4, 3); }
LUA_API int glmMat_mat4x4(lua_State *L) { return glm_createMatrix<glm_Float>(L, 4, 4); }
LUA_API int glmMat_mat(lua_State *L) { return glm_createMatrix<glm_Float>(L, 0, 0); }

/// <summary>
/// Function written to bypass API overheads;
/// </summary>
LUA_API int glmVec_qua(lua_State *L) {
  const TValue *o1 = glm_index2value(L, 1);
  if (o1 == &G(L)->nilvalue)  // No arguments: return the identity.
    return glm_pushquat(L, glm::identity<glm::qua<glm_Float>>());
  else if (ttisnumber(o1)) {
    const TValue *o2 = glm_index2value(L, 2);
    if (ttisvector3(o2))  // <angle, axis>, degrees for gritLua compatibility
      return glm_pushquat(L, glm::angleAxis(cast_glmfloat(glm::radians(nvalue(o1))), glm_vecvalue(o2).v3));
    else if (ttisnumber(o2)) {  // <w, x, y, z>
#if defined(GLM_FORCE_QUAT_DATA_XYZW)
      const glm_Float w = cast_glmfloat(nvalue(o1));
      const glm_Float x = cast_glmfloat(nvalue(o2));
      const glm_Float y = cast_glmfloat(luaL_checknumber(L, 3));
      const glm_Float z = cast_glmfloat(luaL_checknumber(L, 4));
      return glm_pushquat(L, glm::qua<glm_Float>(x, y, z, w));
#else
      return glm_pushquat(L, glm::qua<glm_Float>(
        cast_glmfloat(nvalue(o1)),
        cast_glmfloat(nvalue(o2)),
        cast_glmfloat(luaL_checknumber(L, 3)),
        cast_glmfloat(luaL_checknumber(L, 4)))
      );
#endif
    }
    return luaL_error(L, "{w, x, y, z} or {angle, axis} expected");
  }
  else if (ttisvector3(o1)) {
    const TValue *o2 = glm_index2value(L, 2);
    if (!_isvalid(L, o2))  // <euler>
      return glm_pushquat(L, glm::qua<glm_Float>(glm_vecvalue(o1).v3));
    else if (ttisnumber(o2))  // <xyz, w>
      return glm_pushquat(L, glm::qua<glm_Float>(cast_glmfloat(nvalue(o2)), glm_vecvalue(o1).v3));
    else if (ttisvector3(o2))  // <from, to>
      return glm_pushquat(L, glm::qua<glm_Float>(glm_vecvalue(o1).v3, glm_vecvalue(o2).v3));
    return luaL_error(L, "{euler}, {from, to}, or {xyz, w} expected");
  }
  else if (ttisquat(o1)) {
    lua_pushvalue(L, 1);
    return 1;
  }
  else if (ttismatrix(o1)) {
    const glmMatrix &m = glm_mvalue(o1);
    if (m.size == m.secondary) {
      switch (m.size) {
        case 3: return glm_pushquat(L, glm::qua<glm_Float>(m.m33));
        case 4: return glm_pushquat(L, glm::qua<glm_Float>(m.m44));
        default:
          return luaL_typeerror(L, 1, LABEL_MATRIX "3x3 or " LABEL_MATRIX "4x4");
      }
    }
  }
  return luaL_typeerror(L, 1, LABEL_NUMBER ", " LABEL_VECTOR3 ", or " LABEL_MATRIX);
}

/* }================================================================== */

/*
** {==================================================================
** Intentionally ugly code
**
** @TODO: Profile/tune statements below.
** ===================================================================
*/

/*
** Create a new matrix collectible and set it to the stack.
**
** A dimensionality override is included to simplify the below logic for
** operations that operate on a per-value basis. Allowing the use of more
** generalized operations instead of logic for all nine matrix types.
*/
#define glm_newmvalue(L, obj, x, nsize, nsecondary) \
  LUA_MLM_BEGIN                                     \
  GCMatrix *mat = glmMat_new(L);                    \
  mat->mat4 = (x);                                  \
  mat->mat4.size = nsize;                           \
  mat->mat4.secondary = nsecondary;                 \
  glm_setmvalue2s(L, obj, mat);                     \
  luaC_checkGC(L);                                  \
  LUA_MLM_END

/*
** Operations on integer vectors (or floating-point vectors that are int-casted).
**
** @TODO: Once int-vectors become natively supported, this will require a rewrite
*/
#define INT_VECTOR_OPERATION(F, res, v, p2)                                                      \
  LUA_MLM_BEGIN                                                                                  \
  if (tt_p1 == tt_p2) {                                                                          \
    const glmVector &v2 = glm_vecvalue(p2);                                                      \
    glm_setvvalue2s(res, F(cast_vec4(v.v4, lua_Integer), cast_vec4(v2.v4, lua_Integer)), tt_p1); \
    return 1;                                                                                    \
  }                                                                                              \
  else if (tt_p2 == LUA_VNUMINT) {                                                               \
    glm_setvvalue2s(res, F(cast_vec4(v.v4, lua_Integer), ivalue(p2)), tt_p1);                    \
    return 1;                                                                                    \
  }                                                                                              \
  LUA_MLM_END

/* F(matNxM, matNxM): e.g., matNxM + matNxM. */
#define INDEP_MATRIX_OPERATION(L, F, res, m, p2)                                    \
  LUA_MLM_BEGIN                                                                     \
  const glmMatrix &m2 = glm_mvalue((p2));                                           \
  switch (m.size) {                                                                 \
    case 2: glm_newmvalue(L, res, F(m.m24, m2.m24), m.size, m.secondary); return 1; \
    case 3: glm_newmvalue(L, res, F(m.m34, m2.m34), m.size, m.secondary); return 1; \
    case 4: glm_newmvalue(L, res, F(m.m44, m2.m44), m.size, m.secondary); return 1; \
    default:                                                                        \
      break;                                                                        \
  }                                                                                 \
  LUA_MLM_END

/* F(value, matNxM): e.g., 3 * matNxM */
#define INDEP_SCALAR_MATRIX_OPERATION(L, F, res, s, p2)                           \
  LUA_MLM_BEGIN                                                                   \
  const glmMatrix &m2 = glm_mvalue(p2);                                           \
  switch (m2.size) {                                                              \
    case 2: glm_newmvalue(L, res, F(s, m2.m24), m2.size, m2.secondary); return 1; \
    case 3: glm_newmvalue(L, res, F(s, m2.m34), m2.size, m2.secondary); return 1; \
    case 4: glm_newmvalue(L, res, F(s, m2.m44), m2.size, m2.secondary); return 1; \
    default:                                                                      \
      break;                                                                      \
  }                                                                               \
  LUA_MLM_END

/* F(matNxM, value): e.g., matNxM * 3 */
#define MATRIX_SCALAR_OPERATION(L, F, res, m, p2)                              \
  LUA_MLM_BEGIN                                                                \
  const glm_Float s = glm_toflt(p2);                                           \
  switch (m.size) {                                                            \
    case 2: glm_newmvalue(L, res, F(m.m24, s), m.size, m.secondary); return 1; \
    case 3: glm_newmvalue(L, res, F(m.m34, s), m.size, m.secondary); return 1; \
    case 4: glm_newmvalue(L, res, F(m.m44, s), m.size, m.secondary); return 1; \
    default:                                                                   \
      break;                                                                   \
  }                                                                            \
  LUA_MLM_END

#define MATRIX_DIMS(A, B) m##A##B
#define MATRIX_MULTIPLICATION_OPERATION(L, res, m1, m2, C, R)                                     \
  LUA_MLM_BEGIN                                                                                   \
  switch (m2.size) {                                                                              \
    case 2: glm_newmvalue(L, res, (m1.MATRIX_DIMS(C, R) * m2.MATRIX_DIMS(2, C)), 2, R); return 1; \
    case 3: glm_newmvalue(L, res, (m1.MATRIX_DIMS(C, R) * m2.MATRIX_DIMS(3, C)), 3, R); return 1; \
    case 4: glm_newmvalue(L, res, (m1.MATRIX_DIMS(C, R) * m2.MATRIX_DIMS(4, C)), 4, R); return 1; \
    default:                                                                                      \
      break;                                                                                      \
  }                                                                                               \
  LUA_MLM_END

static int num_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  const glm_Float s = glm_toflt(p1);
  switch (event) {
    case TM_ADD: {
      switch (ttype(p2)) {
        case LUA_TVECTOR: glm_setvvalue2s(res, s + glm_vecvalue(p2).v4, ttypetag(p2)); return 1;
        case LUA_TMATRIX: {
          const glmMatrix &m2 = glm_mvalue(p2);
          if (m2.size == m2.secondary) {
            switch (m2.size) {
              case 2: glm_newmvalue(L, res, s + m2.m22, 2, 2); return 1;
              case 3: glm_newmvalue(L, res, s + m2.m33, 3, 3); return 1;
              case 4: glm_newmvalue(L, res, s + m2.m44, 4, 4); return 1;
              default:
                break;
            }
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    case TM_SUB: {
      switch (ttype(p2)) {
        case LUA_TVECTOR: glm_setvvalue2s(res, s - glm_vecvalue(p2).v4, ttypetag(p2)); return 1;
        case LUA_TMATRIX: {
          const glmMatrix &m2 = glm_mvalue(p2);
          if (m2.size == m2.secondary) {
            switch (m2.size) {
              case 2: glm_newmvalue(L, res, s - m2.m22, 2, 2); return 1;
              case 3: glm_newmvalue(L, res, s - m2.m33, 3, 3); return 1;
              case 4: glm_newmvalue(L, res, s - m2.m44, 4, 4); return 1;
              default:
                break;
            }
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    case TM_MUL: {
      switch (ttypetag(p2)) {
        case LUA_VVECTOR2:
        case LUA_VVECTOR3:
        case LUA_VVECTOR4: glm_setvvalue2s(res, s * glm_vecvalue(p2).v4, ttypetag(p2)); return 1;
        case LUA_VQUAT: glm_setvvalue2s(res, s * glm_quatvalue(p2).q, LUA_VQUAT); return 1;
        case LUA_VMATRIX: INDEP_SCALAR_MATRIX_OPERATION(L, operator*, res, s, p2); break;
        default:
          break;
      }
      break;
    }
    case TM_DIV: {
      switch (ttypetag(p2)) {
        case LUA_VVECTOR2:
        case LUA_VVECTOR3:
        case LUA_VVECTOR4: glm_setvvalue2s(res, s / glm_vecvalue(p2).v4, ttypetag(p2)); return 1;
        case LUA_VQUAT: glm_setvvalue2s(res, s * glm::inverse(glm_quatvalue(p2).q), LUA_VQUAT); return 1;
        case LUA_VMATRIX: INDEP_SCALAR_MATRIX_OPERATION(L, operator/, res, s, p2); break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  return 0;
}

static int vec_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  UNUSED(L);
  const glmVector &v = glm_vvalue(p1);
  const lu_byte tt_p1 = ttypetag(p1);
  const lu_byte tt_p2 = ttypetag(p2);
  switch (event) {
    case TM_ADD: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, operator+(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, operator+(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      break;
    }
    case TM_SUB: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, operator-(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, operator-(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      break;
    }
    case TM_MUL: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, operator*(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, operator*(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VQUAT) {
        switch (tt_p1) {
          case LUA_VVECTOR3: glm_setvvalue2s(res, v.v3 * glm_quatvalue(p2).q, LUA_VVECTOR3); return 1;
#if defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
          case LUA_VVECTOR4: {
            const glm::qua<glm_Float> x = glm::inverse(glm_quatvalue(p2).q);
            const glm::vec<4, glm_Float> result = glm::detail::compute_quat_mul_vec4<glm_Float, glm::defaultp, false>::call(x, v.v4);
            glm_setvvalue2s(res, result, LUA_VVECTOR4);
            return 1;
          }
#else
          case LUA_VVECTOR4: glm_setvvalue2s(res, v.v4 * glm_quatvalue(p2).q, LUA_VVECTOR4); return 1;
#endif
          default:
            break;
        }
      }
      else if (tt_p2 == LUA_VMATRIX && mvalue(p2).secondary == glm_dimensions(tt_p1)) {
        const glmMatrix &m2 = glm_mvalue(p2);
        switch (m2.size) {
          case 2: {
            switch (m2.secondary) {
              case 2: glm_setvvalue2s(res, v.v2 * m2.m22, LUA_VVECTOR2); return 1;
              case 3: glm_setvvalue2s(res, v.v3 * m2.m23, LUA_VVECTOR2); return 1;
              case 4: glm_setvvalue2s(res, v.v4 * m2.m24, LUA_VVECTOR2); return 1;
              default:
                break;
            }
            break;
          }
          case 3: {
            switch (m2.secondary) {
              case 2: glm_setvvalue2s(res, v.v2 * m2.m32, LUA_VVECTOR3); return 1;
              case 3: glm_setvvalue2s(res, v.v3 * m2.m33, LUA_VVECTOR3); return 1;
              case 4: glm_setvvalue2s(res, v.v4 * m2.m34, LUA_VVECTOR3); return 1;
              default:
                break;
            }
            break;
          }
          case 4: {
            switch (m2.secondary) {
              case 2: glm_setvvalue2s(res, v.v2 * m2.m42, LUA_VVECTOR4); return 1;
              case 3: glm_setvvalue2s(res, v.v3 * m2.m43, LUA_VVECTOR4); return 1;
              case 4: glm_setvvalue2s(res, v.v4 * m2.m44, LUA_VVECTOR4); return 1;
              default:
                break;
            }
            break;
          }
          default:
            break;
        }
      }
      // Rotating a point by only the rotation part of a transformation matrix.
      else if (tt_p1 == LUA_VVECTOR3 && tt_p2 == LUA_VMATRIX) {
        const glmMatrix &m2 = glm_mvalue(p2);
        if (tt_p1 == LUA_VVECTOR3 && tt_p2 == LUA_VMATRIX && m2.size == 4 && m2.secondary == 4) {
          glm_setvvalue2s(res, (glm::mat<4, 4, glm_Float>::col_type(v.v3, glm_Float(1)) * m2.m44), LUA_VVECTOR4);
          return 1;
        }
      }
      break;
    }
    case TM_DIV: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, operator/(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, operator/(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VMATRIX) {
        const glmMatrix &m2 = glm_mvalue(p2);
        if (m2.size == m2.secondary && tt_p2 == glm_variant(m2.size)) {
          switch (m2.size) {
            case LUA_VVECTOR2: glm_setvvalue2s(res, v.v2 / m2.m22, LUA_VVECTOR2); return 1;
            case LUA_VVECTOR3: glm_setvvalue2s(res, v.v3 / m2.m33, LUA_VVECTOR3); return 1;
            case LUA_VVECTOR4: glm_setvvalue2s(res, v.v4 / m2.m44, LUA_VVECTOR4); return 1;
            default:
              break;
          }
          break;
        }
      }
      break;
    }
    case TM_IDIV: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, glm::__objFloorDivide(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        const glm_Float s = glm_toflt(p2);
        switch (tt_p1) {
          case LUA_VVECTOR2: glm_setvvalue2s(res, glm::__objFloorDivide(v.v2, s), LUA_VVECTOR2); return 1;
          case LUA_VVECTOR3: glm_setvvalue2s(res, glm::__objFloorDivide(v.v3, s), LUA_VVECTOR3); return 1;
          case LUA_VVECTOR4: glm_setvvalue2s(res, glm::__objFloorDivide(v.v4, s), LUA_VVECTOR4); return 1;
          default:
            break;
        }
      }
      break;
    }
    case TM_MOD: {  // Using fmod for the same reasons described in llimits.h
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, glm::fmod(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, glm::fmod(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      break;
    }
    case TM_POW: {
      if (tt_p1 == tt_p2) {
        glm_setvvalue2s(res, glm::pow(v.v4, glm_vvalue(p2).v4), tt_p1);
        return 1;
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, glm::__objPow(v.v4, glm_toflt(p2)), tt_p1);
        return 1;
      }
      break;
    }
    case TM_UNM: {
      switch (tt_p1) {
        case LUA_VVECTOR2:
        case LUA_VVECTOR3:
        case LUA_VVECTOR4: glm_setvvalue2s(res, -v.v4, tt_p1); return 1;
        default:
          break;
      }
      break;
    }
    case TM_BAND: INT_VECTOR_OPERATION(operator&, res, v, p2); break;
    case TM_BOR: INT_VECTOR_OPERATION(operator|, res, v, p2); break;
    case TM_BXOR: INT_VECTOR_OPERATION(operator^, res, v, p2); break;
    case TM_SHL: INT_VECTOR_OPERATION(operator<<, res, v, p2); break;
    case TM_SHR: INT_VECTOR_OPERATION(operator>>, res, v, p2); break;
    default:
      break;
  }
  return 0;
}

static int quat_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  UNUSED(L);
  const glmVector &v = glm_vvalue(p1);
  const lu_byte tt_p2 = ttypetag(p2);
  switch (event) {
    case TM_UNM: glm_setvvalue2s(res, -v.q, LUA_VQUAT); return 1;
    case TM_ADD: {
      if (tt_p2 == LUA_VQUAT) {
        glm_setvvalue2s(res, glm_quatvalue(p1).q + glm_quatvalue(p2).q, LUA_VQUAT);
        return 1;
      }
      break;
    }
    case TM_SUB: {
      if (tt_p2 == LUA_VQUAT) {
#if defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
        const glm::qua<glm_Float> &x = glm_quatvalue(p1).q;
        const glm::qua<glm_Float> &y = glm_quatvalue(p2).q;
        const glm::qua<glm_Float> result = glm::detail::compute_quat_sub<glm_Float, glm::defaultp, false>::call(x, y);
        glm_setvvalue2s(res, result, LUA_VQUAT);
        return 1;
#else
        glm_setvvalue2s(res, glm_quatvalue(p1).q - glm_quatvalue(p2).q, LUA_VQUAT);
        return 1;
#endif
      }
      break;
    }
    case TM_POW: {
      if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        glm_setvvalue2s(res, glm::pow(v.q, glm_toflt(p2)), LUA_VQUAT);
        return 1;
      }
      break;
    }
    case TM_MUL: {
      switch (tt_p2) {
        case LUA_VNUMINT: glm_setvvalue2s(res, v.q * cast_glmfloat(ivalue(p2)), LUA_VQUAT); return 1;
        case LUA_VNUMFLT: glm_setvvalue2s(res, v.q * cast_glmfloat(fltvalue(p2)), LUA_VQUAT); return 1;
        case LUA_VVECTOR3: glm_setvvalue2s(res, v.q * glm_vecvalue(p2).v3, LUA_VVECTOR3); return 1;
#if defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
        case LUA_VVECTOR4: {
          const glm::vec<4, glm_Float> &y = glm_vecvalue(p2).v4;
          const glm::vec<4, glm_Float> result = glm::detail::compute_quat_mul_vec4<glm_Float, glm::defaultp, false>::call(v.q, y);
          glm_setvvalue2s(res, result, LUA_VVECTOR4);
          return 1;
        }
#else
        case LUA_VVECTOR4: glm_setvvalue2s(res, v.q * glm_vecvalue(p2).v4, LUA_VVECTOR4); return 1;
#endif
        case LUA_VQUAT: glm_setvvalue2s(res, v.q * glm_quatvalue(p2).q, LUA_VQUAT); return 1;
        default:
          break;
      }
      break;
    }
    case TM_DIV: {
      if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT) {
        const glm_Float s = glm_toflt(p2);

        glm::qua<glm_Float> result = glm::identity<glm::qua<glm_Float>>();
        if (glm::notEqual(s, cast_glmfloat(0), glm::epsilon<glm_Float>()))
          result = v.q / s;

        glm_setvvalue2s(res, result, LUA_VQUAT);
        return 1;
      }
      break;
    }
    default:
      break;
  }
  return 0;
}

static int mat_trybinTM(lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  const glmMatrix &m = glm_mvalue(p1);
  const lu_byte tt_p2 = ttypetag(p2);
  switch (event) {
    case TM_ADD: {
      if (tt_p2 == LUA_VMATRIX && m.size == mvalue(p2).size && m.secondary == mvalue(p2).secondary)
        INDEP_MATRIX_OPERATION(L, operator+, res, m, p2);
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT)
        MATRIX_SCALAR_OPERATION(L, operator+, res, m, p2);
      break;
    }
    case TM_SUB: {
      if (tt_p2 == LUA_VMATRIX && m.size == mvalue(p2).size && m.secondary == mvalue(p2).secondary)
        INDEP_MATRIX_OPERATION(L, operator-, res, m, p2);
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT)
        MATRIX_SCALAR_OPERATION(L, operator-, res, m, p2);
      break;
    }
    case TM_MUL: {
      if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT)
        MATRIX_SCALAR_OPERATION(L, operator*, res, m, p2);
      else if (tt_p2 == LUA_VVECTOR3 && m.size == 4 && m.secondary == 4) {
        const typename glm::mat<4, 4, glm_Float>::col_type &r =
#if defined(LUA_GLM_ROTATE_DIRECTION)
          // Transforms the given direction vector by this matrix m: M * (x, y, z, 0).
          m.m44 * glm::mat<4, 4, glm_Float>::col_type(glm_vvalue(p2).v3, glm_Float(0));
#else
          // Transforms the given point vector by this matrix M: M * (x, y, z, 1).
          m.m44 * glm::mat<4, 4, glm_Float>::col_type(glm_vvalue(p2).v3, glm_Float(1));
#endif
        glm_setvvalue2s(res, (glm::vec<3, glm_Float>(r.x, r.y, r.z)), LUA_VVECTOR3);
        return 1;
      }
      else if (tt_p2 == LUA_VMATRIX && m.size == mvalue(p2).secondary) {
        const glmMatrix &m2 = glm_mvalue(p2);
        switch (m.size) {
          case 2: {
            switch (m.secondary) {
              case 2: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 2, 2); break;
              case 3: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 2, 3); break;
              case 4: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 2, 4); break;
              default:
                break;
            }
            break;
          }
          case 3: {
            switch (m.secondary) {
              case 2: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 3, 2); break;
              case 3: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 3, 3); break;
              case 4: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 3, 4); break;
              default:
                break;
            }
            break;
          }
          case 4: {
            switch (m.secondary) {
              case 2: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 4, 2); break;
              case 3: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 4, 3); break;
              case 4: MATRIX_MULTIPLICATION_OPERATION(L, res, m, m2, 4, 4); break;
              default:
                break;
            }
            break;
          }
          default:
            break;
        }
      }
      else if (tt_p2 == glm_variant(m.size)) {
        const glmVector &v2 = glm_vvalue(p2);
        switch (m.size) {
          case 2: {
            switch (m.secondary) {
              case 2: glm_setvvalue2s(res, m.m22 * v2.v2, LUA_VVECTOR2); return 1;
              case 3: glm_setvvalue2s(res, m.m23 * v2.v2, LUA_VVECTOR3); return 1;
              case 4: glm_setvvalue2s(res, m.m24 * v2.v2, LUA_VVECTOR4); return 1;
              default:
                break;
            }
            break;
          }
          case 3: {
            switch (m.secondary) {
              case 2: glm_setvvalue2s(res, m.m32 * v2.v3, LUA_VVECTOR2); return 1;
              case 3: glm_setvvalue2s(res, m.m33 * v2.v3, LUA_VVECTOR3); return 1;
              case 4: glm_setvvalue2s(res, m.m34 * v2.v3, LUA_VVECTOR4); return 1;
              default:
                break;
            }
            break;
          }
          case 4: {
            switch (m.secondary) {
              case 2: glm_setvvalue2s(res, m.m42 * v2.v4, LUA_VVECTOR2); return 1;
              case 3: glm_setvvalue2s(res, m.m43 * v2.v4, LUA_VVECTOR3); return 1;
              case 4: glm_setvvalue2s(res, m.m44 * v2.v4, LUA_VVECTOR4); return 1;
              default:
                break;
            }
            break;
          }
          default:
            break;
        }
      }
      break;
    }
    case TM_DIV: {
      if (m.size == m.secondary && tt_p2 == LUA_VMATRIX) {  // operator/(matNxN, matNxN)
        const glmMatrix &m2 = glm_mvalue(p2);
        if (m.size == m2.size && m2.size == m2.secondary) {
          switch (m.size) {
            case 2: glm_newmvalue(L, res, m.m22 / m2.m22, 2, 2); return 1;
            case 3: glm_newmvalue(L, res, m.m33 / m2.m33, 3, 3); return 1;
            case 4: glm_newmvalue(L, res, m.m44 / m2.m44, 4, 4); return 1;
            default:
              break;
          }
        }
      }
      else if (tt_p2 == glm_variant(m.size)) {  // operator/(matrix, vector)
        const glmVector &v2 = glm_vvalue(p2);
        switch (m.size) {
          case 2: glm_setvvalue2s(res, m.m22 / v2.v2, LUA_VVECTOR2); return 1;
          case 3: glm_setvvalue2s(res, m.m33 / v2.v3, LUA_VVECTOR3); return 1;
          case 4: glm_setvvalue2s(res, m.m44 / v2.v4, LUA_VVECTOR4); return 1;
          default:
            break;
        }
      }
      else if (tt_p2 == LUA_VNUMINT || tt_p2 == LUA_VNUMFLT)
        MATRIX_SCALAR_OPERATION(L, operator/, res, m, p2);
      break;
    }
    case TM_UNM: {
      switch (m.size) {
        case 2: glm_newmvalue(L, res, -m.m24, m.size, m.secondary); return 1;
        case 3: glm_newmvalue(L, res, -m.m34, m.size, m.secondary); return 1;
        case 4: glm_newmvalue(L, res, -m.m44, m.size, m.secondary); return 1;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  return 0;
}

/* }================================================================== */
