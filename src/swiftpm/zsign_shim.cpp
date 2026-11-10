#include "zsign_shim.h"

#include "common.h"
#include "archive.h"
#include "bundle.h"
#include "openssl.h"

#include <list>
#include <mutex>

static std::mutex s_lock;
static std::string s_log;

static void CaptureLog(const char* szLog)
{
	s_log.append(szLog);
}

static bool Finish(bool bResult, char** error)
{
	if (!bResult && NULL != error) {
		std::string strMessage = s_log.empty() ? "zsign failed without logging an error" : s_log;
		*error = strdup(strMessage.c_str());
	}
	ZLog::SetSink(NULL);
	return bResult;
}

static void Begin(char** error)
{
	if (NULL != error) {
		*error = NULL;
	}
	s_log.clear();
	ZLog::SetLogLever(ZLog::E_ERROR);
	ZLog::SetSink(CaptureLog);
}

static std::string OrEmpty(const char* szValue)
{
	return (NULL != szValue) ? szValue : "";
}

bool zsign_extract(const char* ipa_path, const char* folder, char** error)
{
	std::lock_guard<std::mutex> guard(s_lock);
	Begin(error);
	return Finish(Zip::Extract(ipa_path, folder), error);
}

bool zsign_sign_folder(const char* folder,
						const char* private_key_path,
						const char* certificate_path,
						const char* password,
						const zsign_asset_t* assets,
						size_t asset_count,
						char** error)
{
	std::lock_guard<std::mutex> guard(s_lock);
	Begin(error);

	if (0 == asset_count) {
		ZLog::Error(">>> No signing assets provided!\n");
		return Finish(false, error);
	}

	std::list<ZSignAsset> signAssets;
	for (size_t i = 0; i < asset_count; i++) {
		signAssets.emplace_back();
		if (!signAssets.back().Init(OrEmpty(certificate_path),
									OrEmpty(private_key_path),
									OrEmpty(assets[i].profile_path),
									OrEmpty(assets[i].entitlements_path),
									OrEmpty(password),
									false,
									true,
									false)) {
			return Finish(false, error);
		}
	}

	ZBundle bundle;
	std::vector<std::string> arrNone;
	bool bResult = bundle.SignFolder(&signAssets, folder, "", "", "", arrNone, arrNone, true, false, false);
	return Finish(bResult, error);
}

bool zsign_archive(const char* folder, const char* ipa_path, int zip_level, char** error)
{
	std::lock_guard<std::mutex> guard(s_lock);
	Begin(error);
	return Finish(Zip::Archive(folder, ipa_path, zip_level), error);
}

void zsign_free(char* pointer)
{
	free(pointer);
}
