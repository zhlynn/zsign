#include "metadata.h"
#include "json.h"
#include "sha.h"
#include "fs.h"
#include "util.h"

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
		string strHash;
		ZSHA::SHA1Text(strBestIconPath, strHash);
		strIconName = strHash + ".png";
		ZFile::CopyFile(strBestIconPath.c_str(), (strOutputDir + "/" + strIconName).c_str());
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
