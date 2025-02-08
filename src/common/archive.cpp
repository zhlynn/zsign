#include "archive.h"

#ifdef _WIN32
#include <minizip/zip.h>
#include <minizip/unzip.h>
#else
#include <zip.h>
#include <unzip.h>
#endif

void Zip::GetModificationTime(const char* path, void* zfi)
{
	zip_fileinfo* zi = (zip_fileinfo*)zfi;
	struct stat st = { 0 };
	memset(zi, 0, sizeof(zip_fileinfo));
	if (0 == stat(path, &st)) {
#ifdef _WIN32
		struct tm tm = { 0 };
		localtime_s(&tm, &st.st_mtime);
		zi->tmz_date.tm_sec = tm.tm_sec;
		zi->tmz_date.tm_min = tm.tm_min;
		zi->tmz_date.tm_hour = tm.tm_hour;
		zi->tmz_date.tm_mday = tm.tm_mday;
		zi->tmz_date.tm_mon = tm.tm_mon;
		zi->tmz_date.tm_year = tm.tm_year + 1900;
#else
		struct tm* tm = localtime(&st.st_mtime);
		zi->tmz_date.tm_sec = tm->tm_sec;
		zi->tmz_date.tm_min = tm->tm_min;
		zi->tmz_date.tm_hour = tm->tm_hour;
		zi->tmz_date.tm_mday = tm->tm_mday;
		zi->tmz_date.tm_mon = tm->tm_mon;
		zi->tmz_date.tm_year = tm->tm_year + 1900;
#endif
	}
}

bool Zip::_WriteFileToZip(void* hZip, const string& strFile, const string& strRelativePath, int zip_level)
{
	FILE* fp = NULL;
	_fopen64(fp, strFile.c_str(), "rb");
	if (NULL == fp) {
		ZLog::ErrorV(">>> Zip: Failed to open file: %s\n", strFile.c_str());
		return false;
	}

	zip_fileinfo zi = { 0 };
	GetModificationTime(strFile.c_str(), &zi);
	if (ZIP_OK != zipOpenNewFileInZip3_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0)) {
		fclose(fp);
		ZLog::ErrorV(">>> Zip: Failed to add file to zip: %s\n", strRelativePath.c_str());
		return false;
	}

	bool bRet = true;
	char buffer[4096];
	size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
	while (bytes_read > 0) {
		if (zipWriteInFileInZip(hZip, buffer, (uint32_t)bytes_read) < 0) {
			bRet = false;
			break;
		}
		bytes_read = fread(buffer, 1, sizeof(buffer), fp);
	}

	zipCloseFileInZip(hZip);
	fclose(fp);
	return bRet;
}

bool Zip::_CreateFolderToZip(void* hZip, const string& strFolder, const string& strRelativePath, int zip_level)
{
	zip_fileinfo zi = { 0 };
	GetModificationTime(strFolder.c_str(), &zi);
	if (ZIP_OK != zipOpenNewFileInZip3_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0)) {
		ZLog::ErrorV(">>> Zip: Failed to create folder to zip: %s\n", strRelativePath.c_str());
		return false;
	}
	zipCloseFileInZip(hZip);
	return true;
}

bool Zip::Archive(const string& strFolder, const string& strZipFile, int nZipLevel)
{
	 if (nZipLevel < 0 || nZipLevel > 9) {
		ZLog::ErrorV(">>> Zip: Invalid compression level: %d\n", nZipLevel);
        return false;
    }
    
    zipFile zf = zipOpen64(strZipFile.c_str(), 0);
    if (!zf) {
		ZLog::ErrorV(">>> Zip: Failed to create zip file: %s\n", strZipFile.c_str());
        return false;
    }

	bool bRet = true;
	ZFile::EnumFolder(strFolder.c_str(), true, NULL, [&](bool bFolder, const string& strPath) {
		string strRelativePath = strPath.substr(strFolder.size() + 1);
		ZUtil::StringReplace(strRelativePath, "\\", "/");

#ifdef _WIN32
		iconv ic;
		strRelativePath = ic.A2U8(strRelativePath);
#endif

		if (bFolder) {
			strRelativePath += "/";
			if (!_CreateFolderToZip(zf, strPath, strRelativePath, nZipLevel)) {
				bRet = false;
				return true;
			}
		} else {
			if (!_WriteFileToZip(zf, strPath, strRelativePath, nZipLevel)) {
				bRet = false;
				return true;
			}
		}
		return false;
	});

    zipClose(zf, NULL);
	return bRet;
}

bool Zip::_EnumZipItems(const char* zip_file, enum_zip_items_callback callback)
{
	unzFile uf = unzOpen64(zip_file);
	if (NULL == uf) {
		return false;
	}

	unz_global_info64 gi;
	if (UNZ_OK != unzGetGlobalInfo64(uf, &gi)) {
		unzClose(uf);
		return false;
	}

	bool bRet = true;
	unz_file_info64 fi = { 0 };
	char szPath[PATH_MAX] = { 0 };
	for (int i = 0; i < gi.number_entry; i++) {
		if (UNZ_OK != unzGetCurrentFileInfo64(uf, &fi, szPath, PATH_MAX, NULL, 0, NULL, 0)) {
			bRet = false;
			break;
		}

		string strPath = szPath;
		ZUtil::StringTrim(strPath);

#ifdef _WIN32
		iconv ic;
		strPath = ic.U82A(strPath);
#endif

		bool bFolder = false;
		if (('/' == strPath.back())) {
			bFolder = true;
			strPath.pop_back();
		}

		if (NULL != callback) {
			if (!callback(uf, bFolder, strPath)) {
				bRet = false;
				break;
			}
		}

		if (i < gi.number_entry - 1) {
			if (UNZ_OK != unzGoToNextFile(uf)) {
				bRet = false;
				break;
			}
		}
	}

	unzClose(uf);
	return bRet;
}

bool Zip::_ReadFileFromZip(void* hZip, const string& strPath, const string& strRootFolder)
{
	string strFile = strRootFolder + "/" + strPath;
	string strFolder = strFile;
	ZFile::PathRemoveFileSpec(strFolder);
	if (!ZFile::CreateFolder(strFolder.c_str())) {
		return false;
	}

	if (UNZ_OK != unzOpenCurrentFile(hZip)) {
		return false;
	}

	FILE* fp = NULL;
	_fopen64(fp, strFile.c_str(), "wb");
	if (NULL == fp) {
		unzCloseCurrentFile(hZip);
		return false;
	}

	bool bRet = true;
	uint32_t uBufSize = 512 * 1024;
	char* pbuff = (char*)malloc(uBufSize);
	if (NULL != pbuff) {
		int32_t nReaded = unzReadCurrentFile(hZip, pbuff, uBufSize);
		while (nReaded > 0) {
			if (nReaded != fwrite(pbuff, 1, nReaded, fp)) {
				bRet = false;
				break;
			}
			nReaded = unzReadCurrentFile(hZip, pbuff, uBufSize);
		}
		free(pbuff);
	} else {
		bRet = false;
	}

	fclose(fp);
	unzCloseCurrentFile(hZip);
	return bRet;
}

bool Zip::_Extract(const char* zip_file, const char* output_folder)
{
	return _EnumZipItems(zip_file, [&](unzFile uFile, bool bFolder, const string& strPath) {
		if (bFolder) {
			if (!ZFile::CreateFolderV("%s/%s", output_folder, strPath.c_str())) {
				return false;
			}
		} else {
			if (!_ReadFileFromZip(uFile, strPath, output_folder)) {
				return false;
			}
		}
		return true;
	});
}

bool Zip::Extract(const char* zip_file, const char* output_folder)
{
	ZFile::RemoveFolder(output_folder);
	if (!_Extract(zip_file, output_folder)) {
		ZFile::RemoveFolder(output_folder);
		return false;
	}
	return true;
}
