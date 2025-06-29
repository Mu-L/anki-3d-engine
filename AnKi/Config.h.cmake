// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

// ====================== WARNING: THIS FILE IS AUTO-GENERATED BY THE BUILD SYSTEM. DON'T ALTER IT =====================

#pragma once

/// @addtogroup config
/// @{

#define _ANKI_STR_HELPER(x) #x
#define _ANKI_STR(x) _ANKI_STR_HELPER(x)

#define ANKI_VERSION_MINOR ${ANKI_VERSION_MINOR}
#define ANKI_VERSION_MAJOR ${ANKI_VERSION_MAJOR}
#define ANKI_VERSION_STR "${ANKI_VERSION_MAJOR}.${ANKI_VERSION_MINOR}"
#define ANKI_REVISION ${ANKI_REVISION}

#define ANKI_EXTRA_CHECKS ${_ANKI_EXTRA_CHECKS}
#define ANKI_DEBUG_SYMBOLS ${ANKI_DEBUG_SYMBOLS}
#define ANKI_OPTIMIZE ${ANKI_OPTIMIZE}
#define ANKI_TESTS ${ANKI_TESTS}
#define ANKI_TRACING_ENABLED ${_ANKI_TRACING_ENABLED}
#define ANKI_STATS_ENABLED ${_ANKI_STATS_ENABLED}
#define ANKI_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#define ANKI_DLSS ${_ANKI_DLSS_ENABLED}

// Compiler
#if defined(__clang__)
#	define ANKI_COMPILER_CLANG 1
#	define ANKI_COMPILER_GCC 0
#	define ANKI_COMPILER_MSVC 0
#	define ANKI_COMPILER_GCC_COMPATIBLE 1
#	define ANKI_COMPILER_STR "clang " __VERSION__
#elif defined(__GNUC__) || defined(__GNUG__)
#	define ANKI_COMPILER_CLANG 0
#	define ANKI_COMPILER_GCC 1
#	define ANKI_COMPILER_MSVC 0
#	define ANKI_COMPILER_GCC_COMPATIBLE 1
#	define ANKI_COMPILER_STR "gcc " __VERSION__
#elif defined(_MSC_VER)
#	define ANKI_COMPILER_CLANG 0
#	define ANKI_COMPILER_GCC 0
#	define ANKI_COMPILER_MSVC 1
#	define ANKI_COMPILER_GCC_COMPATIBLE 0
#	define ANKI_COMPILER_STR "MSVC " _ANKI_STR(_MSC_FULL_VER)
#else
#	error Unknown compiler
#endif

// Operating system
#if defined(__WIN32__) || defined(_WIN32)
#	define ANKI_OS_LINUX 0
#	define ANKI_OS_ANDROID 0
#	define ANKI_OS_MACOS 0
#	define ANKI_OS_IOS 0
#	define ANKI_OS_WINDOWS 1
#	define ANKI_OS_STR "Windows"
#elif defined(__APPLE_CC__)
#	if (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 40000) || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 40000)
#		define ANKI_OS_LINUX 0
#		define ANKI_OS_ANDROID 0
#		define ANKI_OS_MACOS 0
#		define ANKI_OS_IOS 1
#		define ANKI_OS_WINDOWS 0
#		define ANKI_OS_STR "iOS"
#	else
#		define ANKI_OS_LINUX 0
#		define ANKI_OS_ANDROID 0
#		define ANKI_OS_MACOS 1
#		define ANKI_OS_IOS 0
#		define ANKI_OS_WINDOWS 0
#		define ANKI_OS_STR "MacOS"
#	endif
#elif defined(__ANDROID__)
#	define ANKI_OS_LINUX 0
#	define ANKI_OS_ANDROID 1
#	define ANKI_OS_MACOS 0
#	define ANKI_OS_IOS 0
#	define ANKI_OS_WINDOWS 0
#	define ANKI_OS_STR "Android"
#elif defined(__linux__)
#	define ANKI_OS_LINUX 1
#	define ANKI_OS_ANDROID 0
#	define ANKI_OS_MACOS 0
#	define ANKI_OS_IOS 0
#	define ANKI_OS_WINDOWS 0
#	define ANKI_OS_STR "Linux"
#else
#	error Unknown OS
#endif

// POSIX system or not
#if ANKI_OS_LINUX || ANKI_OS_ANDROID || ANKI_OS_MACOS || ANKI_OS_IOS
#	define ANKI_POSIX 1
#else
#	define ANKI_POSIX 0
#endif

// CPU architecture
#if ANKI_COMPILER_GCC_COMPATIBLE
#	if defined(__arm__) || defined(__aarch64__)
#		define ANKI_CPU_ARCH_X86 0
#		define ANKI_CPU_ARCH_ARM 1
#	elif defined(__i386__) || defined(__amd64__)
#		define ANKI_CPU_ARCH_X86 1
#		define ANKI_CPU_ARCH_ARM 0
#	endif
#elif ANKI_COMPILER_MSVC
#	if defined(_M_ARM) || defined(_M_ARM64)
#		define ANKI_CPU_ARCH_X86 0
#		define ANKI_CPU_ARCH_ARM 1
#	elif defined(_M_X64) || defined(_M_IX86)
#		define ANKI_CPU_ARCH_X86 1
#		define ANKI_CPU_ARCH_ARM 0
#	endif
#endif

#if defined(ANKI_CPU_ARCH_X86) && ANKI_CPU_ARCH_X86
#	define ANKI_GPU_ARCH_STR "x86"
#elif defined(ANKI_CPU_ARCH_ARM) && ANKI_CPU_ARCH_ARM
#	define ANKI_GPU_ARCH_STR "arm"
#else
#	error Unknown CPU arch
#endif

// SIMD
#define ANKI_ENABLE_SIMD ${_ANKI_ENABLE_SIMD}

#if !ANKI_ENABLE_SIMD
#	define ANKI_SIMD_NONE 1
#	define ANKI_SIMD_SSE 0
#	define ANKI_SIMD_NEON 0
#	define ANKI_SIMD_STR "None"
#elif ANKI_CPU_ARCH_X86
#	define ANKI_SIMD_NONE 0
#	define ANKI_SIMD_SSE 1
#	define ANKI_SIMD_NEON 0
#	define ANKI_SIMD_STR "SSE"
#else
#	define ANKI_SIMD_NONE 0
#	define ANKI_SIMD_SSE 0
#	define ANKI_SIMD_NEON 1
#	define ANKI_SIMD_STR "Neon"
#endif

// Graphics backend
#if ${_ANKI_GR_BACKEND} == 0
#	define ANKI_GR_BACKEND_VULKAN 1
#	define ANKI_GR_BACKEND_DIRECT3D 0
#	define ANKI_GR_BACKEND_STR "Vulkan"
#else
#	define ANKI_GR_BACKEND_VULKAN 0
#	define ANKI_GR_BACKEND_DIRECT3D 1
#	define ANKI_GR_BACKEND_STR "D3D"
#endif

// Windowing system
#if ${_ANKI_WINDOWING_SYSTEM} == 0
#	define ANKI_WINDOWING_SYSTEM_HEADLESS 1
#	define ANKI_WINDOWING_SYSTEM_SDL 0
#	define ANKI_WINDOWING_SYSTEM_ANDROID 0
#	define ANKI_WINDOWING_SYSTEM_STR "Headless"
#elif ${_ANKI_WINDOWING_SYSTEM} == 1
#	define ANKI_WINDOWING_SYSTEM_HEADLESS 0
#	define ANKI_WINDOWING_SYSTEM_SDL 1
#	define ANKI_WINDOWING_SYSTEM_ANDROID 0
#	define ANKI_WINDOWING_SYSTEM_STR "SDL"
#elif ${_ANKI_WINDOWING_SYSTEM} == 2
#	define ANKI_WINDOWING_SYSTEM_HEADLESS 0
#	define ANKI_WINDOWING_SYSTEM_SDL 0
#	define ANKI_WINDOWING_SYSTEM_ANDROID 1
#	define ANKI_WINDOWING_SYSTEM_STR "Android"
#endif

// Mobile or not
#define ANKI_PLATFORM_MOBILE ${_ANKI_PLATFORM_MOBILE}

// Some compiler attributes
#if ANKI_COMPILER_GCC_COMPATIBLE
#	define ANKI_RESTRICT __restrict
#	define ANKI_FORCE_INLINE inline __attribute__((always_inline))
#	define ANKI_DONT_INLINE __attribute__((noinline))
#	define ANKI_UNUSED __attribute__((__unused__))
#	define ANKI_COLD __attribute__((cold, optimize("Os")))
#	define ANKI_HOT __attribute__ ((hot))
#	define ANKI_UNREACHABLE() __builtin_unreachable()
#	define ANKI_PREFETCH_MEMORY(addr) __builtin_prefetch(addr)
#	define ANKI_CHECK_FORMAT(fmtArgIdx, firstArgIdx) __attribute__((format(printf, fmtArgIdx + 1, firstArgIdx + 1))) // On methods you need to include "this"
#	define ANKI_PURE __attribute__((pure))
#	define ANKI_NO_SANITIZE __attribute__((no_sanitize("address")))
#elif ANKI_COMPILER_MSVC
#	define ANKI_RESTRICT
#	define ANKI_FORCE_INLINE __forceinline
#	define ANKI_DONT_INLINE
#	define ANKI_UNUSED
#	define ANKI_COLD
#	define ANKI_HOT
#	define ANKI_UNREACHABLE() __assume(false)
#	define ANKI_PREFETCH_MEMORY(addr) (void)(addr)
#	define ANKI_CHECK_FORMAT(fmtArgIdx, firstArgIdx)
#	define ANKI_PURE
#	define ANKI_NO_SANITIZE
#else
#	define ANKI_RESTRICT
#	define ANKI_FORCE_INLINE
#	define ANKI_DONT_INLINE
#	define ANKI_UNUSED
#	define ANKI_COLD
#	define ANKI_HOT
#	define ANKI_UNREACHABLE()
#	define ANKI_PREFETCH_MEMORY(addr) (void)(addr)
#	define ANKI_CHECK_FORMAT(fmtArgIdx, firstArgIdx)
#	define ANKI_PURE
#	define ANKI_NO_SANITIZE
#endif

namespace anki {
inline constexpr const char* kAnKiBuildConfigString =
"ver " ANKI_VERSION_STR

", git hash " ANKI_REVISION

", " ANKI_COMPILER_STR

", " ANKI_OS_STR " OS"

", " ANKI_GPU_ARCH_STR

", " ANKI_SIMD_STR " SIMD"

", " ANKI_GR_BACKEND_STR " GFX backend"

", " ANKI_WINDOWING_SYSTEM_STR " window system"

#if ANKI_EXTRA_CHECKS
", extra checks ON"
#else
", extra checks OFF"
#endif

#if ANKI_DEBUG_SYMBOLS
", debug symbols ON"
#else
", debug symbols OFF"
#endif

#if ANKI_OPTIMIZE
", optimizations ON"
#else
", optimizations OFF"
#endif

#if ANKI_TRACING_ENABLED
", tracing ON"
#else
", tracing OFF"
#endif

#if ANKI_STATS_ENABLED
", stats ON"
#else
", stats OFF"
#endif
;
}

// Pack structs
#if ANKI_COMPILER_MSVC
#	define ANKI_BEGIN_PACKED_STRUCT __pragma(pack (push, 1))
#	define ANKI_END_PACKED_STRUCT __pragma(pack (pop))
#else
#	define ANKI_BEGIN_PACKED_STRUCT _Pragma("pack (push, 1)")
#	define ANKI_END_PACKED_STRUCT _Pragma("pack (pop)")
#endif

// Constants
#define ANKI_SAFE_ALIGNMENT 32
#define ANKI_CACHE_LINE_SIZE 64

// Misc
#define ANKI_FILE __FILE__
#define ANKI_FUNC __func__

// A macro used to mark some functions or variables as AnKi internal.
#ifdef ANKI_SOURCE_FILE
#	define ANKI_INTERNAL
#else
#	define ANKI_INTERNAL [[deprecated("This is an AnKi internal interface. Don't use it")]]
#endif

// Macro that temporarily disable the ANKI_INTERNAL. It's needed in some cases where an ANKI_INTERNAL is called in a
// header
#if ANKI_COMPILER_MSVC
#	define ANKI_CALL_INTERNAL(...) \
	__pragma(warning(push)) \
	__pragma(warning(disable:4996)) \
	__VA_ARGS__ \
	__pragma(warning(pop))
#elif ANKI_COMPILER_GCC_COMPATIBLE
#	define ANKI_CALL_INTERNAL(...) \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"") \
	__VA_ARGS__ \
	_Pragma("GCC diagnostic pop")
#else
#	define ANKI_CALL_INTERNAL(...)
#endif

// Define the main() function.
namespace anki {
void preMain();
void postMain();
}
#if ANKI_OS_ANDROID
extern "C" {
struct android_app;
}

namespace anki {
extern android_app* g_androidApp;

void* getAndroidCommandLineArguments(int& argc, char**& argv);
void cleanupGetAndroidCommandLineArguments(void* ptr);
}

#	define ANKI_MAIN_FUNCTION(myMain) \
	int myMain(int argc, char* argv[]); \
	extern "C" void android_main(android_app* app) \
	{ \
		anki::g_androidApp = app; \
		preMain(); \
		char** argv; \
		int argc; \
		void* cleanupToken = anki::getAndroidCommandLineArguments(argc, argv); \
		myMain(argc, argv); \
		anki::cleanupGetAndroidCommandLineArguments(cleanupToken); \
		postMain(); \
	}
#else
#	define ANKI_MAIN_FUNCTION(myMain) \
	int myMain(int argc, char* argv[]); \
	int main(int argc, char* argv[]) \
	{ \
		preMain(); \
		const int exitCode = myMain(argc, argv); \
		postMain(); \
		return exitCode; \
	}
#endif
/// @}
