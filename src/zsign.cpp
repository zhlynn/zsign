#include "common.h"
#include "macho.h"
#include "bundle.h"
#include "openssl.h"
#include "timer.h"
#include "archive.h"

#ifdef _WIN32
#include "common_win32.h"
#endif

#define ZSIGN_VERSION "0.7"

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
	{"weak", no_argument, NULL, 'w'},
	{"temp_folder", required_argument, NULL, 't'},
	{"sha256_only", no_argument, NULL, '2'},
	{"install", no_argument, NULL, 'i'},
	{"check", no_argument, NULL, 'C'},
	{"quiet", no_argument, NULL, 'q'},
	{"help", no_argument, NULL, 'h'},
	{}
};

int usage()
{
	ZLog::PrintV("zsign (v%s) is a codesign alternative for iOS12+ on macOS, Linux and Windows. \nVisit https://github.com/zhlynn/zsign for more information.\n\n", ZSIGN_VERSION);
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
	ZLog::Print("-w, --weak\t\tInject dylib as LC_LOAD_WEAK_DYLIB.\n");
	ZLog::Print("-i, --install\t\tInstall ipa file using ideviceinstaller command for test.\n");
	ZLog::Print("-t, --temp_folder\tPath to temporary folder for intermediate files.\n");
	ZLog::Print("-2, --sha256_only\tSerialize a single code directory that uses SHA256.\n");
	ZLog::Print("-C, --check\t\tCheck if the file is signed.\n");
	ZLog::Print("-q, --quiet\t\tQuiet operation.\n");
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
	bool bSHA256Only = false;
	bool bCheckSignature = false;
	uint32_t uZipLevel = 0;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strOutputFile;
	string strDisplayName;
	string strEntitleFile;
	vector<string> arrDylibFiles;
	string strTempFolder = ZFile::GetTempFolder();

	int opt = 0;
	int argslot = -1;
	while (-1 != (opt = getopt_long(argc, argv, "dfva2hiqwc:k:m:o:p:e:b:n:z:l:t:r:",
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
			bSHA256Only = true;
			break;
		case 'C':
			bCheckSignature = true;
			break;
		case 'q':
			ZLog::SetLogLever(ZLog::E_NONE);
			break;
		case 'v': {
			printf("version: %s\n", ZSIGN_VERSION);
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

	if (ZLog::IsDebug()) {
		ZFile::CreateFolder("./.zsign_debug");
		for (int i = optind; i < argc; i++) {
			ZLog::DebugV(">>> Argument:\t%s\n", argv[i]);
		}
	}

	bool bZipFile = ZFile::IsZipFile(strPath.c_str());
	if (!bZipFile && !ZFile::IsFolder(strPath.c_str())) { // macho file
		ZMachO* macho = new ZMachO();
		if (!macho->Init(strPath.c_str())) {
			ZLog::ErrorV(">>> Invalid mach-o file! %s\n", strPath.c_str());
			return -1;
		}

		if (!bAdhoc && arrDylibFiles.empty() && (strPKeyFile.empty() || strProvFile.empty())) {
			if (bCheckSignature) {
				return macho->CheckSignature() ? 0 : -2;
			} else {
				macho->PrintInfo();
				return 0;
			}
		}

		ZSignAsset zsa;
		if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, true)) {
			return -1;
		}

		if (!arrDylibFiles.empty()) {
			for (string dyLibFile : arrDylibFiles) {
				if (!macho->InjectDylib(bWeakInject, dyLibFile.c_str())) {
					return -1;
				}
			}
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
	bool bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, bForce, bWeakInject, bEnableCache);
	atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

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
			}
		} else {
			ZLog::Error(">>> Can't find payload directory!\n");
			bRet = false;
		}
	}

	//install
	if (bRet && bInstall) {
		bRet = ZUtil::SystemExecV("ideviceinstaller -i  \"%s\"", strOutputFile.c_str());
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
