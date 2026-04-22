//
//  zsign.mm
//  feather
//
//  Created by HAHALOSAH on 5/22/24.
//

#include "zsign.hpp"
#include "common.h"
#include "openssl.h"
#include "macho.h"
#include "bundle.h"
#include "openssl.h"
#include "timer.h"
#include "archive.h"

#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/asn1.h>

extern "C" {

bool CheckIfSigned(NSString *filePath) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		
		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}
		
		bool success = machO.CheckSignature();
		
		machO.Free();
		
		if (success) {
			gtimer.Print(">>> MachO is signed!");
			return true;
		} else {
			gtimer.Print(">>> MachO is not signed.");
			return false;
		}
	}
}

bool InjectDyLib(NSString *filePath, NSString *dylibPath, bool weakInject) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::string dylibPathStr = [dylibPath UTF8String];
		
		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}
		
		bool success = machO.InjectDylib(weakInject, dylibPathStr.c_str());
		
		machO.Free();
		
		if (success) {
			gtimer.Print(">>> Dylib injected successfully!");
			return true;
		} else {
			gtimer.Print(">>> Failed to inject dylib.");
			return false;
		}
	}
}

bool UninstallDylibs(NSString *filePath, NSArray<NSString *> *dylibPathsArray) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::set<std::string> dylibsToRemove;
		
		for (NSString *dylibPath in dylibPathsArray) {
			dylibsToRemove.insert([dylibPath UTF8String]);
		}
		
		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}
		
		machO.RemoveDylibs(dylibsToRemove);
		
		machO.Free();
		
		gtimer.Print(">>> Dylibs uninstalled successfully!");
		return true;
	}
}

NSArray<NSString *> *ListDylibs(NSString *filePath) {
	ZTimer gtimer;
	@autoreleasepool {
		NSMutableArray<NSString *> *dylibPathsArray = [NSMutableArray array];
		
		std::string filePathStr = [filePath UTF8String];
		
		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return nil;
		}
		
		std::vector<std::string> dylibPaths = machO.ListDylibs();
		
		if (!dylibPaths.empty()) {
			gtimer.Print(">>> List of dylibs in the Mach-O file:");
			for (const std::string &dylibPath : dylibPaths) {
				NSString *dylibPathStr = [NSString stringWithUTF8String:dylibPath.c_str()];
				[dylibPathsArray addObject:dylibPathStr];
			}
		} else {
			gtimer.Print(">>> No dylibs found in the Mach-O file.");
		}
		
		machO.Free();
		
		return [dylibPathsArray copy];
	}
}

bool ChangeDylibPath(NSString *filePath, NSString *oldPath, NSString *newPath) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::string oldPathStr = [oldPath UTF8String];
		std::string newPathStr = [newPath UTF8String];
		
		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}
		
		bool success = machO.ChangeDylibPath(oldPathStr.c_str(), newPathStr.c_str());
		
		machO.Free();
		
		if (success) {
			gtimer.Print(">>> Dylib path changed successfully!");
			return true;
		} else {
			gtimer.Print(">>> Failed to change dylib path.");
			return false;
		}
	}
}

int zsign(
	NSString *app,
	NSString *prov,
	NSString *key,
	NSString *pass,
	NSString *entitlement,
	NSString *bundleid,
	NSString *displayname,
	NSString *bundleversion,
	bool adhoc,
	bool excludeprovion,
	void(^completionHandler)(BOOL success, NSError *error)
) {
	ZTimer atimer;
	ZTimer gtimer;
	
	bool bForce = true;
	bool bWeakInject = false;
	bool bAdhoc = adhoc;
	bool bSHA256Only = false;
	
	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strDisplayName;
	string strEntitleFile;
	vector<string> arrDylibFiles;
	vector<string> arrDisDylibFiles;
	
	strPKeyFile = [key cStringUsingEncoding:NSUTF8StringEncoding];
	strProvFile = [prov cStringUsingEncoding:NSUTF8StringEncoding];
	strPassword = [pass cStringUsingEncoding:NSUTF8StringEncoding];
	strEntitleFile = [entitlement cStringUsingEncoding:NSUTF8StringEncoding];
	
	strBundleId = [bundleid cStringUsingEncoding:NSUTF8StringEncoding];
	strDisplayName = [displayname cStringUsingEncoding:NSUTF8StringEncoding];
	strBundleVersion = [bundleversion cStringUsingEncoding:NSUTF8StringEncoding];
	
	string strPath = [app cStringUsingEncoding:NSUTF8StringEncoding];
	if (!ZFile::IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		return -1;
	}
	
	ZSignAsset zsa;
	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, false)) {
		return -1;
	}
	
	bool bEnableCache = true;
	string strFolder = strPath;
	
	atimer.Reset();
	ZBundle bundle;
	bool bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, bForce, bWeakInject, bEnableCache, excludeprovion);
	ZLog::PrintV(">>> Signing:\t%s %s\n", strPath.c_str(), (bAdhoc ? " (Ad-hoc)" : ""));
	atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");
	
	completionHandler(bRet);
	
	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}


int checkCert(
	NSString *prov,
	NSString *key,
	NSString *pass,
	void(^completionHandler)(int status, NSDate* expirationDate, NSString *error)
) {
	ZTimer gtimer;
	
	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strEntitleFile;
		
	if (!key || !prov || !pass) {
		completionHandler(2, nil, @"One or more required paths or password is missing.");
		return -1;
	}
		
	strPKeyFile = [key cStringUsingEncoding:NSUTF8StringEncoding];
	strProvFile = [prov cStringUsingEncoding:NSUTF8StringEncoding];
	strPassword = [pass cStringUsingEncoding:NSUTF8StringEncoding];

	__block ZSignAsset zsa;
		
	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, false, false, false)) {
		completionHandler(2, nil, @"Unable to initialize certificate. Please check your password.");
		return -1;
	}
		
	X509* cert = (X509*)zsa.m_x509Cert;
	BIO *brother1;
	unsigned long issuerHash = X509_issuer_name_hash((X509*)cert);
	if (0x817d2f7a == issuerHash) {
		brother1 = BIO_new_mem_buf(ZSignAsset::s_szAppleDevCACert, (int)strlen(ZSignAsset::s_szAppleDevCACert));
	} else if (0x9b16b75c == issuerHash) {
		brother1 = BIO_new_mem_buf(ZSignAsset::s_szAppleDevCACertG3, (int)strlen(ZSignAsset::s_szAppleDevCACertG3));
	} else {
		completionHandler(2, nil, @"Unable to determine issuer of the certificate. It is signed by Apple Developer?");
		return -2;
	}
		
	if (!brother1)
	{
		completionHandler(2, nil, @"Unable to initialize issuer certificate.");
		return -3;
	}
		
	X509 *issuer = PEM_read_bio_X509(brother1, NULL, 0, NULL);
	
	if (!cert || !issuer) {
		completionHandler(2, nil, @"Error loading cert or issuer");
		return -4;
	}
	
	// Extract OCSP URL from cert
	STACK_OF(ACCESS_DESCRIPTION)* aia = (STACK_OF(ACCESS_DESCRIPTION)*)X509_get_ext_d2i((X509*)cert, NID_info_access, 0, 0);
	if (!aia) {
		completionHandler(2, nil, @"No AIA (OCSP) extension found in certificate");
		return -5;
	}
	
	ASN1_IA5STRING* uri = nullptr;
	for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(aia); i++) {
		ACCESS_DESCRIPTION* ad = sk_ACCESS_DESCRIPTION_value(aia, i);
		if (OBJ_obj2nid(ad->method) == NID_ad_OCSP &&
			ad->location->type == GEN_URI) {
			uri = ad->location->d.uniformResourceIdentifier;
			
			break;
		}
	}
	
	if (!uri) {
		completionHandler(2, nil, @"No OCSP URI found in certificate.");
		return -6;
	}
	
	OCSP_REQUEST* req = OCSP_REQUEST_new();
	OCSP_CERTID* cert_id = OCSP_cert_to_id(nullptr, (X509*)cert, issuer);
	OCSP_request_add0_id(req, cert_id);
	cert_id = OCSP_cert_to_id(nullptr, (X509*)cert, issuer);
	unsigned char* der = 0;
	int len = i2d_OCSP_REQUEST(req, &der);
	
	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:(const char *)uri->data]]];
	[request setHTTPMethod:@"POST"];
	[request setHTTPBody:[NSData dataWithBytes:der length:len]];
	[request setValue:@"application/ocsp-request" forHTTPHeaderField:@"Content-Type"];
	[request setValue:@"application/ocsp-response" forHTTPHeaderField:@"Accept"];
	
	OPENSSL_free(der);
	if (aia) {
		sk_ACCESS_DESCRIPTION_pop_free(aia, ACCESS_DESCRIPTION_free);
	}
	OCSP_REQUEST_free(req);
	X509_free(issuer);
	BIO_free(brother1);
	
	NSURLSession *session = [NSURLSession sharedSession];
	NSURLSessionDataTask *task = [session dataTaskWithRequest:request
											completionHandler:^(NSData * _Nullable data,
																NSURLResponse * _Nullable response,
																NSError * _Nullable error) {
		if (error) {
			completionHandler(2, nil, error.localizedDescription);
			return;
		}
		
		NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
		if (httpResponse.statusCode == 200 && data) {
			const void *respBytes = [data bytes];
			OCSP_RESPONSE *resp;
			d2i_OCSP_RESPONSE(&resp, (const unsigned char**)&respBytes, data.length);
			OCSP_BASICRESP *basic = OCSP_response_get1_basic(resp);
			ASN1_TIME *expirationDateAsn1 = X509_get_notAfter(cert);
			NSString *fullDateString = [NSString stringWithFormat:@"20%s", expirationDateAsn1->data];
			
			NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
			formatter.dateFormat = @"yyyyMMddHHmmss'Z'";
			formatter.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
			formatter.locale = NSLocale.currentLocale;
			NSDate *expirationDate = [formatter dateFromString:fullDateString];
			
			int status, reason;
			if (OCSP_resp_find_status(basic, cert_id, &status, &reason, NULL, NULL, NULL)) {
				completionHandler(status, expirationDate, nil);
			} else {
				completionHandler(2, expirationDate, nil);
			}
			
			OCSP_CERTID_free(cert_id);
			OCSP_BASICRESP_free(basic);
			OCSP_RESPONSE_free(resp);
			
			
		} else {
			completionHandler(2, nil, @"Invalid response or no data");
			return;
		}
	}];
	
	[task resume];
	return 1;
}



}
