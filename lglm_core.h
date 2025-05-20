/*
** $Id: lglm_core.h $
** Internal definitions for GLM objects
** See Copyright Notice in lua.h
*/

#ifndef lglm_core_h
#define lglm_core_h

#include "luaconf.h"
#include "lua.h"
#include "lobject.h"
#include "ltm.h"

/* Handle should-be-deprecated-instead-of-removed GLM macro GLM_FORCE_QUAT_DATA_WXYZ */
#if defined(GLM_FORCE_QUAT_DATA_WXYZ)
  #undef GLM_FORCE_QUAT_DATA_XYZW
#endif

/*
** This value should be greater than: (MAXNUMBER2STR * 16) + 64:
**  [d]mat4x4((%f, %f, %f, %f), (%f, %f, %f, %f), (%f, %f, %f, %f), (%f, %f, %f, %f))
*/
#define GLM_STRING_BUFFER 1024

/*
** Return the vector variant (tag) associated with 'dimensions'. Note, this
** function does not sanitize the input: assumes [1, 4].
*/
static LUA_INLINE lu_byte glm_variant (grit_length_t dimensions) {
  return l_unlikely(dimensions == 1) ? LUA_VVECTOR1 : cast_byte(LUA_TVECTOR | ((((dimensions - 2) & 0x3)) << 4));
}

/*
** Return the vector dimensions associated with the variant (tag). Note, this
** function does not sanitize the input: 'rtt' must be equal to
* makevariant(LUA_TVECTOR, X) for some 'X'
*/
static LUA_INLINE grit_length_t glm_dimensions (lu_byte rtt) {
  const grit_length_t dims = cast(grit_length_t, (rtt & 0x30) >> 4); /* variant bits 4-5 */
  return (rtt == LUA_VQUAT) ? 4 : (2 + dims); /* quat uses 3rd bit. */
}

/*
** {==================================================================
** Internal vector functions
** ===================================================================
*/

/* Fast path equivalent macros. */
#define glmVec_fastgeti(T, I, S) (vecgeti((T), (I), (S)) != LUA_TNONE)
#define glmVec_fastgets(T, K, S) \
  ((tsslen((K)) == 1) && (vecgets((T), getstr((K)), (S)) != LUA_TNONE))

/* Helper function for generalized vector int-access. */
static LUA_INLINE int vecgeti (const TValue *obj, lua_Integer n, StkId res) {
  glm::length_t _n = i_glmlen(n);
  if (l_likely(_n >= 1 && _n <= glm_dimensions(ttypetag(obj)))) {  /* Accessing vectors is 0-based */
#if !defined(GLM_FORCE_QUAT_DATA_XYZW)  /* quaternion has WXYZ layout */
    if (ttypetag(obj) == LUA_VQUAT) _n = (_n % 4) + 1;
#endif

    /*
    ** @NOTE: This approach assumes a packed x,y,z,w struct. This was changed
    ** from a switch statement after profiling access times.
    */
    setfltvalue(s2v(res), cast_num((&vvalue(obj).v4.x)[_n - 1]));
    return LUA_TNUMBER;
  }
  return LUA_TNONE;
}

/* Helper function for generalized vector string-access. */
static LUA_INLINE int vecgets (const TValue *obj, const char *k, StkId res) {
  const grit_length_t _d = glm_dimensions(ttypetag(obj));
  glm::length_t _n = 0;
  switch (*k) {
    case 'x': case 'r': case '1': _n = 1; break;
    case 'y': case 'g': case '2': _n = 2; break;
    case 'z': case 'b': case '3': _n = 3; break;
    case 'w': case 'a': case '4': _n = 4; break;
    case 'n': {  /* Dimension fields takes priority over metamethods */
      setivalue(s2v(res), cast(lua_Integer, _d));
      return LUA_TNUMBER;
    }
    default:
      break;
  }

  if (l_likely(_n >= 1 && _n <= _d)) {
#if !defined(GLM_FORCE_QUAT_DATA_XYZW)  /* quaternion has WXYZ layout */
    if (ttypetag(obj) == LUA_VQUAT) _n = (_n % 4) + 1;
#endif
    setfltvalue(s2v(res), cast_num((&vvalue(obj).v4.x)[_n - 1]));
    return LUA_TNUMBER;
  }
  return LUA_TNONE;
}

/* rawgeti variant for vector types */
LUAI_FUNC int glmVec_rawgeti(lua_State *L, const TValue *obj, lua_Integer n);

/* fast-path getfield variant for vector types */
LUAI_FUNC int glmVec_get1(lua_State *L, const TValue *obj, const char *k);

/* rawget variant for matrix types */
LUAI_FUNC int glmVec_rawget(lua_State *L, const TValue *obj);

/* OP_GETTABLE variant for vector types. */
LUAI_FUNC void glmVec_get (lua_State *L, const TValue *obj, TValue *key, StkId res);

/* OP_GETI variant for vector types. */
LUAI_FUNC void glmVec_geti (lua_State *L, const TValue *obj, lua_Integer key, StkId res);

/* Place the magnitude of the matrix (o) at the specified stack index (ra) */
LUAI_FUNC void glmVec_objlen (const TValue *obj, StkId res);

/* luaV_equalobj variant for matrix types */
LUAI_FUNC int glmVec_equalObj (lua_State *L, const TValue *o1, const TValue *o2, int rtt);

/* Attempt to concatenate a number (or vector) to another  */
LUAI_FUNC int glmVec_concat (const TValue *obj, const TValue *value, StkId res);

/* converts a vector to a string. */
LUAI_FUNC int glmVec_tostr (const TValue *obj, char *buff, size_t len);

/* Check whether key 'k1' is equal to the key in node 'n2'. */
LUAI_FUNC int glmVec_equalKey (const TValue *k1, const Node *n2, int rtt);

/* Compute the hash of the vector obj */
LUAI_FUNC size_t glmVec_hash (const TValue *obj);

/* Return true if each vector component is finite */
LUAI_FUNC int glmVec_isfinite (const TValue *obj);

/*
** Pops a key from the stack and pushes a <key, value> pair from the vector at
** the given stack index, the "next" pair after the given key.
**
** If there are no more elements in the vector, then returns 0 and pushes
** nothing.
*/
LUAI_FUNC int glmVec_next (const TValue *obj, StkId key);

/* trybinTM handler for GLM objects */
LUAI_FUNC int glm_trybinTM (lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);

/* }================================================================== */

/*
** {==================================================================
** Internal matrix functions
** ===================================================================
*/

#define luaMat_cast_m2(M) { (M)->x, (M)->y, 0, 0 }
#define luaMat_cast_m3(M) { (M)->x, (M)->y, (M)->z, 0 }
#define luaMat_cast_m4(M) M

/* Fast path equivalent macros. */
#define glmMat_fastgeti(T, I, S) (matgeti((T), (I), (S)) != LUA_TNONE)

/* Helper function for generalized matrix int-access. */
static LUA_INLINE int matgeti (const TValue *obj, lua_Integer n, StkId res) {
  const glm::length_t gidx = i_glmlen(n);
  const glmMatrix &m = mvalue(obj);
  if (l_likely(gidx >= 1 && gidx <= m.size)) {
    switch (m.secondary) {
      case 2: setvvalue(s2v(res), m.m42[gidx - 1], LUA_VVECTOR2); return LUA_VVECTOR2;
      case 3: setvvalue(s2v(res), m.m43[gidx - 1], LUA_VVECTOR3); return LUA_VVECTOR3;
      case 4: setvvalue(s2v(res), m.m44[gidx - 1], LUA_VVECTOR4); return LUA_VVECTOR4;
      default:
        break;
    }
  }
  return LUA_TNONE;
}

/* Create a new collectible matrix object, linking it to the allgc list */
LUAI_FUNC GCMatrix *glmMat_new (lua_State *L);

/* rawgeti variant for matrix types */
LUAI_FUNC int glmMat_rawgeti(lua_State *L, const TValue *obj, lua_Integer n);

/* rawget variant for matrix types */
LUAI_FUNC int glmMat_rawget(lua_State *L, const TValue *obj);

/* lua_rawset variant for matrix types */
LUAI_FUNC void glmMat_rawset (lua_State *L, const TValue *obj, TValue *key, TValue *val);

/* OP_GETTABLE variant for matrix types */
LUAI_FUNC void glmMat_get (lua_State *L, const TValue *obj, TValue *key, StkId res);

/* OP_GETI variant for vector types. */
LUAI_FUNC void glmMat_geti (lua_State *L, const TValue *obj, lua_Integer key, StkId res);

/* lua_settable variant for matrix types */
LUAI_FUNC void glmMat_set (lua_State *L, const TValue *obj, TValue *key, TValue *val);

/* OP_SETI variant for vector types. */
LUAI_FUNC void glmMat_seti (lua_State *L, const TValue *obj, lua_Integer key, TValue *val);

/* Place the magnitude of the matrix (o) at the specified stack index (ra) */
LUAI_FUNC void glmMat_objlen (const TValue *obj, StkId res);

/* converts a vector to a string. */
LUAI_FUNC int glmMat_tostr (const TValue *obj, char *buff, size_t len);

/*
** Pops a key from the stack and pushes a <key, value> pair from the matrix at
** the given stack index, the "next" pair after the given key.
**
** If there are no more elements in the vector, then returns 0 and pushes
** nothing.
*/
LUAI_FUNC int glmMat_next (const TValue *obj, StkId key);

/* luaV_equalobj variant for matrix types */
LUAI_FUNC int glmMat_equalObj (lua_State *L, const TValue *o1, const TValue *o2);

/* }================================================================== */

/*
** {==================================================================
** Miscellaneous
** ===================================================================
*/

/*
** Jenkins' one-at-a-time hash.
**
** It is the assume the string is properly delimited.
**
** @TODO: Change API to use lua_Unsigned
** @TODO: Compile-time option to allow 32-bit or 64-bit hashing.
*/
LUAI_FUNC lua_Integer luaO_HashString (const char* string, size_t length, int ignore_case);

/* }================================================================== */

#endif
