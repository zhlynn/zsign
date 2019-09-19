#pragma once
#include "common/json.h"

bool GenerateCMS(const string &strSignerCertData, const string &strSignerPKeyData, const string &strCDHashData, const string &strCDHashPlist, string &strCMSOutput);
bool GetCMSContent(const string &strCMSDataInput, string &strContentOutput);
bool GetCertSubjectCN(const string &strCertData, string &strSubjectCN);
bool GetCMSInfo(uint8_t *pCMSData, uint32_t uCMSLength, JValue &jvOutput);

class ZSignAsset
{
public:
	ZSignAsset();

public:
	bool GenerateCMS(const string &strCDHashData, const string &strCDHashesPlist, string &strCMSOutput);
	bool Init(const string &strSignerCertFile, const string &strSignerPKeyFile, const string &strProvisionFile, const string &strEntitlementsFile, const string &strPassword);

public:
	string m_strTeamId;
	string m_strSubjectCN;
	string m_strProvisionData;
	string m_strEntitlementsData;

private:
	void *m_evpPkey;
	void *m_x509Cert;
};