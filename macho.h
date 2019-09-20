#pragma once
#include "archo.h"

class ZMachO
{
public:
	ZMachO();
	~ZMachO();

public:
	bool Init(const char *szFile);
	bool InitV(const char *szFormatPath, ...);
	bool Free();
	void PrintInfo();
	bool Sign(ZSignAsset *pSignAsset, bool bForce, string strBundleId, string strInfoPlistSHA1, string strInfoPlistSHA256, const string &strCodeResourcesData);
	bool InjectDyLib(bool bWeakInject, const char *szDyLibPath, bool &bCreate);

private:
	bool OpenFile(const char *szPath);
	bool CloseFile();
	bool ReallocCodeSignSpace();
	bool NewArchO(uint8_t *pBase, uint32_t uLength);
	void FreeArchOes();

private:
	uint8_t *m_pBase;
	size_t m_sSize;
	string m_strFile;
	vector<ZArchO *> m_arrArchOes;
	bool m_bCSRealloced;
};
