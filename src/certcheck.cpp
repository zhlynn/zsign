#include "common.h"
#include "certcheck.h"
#include "openssl.h"
#include "mach-o.h"
#include "signing.h"
#include "macho.h"

#include "third-party/minizip/zip.h"
#include "third-party/minizip/unzip.h"

#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/cms.h>
#include <openssl/ocsp.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <openssl/x509v3.h>
#include <ctime>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int ssize_t;
#define OCSP_CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#define OCSP_CLOSE_SOCKET(s) close(s)
#endif

// ─── helpers ───────────────────────────────────────────────────────

static string SerialToHex(X509* cert)
{
	ASN1_INTEGER* serial = X509_get_serialNumber(cert);
	if (!serial) return "";
	BIGNUM* bn = ASN1_INTEGER_to_BN(serial, NULL);
	if (!bn) return "";
	char* hex = BN_bn2hex(bn);
	BN_free(bn);
	if (!hex) return "";
	string raw(hex);
	OPENSSL_free(hex);
	string formatted;
	for (size_t i = 0; i < raw.size(); i++) {
		if (i > 0 && i % 2 == 0) formatted += ':';
		formatted += raw[i];
	}
	return formatted;
}

static string TimeToISO(const ASN1_TIME* t)
{
	if (!t) return "N/A";
	struct tm stm;
	memset(&stm, 0, sizeof(stm));
	if (ASN1_TIME_to_tm(t, &stm) != 1) return "N/A";
	char buf[64];
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &stm);
	return string(buf);
}

static int DaysRemaining(const ASN1_TIME* t)
{
	if (!t) return -1;
	int day = 0, sec = 0;
	if (ASN1_TIME_diff(&day, &sec, NULL, t) != 1) return -1;
	return day;
}

static string GetNameField(X509_NAME* name, int nid)
{
	if (!name) return "";
	int idx = X509_NAME_get_index_by_NID(name, nid, -1);
	if (idx < 0) return "";
	X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
	if (!entry) return "";
	ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
	if (!data) return "";
	unsigned char* utf8 = NULL;
	int len = ASN1_STRING_to_UTF8(&utf8, data);
	if (len < 0 || !utf8) return "";
	string result((const char*)utf8, len);
	OPENSSL_free(utf8);
	return result;
}

static string GetKeyAlgorithm(X509* cert)
{
	EVP_PKEY* pkey = X509_get0_pubkey(cert);
	if (!pkey) return "Unknown";
	int type = EVP_PKEY_id(pkey);
	int bits = EVP_PKEY_bits(pkey);
	string algo;
	switch (type) {
	case EVP_PKEY_RSA: algo = "RSA"; break;
	case EVP_PKEY_EC:  algo = "EC"; break;
	case EVP_PKEY_ED25519: algo = "Ed25519"; break;
	default: algo = OBJ_nid2sn(type); break;
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "%s %d-bit", algo.c_str(), bits);
	return string(buf);
}

static string DetectCertType(const string& cn)
{
	if (cn.find("Apple Distribution") != string::npos) return "Apple Distribution";
	if (cn.find("iPhone Distribution") != string::npos) return "iPhone Distribution";
	if (cn.find("Apple Development") != string::npos) return "Apple Development";
	if (cn.find("iPhone Developer") != string::npos) return "iPhone Developer";
	if (cn.find("Mac Developer") != string::npos) return "Mac Developer";
	if (cn.find("Developer ID Application") != string::npos) return "Developer ID Application";
	if (cn.find("Developer ID Installer") != string::npos) return "Developer ID Installer";
	return "Certificate";
}

// ─── issuer resolution ──────────────────────────────────────────────

static X509* LoadEmbeddedCert(const char* pem)
{
	BIO* bio = BIO_new_mem_buf(pem, (int)strlen(pem));
	if (!bio) return NULL;
	X509* cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	return cert;
}

static X509* ResolveIssuer(X509* cert)
{
	unsigned long issuerHash = X509_issuer_name_hash(cert);
	if (issuerHash == 0x817d2f7a)
		return LoadEmbeddedCert(ZSignAsset::s_szAppleDevCACert);
	if (issuerHash == 0x9b16b75c)
		return LoadEmbeddedCert(ZSignAsset::s_szAppleDevCACertG3);

	X509* issuer = LoadEmbeddedCert(ZSignAsset::s_szAppleDevCACertG3);
	if (issuer && X509_check_issued(issuer, cert) == X509_V_OK) return issuer;
	if (issuer) X509_free(issuer);

	issuer = LoadEmbeddedCert(ZSignAsset::s_szAppleDevCACert);
	if (issuer && X509_check_issued(issuer, cert) == X509_V_OK) return issuer;
	if (issuer) X509_free(issuer);

	return NULL;
}

// ─── file type detection ────────────────────────────────────────────

enum CertFileType {
	CERT_FILE_UNKNOWN = 0,
	CERT_FILE_PROVISION,
	CERT_FILE_P12,
	CERT_FILE_CER,
	CERT_FILE_PEM,
	CERT_FILE_IPA,
	CERT_FILE_MACHO,
};

static CertFileType DetectFileType(const string& path, const string& data)
{
	string ext;
	size_t dotPos = path.rfind('.');
	if (dotPos != string::npos) {
		ext = path.substr(dotPos);
		transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	}

	if (ext == ".mobileprovision" || ext == ".provisionprofile") return CERT_FILE_PROVISION;
	if (ext == ".p12" || ext == ".pfx") return CERT_FILE_P12;
	if (ext == ".cer" || ext == ".der" || ext == ".crt") return CERT_FILE_CER;
	if (ext == ".pem") return CERT_FILE_PEM;
	if (ext == ".ipa" || ext == ".zip") return CERT_FILE_IPA;

	if (data.size() >= 4) {
		const uint8_t* d = (const uint8_t*)data.data();
		if (d[0] == 0x50 && d[1] == 0x4B && d[2] == 0x03 && d[3] == 0x04) return CERT_FILE_IPA;
		if ((d[0] == 0xFE && d[1] == 0xED && d[2] == 0xFA && d[3] == 0xCE) ||
			(d[0] == 0xCE && d[1] == 0xFA && d[2] == 0xED && d[3] == 0xFE) ||
			(d[0] == 0xFE && d[1] == 0xED && d[2] == 0xFA && d[3] == 0xCF) ||
			(d[0] == 0xCF && d[1] == 0xFA && d[2] == 0xED && d[3] == 0xFE) ||
			(d[0] == 0xCA && d[1] == 0xFE && d[2] == 0xBA && d[3] == 0xBE))
			return CERT_FILE_MACHO;
		if (data.find("<?xml") != string::npos && data.find("</plist>") != string::npos)
			return CERT_FILE_PROVISION;
		if (d[0] == 0x30 && data.size() > 500) return CERT_FILE_P12;
		if (d[0] == 0x30) return CERT_FILE_CER;
		if (data.find("-----BEGIN") != string::npos) return CERT_FILE_PEM;
	}
	return CERT_FILE_UNKNOWN;
}

// ─── cert extraction ────────────────────────────────────────────────

static X509* LoadFromProvision(const string& data)
{
	BIO* bio = BIO_new_mem_buf(data.data(), (int)data.size());
	if (!bio) return NULL;
	CMS_ContentInfo* cms = d2i_CMS_bio(bio, NULL);
	BIO_free(bio);
	if (!cms) return NULL;

	ASN1_OCTET_STRING** pos = CMS_get0_content(cms);
	if (!pos || !(*pos)) { CMS_ContentInfo_free(cms); return NULL; }
	string xmlContent((const char*)(*pos)->data, (*pos)->length);
	CMS_ContentInfo_free(cms);

	size_t keyPos = xmlContent.find("<key>DeveloperCertificates</key>");
	if (keyPos == string::npos) return NULL;
	size_t arrayStart = xmlContent.find("<array>", keyPos);
	size_t dataStart = xmlContent.find("<data>", arrayStart);
	size_t dataEnd = xmlContent.find("</data>", dataStart);
	if (dataStart == string::npos || dataEnd == string::npos) return NULL;
	dataStart += 6;
	string cleanB64;
	for (size_t i = dataStart; i < dataEnd; i++) {
		if (!isspace(xmlContent[i])) cleanB64 += xmlContent[i];
	}

	BIO* b64Bio = BIO_new(BIO_f_base64());
	BIO* memBio = BIO_new_mem_buf(cleanB64.data(), (int)cleanB64.size());
	BIO_set_flags(b64Bio, BIO_FLAGS_BASE64_NO_NL);
	memBio = BIO_push(b64Bio, memBio);
	vector<uint8_t> certData(cleanB64.size());
	int decoded = BIO_read(memBio, certData.data(), (int)certData.size());
	BIO_free_all(memBio);
	if (decoded <= 0) return NULL;
	const uint8_t* p = certData.data();
	return d2i_X509(NULL, &p, decoded);
}

static X509* LoadFromP12(const string& data, const string& password, STACK_OF(X509)** ca)
{
	BIO* bio = BIO_new_mem_buf(data.data(), (int)data.size());
	if (!bio) return NULL;
	OSSL_PROVIDER_load(NULL, "default");
	OSSL_PROVIDER_load(NULL, "legacy");
	ERR_clear_error();
	PKCS12* p12 = d2i_PKCS12_bio(bio, NULL);
	BIO_free(bio);
	if (!p12) return NULL;
	X509* cert = NULL;
	EVP_PKEY* pkey = NULL;
	if (PKCS12_parse(p12, password.c_str(), &pkey, &cert, ca) != 1) { PKCS12_free(p12); return NULL; }
	if (pkey) EVP_PKEY_free(pkey);
	PKCS12_free(p12);
	return cert;
}

static X509* LoadFromCER(const string& data)
{
	const uint8_t* p = (const uint8_t*)data.data();
	X509* cert = d2i_X509(NULL, &p, (long)data.size());
	if (cert) return cert;
	BIO* bio = BIO_new_mem_buf(data.data(), (int)data.size());
	if (!bio) return NULL;
	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	return cert;
}

// ─── Leaf cert from CMS ─────────────────────────────────────────────

static X509* FindLeafCert(STACK_OF(X509)* certs)
{
	if (!certs || sk_X509_num(certs) <= 0) return NULL;
	if (sk_X509_num(certs) == 1) {
		X509* c = sk_X509_value(certs, 0); X509_up_ref(c); return c;
	}
	for (int i = 0; i < sk_X509_num(certs); i++) {
		X509* c = sk_X509_value(certs, i);
		if (X509_check_ca(c) > 0) continue;
		X509_up_ref(c); return c;
	}
	X509* c = sk_X509_value(certs, sk_X509_num(certs) - 1);
	X509_up_ref(c); return c;
}

static X509* ExtractCertFromCMS(uint8_t* pCMSData, uint32_t uCMSLength)
{
	if (!pCMSData || uCMSLength <= 0) return NULL;
	BIO* bio = BIO_new_mem_buf(pCMSData, uCMSLength);
	if (!bio) return NULL;
	CMS_ContentInfo* cms = d2i_CMS_bio(bio, NULL);
	BIO_free(bio);
	if (!cms) return NULL;
	STACK_OF(X509)* certs = CMS_get1_certs(cms);
	CMS_ContentInfo_free(cms);
	X509* leaf = FindLeafCert(certs);
	if (certs) sk_X509_pop_free(certs, X509_free);
	return leaf;
}

// ─── Mach-O cert extraction ─────────────────────────────────────────

struct MachOSignInfo {
	bool isSigned;
	X509* cert;
};

static MachOSignInfo ExtractFromMachOData(uint8_t* pBase, uint32_t uLength)
{
	MachOSignInfo info = { false, NULL };
	ZArchO archo;
	if (!archo.Init(pBase, uLength)) return info;
	if (archo.m_pSignBase && archo.m_uSignLength > 0) {
		info.isSigned = true;
		CS_SuperBlob* psb = (CS_SuperBlob*)archo.m_pSignBase;
		if (CSMAGIC_EMBEDDED_SIGNATURE == LE(psb->magic)) {
			CS_BlobIndex* pbi = (CS_BlobIndex*)(archo.m_pSignBase + sizeof(CS_SuperBlob));
			for (uint32_t i = 0; i < LE(psb->count); i++, pbi++) {
				if (LE(pbi->type) != CSSLOT_SIGNATURESLOT) continue;
				uint8_t* pSlotBase = archo.m_pSignBase + LE(pbi->offset);
				uint32_t uSlotLength = LE(*(((uint32_t*)pSlotBase) + 1));
				if (uSlotLength <= 8) continue;
				info.cert = ExtractCertFromCMS(pSlotBase + 8, uSlotLength - 8);
				break;
			}
		}
	}
	return info;
}

// ─── Read from zip (no extraction) ──────────────────────────────────

static bool ReadFileFromZipToMemory(unzFile uf, string& outData)
{
	if (UNZ_OK != unzOpenCurrentFile(uf)) return false;
	outData.clear();
	char buf[65536];
	int nRead;
	while ((nRead = unzReadCurrentFile(uf, buf, sizeof(buf))) > 0)
		outData.append(buf, nRead);
	unzCloseCurrentFile(uf);
	return (nRead >= 0);
}

static MachOSignInfo LoadFromIPA(const string& ipaPath)
{
	MachOSignInfo info = { false, NULL };
	unzFile uf = unzOpen64(ipaPath.c_str());
	if (!uf) return info;

	unz_global_info64 gi;
	if (UNZ_OK != unzGetGlobalInfo64(uf, &gi)) { unzClose(uf); return info; }

	string strAppFolder, strInfoPlistData, strExecName;
	char szPath[PATH_MAX];

	for (uint64_t i = 0; i < gi.number_entry; i++) {
		unz_file_info64 fi;
		if (UNZ_OK != unzGetCurrentFileInfo64(uf, &fi, szPath, PATH_MAX, NULL, 0, NULL, 0)) break;
		string path = szPath;
		if (path.find("Payload/") == 0 && path.find(".app/Info.plist") != string::npos) {
			size_t s1 = path.find('/'), s2 = path.find('/', s1 + 1), s3 = path.find('/', s2 + 1);
			if (s3 == string::npos) {
				strAppFolder = path.substr(0, s2 + 1);
				ReadFileFromZipToMemory(uf, strInfoPlistData);
				break;
			}
		}
		if (i < gi.number_entry - 1 && UNZ_OK != unzGoToNextFile(uf)) break;
	}

	if (strInfoPlistData.empty()) { unzClose(uf); return info; }

	jvalue jvInfo;
	if (jvInfo.read_plist(strInfoPlistData))
		strExecName = jvInfo["CFBundleExecutable"].as_cstr();
	if (strExecName.empty()) { unzClose(uf); return info; }

	string strExecPath = strAppFolder + strExecName;

	string strBinaryData;
	if (UNZ_OK != unzGoToFirstFile(uf)) { unzClose(uf); return info; }
	for (uint64_t i = 0; i < gi.number_entry; i++) {
		unz_file_info64 fi;
		if (UNZ_OK != unzGetCurrentFileInfo64(uf, &fi, szPath, PATH_MAX, NULL, 0, NULL, 0)) break;
		if (string(szPath) == strExecPath) { ReadFileFromZipToMemory(uf, strBinaryData); break; }
		if (i < gi.number_entry - 1 && UNZ_OK != unzGoToNextFile(uf)) break;
	}
	unzClose(uf);

	if (strBinaryData.empty()) return info;
	return ExtractFromMachOData((uint8_t*)strBinaryData.data(), (uint32_t)strBinaryData.size());
}

// ─── OCSP check ─────────────────────────────────────────────────────

struct OCSPResult {
	string status;
	string revokedTime;
	string errorDetail;
};

static bool ExtractOCSPUrl(X509* cert, string& host, string& port, string& path)
{
	AUTHORITY_INFO_ACCESS* aia = (AUTHORITY_INFO_ACCESS*)X509_get_ext_d2i(cert, NID_info_access, NULL, NULL);
	if (!aia) return false;
	for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(aia); i++) {
		ACCESS_DESCRIPTION* ad = sk_ACCESS_DESCRIPTION_value(aia, i);
		if (OBJ_obj2nid(ad->method) != NID_ad_OCSP) continue;
		if (ad->location->type != GEN_URI) continue;
		const unsigned char* uriData = ASN1_STRING_get0_data(ad->location->d.uniformResourceIdentifier);
		int uriLen = ASN1_STRING_length(ad->location->d.uniformResourceIdentifier);
		if (!uriData || uriLen <= 0) continue;
		string url((const char*)uriData, uriLen);
		// parse http://host[:port]/path
		size_t schemeEnd = url.find("://");
		if (schemeEnd == string::npos) continue;
		size_t hostStart = schemeEnd + 3;
		size_t pathStart = url.find('/', hostStart);
		string hostPort = (pathStart != string::npos) ? url.substr(hostStart, pathStart - hostStart) : url.substr(hostStart);
		path = (pathStart != string::npos) ? url.substr(pathStart) : "/";
		size_t colonPos = hostPort.find(':');
		if (colonPos != string::npos) {
			host = hostPort.substr(0, colonPos);
			port = hostPort.substr(colonPos + 1);
		} else {
			host = hostPort;
			port = "80";
		}
		AUTHORITY_INFO_ACCESS_free(aia);
		return true;
	}
	AUTHORITY_INFO_ACCESS_free(aia);
	return false;
}

static OCSPResult PerformOCSP(X509* cert, X509* issuer)
{
	OCSPResult result;
	result.status = "Error";
	if (!cert || !issuer) { result.errorDetail = "Missing certificate or issuer"; return result; }

	string ocspHost, ocspPort, ocspPath;
	if (!ExtractOCSPUrl(cert, ocspHost, ocspPort, ocspPath)) {
		ocspHost = "ocsp.apple.com";
		ocspPort = "80";
		ocspPath = "/ocsp03-wwdr01";
		string issuerCN = GetNameField(X509_get_subject_name(issuer), NID_commonName);
		if (issuerCN.find("G6") != string::npos) ocspPath = "/ocsp03-wwdrg6";
		else if (issuerCN.find("G3") != string::npos) ocspPath = "/ocsp03-wwdrg3";
		else if (issuerCN.find("G2") != string::npos) ocspPath = "/ocsp03-wwdrg2";
	}

	OCSP_CERTID* certId = OCSP_cert_to_id(EVP_sha1(), cert, issuer);
	if (!certId) { result.errorDetail = "Failed to create cert ID"; return result; }
	OCSP_REQUEST* req = OCSP_REQUEST_new();
	if (!req) { OCSP_CERTID_free(certId); result.errorDetail = "Request failed"; return result; }
	if (!OCSP_request_add0_id(req, certId)) { OCSP_REQUEST_free(req); result.errorDetail = "Add ID failed"; return result; }

	unsigned char* derReq = NULL;
	int derReqLen = i2d_OCSP_REQUEST(req, &derReq);
	if (derReqLen <= 0 || !derReq) { OCSP_REQUEST_free(req); result.errorDetail = "Serialize failed"; return result; }

	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(ocspHost.c_str(), ocspPort.c_str(), &hints, &res) != 0 || !res) {
		OPENSSL_free(derReq); OCSP_REQUEST_free(req);
		result.errorDetail = "DNS failed"; return result;
	}
#ifdef _WIN32
	SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock == INVALID_SOCKET) {
#else
	int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
#endif
		freeaddrinfo(res); OPENSSL_free(derReq); OCSP_REQUEST_free(req);
		result.errorDetail = "Socket failed"; return result;
	}
	if (connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0) {
		freeaddrinfo(res); OCSP_CLOSE_SOCKET(sock); OPENSSL_free(derReq); OCSP_REQUEST_free(req);
		result.errorDetail = "Connect failed"; return result;
	}
	freeaddrinfo(res);

	char hdr[512];
	snprintf(hdr, sizeof(hdr),
		"POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: zsign\r\nContent-Type: application/ocsp-request\r\nContent-Length: %d\r\n\r\n",
		ocspPath.c_str(), ocspHost.c_str(), derReqLen);
	string request(hdr);
	request.append((const char*)derReq, derReqLen);
	OPENSSL_free(derReq);
	OCSP_REQUEST_free(req);

	const char* sendPtr = request.data();
	size_t sendRemain = request.size();
	while (sendRemain > 0) {
		ssize_t sent = send(sock, sendPtr, (int)sendRemain, 0);
		if (sent <= 0) { OCSP_CLOSE_SOCKET(sock); result.errorDetail = "Send failed"; return result; }
		sendPtr += sent;
		sendRemain -= sent;
	}

	// read response: first read until headers are complete
	string resp;
	char rb[4096];
	while (resp.find("\r\n\r\n") == string::npos) {
		ssize_t br = recv(sock, rb, sizeof(rb), 0);
		if (br <= 0) break;
		resp.append(rb, br);
	}

	size_t he = resp.find("\r\n\r\n");
	if (he == string::npos) { OCSP_CLOSE_SOCKET(sock); result.errorDetail = "Invalid response"; return result; }

	// parse Content-Length and read remaining body
	long contentLength = 0;
	{
		size_t clPos = resp.find("Content-Length:");
		if (clPos == string::npos) clPos = resp.find("content-length:");
		if (clPos != string::npos) contentLength = atol(resp.c_str() + clPos + 15);
	}
	size_t bodyHave = resp.size() - he - 4;
	while (contentLength > 0 && bodyHave < (size_t)contentLength) {
		ssize_t br = recv(sock, rb, sizeof(rb), 0);
		if (br <= 0) break;
		resp.append(rb, br);
		bodyHave += br;
	}
	OCSP_CLOSE_SOCKET(sock);

	const unsigned char* bodyPtr = (const unsigned char*)resp.data() + he + 4;
	long bodyLen = (long)(resp.size() - he - 4);
	if (bodyLen <= 0) { result.errorDetail = "Empty body"; return result; }

	OCSP_RESPONSE* oresp = d2i_OCSP_RESPONSE(NULL, &bodyPtr, bodyLen);
	if (!oresp) { result.errorDetail = "Parse failed"; return result; }
	if (OCSP_response_status(oresp) != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		OCSP_RESPONSE_free(oresp); result.errorDetail = "OCSP error"; return result;
	}

	OCSP_BASICRESP* basic = OCSP_response_get1_basic(oresp);
	if (!basic) { OCSP_RESPONSE_free(oresp); result.errorDetail = "Parse error"; return result; }

	int cs = -1, reason = 0;
	ASN1_GENERALIZEDTIME *rt = NULL, *tu = NULL, *nu = NULL;
	OCSP_CERTID* lid = OCSP_cert_to_id(EVP_sha1(), cert, issuer);
	if (!lid) { OCSP_BASICRESP_free(basic); OCSP_RESPONSE_free(oresp); result.errorDetail = "Lookup failed"; return result; }

	if (OCSP_resp_find_status(basic, lid, &cs, &reason, &rt, &tu, &nu) != 1) {
		OCSP_CERTID_free(lid); OCSP_BASICRESP_free(basic); OCSP_RESPONSE_free(oresp);
		result.status = "Unknown"; result.errorDetail = "Not in response"; return result;
	}
	OCSP_CERTID_free(lid);

	switch (cs) {
	case V_OCSP_CERTSTATUS_GOOD: result.status = "Valid"; result.errorDetail = ""; break;
	case V_OCSP_CERTSTATUS_REVOKED:
		result.status = "Revoked";
		if (rt) { BIO* tb = BIO_new(BIO_s_mem()); if (tb) { ASN1_GENERALIZEDTIME_print(tb, rt); BUF_MEM* bp = NULL; BIO_get_mem_ptr(tb, &bp); if (bp) result.revokedTime.assign(bp->data, bp->length); BIO_free(tb); } }
		break;
	default: result.status = "Unknown"; break;
	}

	OCSP_BASICRESP_free(basic);
	OCSP_RESPONSE_free(oresp);
	return result;
}

// ─── display (simple text, matches zsign style) ──────────────────────

static void PrintCertInfo(X509* cert, const string& fileTypeStr, bool showSigned, bool isSigned)
{
	string cn = GetNameField(X509_get_subject_name(cert), NID_commonName);
	string org = GetNameField(X509_get_subject_name(cert), NID_organizationName);
	string ou = GetNameField(X509_get_subject_name(cert), NID_organizationalUnitName);
	string issuerCN = GetNameField(X509_get_issuer_name(cert), NID_commonName);
	string serial = SerialToHex(cert);
	string keyAlgo = GetKeyAlgorithm(cert);
	string certType = DetectCertType(cn);
	string issuedStr = TimeToISO(X509_get0_notBefore(cert));
	string expiresStr = TimeToISO(X509_get0_notAfter(cert));
	int daysLeft = DaysRemaining(X509_get0_notAfter(cert));

	if (showSigned) {
		ZLog::PrintV(">>> Signed:\t%s\n", isSigned ? "Yes" : "No");
	}
	ZLog::PrintV(">>> Name:\t%s\n", cn.c_str());
	ZLog::PrintV(">>> Type:\t%s\n", certType.c_str());
	if (!org.empty()) ZLog::PrintV(">>> Org:\t%s\n", org.c_str());
	if (!ou.empty()) ZLog::PrintV(">>> Team:\t%s\n", ou.c_str());
	ZLog::PrintV(">>> Serial:\t%s\n", serial.c_str());
	ZLog::PrintV(">>> Issued:\t%s\n", issuedStr.c_str());

	if (daysLeft < 0) {
		ZLog::ErrorV(">>> Expires:\t%s (EXPIRED %d days ago)\n", expiresStr.c_str(), -daysLeft);
	} else if (daysLeft < 30) {
		ZLog::WarnV(">>> Expires:\t%s (%d days remaining!)\n", expiresStr.c_str(), daysLeft);
	} else {
		ZLog::SuccessV(">>> Expires:\t%s (%d days remaining)\n", expiresStr.c_str(), daysLeft);
	}

	ZLog::PrintV(">>> Algorithm:\t%s\n", keyAlgo.c_str());
	ZLog::PrintV(">>> Issuer:\t%s\n", issuerCN.c_str());
}

static int PrintOCSPResult(const OCSPResult& result)
{
	if (result.status == "Valid") {
		ZLog::Print(">>> OCSP:\tValid (ocsp.apple.com)\n");
	} else if (result.status == "Revoked") {
		ZLog::Print(">>> OCSP:\tREVOKED\n");
		if (!result.revokedTime.empty())
			ZLog::PrintV(">>> Revoked:\t%s\n", result.revokedTime.c_str());
	} else if (result.status == "Unknown") {
		ZLog::Print(">>> OCSP:\tUnknown\n");
	} else {
		ZLog::Print(">>> OCSP:\tError\n");
	}

	if (!result.errorDetail.empty())
		ZLog::PrintV(">>> Detail:\t%s\n", result.errorDetail.c_str());

	if (result.status == "Valid") return 0;
	if (result.status == "Revoked") return 1;
	return -1;
}

// ─── main entry ─────────────────────────────────────────────────────

int CheckCertificate(const string& strFilePath, const string& strPassword)
{
	string data;
	if (!ZFile::ReadFile(strFilePath.c_str(), data) || data.empty()) {
		ZLog::ErrorV(">>> Cannot read file: %s\n", strFilePath.c_str());
		return -1;
	}

	CertFileType fileType = DetectFileType(strFilePath, data);
	X509* cert = NULL;
	STACK_OF(X509)* ca = NULL;
	string fileTypeStr;
	bool isSigned = false, showSigned = false;

	switch (fileType) {
	case CERT_FILE_IPA: {
		fileTypeStr = "IPA";
		showSigned = true;
		MachOSignInfo si = LoadFromIPA(strFilePath);
		isSigned = si.isSigned; cert = si.cert;
		break;
	}
	case CERT_FILE_MACHO: {
		fileTypeStr = "Mach-O";
		showSigned = true;
		MachOSignInfo si = ExtractFromMachOData((uint8_t*)data.data(), (uint32_t)data.size());
		isSigned = si.isSigned; cert = si.cert;
		break;
	}
	case CERT_FILE_PROVISION:
		fileTypeStr = "Provision"; cert = LoadFromProvision(data); break;
	case CERT_FILE_P12:
		fileTypeStr = "PKCS#12"; cert = LoadFromP12(data, strPassword, &ca); break;
	case CERT_FILE_CER:
	case CERT_FILE_PEM:
		fileTypeStr = (fileType == CERT_FILE_PEM) ? "PEM" : "DER";
		cert = LoadFromCER(data); break;
	default:
		ZLog::ErrorV(">>> Unknown file type: %s\n", strFilePath.c_str());
		return -1;
	}

	ZLog::PrintV("\n>>> Check:\t%s (%s)\n", strFilePath.c_str(), fileTypeStr.c_str());

	if (showSigned && !isSigned) {
		ZLog::Print(">>> Signed:\tNo\n\n");
		return -2;
	}

	if (!cert) {
		if (showSigned) {
			ZLog::Print(">>> Signed:\tNo\n\n");
			return -2;
		}
		ZLog::ErrorV(">>> Failed to load certificate from %s\n", strFilePath.c_str());
		return -1;
	}

	PrintCertInfo(cert, fileTypeStr, showSigned, isSigned);

	int daysLeft = DaysRemaining(X509_get0_notAfter(cert));
	bool expired = (daysLeft < 0);

	X509* issuer = NULL;
	if (ca && sk_X509_num(ca) > 0) { issuer = sk_X509_value(ca, 0); X509_up_ref(issuer); }
	if (!issuer) issuer = ResolveIssuer(cert);

	int retCode = 0;
	if (!issuer) {
		ZLog::Print(">>> OCSP:\tSkipped (non-WWDR issuer)\n");
		retCode = expired ? 2 : 0;
	} else {
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
		OCSPResult ocspResult = PerformOCSP(cert, issuer);
#ifdef _WIN32
		WSACleanup();
#endif
		retCode = PrintOCSPResult(ocspResult);
		if (expired && retCode == 0) retCode = 2;
		X509_free(issuer);
	}

	ZLog::Print("\n");

	X509_free(cert);
	if (ca) sk_X509_pop_free(ca, X509_free);
	return retCode;
}

// ─── Post-sign binary check ─────────────────────────────────────────

int CheckSignedBinary(const string& strAppFolder)
{
	string strInfoPath = strAppFolder;
	if (strInfoPath.back() != '/') strInfoPath += '/';
	strInfoPath += "Info.plist";

	string strInfoPlist;
	if (!ZFile::ReadFile(strInfoPath.c_str(), strInfoPlist) || strInfoPlist.empty()) return -1;

	jvalue jvInfo;
	if (!jvInfo.read_plist(strInfoPlist)) return -1;

	string strExecName = jvInfo["CFBundleExecutable"].as_cstr();
	if (strExecName.empty()) return -1;

	string strBinaryPath = strAppFolder;
	if (strBinaryPath.back() != '/') strBinaryPath += '/';
	strBinaryPath += strExecName;

	return CheckCertificate(strBinaryPath, "");
}
