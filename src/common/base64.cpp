#include "base64.h"
#include <string.h>
#include <stdint.h>

#define B0(a) (a & 0xFF)
#define B1(a) (a >> 8 & 0xFF)
#define B2(a) (a >> 16 & 0xFF)
#define B3(a) (a >> 24 & 0xFF)

jbase64::jbase64()
{
}

jbase64::~jbase64()
{
	if (!m_array_encodes.empty()) {
		for (size_t i = 0; i < m_array_encodes.size(); i++) {
			delete[] m_array_encodes[i];
		}
		m_array_encodes.clear();
	}

	if (!m_array_decodes.empty()) {
		for (size_t i = 0; i < m_array_decodes.size(); i++) {
			delete[] m_array_decodes[i];
		}
		m_array_decodes.clear();
	}
}

char jbase64::get_b64_char(int index)
{
	static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	if (index >= 0 && index < 64) {
		return table[index];
	}
	return '=';
}

int jbase64::get_b64_index(char ch)
{
	int index = -1;
	if (ch >= 'A' && ch <= 'Z') {
		index = ch - 'A';
	} else if (ch >= 'a' && ch <= 'z') {
		index = ch - 'a' + 26;
	} else if (ch >= '0' && ch <= '9') {
		index = ch - '0' + 52;
	} else if (ch == '+') {
		index = 62;
	} else if (ch == '/') {
		index = 63;
	}
	return index;
}

const char* jbase64::encode(const char* src, int src_len)
{
	if (0 == src_len) {
		src_len = (int)strlen(src);
	}

	if (src_len <= 0) {
		return "";
	}

	char* enc = new char[src_len * 3 + 128];
	m_array_encodes.push_back(enc);

	int i = 0;
	char* p64 = enc;
	unsigned char* pcursor = (unsigned char*)src;
	for (i = 0; i < src_len - 3; i += 3) {
		uint32_t temp = 0;
		memcpy(&temp, pcursor, 3);
		p64[0] = get_b64_char((B0(temp) >> 2) & 0x3F);
		p64[1] = get_b64_char((B0(temp) << 6 >> 2 | B1(temp) >> 4) & 0x3F);
		p64[2] = get_b64_char((B1(temp) << 4 >> 2 | B2(temp) >> 6) & 0x3F);
		p64[3] = get_b64_char((B2(temp) << 2 >> 2) & 0x3F);
		p64 += 4;
		pcursor += 3;
	}

	if (i < src_len) {
		int rest = src_len - i;
		unsigned long temp = 0;
		for (int j = 0; j < rest; ++j) {
			*(((unsigned char*)&temp) + j) = *pcursor++;
		}
		p64[0] = get_b64_char((B0(temp) >> 2) & 0x3F);
		p64[1] = get_b64_char((B0(temp) << 6 >> 2 | B1(temp) >> 4) & 0x3F);
		p64[2] = rest > 1 ? get_b64_char((B1(temp) << 4 >> 2 | B2(temp) >> 6) & 0x3F) : '=';
		p64[3] = rest > 2 ? get_b64_char((B2(temp) << 2 >> 2) & 0x3F) : '=';
		p64 += 4;
	}
	*p64 = '\0';
	return enc;
}

const char* jbase64::encode(const string& input)
{
	return encode(input.data(), (int)input.size());
}

const char* jbase64::decode(const char* src, int src_len, int* pdecode_len)
{
	if (0 == src_len) {
		src_len = (int)strlen(src);
	}

	if (src_len <= 0) {
		if (NULL != pdecode_len) {
			*pdecode_len = 0;
		}
		return "";
	}

	// Max decoded size is ceil(src_len/4)*3; +1 for NUL terminator.
	char* dec = new char[((src_len + 3) / 4) * 3 + 1];
	m_array_decodes.push_back(dec);

	char* pbuf = dec;
	int quartet[4];
	int q = 0;

	for (int i = 0; i < src_len; ++i) {
		char ch = src[i];
		if ('=' == ch) {
			break;
		}
		int idx = get_b64_index(ch);
		if (idx < 0) {
			continue;
		}
		quartet[q++] = idx;
		if (4 == q) {
			*pbuf++ = (char)((quartet[0] << 2) | (quartet[1] >> 4));
			*pbuf++ = (char)((quartet[1] << 4) | (quartet[2] >> 2));
			*pbuf++ = (char)((quartet[2] << 6) | quartet[3]);
			q = 0;
		}
	}

	if (q >= 2) {
		*pbuf++ = (char)((quartet[0] << 2) | (quartet[1] >> 4));
		if (q >= 3) {
			*pbuf++ = (char)((quartet[1] << 4) | (quartet[2] >> 2));
		}
	}

	*pbuf = '\0';

	if (NULL != pdecode_len) {
		*pdecode_len = (int)(pbuf - dec);
	}

	return dec;
}

const char* jbase64::decode(const char* src, string& output)
{
	output.clear();
	int dec_len = 0;
	const char* p = decode(src, 0, &dec_len);
	output.append(p, dec_len);
	return output.data();
}
