#include "metadata.h"
#include "json.h"
#include "sha.h"
#include "fs.h"
#include "util.h"
#include <zlib.h>

static uint32_t ReadBE32(const uint8_t* p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void AppendPngChunk(string& strOut, const char* szType, const uint8_t* pData, uint32_t uLen)
{
	uint8_t head[8] = { (uint8_t)(uLen >> 24), (uint8_t)(uLen >> 16), (uint8_t)(uLen >> 8), (uint8_t)uLen,
						(uint8_t)szType[0], (uint8_t)szType[1], (uint8_t)szType[2], (uint8_t)szType[3] };
	strOut.append((const char*)head, 8);
	if (uLen > 0) {
		strOut.append((const char*)pData, uLen);
	}
	uint32_t uCRC = (uint32_t)crc32(0, head + 4, 4);
	if (uLen > 0) {
		uCRC = (uint32_t)crc32(uCRC, pData, uLen);
	}
	uint8_t tail[4] = { (uint8_t)(uCRC >> 24), (uint8_t)(uCRC >> 16), (uint8_t)(uCRC >> 8), (uint8_t)uCRC };
	strOut.append((const char*)tail, 4);
}

static uint8_t PaethPredictor(int a, int b, int c)
{
	int p = a + b - c;
	int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
	if (pa <= pb && pa <= pc) {
		return (uint8_t)a;
	}
	return (pb <= pc) ? (uint8_t)b : (uint8_t)c;
}

// Xcode runs "pngcrush -iphone" on bundled PNGs, producing Apple's CgBI variant
// (extra CgBI chunk, IDAT deflated without zlib wrapper, BGRA byte order,
// premultiplied alpha) that standard PNG decoders reject. Convert it back to a
// standard PNG; returns false if the data is not a CgBI PNG (use it as-is then).
static bool DecodeCgbiPng(const string& strData, string& strPng)
{
	static const uint8_t magic[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (strData.size() < 8 || 0 != memcmp(strData.data(), magic, 8)) {
		return false;
	}

	const uint8_t* pData = (const uint8_t*)strData.data();
	size_t sSize = strData.size();
	bool bCgbi = false;
	bool bHasIHDR = false;
	uint8_t ihdr[13] = { 0 };
	string strIdat;
	for (size_t pos = 8; pos + 12 <= sSize;) {
		uint32_t uLen = ReadBE32(pData + pos);
		const uint8_t* pType = pData + pos + 4;
		if (uLen > sSize - pos - 12) {
			return false;
		}
		const uint8_t* pChunk = pData + pos + 8;
		if (0 == memcmp(pType, "CgBI", 4)) {
			bCgbi = true;
		} else if (0 == memcmp(pType, "IHDR", 4)) {
			if (13 != uLen) {
				return false;
			}
			memcpy(ihdr, pChunk, 13);
			bHasIHDR = true;
		} else if (0 == memcmp(pType, "IDAT", 4)) {
			strIdat.append((const char*)pChunk, uLen);
		} else if (0 == memcmp(pType, "IEND", 4)) {
			break;
		}
		pos += 12 + (size_t)uLen;
	}
	if (!bCgbi || !bHasIHDR || strIdat.empty()) {
		return false;
	}

	uint32_t uWidth = ReadBE32(ihdr);
	uint32_t uHeight = ReadBE32(ihdr + 4);
	// pngcrush -iphone only emits 8-bit RGBA non-interlaced
	if (8 != ihdr[8] || 6 != ihdr[9] || 0 != ihdr[12]) {
		return false;
	}
	if (0 == uWidth || 0 == uHeight || (uint64_t)uWidth * uHeight > 0x4000000) {
		return false;
	}

	size_t sRowBytes = (size_t)uWidth * 4 + 1;
	size_t sRawSize = sRowBytes * uHeight;
	string strRaw;
	strRaw.resize(sRawSize);
	uint8_t* pRaw = (uint8_t*)&strRaw[0];

	// CgBI's IDAT is a raw deflate stream (no zlib header/checksum)
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	if (Z_OK != inflateInit2(&zs, -15)) {
		return false;
	}
	zs.next_in = (Bytef*)strIdat.data();
	zs.avail_in = (uInt)strIdat.size();
	zs.next_out = pRaw;
	zs.avail_out = (uInt)sRawSize;
	int nRet = inflate(&zs, Z_FINISH);
	bool bDone = (Z_STREAM_END == nRet && sRawSize == zs.total_out);
	inflateEnd(&zs);
	if (!bDone) {
		return false;
	}

	// un-filter every scanline first; filters 2/3/4 reference the previous
	// row, so it must still hold reconstructed (not yet swapped) bytes
	for (uint32_t row = 0; row < uHeight; row++) {
		uint8_t* pRow = pRaw + row * sRowBytes + 1;
		const uint8_t* pPrev = (row > 0) ? (pRow - sRowBytes) : NULL;
		uint8_t uFilter = pRow[-1];
		size_t sLine = sRowBytes - 1;
		switch (uFilter) {
		case 0:
			break;
		case 1:
			for (size_t i = 4; i < sLine; i++) {
				pRow[i] = (uint8_t)(pRow[i] + pRow[i - 4]);
			}
			break;
		case 2:
			if (NULL != pPrev) {
				for (size_t i = 0; i < sLine; i++) {
					pRow[i] = (uint8_t)(pRow[i] + pPrev[i]);
				}
			}
			break;
		case 3:
			for (size_t i = 0; i < sLine; i++) {
				int a = (i >= 4) ? pRow[i - 4] : 0;
				int b = (NULL != pPrev) ? pPrev[i] : 0;
				pRow[i] = (uint8_t)(pRow[i] + ((a + b) >> 1));
			}
			break;
		case 4:
			for (size_t i = 0; i < sLine; i++) {
				int a = (i >= 4) ? pRow[i - 4] : 0;
				int b = (NULL != pPrev) ? pPrev[i] : 0;
				int c = (i >= 4 && NULL != pPrev) ? pPrev[i - 4] : 0;
				pRow[i] = (uint8_t)(pRow[i] + PaethPredictor(a, b, c));
			}
			break;
		default:
			return false;
		}
		pRow[-1] = 0;
	}

	// swap BGRA -> RGBA and un-premultiply alpha
	for (uint32_t row = 0; row < uHeight; row++) {
		uint8_t* pRow = pRaw + row * sRowBytes + 1;
		for (size_t i = 0; i + 4 <= sRowBytes - 1; i += 4) {
			uint8_t b = pRow[i], a = pRow[i + 3];
			pRow[i] = pRow[i + 2];
			pRow[i + 2] = b;
			if (a > 0 && a < 255) {
				pRow[i] = (uint8_t)((pRow[i] * 255 + a / 2) / a);
				pRow[i + 1] = (uint8_t)((pRow[i + 1] * 255 + a / 2) / a);
				pRow[i + 2] = (uint8_t)((pRow[i + 2] * 255 + a / 2) / a);
			}
		}
	}

	uLongf uCompSize = compressBound((uLong)sRawSize);
	string strComp;
	strComp.resize(uCompSize);
	if (Z_OK != compress2((Bytef*)&strComp[0], &uCompSize, pRaw, (uLong)sRawSize, Z_DEFAULT_COMPRESSION)) {
		return false;
	}

	strPng.assign((const char*)magic, 8);
	AppendPngChunk(strPng, "IHDR", ihdr, 13);
	AppendPngChunk(strPng, "IDAT", (const uint8_t*)strComp.data(), (uint32_t)uCompSize);
	AppendPngChunk(strPng, "IEND", NULL, 0);
	return true;
}

static void GetIconNames(jvalue& jvInfo, vector<string>& arrNames)
{
	if (jvInfo.has("CFBundleIcons")) {
		jvalue& jvIcons = jvInfo["CFBundleIcons"];
		if (jvIcons.has("CFBundlePrimaryIcon")) {
			jvalue& jvPrimary = jvIcons["CFBundlePrimaryIcon"];
			jvalue& jvFiles = jvPrimary["CFBundleIconFiles"];
			if (jvFiles.is_array()) {
				for (size_t i = 0; i < jvFiles.size(); i++) {
					string strName = jvFiles[i];
					if (!strName.empty()) {
						arrNames.push_back(strName);
					}
				}
			}
		}
	}

	if (arrNames.empty() && jvInfo.has("CFBundleIconFiles")) {
		jvalue& jvIconFiles = jvInfo["CFBundleIconFiles"];
		if (jvIconFiles.is_array()) {
			for (size_t i = 0; i < jvIconFiles.size(); i++) {
				string strName = jvIconFiles[i];
				if (!strName.empty()) {
					arrNames.push_back(strName);
				}
			}
		}
	}

	if (arrNames.empty()) {
		string strSingleIcon = jvInfo["CFBundleIconFile"];
		if (!strSingleIcon.empty()) {
			arrNames.push_back(strSingleIcon);
		}
	}
}

static bool FindLargestIcon(const string& strAppFolder, const vector<string>& arrIconNames, string& strBestPath)
{
	int64_t nBestSize = 0;
	ZFile::EnumFolder(strAppFolder.c_str(), false, NULL, [&](bool bFolder, const string& strPath) {
		if (bFolder) {
			return false;
		}
		string strBaseName = ZUtil::GetBaseName(strPath.c_str());
		for (const string& strPrefix : arrIconNames) {
			if (0 == strncmp(strBaseName.c_str(), strPrefix.c_str(), strPrefix.size())) {
				int64_t nSize = ZFile::GetFileSize(strPath.c_str());
				if (nSize > nBestSize) {
					nBestSize = nSize;
					strBestPath = strPath;
				}
				break;
			}
		}
		return false;
	});
	return (nBestSize > 0);
}

bool GetMetadata(const string& strAppFolder, const string& strOutputDir, const string& strIpaFile)
{
	string strInfoPlistData;
	string strInfoPlistPath = strAppFolder + "/Info.plist";
	if (!ZFile::ReadFile(strInfoPlistPath.c_str(), strInfoPlistData)) {
		return ZLog::ErrorV(">>> GetMetadata: Can't read %s\n", strInfoPlistPath.c_str());
	}

	jvalue jvInfo;
	jvInfo.read_plist(strInfoPlistData);

	string strAppName = jvInfo["CFBundleDisplayName"];
	if (strAppName.empty()) {
		strAppName = jvInfo["CFBundleName"].as_cstr();
	}

	string strAppVersion = jvInfo["CFBundleShortVersionString"];
	if (strAppVersion.empty()) {
		strAppVersion = jvInfo["CFBundleVersion"].as_cstr();
	}

	string strBundleId = jvInfo["CFBundleIdentifier"];

	// extract icon
	string strIconName;
	vector<string> arrIconNames;
	GetIconNames(jvInfo, arrIconNames);

	string strBestIconPath;
	if (!arrIconNames.empty() && FindLargestIcon(strAppFolder, arrIconNames, strBestIconPath)) {
		string strIconData;
		if (ZFile::ReadFile(strBestIconPath.c_str(), strIconData)) {
			string strStandardPng;
			if (DecodeCgbiPng(strIconData, strStandardPng)) {
				strIconData = strStandardPng;
			}
			string strHash;
			ZSHA::SHA1Text(strBestIconPath, strHash);
			strIconName = strHash + ".png";
			ZFile::WriteFile((strOutputDir + "/" + strIconName).c_str(), strIconData);
		}
	}

	// ipa file info
	int64_t nIpaSize = 0;
	string strFileName;
	if (!strIpaFile.empty()) {
		nIpaSize = ZFile::GetFileSize(strIpaFile.c_str());
		strFileName = ZUtil::GetBaseName(strIpaFile.c_str());
	}

	// write json
	jvalue jvMeta;
	jvMeta["AppName"] = strAppName;
	jvMeta["AppVersion"] = strAppVersion;
	jvMeta["AppBundleIdentifier"] = strBundleId;
	jvMeta["AppSize"] = (int)nIpaSize;
	jvMeta["IconName"] = strIconName.empty() ? "" : strIconName;
	jvMeta["FileName"] = strFileName;
	jvMeta["Timestamp"] = (int)ZUtil::GetUnixStamp();

	string strMetaPath = strOutputDir + "/metadata.json";
	if (!jvMeta.style_write_to_file("%s", strMetaPath.c_str())) {
		return ZLog::ErrorV(">>> GetMetadata: Can't write %s\n", strMetaPath.c_str());
	}

	ZLog::PrintV(">>> Metadata:\t%s\n", strMetaPath.c_str());
	return true;
}
