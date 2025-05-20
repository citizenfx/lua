/*
** $Id: lglmlib.cpp $
** GLM binding library
**
** Porting GLM-specific code between luaglm and cpp should be painless (or as
** painless as possible). With key differences being:
**  1. Static versus Dynamic typing (or 'auto' versus 'local' syntax);
**  2. Namespace resolution (::) versus table access (.) syntax. However, the
**    'GRIT_POWER_NAMESPACE_SEL' build option removes this case at the cost of
**    'goto' no longer being a language feature.
**  3. Constants in luaglm are stored by value, e.g. glm.pi. However in cpp,
**    they are templated constant expressions, e.g., glm::pi<float>().
**  4. Floating-point literals (1.0f) vs. lua_Number (1.0).
**
** See Copyright Notice in lua.h
*/

#define lglmlib_cpp
#define LUA_LIB

#include <functional>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext/scalar_integer.hpp>
#include <glm/ext/scalar_constants.hpp>

#include <lua.hpp>
#include <lglm.hpp>

#include "api.hpp"
#include "lglmlib.hpp"
#if defined(LUA_GLM_GEOM_EXTENSIONS)
  #include "geom.hpp"
#endif

/* Create a "{ function, glm_function }" registry instance */
#define REG_STR(C) #C
#define GLM_LUA_REG(NAME) \
  { "" REG_STR(NAME), GLM_NAME(NAME) }

/* Set a GLM constant value into the table on top of the stack. */
#define GLM_CONSTANT(L, Name)                             \
  LUA_MLM_BEGIN                                           \
  lua_pushnumber((L), GLM_NAMESPACE(Name)<glm_Number>()); \
  lua_setfield((L), -2, "" REG_STR(Name));                \
  LUA_MLM_END

/*
** {==================================================================
** Library
** ===================================================================
*/

/// <summary>
/// Pushes onto the stack the value GLM[k], where GLM is the binding library
/// stored as an upvalue to this metamethod.
/// </summary>
static int glm_libraryindex(lua_State *L) {
  lua_settop(L, 2);
  if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TFUNCTION) {  // Only functions can be accessed
    lua_pop(L, 1);
    lua_pushnil(L);
  }
  return 1;
}

#if defined(LUA_GLM_GEOM_EXTENSIONS)
/// <summary>
/// Helper function for creating meta/library tables.
/// </summary>
static void glm_newmetatable(lua_State *L, const char *meta_name, const char *lib_name, const luaL_Reg *lib) {
  if (luaL_newmetatable(L, meta_name)) {
    luaL_setfuncs(L, lib, 0);
    lua_setfield(L, -2, lib_name);  // Push the library into the GLM library table
  }
  else {
    lua_pop(L, 1);
  }
}
#endif

static const luaL_Reg luaglm_lib[] = {
  /* API */
  #include "lglmlib_reg.hpp"
  /* Lua mathlib */
  { "type", GLM_NULLPTR },
  { "random", GLM_NULLPTR },
  { "randomseed", GLM_NULLPTR },
  { "pi", GLM_NULLPTR },
  { "eps", GLM_NULLPTR },
  { "feps", GLM_NULLPTR },
  { "maxinteger", GLM_NULLPTR },
  { "mininteger", GLM_NULLPTR },
  { "huge", GLM_NULLPTR },
  { "FP_INFINITE", GLM_NULLPTR },
  { "FP_NAN", GLM_NULLPTR },
  { "FP_ZERO", GLM_NULLPTR },
  { "FP_SUBNORMAL", GLM_NULLPTR },
  { "FP_NORMAL", GLM_NULLPTR },
  /* Metamethods */
  { "__index", GLM_NULLPTR },
  /* Geometry API */
#if defined(LUA_GLM_GEOM_EXTENSIONS)
  { "aabb", GLM_NULLPTR },
  { "line", GLM_NULLPTR },
  { "ray", GLM_NULLPTR },
  { "triangle", GLM_NULLPTR },
  { "segment", GLM_NULLPTR },
  { "sphere", GLM_NULLPTR },
  { "plane", GLM_NULLPTR },
  { "polygon", GLM_NULLPTR },
  { "aabb2d", GLM_NULLPTR },
  { "segment2d", GLM_NULLPTR },
  { "circle", GLM_NULLPTR },
#endif
  /* Library Details */
  { "_NAME", GLM_NULLPTR },
  { "_VERSION", GLM_NULLPTR },
  { "_COPYRIGHT", GLM_NULLPTR },
  { "_DESCRIPTION", GLM_NULLPTR },
  { GLM_NULLPTR, GLM_NULLPTR }
};

/* Functions with a lib-glm upvalue */
static const luaL_Reg luaglm_metamethods[] = {
  { "__index", glm_libraryindex },
  { GLM_NULLPTR, GLM_NULLPTR }
};

extern "C" {
  LUAMOD_API int luaopen_glm(lua_State *L) {
    luaL_newlib(L, luaglm_lib);  // Initialize GLM library
#if defined(LUA_GLM_GEOM_EXTENSIONS)
    luaL_newlib(L, luaglm_aabblib); lua_setfield(L, -2, "aabb");
    luaL_newlib(L, luaglm_linelib); lua_setfield(L, -2, "line");
    luaL_newlib(L, luaglm_raylib); lua_setfield(L, -2, "ray");
    luaL_newlib(L, luaglm_trianglelib); lua_setfield(L, -2, "triangle");
    luaL_newlib(L, luaglm_segmentlib); lua_setfield(L, -2, "segment");
    luaL_newlib(L, luaglm_spherelib); lua_setfield(L, -2, "sphere");
    luaL_newlib(L, luaglm_planelib); lua_setfield(L, -2, "plane");
    luaL_newlib(L, luaglm_aabb2dlib); lua_setfield(L, -2, "aabb2d");
    luaL_newlib(L, luaglm_segment2dlib); lua_setfield(L, -2, "segment2d");
    luaL_newlib(L, luaglm_circlelib); lua_setfield(L, -2, "circle");
    // The "polygon" API is a reference to the polygon metatable stored in the registry.
    glm_newmetatable(L, LUA_GLM_POLYGON_META, "polygon", luaglm_polylib);
#endif
#if defined(CONSTANTS_HPP) || defined(EXT_SCALAR_CONSTANTS_HPP)
    GLM_CONSTANT(L, cos_one_over_two);
    GLM_CONSTANT(L, e);
    GLM_CONSTANT(L, epsilon);
    GLM_CONSTANT(L, euler);
    GLM_CONSTANT(L, four_over_pi);
    GLM_CONSTANT(L, golden_ratio);
    GLM_CONSTANT(L, half_pi);
    GLM_CONSTANT(L, ln_ln_two);
    GLM_CONSTANT(L, ln_ten);
    GLM_CONSTANT(L, ln_two);
    GLM_CONSTANT(L, one);
    GLM_CONSTANT(L, one_over_pi);
    GLM_CONSTANT(L, one_over_root_two);
    GLM_CONSTANT(L, one_over_two_pi);
    GLM_CONSTANT(L, quarter_pi);
    GLM_CONSTANT(L, root_five);
    GLM_CONSTANT(L, root_half_pi);
    GLM_CONSTANT(L, root_ln_four);
    GLM_CONSTANT(L, root_pi);
    GLM_CONSTANT(L, root_three);
    GLM_CONSTANT(L, root_two);
    GLM_CONSTANT(L, root_two_pi);
    GLM_CONSTANT(L, third);
    GLM_CONSTANT(L, three_over_two_pi);
    GLM_CONSTANT(L, two_over_pi);
    GLM_CONSTANT(L, two_over_root_pi);
    GLM_CONSTANT(L, two_pi);
    GLM_CONSTANT(L, two_thirds);
    GLM_CONSTANT(L, zero);
    GLM_CONSTANT(L, epsilon);
#endif
    GLM_CONSTANT(L, pi); /* lmathlib */
    lua_pushnumber(L, glm::epsilon<glm_Number>()); lua_setfield(L, -2, "eps");
    lua_pushnumber(L, static_cast<glm_Number>(glm::epsilon<glm_Float>())); lua_setfield(L, -2, "feps");
    lua_pushnumber(L, std::numeric_limits<glm_Number>::infinity()); lua_setfield(L, -2, "huge");
    lua_pushinteger(L, std::numeric_limits<lua_Integer>::max()); lua_setfield(L, -2, "maxinteger");
    lua_pushinteger(L, std::numeric_limits<lua_Integer>::min()); lua_setfield(L, -2, "mininteger");
    lua_pushinteger(L, FP_INFINITE); lua_setfield(L, -2, "FP_INFINITE"); /* c99 */
    lua_pushinteger(L, FP_NAN); lua_setfield(L, -2, "FP_NAN");
    lua_pushinteger(L, FP_ZERO); lua_setfield(L, -2, "FP_ZERO");
    lua_pushinteger(L, FP_SUBNORMAL); lua_setfield(L, -2, "FP_SUBNORMAL");
    lua_pushinteger(L, FP_NORMAL); lua_setfield(L, -2, "FP_NORMAL");

    /* Metamethods that reference the library as an upvalue */
    lua_pushvalue(L, -1);
    luaL_setfuncs(L, luaglm_metamethods, 1);

    /* Library details */
    lua_pushliteral(L, LUA_GLM_NAME); lua_setfield(L, -2, "_NAME");
    lua_pushliteral(L, LUA_GLM_VERSION); lua_setfield(L, -2, "_VERSION");
    lua_pushliteral(L, LUA_GLM_COPYRIGHT); lua_setfield(L, -2, "_COPYRIGHT");
    lua_pushliteral(L, LUA_GLM_DESCRIPTION); lua_setfield(L, -2, "_DESCRIPTION");

    /* Copy lmathlib functions not supported by library. */
    if (lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE) == LUA_TTABLE) {  // [..., glm, load_tab]
      if (lua_getfield(L, -1, LUA_MATHLIBNAME) == LUA_TTABLE) {  // [..., glm, load_tab, math_tab]
        lua_getfield(L, -1, "type"); lua_setfield(L, -4, "type");
        lua_getfield(L, -1, "random"); lua_setfield(L, -4, "random");
        lua_getfield(L, -1, "randomseed"); lua_setfield(L, -4, "randomseed");
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    /* If enabled, replace _G.math with the binding library */
#if defined(LUA_GLM_REPLACE_MATH)
    lua_pushvalue(L, -1);
    lua_setglobal(L, LUA_MATHLIBNAME);
#endif

    /* Setup default metatables */
    lua_lock(L);
    if (G(L)->mt[LUA_TVECTOR] == GLM_NULLPTR)
      G(L)->mt[LUA_TVECTOR] = hvalue(s2v(L->top.p - 1));
    if (G(L)->mt[LUA_TMATRIX] == GLM_NULLPTR)
      G(L)->mt[LUA_TMATRIX] = hvalue(s2v(L->top.p - 1));
    lua_unlock(L);

    return 1;
  }
}

/* }================================================================== */
