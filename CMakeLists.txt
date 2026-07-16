# See License in lua.h.
CMAKE_MINIMUM_REQUIRED( VERSION 3.27 )

PROJECT( lua CXX )

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CXX_EXTENSIONS OFF)

INCLUDE(CheckCXXCompilerFlag)
IF( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  SET(CMAKE_COMPILER_IS_CLANGXX 1) # using Clang or AppleClang
ENDIF()

################################################################################
# Path/Library Setup.
################################################################################

SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/etc/")
IF( NOT CMAKE_BUILD_TYPE )
  SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, \
  options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF()

SET(LIB_LUA_VER "54" CACHE STRING "Suffix of version")
IF( CMAKE_SIZEOF_VOID_P EQUAL 8 ) # TODO: Use w/ LUA_BIT32
  SET(LIB_SUFFIX "64" CACHE STRING "Suffix of the directory name, e.g. 64 for lib64")
ELSE( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  SET(LIB_SUFFIX "" CACHE STRING "Suffix of the directory name, e.g. 64 for lib64")
ENDIF()

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/" CACHE STRING "Library build output directory")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
SET(EXECUTABLE_OUTPUT_PATH         "${CMAKE_BINARY_DIR}/")

SET(CMAKE_INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
SET(CMAKE_INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
SET(CMAKE_INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")

################################################################################
# Compiler Setup
################################################################################

# Linux: Enable all instruction subsets supported by host.
# MSVC: Enables the use of Intel AVX2 if/when possible; @TODO: Follow Linux.
OPTION(LUA_NATIVE_ARCH "Enable compiler optimizations for the native processor architecture" OFF)

# Include additional optimizations for glm-specific vector and matrix operations;
OPTION(LUA_FAST_MATH "Enable fast math optimizations" OFF)

# A subset of LUA_NATIVE_ARCH for only the GLM bindings
OPTION(GLM_NATIVE_ARCH "Enable compiler optimizations for the native processor architecture" OFF)

# Include additional optimizations for glm-specific vector and matrix operations;
# This option is intended to be a subset of LUA_FAST_MATH
OPTION(GLM_FAST_MATH "Enable fast math optimizations" OFF)

# Compilation flags specific to the luaglm object.
SET(glm_nativeFlags "")

# Windows systems
IF( MSVC )
  OPTION(LUA_USE_WINDOWS "Windows specific build" ON)
  OPTION(LUA_BUILD_AS_DLL "Build Lua library as DLL" ON)
  OPTION(LUA_CRT_ALLOC "Replace the Windows CRT allocator with rpmalloc or mimalloc." OFF)
  IF( NOT( ${CMAKE_COMPILER_IS_CLANG} ) )
    MESSAGE(WARNING "MSVC detected! For MSVC/msbuild environments, it is advised to use the LLVM/ClangCL toolset")
  ENDIF()

  IF(CMAKE_BUILD_TYPE MATCHES Debug)
    ADD_COMPILE_OPTIONS(/Oi)
  ELSE()
    ADD_COMPILE_OPTIONS(/Ob2 /Oi /GF /GS- /EHsc /DNDEBUG)
  ENDIF()

  SET(glm_nativeFlags "${MSVC_OPT_FLAG} /GF /GS-")

  IF( LUA_FAST_MATH )
    ADD_COMPILE_OPTIONS(/fp:fast /fp:except-)
  ELSEIF( GLM_FAST_MATH )
    SET(glm_nativeFlags "${glm_nativeFlags} /fp:fast /fp:except-")
  ENDIF()

  IF( LUA_NATIVE_ARCH OR GLM_NATIVE_ARCH )
    CHECK_CXX_COMPILER_FLAG("/arch:AVX2" COMPILER_OPT_ARCH_AVX2_SUPPORTED)
    IF( LUA_NATIVE_ARCH AND COMPILER_OPT_ARCH_AVX2_SUPPORTED )
      ADD_COMPILE_OPTIONS(/arch:AVX2)
    ELSEIF( GLM_NATIVE_ARCH AND COMPILER_OPT_ARCH_AVX2_SUPPORTED )
      SET(glm_nativeFlags "${glm_nativeFlags} /arch:AVX2")
    ENDIF()
  ENDIF()

  ADD_COMPILE_OPTIONS(/nologo /Zc:__cplusplus /TP)
  ADD_COMPILE_DEFINITIONS(NOMINMAX)
  ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_DEPRECATE)
  ADD_COMPILE_DEFINITIONS(_CRT_NONSTDC_NO_DEPRECATE)

  SET(LUA_C_FLAGS "")
  SET(LUA_REL_FLAGS "${LUA_REL_FLAGS} /GL")
  SET(LUA_LINKFLAGS "${LUA_LINKFLAGS} /LTCG")

  # Visual Studio 2019 feature
  CHECK_CXX_COMPILER_FLAG("/Ob3" COMPILER_OPT_OB3)
  IF( COMPILER_OPT_OB3 AND NOT CMAKE_COMPILER_IS_CLANGXX )
    STRING(REGEX REPLACE "[-/]Ob[1-4]" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    STRING(REGEX REPLACE "[-/]Ob[1-4]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    STRING(REGEX REPLACE "[-/]Ob[1-4]" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    STRING(REGEX REPLACE "[-/]Ob[1-4]" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    STRING(REPLACE "/Ob2" "/Ob3" LUA_C_FLAGS "${LUA_C_FLAGS}")

    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Ob3")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob3")
  ENDIF()

  # Override the C runtime allocator on Windows
  # @TODO: snmalloc support
  IF( LUA_CRT_ALLOC )
    IF( CMAKE_BUILD_TYPE AND ${CMAKE_BUILD_TYPE} MATCHES Debug )
      MESSAGE(FATAL_ERROR "The Debug target isn't supported along with LUA_CRT_ALLOC!")
    ENDIF()

    STRING(REGEX REPLACE "/" "\\\\" LUA_CRT_ALLOC ${LUA_CRT_ALLOC})
    IF( LUA_CRT_ALLOC MATCHES "rpmalloc$" )
      ADD_DEFINITIONS(-DENABLE_OVERRIDE -DENABLE_PRELOAD)
      SET(LUA_REL_FLAGS "${LUA_REL_FLAGS} /MT")

      SET(LUA_ALLOCATOR_INCLUDE "${LUA_CRT_ALLOC}")
      SET(LUA_ALLOCATOR_FILES "${LUA_CRT_ALLOC}/rpmalloc/rpmalloc.c")

      # Search LUA_ALLOCATOR_FILES before the CRT library
      ADD_LINK_OPTIONS(/INCLUDE:realloc)
    ELSEIF( LUA_CRT_ALLOC MATCHES "mimalloc$" )
      SET(MIMALLOC_LIB "${LUA_CRT_ALLOC}/out/msvc-x64/Release/mimalloc-static.lib")
      SET(LUA_REL_FLAGS "${LUA_REL_FLAGS} /MD")
      ADD_COMPILE_DEFINITIONS(/FI${LUA_CRT_ALLOC}/include/mimalloc-override.h)

      # This runtime should never be using new/delete and all memory allocations
      # should be routed through lua_Alloc. Worst case scenario a LuaCrtAllocator
      # implementation exists for STL containers.
      # ADD_COMPILE_DEFINITIONS(/FI${LUA_CRT_ALLOC}/include/mimalloc-new-delete.h)

      LINK_LIBRARIES(${MIMALLOC_LIB})
      ADD_LINK_OPTIONS(/INCLUDE:mi_version)
    ELSE()
      MESSAGE(FATAL_ERROR "(${LUA_CRT_ALLOC}) Cannot find the path to for the CRT allocator; currently only rpmalloc and mimalloc are supported")
    ENDIF()
  ENDIF()

  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${LUA_C_FLAGS} ${LUA_REL_FLAGS} /Oy")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${LUA_C_FLAGS} ${LUA_REL_FLAGS}")

  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${LUA_C_FLAGS} ${LUA_REL_FLAGS} /Oy")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${LUA_C_FLAGS} ${LUA_REL_FLAGS}")

  SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${LUA_LINKFLAGS}")
  SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${LUA_LINKFLAGS}")

  SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${LUA_LINKFLAGS}")
  SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} ${LUA_LINKFLAGS}")

  SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${LUA_LINKFLAGS}")
  SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} ${LUA_LINKFLAGS}")

  # W4 to turn compiler warnings all the way up...
  STRING(REGEX REPLACE "[-/]W[1-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  STRING(REGEX REPLACE "[-/]W[1-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")

# Posix systems (including msys2/cygwin)
ELSE()
  OPTION(LUA_BIT32 "32bit compile" OFF)
  OPTION(LUA_USE_POSIX "Use POSIX functionality" ON)
  OPTION(LUA_USE_DLOPEN "Use dynamic linker to load modules" ON)

  SET(glm_nativeFlags "-O3 -DNDEBUG")
  IF( LUA_BIT32 )
    # ADD_REQUIRED_CXX_COMPILER_FLAG(-m32)
    ADD_COMPILE_DEFINITIONS(LUA_32BITS=1)
    IF( "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_COMPILER_IS_CLANG )
      SET(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
      SET(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
    ENDIF()
  ENDIF()

  # Compile everything, or lglm.cpp, or nothing with -ffast-math
  IF( LUA_FAST_MATH )
    ADD_COMPILE_OPTIONS(-ffast-math -fno-finite-math-only)
  ELSEIF( GLM_FAST_MATH )
    SET(glm_nativeFlags "${glm_nativeFlags} -ffast-math -fno-finite-math-only")
  ENDIF()

  # Compile everything, lglm.cpp, or nothing with common performance tweaks
  IF( LUA_NATIVE_ARCH )
    ADD_COMPILE_OPTIONS(-fno-stack-protector)
    IF (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      ADD_COMPILE_OPTIONS(-fno-plt)
    ENDIF()
  ELSEIF( GLM_NATIVE_ARCH )
    SET(glm_nativeFlags "${glm_nativeFlags} --fno-stack-protector")
    IF (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      SET(glm_nativeFlags "${glm_nativeFlags} -fno-plt")
    ENDIF()
  ENDIF()

  # Compile everything, or lglm.cpp, or nothing with -march=native
  IF( LUA_NATIVE_ARCH OR GLM_NATIVE_ARCH )
    CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_ARCH_NATIVE_SUPPORTED)
    IF( LUA_NATIVE_ARCH AND COMPILER_OPT_ARCH_NATIVE_SUPPORTED )
      ADD_COMPILE_OPTIONS(-march=native)
    ELSEIF( GLM_NATIVE_ARCH AND COMPILER_OPT_ARCH_NATIVE_SUPPORTED )
      SET(glm_nativeFlags "${glm_nativeFlags} -march=native")
    ENDIF()
  ENDIF()

  # Change default release build flags
  IF(NOT (${CMAKE_BUILD_TYPE} MATCHES Debug))
    ADD_COMPILE_OPTIONS(-DNDEBUG)
  ENDIF()

  # Temporary fix: CMake no longer defines WIN32 on MSYS.
  IF( CYGWIN OR MSYS )
    SET(LUA_BUILD_AS_DLL ON)
    ADD_COMPILE_DEFINITIONS(_WIN32)
    ADD_COMPILE_OPTIONS(-Wno-attributes)
  ENDIF()

  # Compiler options taken from Lua: lua/blob/master/makefile
  ADD_COMPILE_OPTIONS(
    -Wall -Wextra -Wshadow -Wsign-compare -Wundef -Wwrite-strings
    -Wdisabled-optimization -Wno-inline -Wunused-parameter -Wno-comment
    -Wno-unused-function -Wno-array-bounds -Wno-sign-conversion -Wformat=2
    # -Werror
    # -Wconversion
    # -Wsign-conversion
    # -Wcast-qual
  )

  IF (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    ADD_COMPILE_OPTIONS(
        -Wredundant-decls -Wdouble-promotion -Wstrict-overflow=2
        -Wno-format-nonliteral
    )
  ENDIF()

  IF( CMAKE_COMPILER_IS_CLANG )
    ADD_COMPILE_OPTIONS(-ferror-limit=5 -fno-common -Wno-deprecated
      # Clang Everything support;
      #-Weverything
      #-Wno-gnu-zero-variadic-macro-arguments
      #-Wno-covered-switch-default
      #-Wno-reserved-id-macro
      #-Wno-unused-macros
      #-Wno-padded
      #-Wno-old-style-cast
      #-Wno-extra-semi-stmt
      #-Wno-c++98-compat-pedantic
      #-Wno-zero-as-null-pointer-constant
      #-Wno-gnu-anonymous-struct
      #-Wno-gnu-label-as-value
      #-Wno-nested-anon-types
      #-Wno-enum-enum-conversion
      #-Wno-documentation-unknown-command
      #-Wno-float-equal
    )
  ELSEIF( CMAKE_COMPILER_IS_GNUCXX )
    ADD_COMPILE_OPTIONS(-fmax-errors=5 -Wduplicated-cond -Wrestrict -Wlogical-op
      -Wno-aggressive-loop-optimizations -Wno-ignored-qualifiers
      #-Wnull-dereference
      #-Wduplicated-branches
    )

    # Remove symbols and relocation information from the binding library
    # SET(glm_nativeFlags "${glm_nativeFlags} -s")
  ENDIF()

  # On POSIX Lua links to standard math library "m"
  IF( LUA_USE_POSIX )
    ADD_COMPILE_DEFINITIONS(LUA_USE_POSIX)
    LIST(APPEND LIBS m)
  ENDIF()

  # Link to dynamic linker library "dl"
  IF( LUA_USE_DLOPEN )
    ADD_COMPILE_DEFINITIONS(LUA_USE_DLOPEN)
    IF( NOT APPLE AND NOT _WIN32 )
      LIST(APPEND LIBS dl)
    ENDIF()
  ENDIF()
ENDIF()

################################################################################
# Build Options
################################################################################
# TODO luaconf: LUA_DIRSEP, Manual LUA_INT_TYPE/LUA_FLOAT_TYPE
# TODO lstrlib: LUA_MAXCAPTURES, MAXCCALLS, L_FMTFLAGS, LUAL_PACKPADBYTE
# TODO loadlib: LUA_CSUBSEP, LUA_LSUBSEP, LUA_LLE_FLAGS, LUA_PATH_VAR, LUA_CPATH_VAR
# TODO loslib: LUA_TMPNAMTEMPLATE

OPTION(LUA_INCLUDE_TEST "Include ltests.h" OFF)
OPTION(LUA_USE_APICHECK "Turns on several consistency checks on the C API" OFF)
OPTION(LUAI_ASSERT "Turn on all assertions inside Lua" OFF)
OPTION(HARDSTACKTESTS "Forces a reallocation of the stack at every point where the stack can be reallocated" OFF)
OPTION(HARDMEMTESTS "Forces a full collection at all points where the collector can run" OFF)
OPTION(EMERGENCYGCTESTS "Forces an emergency collection at every single allocation" OFF)
OPTION(EXTERNMEMCHECK "Removes internal consistency checking of blocks being deallocated " OFF)
OPTION(LUA_NOCVTN2S "Turn off automatic coercion from numbers to strings" OFF)
OPTION(LUA_NOCVTS2N "Turn off automatic coercion from strings to numbers" OFF)
OPTION(LUA_COMPAT_5_3 "Controls other macros for compatibility with Lua 5.3" ON)
OPTION(LUA_COMPAT_5_4_0 "Controls other macros for compatibility with Lua 5.4.0" OFF)
OPTION(LUA_COMPAT_MATHLIB "Controls the presence of several deprecated functions in the mathematical library." ON)
OPTION(LUA_USE_JUMPTABLE "Force the use of jump tables in the main interpreter loop" OFF)
OPTION(LUA_USE_LONGJMP "handles errors with _longjmp/_setjmp when compiling as C++" ON)
OPTION(LUA_CPP_EXCEPTIONS "unprotected calls are wrapped in typed C++ exceptions" OFF)

# IF( CMAKE_BUILD_TYPE STREQUAL Debug )
#   SET(LUA_INCLUDE_TEST ON)
# ENDIF()

SET(LUA_F2IMOD_ENUM F2Ieq F2Ifloor F2Iceil)
SET(LUA_F2IMOD F2Ieq CACHE STRING "Rounding modes for float-to-integer coercion")
SET_PROPERTY(CACHE LUA_F2IMOD PROPERTY STRINGS ${LUA_F2IMOD_ENUM})

SET(LUAI_MAXCCALLS CACHE STRING "Minimum size for string buffer")
SET(MAXIWTHABS CACHE STRING
  "MAXimum number of successive Instructions WiTHout ABSolute line information \
  Must be a power of two to allow fast divisions."
)

SET(LUA_MINBUFFER CACHE STRING
  "Maximum depth for nested C calls, syntactical nested non-terminals, and \
  other features implemented through recursion in C. (Value must fit in a \
  16-bit unsigned integer. It must also be compatible with the size of the C stack.)"
)

SET(LUAI_MAXSHORTLEN CACHE STRING
  "Maximum length for short strings, that is, strings that are internalized. \
  (Cannot be smaller than reserved words or tags for metamethods, as these \
  strings must be internalized; #(\"function\") = 8, #(\"__newindex\") = 10.)"
)

SET(MINSTRTABSIZE CACHE STRING
  "Initial size for the string table (must be power of 2). The Lua core alone \
  registers ~50 strings (reserved words + metaevent keys + a few others). \
  Libraries would typically add a few dozens more."
)

SET(STRCACHE_M CACHE STRING "")
SET(STRCACHE_N CACHE STRING
  "Size of cache for strings in the API. \'N\' is the number of sets (better \
  be a prime) and \"M\" is the size of each set (M == 1 makes a direct cache.)"
)

IF( LUA_USE_APICHECK )
  ADD_COMPILE_DEFINITIONS(LUA_USE_APICHECK)
ENDIF()

IF( LUAI_ASSERT )
  ADD_COMPILE_DEFINITIONS(LUAI_ASSERT)
ENDIF()

IF( HARDSTACKTESTS )
  ADD_COMPILE_DEFINITIONS(HARDSTACKTESTS)
ENDIF()

IF( HARDMEMTESTS )
  ADD_COMPILE_DEFINITIONS(HARDMEMTESTS)
ENDIF()

IF( EMERGENCYGCTESTS )
  ADD_COMPILE_DEFINITIONS(EMERGENCYGCTESTS)
ENDIF()

IF( EXTERNMEMCHECK )
  ADD_COMPILE_DEFINITIONS(EXTERNMEMCHECK)
ENDIF()

IF( LUA_NOCVTN2S )
  ADD_COMPILE_DEFINITIONS(LUA_NOCVTN2S)
ENDIF()

IF( LUA_NOCVTS2N )
  ADD_COMPILE_DEFINITIONS(LUA_NOCVTS2N)
ENDIF()

LIST(FIND LUA_F2IMOD_ENUM ${LUA_F2IMOD} index)
IF( NOT index EQUAL -1 )
  ADD_COMPILE_DEFINITIONS(LUA_FLOORN2I=${LUA_F2IMOD})
ENDIF()

IF( LUA_COMPAT_5_3 )
  ADD_COMPILE_DEFINITIONS(LUA_COMPAT_5_3)
  SET(LUA_COMPAT_MATHLIB OFF CACHE INTERNAL "" FORCE)
ENDIF()

IF( LUA_COMPAT_5_4_0 )
  ADD_COMPILE_DEFINITIONS(LUA_COMPAT_5_4_0)
ENDIF()

IF( LUA_COMPAT_MATHLIB )
  ADD_COMPILE_DEFINITIONS(LUA_COMPAT_MATHLIB)
ENDIF()

IF( LUA_USE_JUMPTABLE )
  ADD_COMPILE_DEFINITIONS(LUA_USE_JUMPTABLE=1)
ENDIF()

IF( LUA_USE_LONGJMP )
  ADD_COMPILE_DEFINITIONS(LUA_USE_LONGJMP)
ENDIF()

IF( LUA_CPP_EXCEPTIONS )
  ADD_COMPILE_DEFINITIONS(LUA_CPP_EXCEPTIONS)
ENDIF()

IF( LUAI_MAXCCALLS )
  ADD_COMPILE_DEFINITIONS(LUAI_MAXCCALLS=${LUAI_MAXCCALLS})
ENDIF()

IF( LUA_MINBUFFER )
  ADD_COMPILE_DEFINITIONS(LUA_MINBUFFER=${LUA_MINBUFFER})
ENDIF()

IF( LUAI_MAXSHORTLEN )
  ADD_COMPILE_DEFINITIONS(LUAI_MAXSHORTLEN=${LUAI_MAXSHORTLEN})
ENDIF()

LIST(FIND LUA_F2IMOD_ENUM ${LUA_F2IMOD} index)
IF( NOT index EQUAL -1 )
  ADD_COMPILE_DEFINITIONS(LUA_FLOORN2I=${LUA_F2IMOD})
ENDIF()

# Definitions redefined during testing
IF( NOT LUA_INCLUDE_TEST )
  IF( MAXIWTHABS )
    ADD_COMPILE_DEFINITIONS(MAXIWTHABS=${MAXIWTHABS})
  ENDIF()

  IF( MINSTRTABSIZE )
    ADD_COMPILE_DEFINITIONS(MINSTRTABSIZE=${MINSTRTABSIZE})
  ENDIF()

  IF( STRCACHE_N )
    ADD_COMPILE_DEFINITIONS(STRCACHE_N=${STRCACHE_N})
  ENDIF()

  IF( STRCACHE_M )
    ADD_COMPILE_DEFINITIONS(STRCACHE_M=${STRCACHE_M})
  ENDIF()
ENDIF()

#######################################
# Lua Extensions & PowerPatches
#######################################

OPTION(LUA_C99_MATHLIB "Enable extended C99/C++11 " ON)
OPTION(LUA_SANDBOX "Initial sandboxing support (hide additional ldblib fields)" OFF)
OPTION(LUA_NO_DUMP "Disable the dump module (dump Lua functions as precompiled chunk) " OFF)
OPTION(LUA_NO_BYTECODE "Disables the usage of lua_load with binary (precompiled) chunks" OFF)
OPTION(LUA_NO_PARSER "Compile the Lua core so it does not contain the parsing \
  modules (lcode, llex, lparser); only binary files and strings, precompiled \
  with luac, can be loaded." OFF
)

OPTION(LUA_GLM_NUMBER_TYPE "Use lua_Number as the vector primitive; float otherwise" OFF)
OPTION(LUA_GLM_EPS_EQUAL "luaV_equalobj uses approximately equal (within glm::epsilon) for vector/matrix types (beware of hashing caveats)" OFF)
OPTION(LUA_GLM_ROTATE_DIRECTION "How operator*(glm::mat4x4, glm::vec3) is handled" OFF)
OPTION(LUA_GLM_APICHECK
  "Throw runtime errors when accessing matrices/vectors with invalid \
  keys/key-types. Otherwise, follow tables and set the result to nil" OFF
)

OPTION(GRIT_COMPAT_IPAIRS "Reintroduce compatibility for the ``__ipairs`` metamethod that was deprecated in 5.3 and removed in 5.4" ON)
OPTION(GRIT_POWER_DEFER "Enable the defer statement" OFF)
OPTION(GRIT_POWER_DEFER_OLD "Import func2close from ltests.h into the base library as `defer`" ON)
OPTION(GRIT_POWER_JOAAT "Enable compile time Jenkins' one-at-a-time hashing" ON)
OPTION(GRIT_POWER_COMPOUND "Add compound operators (+=, -=, *=, /=, <<=, >>=, &=, |=, and ^=) to the language." ON)
OPTION(GRIT_POWER_INTABLE "Support for unpacking named values from tables using the in keyword" ON)
OPTION(GRIT_POWER_TABINIT "Syntactic sugar to improve the syntax for specifying sets" ON)
OPTION(GRIT_POWER_SAFENAV "An indexing operation that suppresses errors on accesses into undefined table" ON)
OPTION(GRIT_POWER_CCOMMENT "Support for C-style block comments" ON)
OPTION(GRIT_POWER_NAMESPACE_SEL "Allow double-colon tokens to be used as field selection (e.g., CPP namespaces)" OFF)
OPTION(GRIT_POWER_WOW "Expose lua_createtable and other WowLua compatibility functions" ON)
OPTION(GRIT_POWER_CHRONO "Enable nanosecond resolution timers and x86 rdtsc sampling" ON)
OPTION(GRIT_POWER_EACH "__iter metamethod support; see documentation" ON)
OPTION(GRIT_POWER_BLOB "Enable an API to create non-internalized contiguous byte sequences" ON)
OPTION(GRIT_POWER_READLINE_HISTORY "" ON)

OPTION(LUA_CFX_SANITIZE_POINTERS "Avoid exposing the raw address of pointers" ON)

IF( LUA_C99_MATHLIB )
  ADD_COMPILE_DEFINITIONS(LUA_C99_MATHLIB)
ENDIF()

IF( LUA_SANDBOX )
  ADD_COMPILE_DEFINITIONS(LUA_SANDBOX)
ENDIF()

IF( LUA_GLM_NUMBER_TYPE )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_NUMBER_TYPE)
ENDIF()

IF( LUA_GLM_EPS_EQUAL )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_EPS_EQUAL)
ENDIF()

IF( LUA_GLM_APICHECK )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_APICHECK)
ENDIF()

IF( GRIT_POWER_DEFER )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_DEFER)
ELSEIF( GRIT_POWER_DEFER_OLD )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_DEFER_OLD)
ENDIF()

IF( GRIT_POWER_JOAAT )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_JOAAT)
ENDIF()

IF( GRIT_COMPAT_IPAIRS )
  ADD_COMPILE_DEFINITIONS(GRIT_COMPAT_IPAIRS)
ENDIF()

IF( GRIT_POWER_COMPOUND )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_COMPOUND)
ENDIF()

IF( GRIT_POWER_INTABLE )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_INTABLE)
ENDIF()

IF( GRIT_POWER_TABINIT )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_TABINIT)
ENDIF()

IF( GRIT_POWER_SAFENAV )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_SAFENAV)
ENDIF()

IF( GRIT_POWER_CCOMMENT )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_CCOMMENT)
ENDIF()

IF( GRIT_POWER_NAMESPACE_SEL )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_NAMESPACE_SEL)
ENDIF()

IF( GRIT_POWER_EACH )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_EACH)
ENDIF()

IF( GRIT_POWER_BLOB )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_BLOB)
ENDIF()

IF( GRIT_POWER_WOW )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_WOW)
ENDIF()

IF( GRIT_POWER_CHRONO )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_CHRONO)
ENDIF()

IF( GRIT_POWER_READLINE_HISTORY )
  ADD_COMPILE_DEFINITIONS(GRIT_POWER_READLINE_HISTORY)
ENDIF()

IF( LUA_CFX_SANITIZE_POINTERS )
  ADD_COMPILE_DEFINITIONS(LUA_CFX_SANITIZE_POINTERS)
ENDIF()

#######################################
# GLM Options
#######################################

OPTION(GLM_FORCE_MESSAGES "Platform auto detection and default configuration" OFF)
OPTION(GLM_FORCE_INLINE "Force inline" ON)
OPTION(GLM_FORCE_ALIGNED_GENTYPES "Force GLM to enable aligned types" OFF)
OPTION(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES "Force GLM to use aligned types by default" OFF)
OPTION(GLM_FORCE_INTRINSICS "Using SIMD optimizations" ON)
OPTION(GLM_FORCE_XYZW_ONLY "Only exposes x, y, z and w components" OFF)
OPTION(GLM_FORCE_LEFT_HANDED "Force left handed coordinate system" OFF)
OPTION(GLM_FORCE_DEPTH_ZERO_TO_ONE "Force the use of a clip space between 0 to 1" OFF)
OPTION(GLM_FORCE_SIZE_T_LENGTH "length_t is a typedef of size_t, otherwise length_t is a typedef of int like GLSL defines it." OFF)
OPTION(GLM_FORCE_UNRESTRICTED_GENTYPE "Removing genType restriction" OFF)
OPTION(GLM_FORCE_SILENT_WARNINGS "Silent C++ warnings from language extensions" OFF)

# Formerly known as 'GLM_FORCE_QUAT_DATA_WXYZ', however, changed without support
# for backwards compatibility in-mind; must be compensated for in source.
OPTION(GLM_FORCE_QUAT_DATA_XYZW "Force GLM to store quat data as x,y,z,w instead of w,x,y,z" OFF)
# GLM_FORCE_PRECISION_{ LOWP_FLOAT, MEDIUMP_FLOAT, HIGHP_FLOAT }

ADD_COMPILE_DEFINITIONS(GLM_ENABLE_EXPERIMENTAL)

# lua-glm Options
OPTION(GLM_FORCE_Z_UP "Unit up vector is along the Z-axis (Y-axis otherwise)" ON)
OPTION(LUA_GLM_INCLUDE_ALL "Include all GLM modules, glm/gtc/gtx" ON)
OPTION(LUA_GLM_INCLUDE_EXT "Include ext headers: Stable extensions not specified by GLSL specification " OFF)
OPTION(LUA_GLM_INCLUDE_GTC "Include gtc headers: Recommended extensions not specified by GLSL specification" OFF)
OPTION(LUA_GLM_INCLUDE_GTX "Include gtx headers: Experimental extensions not specified by GLSL specification" OFF)
OPTION(LUA_GLM_ALIASES "Include all function aliases" ON)
OPTION(LUA_GLM_REPLACE_MATH "Replace the global math library on library initialization" OFF)
OPTION(LUA_GLM_GEOM_EXTENSIONS "Extend geometry API" ON)
OPTION(LUA_GLM_RECYCLE "Recycle trailing (unused) function parameters" ON)
OPTION(LUA_GLM_FORCED_RECYCLE
  "All function results must be preallocated, i.e., functions that return \
  matrices must have preallocated matrix trailing function arguments." OFF
)
OPTION(LUA_GLM_DRIFT "Experiment: implicitly correct floating point drift in direction vectors and quaternions" OFF)

IF( GLM_FORCE_MESSAGES )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_MESSAGES)
ENDIF()

# g++ doesn't support force-inlined vardiac functions, see glm::to_string.
# Technically this is a GLM error that should be reported

IF( GLM_FORCE_INLINE )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_INLINE)
ENDIF()

IF( GLM_FORCE_DEFAULT_ALIGNED_GENTYPES )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
ELSEIF( GLM_FORCE_ALIGNED_GENTYPES )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_ALIGNED_GENTYPES)
ENDIF()

IF( GLM_FORCE_INTRINSICS )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_INTRINSICS)
ENDIF()

IF( GLM_FORCE_XYZW_ONLY )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_XYZW_ONLY)
ENDIF()

IF( GLM_FORCE_LEFT_HANDED )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_LEFT_HANDED)
ENDIF()

IF( GLM_FORCE_Z_UP )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_Z_UP)
ENDIF()

IF( GLM_FORCE_DEPTH_ZERO_TO_ONE )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_DEPTH_ZERO_TO_ONE)
ENDIF()

IF( GLM_FORCE_SIZE_T_LENGTH )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_SIZE_T_LENGTH)
ENDIF()

IF( GLM_FORCE_UNRESTRICTED_GENTYPE )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_UNRESTRICTED_GENTYPE)
ENDIF()

IF( GLM_FORCE_SILENT_WARNINGS )
  ADD_COMPILE_DEFINITIONS(GLM_FORCE_SILENT_WARNINGS)
ENDIF()

IF( GLM_FORCE_QUAT_DATA_XYZW )
  MESSAGE(FATAL_ERROR "Disable GLM_FORCE_QUAT_DATA_XYZW" )
  # ADD_COMPILE_DEFINITIONS(GLM_FORCE_QUAT_DATA_XYZW)
ENDIF()

IF( LUA_GLM_INCLUDE_EXT )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_EXT)
ENDIF()

IF( LUA_GLM_INCLUDE_GTC )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_GTC)
ENDIF()

IF( LUA_GLM_INCLUDE_GTX )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_GTX)
ENDIF()

IF( LUA_GLM_INCLUDE_ALL )
  # Future-proof by including the above options
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_EXT)
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_GTC)
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_GTX)
  ADD_COMPILE_DEFINITIONS(LUA_GLM_INCLUDE_ALL)
ENDIF()

IF( LUA_GLM_ALIASES )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_ALIASES)
ENDIF()

IF( LUA_GLM_REPLACE_MATH )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_REPLACE_MATH)
ENDIF()

IF( LUA_GLM_GEOM_EXTENSIONS )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_GEOM_EXTENSIONS)
ENDIF()

IF( LUA_GLM_RECYCLE )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_RECYCLE)
  IF( LUA_GLM_FORCED_RECYCLE )
    ADD_COMPILE_DEFINITIONS(LUA_GLM_FORCED_RECYCLE)
  ENDIF()
ENDIF()

IF( LUA_GLM_DRIFT )
  ADD_COMPILE_DEFINITIONS(LUA_GLM_DRIFT)
ENDIF()

################################################################################
# Compilation
################################################################################

OPTION(ONE_LUA "Compile Lua core, libraries, and interpreter in a single file" OFF)
OPTION(LUA_INCLUDE_LIBGLM "Build lglm into the runtime and include it in the initialization libraries" OFF)

INCLUDE_DIRECTORIES( . )
INCLUDE_DIRECTORIES( ./libs/glm/ )
INCLUDE_DIRECTORIES( ./libs/glm-binding/ )

SET(LUA_PUBLIC_HDRS luaconf.h lua.h lauxlib.h lualib.h lua.hpp lglm.hpp )

SET(SRC_LUA lua.c)
SET(SRC_LUAC luac.c)
SET(SRC_ONELUA onelua.c)
SET(SRC_LUAGLM lglm.cpp)
SET(SRC_LIB
  lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c
  ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c
  loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c
  lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c
)

SET(SRC_LIBGLM libs/glm-binding/lglmlib.cpp)

IF( LUA_INCLUDE_TEST )
  ADD_COMPILE_DEFINITIONS(LUA_USER_H="ltests.h")
  SET(SRC_LIB ${SRC_LIB} ltests.c)
ENDIF()

SET_SOURCE_FILES_PROPERTIES(${SRC_LIB} ${SRC_LUA} ${SRC_LUAC} ${SRC_ONELUA} ${SRC_LUAGLM} PROPERTIES LANGUAGE CXX )

IF( LUA_INCLUDE_LIBGLM )
  ADD_COMPILE_DEFINITIONS(LUA_INCLUDE_LIBGLM)
  SET(SRC_LUAGLM ${SRC_LUAGLM} ${SRC_LIBGLM})
ENDIF()

IF( NOT CMAKE_BUILD_TYPE STREQUAL Debug )
  SET_SOURCE_FILES_PROPERTIES(${SRC_ONELUA} PROPERTIES COMPILE_FLAGS ${glm_nativeFlags})
  SET_SOURCE_FILES_PROPERTIES(${SRC_LUAGLM} PROPERTIES COMPILE_FLAGS ${glm_nativeFlags})
ENDIF()

#######################################
# Library Properties
#######################################

# @TODO: Ideally, the shared and static libraries would only be compiled once
# with a  'shared' position independent object library. However, LUA_NO_DUMP,
# LUA_NO_BYTECODE, and LUA_NO_PARSER are compile time options instead of
# link-time options.

# Create dynamic library (dll, so)
IF( NOT LUA_INCLUDE_TEST AND NOT ONE_LUA )
  ADD_LIBRARY(liblua SHARED ${SRC_LIB} ${SRC_LUAGLM} ${LUA_RC})

  TARGET_LINK_LIBRARIES(liblua ${LIBS})
  TARGET_INCLUDE_DIRECTORIES(liblua SYSTEM PRIVATE ${LUA_ALLOCATOR_INCLUDE})
  TARGET_SOURCES(liblua PRIVATE ${LUA_ALLOCATOR_FILES})

  SET_TARGET_PROPERTIES(liblua PROPERTIES OUTPUT_NAME lua${LIB_LUA_VER} CLEAN_DIRECT_OUTPUT 1)
  IF( LUA_BUILD_AS_DLL )
    TARGET_COMPILE_DEFINITIONS(liblua PUBLIC LUA_BUILD_AS_DLL)
  ENDIF()

  IF( LUA_NO_DUMP )
    TARGET_COMPILE_DEFINITIONS(liblua PUBLIC LUA_NO_DUMP)
  ENDIF()

  IF( LUA_NO_BYTECODE )
    TARGET_COMPILE_DEFINITIONS(liblua PUBLIC LUA_NO_BYTECODE)
  ENDIF()

  IF( LUA_NO_PARSER )
    TARGET_COMPILE_DEFINITIONS(liblua PUBLIC LUA_NO_PARSER)
  ENDIF()
ENDIF()

# Create static library
IF( NOT ONE_LUA )
  ADD_LIBRARY(liblua_static STATIC ${SRC_LIB} ${SRC_LUAGLM})
  TARGET_INCLUDE_DIRECTORIES(liblua_static SYSTEM PRIVATE ${LUA_ALLOCATOR_INCLUDE})
  TARGET_SOURCES(liblua_static PRIVATE ${LUA_ALLOCATOR_FILES})

  TARGET_LINK_LIBRARIES(liblua_static ${LIBS})
  SET_TARGET_PROPERTIES(liblua_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
  SET_TARGET_PROPERTIES(liblua_static PROPERTIES OUTPUT_NAME lua${LIB_LUA_VER}_static CLEAN_DIRECT_OUTPUT 1)
ELSE()
  ADD_COMPILE_DEFINITIONS(LUA_INCLUDE_LIBGLM)
ENDIF()

# Add extra rc files to the windows build
IF( MSVC OR CYGWIN OR MSYS )
  IF( NOT CMAKE_RC_COMPILER )
    SET(CMAKE_RC_COMPILER windres.exe)
  ENDIF()

  SET(LUA_RC etc/lua.rc)
ENDIF()

# lib-glm
IF( NOT LUA_INCLUDE_LIBGLM AND NOT ONE_LUA )
  ADD_LIBRARY(glm MODULE ${SRC_LIBGLM})
  TARGET_INCLUDE_DIRECTORIES(glm PRIVATE ${INCLUDE_DIRECTORIES})
  TARGET_LINK_LIBRARIES(glm ${LIBS} liblua_static)

  # SET_TARGET_PROPERTIES(glm PROPERTIES LINK_FLAGS_RELEASE -s)
  SET_TARGET_PROPERTIES(glm PROPERTIES POSITION_INDEPENDENT_CODE ON)
  SET_TARGET_PROPERTIES(glm PROPERTIES PREFIX "" OUTPUT_NAME "glm" CLEAN_DIRECT_OUTPUT 1)
  IF( LUA_BUILD_AS_DLL )
    TARGET_COMPILE_DEFINITIONS(glm PUBLIC LUA_BUILD_AS_DLL)
  ENDIF()
ENDIF()

#######################################
# Stand-Alone Interpreter
#######################################

IF( ONE_LUA )
  ADD_EXECUTABLE(lua ${SRC_ONELUA} ${LUA_RC})
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC MAKE_LUA)
  TARGET_INCLUDE_DIRECTORIES(lua SYSTEM PRIVATE ${LUA_ALLOCATOR_INCLUDE})
  TARGET_SOURCES(lua PRIVATE ${LUA_ALLOCATOR_FILES})
ELSE()
  ADD_EXECUTABLE(lua ${SRC_LUA} ${LUA_RC})
ENDIF()

SET(LUA_PROGNAME CACHE STRING "Interpreter program name")
SET(LUA_PROMPT CACHE STRING "Default prompt used by stand-alone Lua")
SET(LUA_PROMPT2 CACHE STRING "Default continuation prompt used by stand-alone Lua")
SET(LUA_MAXINPUT CACHE STRING "The maximum length for an input line in the stand-alone interpreter.")

IF( LUA_PROGNAME )
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC LUA_PROGNAME="${LUA_PROGNAME}")
ENDIF()

IF( LUA_PROMPT )
  # echo ${PROMPT}: "└> "
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC LUA_PROMPT="${LUA_PROMPT}")
ENDIF()

IF( LUA_PROMPT2 )
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC LUA_PROMPT2="${LUA_PROMPT2}")
ENDIF()

IF( LUA_MAXINPUT )
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC LUA_MAXINPUT=${LUA_MAXINPUT})
ENDIF()

# Ensure libreadline linkage
IF( APPLE )
  SET(SKIP_BUILD_RPATH TRUE)
  SET(CMAKE_MACOSX_RPATH 1)

  LIST(APPEND LUA_LIBS readline)
  TARGET_COMPILE_DEFINITIONS(lua PUBLIC "LUA_USE_READLINE")
ELSEIF( UNIX )
  FIND_PACKAGE (Readline)
  IF( READLINE_FOUND )
    SET(CMAKE_REQUIRED_LIBRARIES "readline")

    LIST(APPEND LUA_LIBS readline)
    TARGET_COMPILE_DEFINITIONS(lua PUBLIC "LUA_USE_READLINE")
  ENDIF()
ENDIF()

# Lua Libraries
IF( ONE_LUA )
  TARGET_LINK_LIBRARIES(lua ${LIBS} ${LUA_LIBS})
ELSE()
  SET(interpretor_target liblua_static)
  TARGET_LINK_LIBRARIES(lua ${interpretor_target} ${LIBS} ${LUA_LIBS})
ENDIF()

#######################################
# Compiler
#######################################

# Use static library.
IF( NOT LUA_INCLUDE_TEST )
  IF( ONE_LUA )
    ADD_EXECUTABLE(luac ${SRC_ONELUA} ${LUA_RC})
    TARGET_COMPILE_DEFINITIONS(luac PUBLIC MAKE_LUAC)
  ELSE()
    ADD_EXECUTABLE(luac ${SRC_LUAC} ${LUA_RC})
    SET(compiler_target liblua_static)
  ENDIF()

  TARGET_LINK_LIBRARIES(luac ${compiler_target} ${LIBS})
ENDIF()

#######################################
# Install
#######################################

# Don't allow installation if including test builds
SET(SKIP_INSTALL SKIP_INSTALL_ALL OR LUA_INCLUDE_TEST)

IF( NOT SKIP_INSTALL_HEADERS AND NOT ${SKIP_INSTALL} )
  INSTALL(FILES ${LUA_PUBLIC_HDRS} DESTINATION "${CMAKE_INSTALL_INC_DIR}")
ENDIF()

IF( NOT SKIP_INSTALL_LIBRARIES AND NOT ${SKIP_INSTALL} )
  IF( UNIX AND NOT APPLE )
    SET_TARGET_PROPERTIES( lua PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIB_DIR}" )
  ENDIF()

  INSTALL(TARGETS lua ${interpretor_target} ${compiler_target}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BIN_DIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIB_DIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIB_DIR}"
  )
ENDIF()

#######################################
# Testing
#######################################

IF( LUA_INCLUDE_TEST ) # ctest --verbose
  MESSAGE("CTest enabled: make test ARGS=\"-V\" for verbose output")

  ENABLE_TESTING()
  ADD_TEST(NAME test_all
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/testes
    COMMAND lua -e "_port=true" ${PROJECT_SOURCE_DIR}/testes/all.lua
  )
ENDIF()
