#pragma once

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif						

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlobj.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <direct.h>
#include "iconv.h"
#include "getopt.h"

#define PATH_MAX					4096
#define strerror(x)					#x
#define _fopen64(fp, path, mode)	{ fopen_s(&fp, path, mode); }

#ifdef _M_ARM64
#pragma comment(lib, "../lib/openssl/arm64/mt/libssl.lib")
#pragma comment(lib, "../lib/openssl/arm64/mt/libcrypto.lib")
#elif _M_X64
#pragma comment(lib, "../lib/openssl/x64/mt/libssl.lib")
#pragma comment(lib, "../lib/openssl/x64/mt/libcrypto.lib")
#pragma comment(lib, "../lib/zlib/x64/mt/zlib.lib")
#pragma comment(lib, "../lib/minizip/x64/mt/minizip.lib")
#else
#pragma comment(lib, "../lib/openssl/x86/mt/libssl.lib")
#pragma comment(lib, "../lib/openssl/x86/mt/libcrypto.lib")
#endif

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "crypt32.lib")