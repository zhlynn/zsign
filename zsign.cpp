#include "common/common.h"
#include "common/json.h"
#include "openssl.h"
#include "macho.h"
#include "bundle.h"
#include <libgen.h>
#include <dirent.h>
#include <getopt.h>

const struct option options[] = {
	{ "debug",			no_argument,			NULL, 'd' },
	{ "force",			no_argument,			NULL, 'f' },
	{ "verbose",		no_argument,			NULL, 'v' },
	{ "cert",			required_argument,		NULL, 'c' },
	{ "pkey",			required_argument,		NULL, 'k' },
	{ "prov",			required_argument,		NULL, 'm' },
	{ "password",		required_argument,		NULL, 'p' },
	{ "bundleid",		required_argument,		NULL, 'b' },
	{ "bundlename",		required_argument,		NULL, 'n' },
	{ "entitlements",	required_argument,		NULL, 'e' },
	{ "output",			no_argument,			NULL, 'o' },
	{ "ziplevel",		required_argument,		NULL, 'z' },
	{ "dylib",			required_argument,		NULL, 'l' },
	{ "install",		no_argument,			NULL, 'i' },
	{ "quiet",			no_argument,			NULL, 'q' },
	{ "help",			no_argument,			NULL, 'h' },
	{ }
};

int usage()
{
	ZLog::Print("Usage: \n");
	return -1;
}

int main(int argc, char* argv[])
{
	ZTimer gtimer;

	bool bForce = false;
	bool bInstall = false;
	uint32_t uZipLevel = 0;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strDyLibFile;
	string strOutputFile;
	string strDisplayName;
	string strEntitlementsFile;
	
	int opt = 0;
	int argslot = -1;
	while(-1 != (opt = getopt_long(argc, argv, "dfvhc:k:m:o:ip:e:b:n:z:ql:", options, &argslot)))
	{
		switch (opt) 
		{
			case 'd':
				ZLog::SetLogLever(ZLog::E_DEBUG);
				break;
			case 'f':
				bForce = true;
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
			case 'p':
				strPassword = optarg;
				break;
			case 'b':
				strBundleId = optarg;
				break;
			case 'n':
				strDisplayName = optarg;
				break;
			case 'e':
				strEntitlementsFile = optarg;
				break;
			case 'l':
				strDyLibFile = optarg;
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
			case 'q':
				ZLog::SetLogLever(ZLog::E_NONE);
				break;
			case 'v':
				break;
			case 'h':
			case '?':
				return usage();
				break;
		}

		ZLog::DebugV(">>> Option:\t-%c, %s\n", opt, optarg);
	}

	if(optind >= argc)
	{
		return usage();
	}
	
	if(ZLog::IsDebug())
	{
		CreateFolder("./.zsign_debug");
		for(int i = optind; i < argc; i++)
		{
			ZLog::DebugV(">>> Argument:\t%s\n", argv[i]);
		}
	}
	
	string strPath = GetCanonicalizePath(argv[optind]);
	if(!IsFileExists(strPath.c_str()))
	{
		ZLog::ErrorV(">>> Invalid Path! %s\n", strPath.c_str());
		return -1;
	}

	bool bZipFile = false;
	if (!IsFolder(strPath.c_str()))
	{
		bZipFile = IsZipFile(strPath.c_str());
		if(!bZipFile)
		{//macho file
			ZMachO macho;
			if (macho.Init(strPath.c_str()))
			{
				macho.PrintInfo();
			}
			return 0;
		}
	}

	ZTimer timer;
	ZSignAsset zSignAsset;
	if(!zSignAsset.Init(strCertFile, strPKeyFile, strProvFile, strEntitlementsFile, strPassword))
	{
		return -1;
	}

	bool bEnableCache = true;
	string strFolder = strPath;
	if (bZipFile)
	{//ipa file
		bForce = true;
		bEnableCache = false;
		StringFormat(strFolder, "/tmp/zsign_folder_%llu", timer.Reset());
		ZLog::PrintV(">>> Unzip:\t%s (%s) -> %s ... \n", strPath.c_str(), GetFileSizeString(strPath.c_str()).c_str(), strFolder.c_str());
		RemoveFolder(strFolder.c_str());
		if(!SystemExec("unzip -qq -d '%s' '%s'", strFolder.c_str(), strPath.c_str()))
		{
			RemoveFolder(strFolder.c_str());
			ZLog::ErrorV(">>> Unzip Failed!\n");
			return -1;
		}
		timer.PrintResult(true, ">>> Unzip OK!");
	}

	timer.Reset();
	ZAppBundle bundle;
	bool bRet = bundle.SignFolder(&zSignAsset, strFolder, strBundleId, strDisplayName, strDyLibFile, bForce, bEnableCache);
	timer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	if(bInstall && strOutputFile.empty())
	{
		StringFormat(strOutputFile, "/tmp/zsign_temp_%llu.ipa", GetMicroSencond());
	}
	
	if (!strOutputFile.empty())
	{
		timer.Reset();
		size_t pos = bundle.m_strAppFolder.rfind("/Payload");
		if (string::npos == pos)
		{
			ZLog::Error(">>> Can't Find Payload Directory!\n");
			return -1;
		}

		if(!IsPathSuffix(strOutputFile, ".ipa"))
		{
			strOutputFile += ".ipa";
		}
		
		ZLog::PrintV(">>> Archiving: \t%s ... \n", strOutputFile.c_str());
		string strBaseFolder = bundle.m_strAppFolder.substr(0, pos);
		char szOldFolder[PATH_MAX] = {0};
		if (NULL != getcwd(szOldFolder, PATH_MAX))
		{
			if (0 == chdir(strBaseFolder.c_str()))
			{
				uZipLevel = uZipLevel > 9 ? 9 : uZipLevel;
				RemoveFile(strOutputFile.c_str());
				SystemExec("zip -q -%u -r '%s' Payload", uZipLevel, strOutputFile.c_str());
				chdir(szOldFolder);
				if(!IsFileExists(strOutputFile.c_str()))
				{
					ZLog::Error(">>> Archive Failed!\n");
					return -1;
				}
			}
		}
		timer.PrintResult(true, ">>> Archive OK! (%s)", GetFileSizeString(strOutputFile.c_str()).c_str());
	}

	if(bRet && bInstall)
	{
		SystemExec("ideviceinstaller -i '%s'", strOutputFile.c_str());
	}

	if(0 == strOutputFile.find("/tmp/zsign_tmp_"))
	{
		RemoveFile(strOutputFile.c_str());
	}

	if(0 == strFolder.find("/tmp/zsign_folder_"))
	{
		RemoveFolder(strFolder.c_str());
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}