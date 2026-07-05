#include "common.h"
#include <list>
#include <set>
#include "macho.h"
#include "bundle.h"
#include "openssl.h"
#include "timer.h"
#include "archive.h"
#include "metadata.h"
#include "certcheck.h"

#ifdef _WIN32
#include "common_win32.h"
#endif

#ifndef ZSIGN_VERSION
#define ZSIGN_VERSION 0.0.0-dev
#endif
#define ZSIGN_STR_(x) #x
#define ZSIGN_STR(x) ZSIGN_STR_(x)
#define ZSIGN_VERSION_STR ZSIGN_STR(ZSIGN_VERSION)

const struct option options[] = {
	{"debug", no_argument, NULL, 'd'},
	{"force", no_argument, NULL, 'f'},
	{"verbose", no_argument, NULL, 'V'},
	{"adhoc", no_argument, NULL, 'a'},
	{"cert", required_argument, NULL, 'c'},
	{"pkey", required_argument, NULL, 'k'},
	{"prov", required_argument, NULL, 'm'},
	{"password", required_argument, NULL, 'p'},
	{"bundle_id", required_argument, NULL, 'b'},
	{"bundle_name", required_argument, NULL, 'n'},
	{"bundle_version", required_argument, NULL, 'r'},
	{"entitlements", required_argument, NULL, 'e'},
	{"output", required_argument, NULL, 'o'},
	{"zip_level", required_argument, NULL, 'z'},
	{"dylib", required_argument, NULL, 'l'},
	{"rm_dylib", required_argument, NULL, 'D'},
	{"weak", no_argument, NULL, 'w'},
	{"temp_folder", required_argument, NULL, 't'},
	{"sha256_only", no_argument, NULL, '2'},
	{"legacy_sha1", no_argument, NULL, 'L'},
	{"install", no_argument, NULL, 'i'},
	{"check", no_argument, NULL, 'C'},
	{"quiet", no_argument, NULL, 'q'},
	{"metadata", required_argument, NULL, 'x'},
	{"rm_provision", no_argument, NULL, 'R'},
	{"enable_docs", no_argument, NULL, 'S'},
	{"min_version", required_argument, NULL, 'M'},
	{"rm_extensions", no_argument, NULL, 'E'},
	{"rm_watch", no_argument, NULL, 'W'},
	{"rm_uisd", no_argument, NULL, 'U'},
	{"help", no_argument, NULL, 'h'},
	{}
};

int usage()
{
	ZLog::PrintV("zsign (v%s) is a codesign alternative for iOS12+ on macOS, Linux and Windows. \nVisit https://github.com/zhlynn/zsign for more information.\n\n", ZSIGN_VERSION_STR);
	ZLog::Print("Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder\n");
	ZLog::Print("options:\n");
	ZLog::Print("-k, --pkey\t\tPath to private key or p12 file. (PEM or DER format)\n");
	ZLog::Print("-m, --prov\t\tPath to mobile provisioning profile.\n");
	ZLog::Print("-c, --cert\t\tPath to certificate file. (PEM or DER format)\n");
	ZLog::Print("-a, --adhoc\t\tPerform ad-hoc signature only.\n");
	ZLog::Print("-d, --debug\t\tGenerate debug output files. (.zsign_debug folder)\n");
	ZLog::Print("-f, --force\t\tForce sign without cache when signing folder.\n");
	ZLog::Print("-o, --output\t\tPath to output ipa file.\n");
	ZLog::Print("-p, --password\t\tPassword for private key or p12 file.\n");
	ZLog::Print("-b, --bundle_id\t\tNew bundle id to change.\n");
	ZLog::Print("-n, --bundle_name\tNew bundle name to change.\n");
	ZLog::Print("-r, --bundle_version\tNew bundle version to change.\n");
	ZLog::Print("-e, --entitlements\tNew entitlements to change.\n");
	ZLog::Print("-z, --zip_level\t\tCompressed level when output the ipa file. (0-9)\n");
	ZLog::Print("-l, --dylib\t\tPath to inject dylib file. Use -l multiple time to inject multiple dylib files at once.\n");
	ZLog::Print("-D, --rm_dylib\t\tName of dylib to remove. Use -D multiple times to remove multiple dylibs at once.\n");
	ZLog::Print("-w, --weak\t\tInject dylib as LC_LOAD_WEAK_DYLIB.\n");
	ZLog::Print("-i, --install\t\tInstall ipa file using ideviceinstaller command for test.\n");
	ZLog::Print("-t, --temp_folder\tPath to temporary folder for intermediate files.\n");
	ZLog::Print("-2, --sha256_only\t(Deprecated, now the default.) Kept for backward compatibility.\n");
	ZLog::Print("-L, --legacy_sha1\tEmit a dual SHA1+SHA256 CodeDirectory for iOS <= 10 compatibility.\n");
	ZLog::Print("-C, --check\t\tCheck certificate validity and OCSP revocation status.\n");
	ZLog::Print("-q, --quiet\t\tQuiet operation.\n");
	ZLog::Print("-x, --metadata\t\tExtract metadata and icon to the specified directory.\n");
	ZLog::Print("-R, --rm_provision\tRemove mobileprovision file after signing.\n");
	ZLog::Print("-S, --enable_docs\tEnable UISupportsDocumentBrowser and UIFileSharingEnabled.\n");
	ZLog::Print("-M, --min_version\tSet MinimumOSVersion in Info.plist.\n");
	ZLog::Print("-E, --rm_extensions\tRemove all app extensions (PlugIns/Extensions).\n");
	ZLog::Print("-W, --rm_watch\t\tRemove watch app from the bundle.\n");
	ZLog::Print("-U, --rm_uisd\t\tRemove UISupportedDevices from Info.plist.\n");
	ZLog::Print("-v, --version\t\tShows version.\n");
	ZLog::Print("-h, --help\t\tShows help (this message).\n");

	return -1;
}

int main(int argc, char* argv[])
{
	ZTimer atimer;
	ZTimer gtimer;

	bool bForce = false;
	bool bInstall = false;
	bool bWeakInject = false;
	bool bAdhoc = false;
	bool bSHA256Only = true;
	bool bCheckSignature = false;
	bool bRemoveProvision = false;
	bool bEnableDocuments = false;
	string strMinVersion;
	bool bRemoveExtensions = false;
	bool bRemoveWatchApp = false;
	bool bRemoveUISupportedDevices = false;
	uint32_t uZipLevel = 0;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	vector<string> arrProvFiles;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strOutputFile;
	string strDisplayName;
	string strEntitleFile;
	vector<string> arrDylibFiles;
	vector<string> arrRemoveDylibNames;
	string strMetadataDir;
	string strTempFolder = ZFile::GetTempFolder();

	int opt = 0;
	int argslot = -1;
	while (-1 != (opt = getopt_long(argc, argv, "dfva2LhiqwCRSEWUc:k:m:o:p:e:b:n:z:l:D:t:r:x:M:",
		options, &argslot))) {
		switch (opt) {
		case 'd':
			ZLog::SetLogLever(ZLog::E_DEBUG);
			break;
		case 'f':
			bForce = true;
			break;
		case 'c':
			strCertFile = ZFile::GetFullPath(optarg);
			break;
		case 'k':
			strPKeyFile = ZFile::GetFullPath(optarg);
			break;
		case 'm':
			strProvFile = ZFile::GetFullPath(optarg);
			arrProvFiles.push_back(strProvFile);
			break;
		case 'a':
			bAdhoc = true;
			break;
		case 'p':
			strPassword = optarg;
			break;
		case 'b':
			strBundleId = optarg;
			break;
		case 'r':
			strBundleVersion = optarg;
			break;
		case 'n':
			strDisplayName = optarg;
			break;
		case 'e':
			strEntitleFile = ZFile::GetFullPath(optarg);
			break;
		case 'l':
			arrDylibFiles.push_back(ZFile::GetFullPath(optarg));
			break;
		case 'D':
			arrRemoveDylibNames.push_back(optarg);
			break;
		case 'i':
			bInstall = true;
			break;
		case 'o':
			strOutputFile = ZFile::GetFullPath(optarg);
			break;
		case 'z':
			uZipLevel = atoi(optarg);
			break;
		case 'w':
			bWeakInject = true;
			break;
		case 't':
			strTempFolder = ZFile::GetFullPath(optarg);
			break;
		case '2':
			// Kept for backward compatibility; SHA256-only is the default now.
			bSHA256Only = true;
			break;
		case 'L':
			bSHA256Only = false;
			break;
		case 'C':
			bCheckSignature = true;
			break;
		case 'q':
			ZLog::SetLogLever(ZLog::E_NONE);
			break;
		case 'x':
			strMetadataDir = ZFile::GetFullPath(optarg);
			break;
		case 'R':
			bRemoveProvision = true;
			break;
		case 'S':
			bEnableDocuments = true;
			break;
		case 'M':
			strMinVersion = optarg;
			break;
		case 'E':
			bRemoveExtensions = true;
			break;
		case 'W':
			bRemoveWatchApp = true;
			break;
		case 'U':
			bRemoveUISupportedDevices = true;
			break;
		case 'v': {
			printf("version: %s\n", ZSIGN_VERSION_STR);
			return 0;
			}
			break;
		case 'h':
		case '?':
			return usage();
			break;
		}

		ZLog::DebugV(">>> Option:\t-%c, %s\n", opt, optarg);
	}

	if (optind >= argc) {
		return usage();
	}

	if (!ZFile::IsFolder(strTempFolder.c_str())) {
		ZLog::ErrorV(">>> Invalid temp folder! %s\n", strTempFolder.c_str());
		return -1;
	}

	string strPath = ZFile::GetFullPath(argv[optind]);
	if (!ZFile::IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		return -1;
	}

	if (uZipLevel < 0 || uZipLevel > 9) {
		ZLog::ErrorV(">>> Invalid zip level! Please input 0 - 9.\n");
		return -1;
	}

	for (const string& strDylibFile : arrDylibFiles) {
		if (!ZFile::IsFileExists(strDylibFile.c_str())) {
			ZLog::ErrorV(">>> Dylib file not found! %s\n", strDylibFile.c_str());
			return -1;
		}
		ZMachO dylibMachO;
		if (!dylibMachO.Init(strDylibFile.c_str())) {
			ZLog::ErrorV(">>> Invalid dylib file! Not a valid Mach-O format. %s\n", strDylibFile.c_str());
			return -1;
		}
	}

	if (ZLog::IsDebug()) {
		ZFile::CreateFolder("./.zsign_debug");
		for (int i = optind; i < argc; i++) {
			ZLog::DebugV(">>> Argument:\t%s\n", argv[i]);
		}
	}

	if (bCheckSignature && strPKeyFile.empty() && strProvFile.empty()) {
		return CheckCertificate(strPath, strPassword);
	}

	bool bZipFile = ZFile::IsZipFile(strPath.c_str());
	if (!bZipFile && !ZFile::IsFolder(strPath.c_str())) { // macho file
		ZMachO* macho = new ZMachO();
		if (!macho->Init(strPath.c_str())) {
			ZLog::ErrorV(">>> Invalid mach-o file! %s\n", strPath.c_str());
			return -1;
		}

		if (!bAdhoc && arrDylibFiles.empty() && arrRemoveDylibNames.empty() && (strPKeyFile.empty() || strProvFile.empty())) {
			macho->PrintInfo();
			return 0;
		}

		ZSignAsset zsa;
		if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, true)) {
			return -1;
		}

		if (!arrDylibFiles.empty()) {
			for (const string& dyLibFile : arrDylibFiles) {
				if (!macho->InjectDylib(bWeakInject, dyLibFile.c_str())) {
					return -1;
				}
			}
		}

		if (!arrRemoveDylibNames.empty()) {
			set<string> setDylibs;
			for (const string& name : arrRemoveDylibNames) {
				if (name.find('/') != string::npos) {
					setDylibs.insert(name);
				} else {
					setDylibs.insert("@executable_path/" + name);
				}
			}
			macho->RemoveDylibs(setDylibs);
		}

		atimer.Reset();
		ZLog::PrintV(">>> Signing:\t%s %s\n", strPath.c_str(), (bAdhoc ? " (Ad-hoc)" : ""));
		string strInfoSHA1;
		string strInfoSHA256;
		string strCodeResourcesData;
		bool bRet = macho->Sign(&zsa, bForce, strBundleId, strInfoSHA1, strInfoSHA256, strCodeResourcesData);
		atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");
		return bRet ? 0 : -1;
	}

	bool bTempOutputFile = false;
	if (strOutputFile.empty()) {
		if (bInstall) {
			bTempOutputFile = true;
			strOutputFile = ZFile::GetRealPathV("%s/zsign_temp_%llu.ipa", strTempFolder.c_str(), ZUtil::GetMicroSecond());
		} else if (bZipFile) {
			ZLog::ErrorV(">>> Use -o option to specify the output file.\n");
			return -1;
		}
	}

	//init
	ZSignAsset zsa;
	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, false)) {
		return -1;
	}

	//extract
	bool bTempFolder = false;
	bool bEnableCache = true;
	string strFolder = strPath;
	if (bZipFile) {
		bForce = true;
		bTempFolder = true;
		bEnableCache = false;
		strFolder = ZFile::GetRealPathV("%s/zsign_folder_%llu", strTempFolder.c_str(), atimer.Reset());
		ZLog::PrintV(">>> Unzip:\t%s (%s) -> %s ... \n", strPath.c_str(), ZFile::GetFileSizeString(strPath.c_str()).c_str(), strFolder.c_str());
		if (!Zip::Extract(strPath.c_str(), strFolder.c_str())) {
			ZLog::ErrorV(">>> Unzip failed!\n");
			return -1;
		}
		atimer.PrintResult(true, ">>> Unzip OK!");
	}

	//sign
	atimer.Reset();
	ZBundle bundle;
	bundle.m_bEnableDocuments = bEnableDocuments;
	bundle.m_strMinVersion = strMinVersion;
	bundle.m_bRemoveExtensions = bRemoveExtensions;
	bundle.m_bRemoveWatchApp = bRemoveWatchApp;
	bundle.m_bRemoveUISupportedDevices = bRemoveUISupportedDevices;

	bool bRet;
	if (arrProvFiles.size() > 1) {
		list<ZSignAsset> zsaList;
		for (const string& provFile : arrProvFiles) {
			zsaList.push_back(ZSignAsset());
			if (!zsaList.back().Init(strCertFile, strPKeyFile, provFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, false)) {
				ZLog::ErrorV(">>> Failed to init provision: %s\n", provFile.c_str());
				zsaList.pop_back();
			}
		}
		bRet = bundle.SignFolder(&zsaList, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, arrRemoveDylibNames, bForce, bWeakInject, bEnableCache, bRemoveProvision);
	} else {
		bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, arrRemoveDylibNames, bForce, bWeakInject, bEnableCache, bRemoveProvision);
	}
	atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	// Post-sign certificate check
	if (bRet && bCheckSignature && !bundle.m_strAppFolder.empty()) {
		CheckSignedBinary(bundle.m_strAppFolder);
	}

	//archive
	if (bRet && !strOutputFile.empty()) {
		size_t pos = bundle.m_strAppFolder.rfind("Payload");
		if (string::npos != pos && pos > 0) {
			atimer.Reset();
			ZLog::PrintV(">>> Archiving: \t%s ... \n", strOutputFile.c_str());
			string strBaseFolder = bundle.m_strAppFolder.substr(0, pos - 1);
			if (!Zip::Archive(strBaseFolder.c_str(), strOutputFile.c_str(), uZipLevel)) {
				ZLog::Error(">>> Archive failed!\n");
				bRet = false;
			} else {
				atimer.PrintResult(true, ">>> Archive OK! (%s)", ZFile::GetFileSizeString(strOutputFile.c_str()).c_str());
				if (bRet && !strMetadataDir.empty()) {
					ZFile::CreateFolder(strMetadataDir.c_str());
					GetMetadata(bundle.m_strAppFolder, strMetadataDir, strOutputFile);
				}
			}
		} else {
			ZLog::Error(">>> Can't find payload directory!\n");
			bRet = false;
		}
	}

	//install
	if (bRet && bInstall) {
		bRet = ZUtil::SystemExecV("ideviceinstaller install  \"%s\"", strOutputFile.c_str());
	}

	//clean
	if (bTempFolder) {
		ZFile::RemoveFolder(strFolder.c_str());
	}

	if (bTempOutputFile) {
		ZFile::RemoveFile(strOutputFile.c_str());
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}
