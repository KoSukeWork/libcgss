#pragma once

#if (defined(_WIN64) || defined(__LP64__) || defined(__LLP64__))
#define __CGSS_ARCH_X64__
#else
#define __CGSS_ARCH_X86__
#endif

#if (defined(_WIN32) || defined(__CYGWIN__))
#define __CGSS_OS_WINDOWS__
#ifndef _MBCS
#define _MBCS
#endif
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#else
#define __CGSS_OS_UNIX__
#endif

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW__)
#ifdef __CGSS_BUILDING_DLL__
#ifdef __GNUC__
#define CGSS_EXPORT __attribute__ ((dllexport))
#else
#define CGSS_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define CGSS_EXPORT __attribute__ ((dllimport))
#else
#define CGSS_EXPORT __declspec(dllimport)
#endif
#endif
#define CGSS_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define CGSS_EXPORT __attribute__ ((visibility ("default")))
#define CGSS_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define CGSS_EXPORT
#define CGSS_DLL_LOCAL
#endif
#endif

#ifdef __cplusplus
#ifndef EXTERN_C
#define EXTERN_C extern "C"
#endif
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#else
#define EXTERN_C
#endif

#include <cstdint>

typedef uint32_t bool_t;

#ifdef _MSC_VER
#define CGSS_API(ret_type, name) EXTERN_C CGSS_EXPORT ret_type STDCALL name
#else
#define CGSS_API(ret_type, name) EXTERN_C CGSS_EXPORT STDCALL ret_type name
#endif
#define CGSS_IMPL_RET(ret_type) EXTERN_C ret_type STDCALL

#ifdef __cplusplus
#ifndef PURE
#define PURE =0
#endif
#endif
#ifndef _OUT_
#define _OUT_
#endif
#ifndef _IN_
#define _IN_
#endif
#ifndef _REF_
#define _REF_
#endif

#ifdef __cplusplus
#define CGSS_NS_BEGIN namespace cgss {
#define CGSS_NS_END   }

#define PURE_STATIC(className) \
    private: \
        className() = delete; \
        className(const className &) = delete
#endif

#ifdef __cplusplus
#define __extends(parent) private: typedef parent super
#endif
