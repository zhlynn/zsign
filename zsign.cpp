#include "bundle.h"
#include "common/common.h"
#include "macho.h"
#include "openssl.h"
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>

#include <memory>
#include <string>
#include <vector>

const struct option options[] = {
	{"debug", no_argument, NULL, 'd'},
	{"force", no_argument, NULL, 'f'},
	{"verbose", no_argument, NULL, 'V'},
	{"adhoc", no_argument, NULL, 'a'},
	{"single_inplace", no_argument, NULL, 's'},
	{"sha256_only", no_argument, NULL, '2'},
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
	{"weak", no_argument, NULL, 'w'},
	{"install", no_argument, NULL, 'i'},
	{"quiet", no_argument, NULL, 'q'},
	{"help", no_argument, NULL, 'h'},
	{} };

int usage()
{
	ZLog::Print("Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder\n");
	ZLog::Print("options:\n");
	ZLog::Print("-k, --pkey\t\tPath to private key or p12 file. (PEM or DER format)\n");
	ZLog::Print("-m, --prov\t\tPath to mobile provisioning profile.\n");
	ZLog::Print("-a, --adhoc\t\tPerform ad-hoc signature only.\n");
	ZLog::Print("-s, --single_inplace\tRe-sign a single Mach-O binary in place. (incompatible with `-o`)\n");
	ZLog::Print("-2, --sha256_only\tSerialize a single code directory that uses SHA256.\n");
	ZLog::Print("-c, --cert\t\tPath to certificate file. (PEM or DER format)\n");
	ZLog::Print("-d, --debug\t\tGenerate debug output files. (.zsign_debug folder)\n");
	ZLog::Print("-f, --force\t\tForce sign without cache when signing folder.\n");
	ZLog::Print("-V, --verbose\t\tEnable extra verbosity in log.\n");
	ZLog::Print("-o, --output\t\tPath to output ipa file.\n");
	ZLog::Print("-p, --password\t\tPassword for private key or p12 file.\n");
	ZLog::Print("-b, --bundle_id\t\tNew bundle id to change.\n");
	ZLog::Print("-n, --bundle_name\tNew bundle name to change.\n");
	ZLog::Print("-r, --bundle_version\tNew bundle version to change.\n");
	ZLog::Print("-e, --entitlements\tNew entitlements to change.\n");
	ZLog::Print("-z, --zip_level\t\tCompressed level when output the ipa file. (0-9)\n");
	ZLog::Print("-l, --dylib\t\tPath to inject dylib file. Use -l multiple time to inject multiple dylib files at once.\n");
	ZLog::Print("-w, --weak\t\tInject dylib as LC_LOAD_WEAK_DYLIB.\n");
	ZLog::Print("-i, --install\t\tInstall ipa file using ideviceinstaller ""command for test.\n");
	ZLog::Print("-q, --quiet\t\tQuiet operation.\n");
	ZLog::Print("-v, --version\t\tShows version.\n");
	ZLog::Print("-h, --help\t\tShows help (this message).\n");

	return -1;
}

string getCacheDirectory()
{
    const char* homeDir = nullptr;

#ifdef _WIN32
    char homeDirBuffer[MAX_PATH];
    if (GetEnvironmentVariable("USERPROFILE", homeDirBuffer, MAX_PATH) > 0)
    {
        homeDir = homeDirBuffer;
    }
    else
    {
        ZLog::ErrorV(">>> USERPROFILE environment variable is not set.");
        return "";
    }
#else
    homeDir = std::getenv("HOME");
    if (homeDir == nullptr)
    {
        ZLog::ErrorV(">>> HOME environment variable is not set.");
        return "";
    }
#endif

    std::string cacheDir = std::string(homeDir) + "/.cache/";

    struct stat st;
    if (stat(cacheDir.c_str(), &st) != 0){
        if (mkdir(cacheDir.c_str(), 0700) != 0) {
            ZLog::ErrorV("Error creating cache directory: %s", cacheDir.c_str());
            return "";
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        ZLog::ErrorV("Path exists but is not a directory: %s", cacheDir.c_str());
        return "";
    }
    return cacheDir;
}

int main(int argc, char* argv[])
{
	ZTimer gtimer;

	bool bForce = false;
	bool bInstall = false;
	bool bWeakInject = false;
	bool bAdhoc = false;
	bool bSingleInplace = false;
	bool bSHA256Only = false;
	uint32_t uZipLevel = 0;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strOutputFile;
	string strDisplayName;
	string strEntitlementsFile;

	vector<string> arrDyLibFiles;

	int opt = 0;
	int argslot = -1;
	while (-1 != (opt = getopt_long(argc, argv, "dfVvas2hc:k:m:o:ip:e:b:n:z:ql:w", options, &argslot)))
	{
		switch (opt) {
		case 'd':
			ZLog::SetLogLever(ZLog::E_DEBUG);
			break;
		case 'f':
			bForce = true;
			break;
		case 'V':
			ZLog::SetVerbose(true);
			break;
		case 'c':
			strCertFile = optarg;
			break;
		case 'k':
			strPKeyFile = optarg;
			break;
		case 'm':
			strProvFile = optarg;
			break;
		case 'a':
			bAdhoc = true;
			break;
		case 's':
			bSingleInplace = true;
			break;
		case '2':
			bSHA256Only = true;
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
			strEntitlementsFile = optarg;
			break;
		case 'l':
			arrDyLibFiles.push_back(optarg);
			break;
		case 'i':
			bInstall = true;
			break;
		case 'o':
			strOutputFile = GetCanonicalizePath(optarg);
			break;
		case 'z':
			uZipLevel = atoi(optarg);
			break;
		case 'w':
			bWeakInject = true;
			break;
		case 'q':
			ZLog::SetLogLever(ZLog::E_NONE);
			break;
		case 'v':
		{
			printf("version: 0.5\n");
			return 0;
		} break;
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
	if (bSingleInplace && !strOutputFile.empty()) {
		ZLog::ErrorV(">>> Only one of `--single_inplace` or `--output` can be specified\n");
		return usage();
	}

	if (ZLog::IsDebug()) {
		CreateFolder("./.zsign_debug");
		for (int i = optind; i < argc; i++) {
			ZLog::DebugV(">>> Argument:\t%s\n", argv[i]);
		}
	}

	string strPath = GetCanonicalizePath(argv[optind]);
	if (!IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		return -1;
	}

	bool bZipFile = false;
	ZMachO* macho = NULL;
	if (!IsFolder(strPath.c_str())) {
		bZipFile = IsZipFile(strPath.c_str());
		if (!bZipFile) { // macho file
			macho = new ZMachO();
			if (!macho->Init(strPath.c_str())) {
				ZLog::ErrorV(">>> Invalid mach-o file! %s\n", strPath.c_str());
				return -1;
			}
			if (!arrDyLibFiles.empty()) { // inject dylib
				bool bCreate = false;

				for (string dyLibFile : arrDyLibFiles)
					macho->InjectDyLib(bWeakInject, dyLibFile.c_str(), bCreate);
			} else if (!bSingleInplace) {
				macho->PrintInfo();
			}
			if (!bSingleInplace) { // no in-place sign requested; exit
				macho->Free();
				return 0;
			}
		}
	}

	ZSignAsset zsa;
	if (bAdhoc) {
		if (!zsa.Init(strEntitlementsFile)) {
			return -1;
		}
	} else {
		if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitlementsFile, strPassword)) {
			return -1;
		}
	}

	zsa.m_bUseSHA256Only = bSHA256Only;
	zsa.m_bSingleBinary = (NULL != macho);

	if (zsa.m_bSingleBinary) {
		ZLog::PrintV(">>>%s Signing:\t%s\n", (zsa.m_bAdhoc ? " Ad-hoc" : ""), strPath.c_str());
		string strInfoPlistSHA1;
		string strInfoPlistSHA256;
		string strCodeResourcesData;
		macho->Sign(&zsa, bForce, strBundleId, strInfoPlistSHA1, strInfoPlistSHA256, strCodeResourcesData);
		macho->Free();
		return 0;
	}

	ZTimer timer;

	bool bEnableCache = true;
	string strFolder = strPath;
	if (bZipFile) { // ipa file
		bForce = true;
		bEnableCache = false;
		StringFormatV(strFolder, "%s/zsign_folder_%llu", getCacheDirectory().c_str(), timer.Reset());
		ZLog::PrintV(">>> Unzip:\t%s (%s) -> %s ... \n", strPath.c_str(), GetFileSizeString(strPath.c_str()).c_str(), strFolder.c_str());
		RemoveFolder(strFolder.c_str());
		if (!SystemExecV("unzip -qq -d '%s' '%s'", strFolder.c_str(), strPath.c_str())) {
			RemoveFolder(strFolder.c_str());
			ZLog::ErrorV(">>> Unzip failed!\n");
			return -1;
		}
		timer.PrintResult(true, ">>> Unzip OK!");
	}

	timer.Reset();
	ZAppBundle bundle;
	bool bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDyLibFiles, bForce, bWeakInject, bEnableCache);
	timer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	if (bInstall && strOutputFile.empty()) {
		StringFormatV(strOutputFile, "%s/zsign_temp_%llu.ipa", getCacheDirectory().c_str(), GetMicroSecond());
	}

	if (!strOutputFile.empty()) {
		timer.Reset();
		size_t pos = bundle.m_strAppFolder.rfind("/Payload");
		if (string::npos == pos) {
			ZLog::Error(">>> Can't find payload directory!\n");
			return -1;
		}

		ZLog::PrintV(">>> Archiving: \t%s ... \n", strOutputFile.c_str());
		string strBaseFolder = bundle.m_strAppFolder.substr(0, pos);
		char szOldFolder[PATH_MAX] = { 0 };
		if (NULL != getcwd(szOldFolder, PATH_MAX)) {
			if (0 == chdir(strBaseFolder.c_str())) {
				uZipLevel = uZipLevel > 9 ? 9 : uZipLevel;
				RemoveFile(strOutputFile.c_str());
				SystemExecV("zip -q -%u -r '%s' Payload", uZipLevel, strOutputFile.c_str());
				chdir(szOldFolder);
				if (!IsFileExists(strOutputFile.c_str())) {
					ZLog::Error(">>> Archive failed!\n");
					return -1;
				}
			}
		}
		timer.PrintResult(true, ">>> Archive OK! (%s)", GetFileSizeString(strOutputFile.c_str()).c_str());
	}

	if (bRet && bInstall) {
		SystemExecV("ideviceinstaller -i '%s'", strOutputFile.c_str());
	}

	string tempPattern = getCacheDirectory() + "/zsign_temp_";
	if (0 == strOutputFile.find(tempPattern)) {
		RemoveFile(strOutputFile.c_str());
	}
	string folderPattern = getCacheDirectory() + "/zsign_folder_";
	if (0 == strFolder.find(folderPattern)) {
		RemoveFolder(strFolder.c_str());
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}
