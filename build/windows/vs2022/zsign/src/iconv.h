#pragma once
#include "json.h"
#include "common.h"
#include <vector>
using namespace std;

class iconv
{
public:
	iconv(void);
	~iconv(void);

public:
	const wchar_t*	A2U(const char* ansi);
	const wchar_t*	A2U(const string& ansi);
	const wchar_t*	A2U(const jvalue& ansi);
	const char*		A2U8(const char* ansi);
	const char*		A2U8(const string& ansi);
	const char*		A2U8(const jvalue& ansi);
	const wchar_t*	U82U(const char* ansi);
	const wchar_t*	U82U(const string& ansi);
	const wchar_t*	U82U(const jvalue& ansi);
	const char*		U82A(const char* ansi);
	const char*		U82A(const string& ansi);
	const char*		U82A(const jvalue& ansi);
	const char*		U2A(const wchar_t* unicode);
	const char*		U2A(const wstring& unicode);
	const char*		U2U8(const wchar_t* unicode);
	const char*		U2U8(const wstring& unicode);

private:
	const wchar_t*	M2W(const char* ansi, uint32_t uCodePage);
	const char*	W2M(const wchar_t* unicode, uint32_t uCodePage);
	
private:
	vector<char*>  m_arrAnsi;
	vector<wchar_t*> m_arrUnicode;
};