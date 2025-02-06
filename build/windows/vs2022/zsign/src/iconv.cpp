#include "iconv.h"

iconv::iconv(void)
{

}

iconv::~iconv(void)
{
	if (!m_arrAnsi.empty()) {
		for (size_t i = 0; i < m_arrAnsi.size(); i++) {
			delete[] m_arrAnsi[i];
		}
		m_arrAnsi.clear();
	}
	if (!m_arrUnicode.empty()) {
		for (size_t i = 0; i < m_arrUnicode.size(); i++) {
			delete[] m_arrUnicode[i];
		}
		m_arrUnicode.clear();
	}
}

const wchar_t* iconv::A2U(const char* ansi)
{
	return M2W(ansi, CP_ACP);
}

const wchar_t* iconv::A2U(const string& ansi)
{
	return A2U(ansi.c_str());
}

const wchar_t* iconv::A2U(const jvalue& ansi)
{
	return A2U(ansi.as_cstr());
}

const char* iconv::U2A(const wchar_t* unicode)
{
	return W2M(unicode, CP_ACP);
}

const char* iconv::U2A(const wstring& unicode)
{
	return U2A(unicode.c_str());
}

const char* iconv::U2U8(const wchar_t* unicode)
{
	return W2M(unicode, CP_UTF8);
}

const char* iconv::U2U8(const wstring& unicode)
{
	return U2U8(unicode.c_str());
}

const wchar_t* iconv::U82U(const char* ansi)
{
	return M2W(ansi, CP_UTF8);
}

const wchar_t* iconv::U82U(const string& ansi)
{
	return U82U(ansi.c_str());
}

const wchar_t* iconv::U82U(const jvalue& ansi)
{
	return U82U(ansi.as_cstr());
}

const char* iconv::U82A(const char* ansi)
{
	return U2A(U82U(ansi));
}

const char* iconv::U82A(const jvalue& ansi)
{
	return U82A(ansi.as_cstr());
}

const char* iconv::U82A(const string& ansi)
{
	return U82A(ansi.c_str());
}

const char* iconv::A2U8(const char* ansi)
{
	return U2U8(A2U(ansi));
}

const char* iconv::A2U8(const string& ansi)
{
	return A2U8(ansi.c_str());
}

const char* iconv::A2U8(const jvalue& ansi)
{
	return A2U8(ansi.as_cstr());
}

const wchar_t* iconv::M2W(const char* ansi, uint32_t uCodePage)
{
	if (NULL == ansi) {
		return L"";
	}

	int nLen = ::MultiByteToWideChar(uCodePage, NULL, ansi, -1, 0, 0);
	if (nLen <= 0) {
		return L"";
	}

	wchar_t* unicode = new wchar_t[nLen + 1];
	nLen = ::MultiByteToWideChar(uCodePage, NULL, ansi, -1, unicode, nLen + 1);
	unicode[nLen] = 0;
	m_arrUnicode.push_back(unicode);
	return unicode;
}

const char* iconv::W2M(const wchar_t* unicode, uint32_t uCodePage)
{
	if (NULL == unicode) {
		return "";
	}

	int nLen = ::WideCharToMultiByte(uCodePage, NULL, unicode, -1, 0, 0, 0, 0);
	if (nLen <= 0) {
		return "";
	}

	char* ansi = new char[nLen + 1];
	nLen = ::WideCharToMultiByte(uCodePage, NULL, unicode, -1, ansi, nLen + 1, 0, 0);
	ansi[nLen] = 0;
	m_arrAnsi.push_back(ansi);
	return ansi;
}