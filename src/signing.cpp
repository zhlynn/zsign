#include "common.h"
#include "json.h"
#include "mach-o.h"
#include "openssl.h"
#include "signing.h"
#include <algorithm>
#include <openssl/sha.h>

void ZSign::_DERLength(string& strBlob, uint64_t uLength)
{
	if (uLength < 128) {
		strBlob.append(1, (char)uLength);
	} else {
		uint32_t sLength = (64 - ZUtil::builtin_clzll(uLength) + 7) / 8;
		strBlob.append(1, (char)(0x80 | sLength));
		sLength *= 8;
		do {
			strBlob.append(1, (char)(uLength >> (sLength -= 8)));
		} while (sLength != 0);
	}
}

string ZSign::_DER(const jvalue& data)
{
	string strOutput;
	if (data.is_bool()) {
		strOutput.append(1, 0x01);
		strOutput.append(1, 1);
		strOutput.append(1, data.as_bool() ? (char)0xff : (char)0x00);
	} else if (data.is_int()) {
		uint64_t uVal = data.as_int64();
		strOutput.append(1, 0x02);
		_DERLength(strOutput, uVal);

		uint32_t sLength = (64 - ZUtil::builtin_clzll(uVal) + 7) / 8;
		sLength *= 8;
		do {
			strOutput.append(1, (char)(uVal >> (sLength -= 8)));
		} while (sLength != 0);
	} else if (data.is_string()) {
		string strVal = data.as_cstr();
		strOutput.append(1, 0x0c);
		_DERLength(strOutput, strVal.size());
		strOutput += strVal;
	} else if (data.is_array()) {
		string strArray;
		size_t size = data.size();
		for (size_t i = 0; i < size; i++) {
			strArray += _DER(data[i]);
		}
		strOutput.append(1, 0x30);
		_DERLength(strOutput, strArray.size());
		strOutput += strArray;
	} else if (data.is_object()) {
		vector<string> arrKeys;
		data.get_keys(arrKeys);
		std::sort(arrKeys.begin(), arrKeys.end());

		string strDict;
		for (size_t i = 0; i < arrKeys.size(); i++) {
			string& strKey = arrKeys[i];
			string strVal = _DER(data[strKey]);

			string strEntry;
			strEntry.append(1, 0x0c);
			_DERLength(strEntry, strKey.size());
			strEntry += strKey;
			strEntry += strVal;

			strDict.append(1, 0x30);
			_DERLength(strDict, strEntry.size());
			strDict += strEntry;
		}

		strOutput.append(1, (char)0xb0);
		_DERLength(strOutput, strDict.size());
		strOutput += strDict;
	} else if (data.is_double()) {
		assert(false);
	} else if (data.is_date()) {
		assert(false);
	} else if (data.is_data()) {
		assert(false);
	} else {
		assert(false && "Unsupported Entitlements DER Type");
	}

	return strOutput;
}

uint32_t ZSign::SlotParseGeneralHeader(const char* szSlotName, uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = LE(*(((uint32_t*)pSlotBase) + 1));
	ZLog::PrintV("\n  > %s: \n", szSlotName);
	ZLog::PrintV("\ttype: \t\t0x%x\n", LE(pbi->type));
	ZLog::PrintV("\toffset: \t%u\n", LE(pbi->offset));
	ZLog::PrintV("\tmagic: \t\t0x%x\n", LE(*((uint32_t*)pSlotBase)));
	ZLog::PrintV("\tlength: \t%u\n", uSlotLength);
	return uSlotLength;
}

void ZSign::SlotParseGeneralTailer(uint8_t* pSlotBase, uint32_t uSlotLength)
{
	ZSHA::PrintData1("\tSHA-1:  \t", pSlotBase, uSlotLength);
	ZSHA::PrintData256("\tSHA-256:\t", pSlotBase, uSlotLength);
}

bool ZSign::SlotParseRequirements(uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = SlotParseGeneralHeader("CSSLOT_REQUIREMENTS", pSlotBase, pbi);
	if (uSlotLength < 8) {
		return false;
	}

#ifndef _WIN32
	if (ZFile::IsFileExists("/usr/bin/csreq")) {
		string strTempFile;
		ZUtil::StringFormatV(strTempFile, "/tmp/Requirements_%llu.blob", ZUtil::GetMicroSecond());
		ZFile::WriteFile(strTempFile.c_str(), (const char*)pSlotBase, uSlotLength);

		string strCommand;
		ZUtil::StringFormatV(strCommand, "/usr/bin/csreq -r '%s' -t ", strTempFile.c_str());
		char result[1024] = { 0 };
		FILE* cmd = popen(strCommand.c_str(), "r");
		while (NULL != fgets(result, sizeof(result), cmd)) {
			printf("\treqtext: \t%s", result);
		}
		pclose(cmd);
		ZFile::RemoveFile(strTempFile.c_str());
	}
#endif

	SlotParseGeneralTailer(pSlotBase, uSlotLength);

	if (ZLog::IsDebug()) {
		ZFile::WriteFile("./.zsign_debug/Requirements.slot", (const char*)pSlotBase, uSlotLength);
	}
	return true;
}

bool ZSign::SlotBuildRequirements(const string& strBundleID, const string& strSubjectCN, string& strOutput)
{
	strOutput.clear();

	// Empty requirement set (ldid-style: magic + length=12 + count=0)
	if (strBundleID.empty() || strSubjectCN.empty()) {
		uint32_t magic = BE((uint32_t)CSMAGIC_REQUIREMENTS);
		uint32_t length = BE((uint32_t)12);
		uint32_t count = 0;
		strOutput.append((const char*)&magic, 4);
		strOutput.append((const char*)&length, 4);
		strOutput.append((const char*)&count, 4);
		return true;
	}

	// Helper: append a uint32 in big-endian
	auto appendBE32 = [&strOutput](uint32_t val) {
		uint32_t be = BE(val);
		strOutput.append((const char*)&be, 4);
	};

	// Helper: append a padded string (uint32 BE length + data + zero-pad to 4-byte alignment)
	auto appendPaddedString = [&strOutput, &appendBE32](const string& str) {
		appendBE32((uint32_t)str.size());
		strOutput.append(str.data(), str.size());
		size_t pad = (4 - (str.size() % 4)) % 4;
		if (pad > 0) strOutput.append(pad, '\0');
	};

	// Apple WWDR intermediate marker OID: 1.2.840.113635.100.6.2.1
	static const uint8_t kAppleWWDROID[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x02, 0x01 };

	// ── Build the inner requirement expression blob (CSMAGIC_REQUIREMENT) ──
	// Generates the designated requirement equivalent to:
	//   identifier "<bundleID>" and anchor apple generic
	//     and certificate leaf[subject.CN] = "<subjectCN>"
	//     and certificate 1[field.1.2.840.113635.100.6.2.1] /* exists */
	string strExpr;
	auto appendExprBE32 = [&strExpr](uint32_t val) {
		uint32_t be = BE(val);
		strExpr.append((const char*)&be, 4);
	};

	// exprForm = 1 (expression follows)
	appendExprBE32(kReqOpTrue);

	// opAnd( opIdent(bundleID), opAnd( opAppleGenericAnchor, opAnd( opCertField(leaf, "subject.CN", matchEqual, subjectCN), opCertGeneric(1, wwdrOID, matchExists) ) ) )

	// opAnd #1
	appendExprBE32(kReqOpAnd);
	// opIdent + bundleID
	appendExprBE32(kReqOpIdent);
	{ // padded bundleID
		uint32_t len = BE((uint32_t)strBundleID.size());
		strExpr.append((const char*)&len, 4);
		strExpr.append(strBundleID.data(), strBundleID.size());
		size_t pad = (4 - (strBundleID.size() % 4)) % 4;
		if (pad > 0) strExpr.append(pad, '\0');
	}

	// opAnd #2
	appendExprBE32(kReqOpAnd);
	// opAppleGenericAnchor
	appendExprBE32(kReqOpAppleGenericAnchor);

	// opAnd #3
	appendExprBE32(kReqOpAnd);
	// opCertField: certificate leaf[subject.CN] = "<subjectCN>"
	appendExprBE32(kReqOpCertField);
	appendExprBE32(0); // slot 0 = leaf certificate
	{ // "subject.CN" padded
		const char kSubjectCN[] = "subject.CN";
		uint32_t len = BE((uint32_t)(sizeof(kSubjectCN) - 1));
		strExpr.append((const char*)&len, 4);
		strExpr.append(kSubjectCN, sizeof(kSubjectCN) - 1);
		size_t pad = (4 - ((sizeof(kSubjectCN) - 1) % 4)) % 4;
		if (pad > 0) strExpr.append(pad, '\0');
	}
	appendExprBE32(kReqMatchEqual);
	{ // padded subjectCN
		uint32_t len = BE((uint32_t)strSubjectCN.size());
		strExpr.append((const char*)&len, 4);
		strExpr.append(strSubjectCN.data(), strSubjectCN.size());
		size_t pad = (4 - (strSubjectCN.size() % 4)) % 4;
		if (pad > 0) strExpr.append(pad, '\0');
	}

	// opCertGeneric: certificate 1[field.1.2.840.113635.100.6.2.1] /* exists */
	appendExprBE32(kReqOpCertGeneric);
	appendExprBE32(1); // slot 1 = intermediate certificate
	{ // WWDR OID padded
		uint32_t len = BE((uint32_t)sizeof(kAppleWWDROID));
		strExpr.append((const char*)&len, 4);
		strExpr.append((const char*)kAppleWWDROID, sizeof(kAppleWWDROID));
		size_t pad = (4 - (sizeof(kAppleWWDROID) % 4)) % 4;
		if (pad > 0) strExpr.append(pad, '\0');
	}
	appendExprBE32(kReqMatchExists);

	// ── Build the outer requirements vector (CSMAGIC_REQUIREMENTS) ──
	// Outer header: magic(4) + length(4) + count(4) + BlobIndex[type(4) + offset(4)] = 20 bytes
	uint32_t reqBlobLen = 8 + (uint32_t)strExpr.size(); // inner magic + inner length + expr
	uint32_t outerHeaderLen = 20;
	uint32_t totalLen = outerHeaderLen + reqBlobLen;

	// Outer: CSMAGIC_REQUIREMENTS header
	appendBE32(CSMAGIC_REQUIREMENTS);
	appendBE32(totalLen);
	appendBE32(1); // count = 1
	appendBE32(kSecDesignatedRequirementType); // type = 3
	appendBE32(outerHeaderLen); // offset to inner blob

	// Inner: CSMAGIC_REQUIREMENT blob
	appendBE32(CSMAGIC_REQUIREMENT);
	appendBE32(reqBlobLen);
	strOutput.append(strExpr.data(), strExpr.size());

	return true;
}

bool ZSign::SlotParseEntitlements(uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = SlotParseGeneralHeader("CSSLOT_ENTITLEMENTS", pSlotBase, pbi);
	if (uSlotLength < 8) {
		return false;
	}

	string strEntitlements = "\t\t\t";
	strEntitlements.append((const char*)pSlotBase + 8, uSlotLength - 8);
	ZUtil::StringReplace(strEntitlements, "\n", "\n\t\t\t");
	ZLog::PrintV("\tentitlements: \n%s\n", strEntitlements.c_str());

	SlotParseGeneralTailer(pSlotBase, uSlotLength);

	if (ZLog::IsDebug()) {
		ZFile::WriteFile("./.zsign_debug/Entitlements.slot", (const char*)pSlotBase, uSlotLength);
		ZFile::WriteFile("./.zsign_debug/Entitlements.plist", (const char*)pSlotBase + 8, uSlotLength - 8);
	}
	return true;
}

bool ZSign::SlotParseDerEntitlements(uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = SlotParseGeneralHeader("CSSLOT_DER_ENTITLEMENTS", pSlotBase, pbi);
	if (uSlotLength < 8) {
		return false;
	}

	SlotParseGeneralTailer(pSlotBase, uSlotLength);

	if (ZLog::IsDebug()) {
		ZFile::WriteFile("./.zsign_debug/Entitlements.der.slot", (const char*)pSlotBase, uSlotLength);
	}
	return true;
}

bool ZSign::SlotBuildEntitlements(const string& strEntitlements, string& strOutput)
{
	strOutput.clear();
	if (strEntitlements.empty()) {
		return false;
	}

	uint32_t uMagic = BE((uint32_t)CSMAGIC_EMBEDDED_ENTITLEMENTS);
	uint32_t uLength = BE((uint32_t)strEntitlements.size() + 8);

	strOutput.append((const char*)&uMagic, sizeof(uMagic));
	strOutput.append((const char*)&uLength, sizeof(uLength));
	strOutput.append(strEntitlements.data(), strEntitlements.size());

	return true;
}

bool ZSign::SlotBuildDerEntitlements(const string& strEntitlements, string& strOutput)
{
	strOutput.clear();
	if (strEntitlements.empty()) {
		return false;
	}

	jvalue jvInfo;
	jvInfo.read_plist(strEntitlements);

	string strInnerDict = _DER(jvInfo);

	string strVersion;
	strVersion.append(1, 0x02);
	strVersion.append(1, 0x01);
	strVersion.append(1, 0x01);

	string strBody = strVersion + strInnerDict;

	string strRawEntitlementsData;
	strRawEntitlementsData.append(1, (char)0x70);
	_DERLength(strRawEntitlementsData, strBody.size());
	strRawEntitlementsData += strBody;

	uint32_t uMagic = BE((uint32_t)CSMAGIC_EMBEDDED_DER_ENTITLEMENTS);
	uint32_t uLength = BE((uint32_t)strRawEntitlementsData.size() + 8);

	strOutput.append((const char*)&uMagic, sizeof(uMagic));
	strOutput.append((const char*)&uLength, sizeof(uLength));
	strOutput.append(strRawEntitlementsData.data(), strRawEntitlementsData.size());

	return true;
}

bool ZSign::SlotParseCodeDirectory(uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = SlotParseGeneralHeader("CSSLOT_CODEDIRECTORY", pSlotBase, pbi);
	if (uSlotLength < 8) {
		return false;
	}

	vector<uint8_t*> arrCodeSlots;
	vector<uint8_t*> arrSpecialSlots;
	CS_CodeDirectory cdHeader = *((CS_CodeDirectory*)pSlotBase);
	uint8_t* pHashes = pSlotBase + LE(cdHeader.hashOffset);
	for (uint32_t i = 0; i < LE(cdHeader.nCodeSlots); i++) {
		arrCodeSlots.push_back(pHashes + cdHeader.hashSize * i);
	}
	for (uint32_t i = 0; i < LE(cdHeader.nSpecialSlots); i++) {
		arrSpecialSlots.push_back(pHashes - cdHeader.hashSize * (i + 1));
	}

	ZLog::PrintV("\tversion: \t0x%x\n", LE(cdHeader.version));
	ZLog::PrintV("\tflags: \t\t%u\n", LE(cdHeader.flags));
	ZLog::PrintV("\thashOffset: \t%u\n", LE(cdHeader.hashOffset));
	ZLog::PrintV("\tidentOffset: \t%u\n", LE(cdHeader.identOffset));
	ZLog::PrintV("\tnSpecialSlots: \t%u\n", LE(cdHeader.nSpecialSlots));
	ZLog::PrintV("\tnCodeSlots: \t%u\n", LE(cdHeader.nCodeSlots));
	ZLog::PrintV("\tcodeLimit: \t%u\n", LE(cdHeader.codeLimit));
	ZLog::PrintV("\thashSize: \t%u\n", cdHeader.hashSize);
	ZLog::PrintV("\thashType: \t%u\n", cdHeader.hashType);
	ZLog::PrintV("\tspare1: \t%u\n", cdHeader.spare1);
	ZLog::PrintV("\tpageSize: \t%u\n", cdHeader.pageSize);
	ZLog::PrintV("\tspare2: \t%u\n", LE(cdHeader.spare2));

	uint32_t uVersion = LE(cdHeader.version);
	if (uVersion >= 0x20100) {
		ZLog::PrintV("\tscatterOffset: \t%u\n", LE(cdHeader.scatterOffset));
	}
	if (uVersion >= 0x20200) {
		ZLog::PrintV("\tteamOffset: \t%u\n", LE(cdHeader.teamOffset));
	}
	if (uVersion >= 0x20300) {
		ZLog::PrintV("\tspare3: \t%u\n", LE(cdHeader.spare3));
		ZLog::PrintV("\tcodeLimit64: \t%llu\n", LE(cdHeader.codeLimit64));
	}
	if (uVersion >= 0x20400) {
		ZLog::PrintV("\texecSegBase: \t%llu\n", LE(cdHeader.execSegBase));
		ZLog::PrintV("\texecSegLimit: \t%llu\n", LE(cdHeader.execSegLimit));
		ZLog::PrintV("\texecSegFlags: \t%llu\n", LE(cdHeader.execSegFlags));
	}

	ZLog::PrintV("\tidentifier: \t%s\n", pSlotBase + LE(cdHeader.identOffset));
	if (uVersion >= 0x20200) {
		ZLog::PrintV("\tteamid: \t%s\n", pSlotBase + LE(cdHeader.teamOffset));
	}

	ZLog::PrintV("\tSpecialSlots:\n");
	for (int i = LE(cdHeader.nSpecialSlots) - 1; i >= 0; i--) {
		const char* suffix = "\t\n";
		switch (i) {
		case 0:
			suffix = "\tInfo.plist\n";
			break;
		case 1:
			suffix = "\tRequirements Slot\n";
			break;
		case 2:
			suffix = "\tCodeResources\n";
			break;
		case 3:
			suffix = "\tApplication Specific\n";
			break;
		case 4:
			suffix = "\tEntitlements Slot\n";
			break;
		case 6:
			suffix = "\tEntitlements(DER) Slot\n";
			break;
		}
		ZSHA::Print("\t\t\t", arrSpecialSlots[i], cdHeader.hashSize, suffix);
	}

	if (ZLog::IsDebug()) {
		ZLog::Print("\tCodeSlots:\n");
		for (uint32_t i = 0; i < LE(cdHeader.nCodeSlots); i++) {
			ZSHA::Print("\t\t\t", arrCodeSlots[i], cdHeader.hashSize);
		}
	} else {
		ZLog::Print("\tCodeSlots: \tomitted. (use -d option for details)\n");
	}

	SlotParseGeneralTailer(pSlotBase, uSlotLength);

	if (ZLog::IsDebug()) {
		if (1 == cdHeader.hashType) {
			ZFile::WriteFile("./.zsign_debug/CodeDirectory_SHA1.slot", (const char*)pSlotBase, uSlotLength);
		} else if (2 == cdHeader.hashType) {
			ZFile::WriteFile("./.zsign_debug/CodeDirectory_SHA256.slot", (const char*)pSlotBase, uSlotLength);
		}
	}

	return true;
}

bool ZSign::SlotBuildCodeDirectory(bool bAlternate,
	uint8_t* pCodeBase,
	uint32_t uCodeLength,
	uint8_t* pCodeSlotsData,
	uint32_t uCodeSlotsDataLength,
	uint64_t execSegLimit,
	uint64_t execSegFlags,
	const string& strBundleId,
	const string& strTeamId,
	const string& strInfoPlistSHA,
	const string& strRequirementsSlotSHA,
	const string& strCodeResourcesSHA,
	const string& strEntitlementsSlotSHA,
	const string& strDerEntitlementsSlotSHA,
	bool isExecuteArch,
	bool isAdhoc,
	string& strOutput)
{
	strOutput.clear();
	if (NULL == pCodeBase || uCodeLength <= 0 || strBundleId.empty() || (strTeamId.empty() && !isAdhoc)) {
		return false;
	}

	uint32_t uVersion = 0x20400;

	CS_CodeDirectory cdHeader;
	memset(&cdHeader, 0, sizeof(cdHeader));
	cdHeader.magic = BE((uint32_t)CSMAGIC_CODEDIRECTORY);
	cdHeader.length = 0;
	cdHeader.version = BE(uVersion);
	cdHeader.flags = isAdhoc ? BE(static_cast<uint32_t>(CS_SEC_CODESIGNATURE_ADHOC)) : 0U;
	cdHeader.hashOffset = 0;
	cdHeader.identOffset = 0;
	cdHeader.nSpecialSlots = 0;
	cdHeader.nCodeSlots = 0;
	cdHeader.codeLimit = BE(uCodeLength);
	cdHeader.hashSize = bAlternate ? 32 : 20;
	cdHeader.hashType = bAlternate ? 2 : 1;
	cdHeader.spare1 = 0;
	cdHeader.pageSize = 12;
	cdHeader.spare2 = 0;
	cdHeader.scatterOffset = 0;
	cdHeader.teamOffset = 0;
	cdHeader.execSegBase = 0;
	cdHeader.execSegLimit = BE(execSegLimit);
	cdHeader.execSegFlags = BE(execSegFlags);

	string strEmptySHA;
	strEmptySHA.append(cdHeader.hashSize, 0);
	vector<string> arrSpecialSlots;

	if (isExecuteArch) {
		arrSpecialSlots.push_back(strDerEntitlementsSlotSHA.empty() ? strEmptySHA : strDerEntitlementsSlotSHA);
		arrSpecialSlots.push_back(strEmptySHA);
	}
	arrSpecialSlots.push_back(strEntitlementsSlotSHA.empty() ? strEmptySHA : strEntitlementsSlotSHA);
	arrSpecialSlots.push_back(strEmptySHA);
	arrSpecialSlots.push_back(strCodeResourcesSHA.empty() ? strEmptySHA : strCodeResourcesSHA);
	arrSpecialSlots.push_back(strRequirementsSlotSHA.empty() ? strEmptySHA : strRequirementsSlotSHA);
	arrSpecialSlots.push_back(strInfoPlistSHA.empty() ? strEmptySHA : strInfoPlistSHA);

	// Trailing entries whose hash == strEmptySHA in `arrSpecialSlots` can be omitted; erase them.
	// Special slots have negative indexes and come before code slots, i.e. index -1 is the 'Info.plist'
	// slot, and -2 is 'Requirements slot'.
	// Note that in `arrSpecialSlots` is reversed and trailing elements appear at front.
	auto itLastUsedSpecialSlot = std::find_if(arrSpecialSlots.begin(), arrSpecialSlots.end(),
		[&](const string& strSHA) { return strSHA != strEmptySHA; });
	if (itLastUsedSpecialSlot != arrSpecialSlots.begin()) {
		arrSpecialSlots.erase(arrSpecialSlots.begin(), itLastUsedSpecialSlot);
	}

	uint32_t uPageSize = 1u << cdHeader.pageSize;
	uint32_t uPages = uCodeLength / uPageSize;
	uint32_t uRemain = uCodeLength % uPageSize;
	uint32_t uCodeSlots = uPages + (uRemain > 0 ? 1 : 0);

	uint32_t uHeaderLength = 44;
	if (uVersion >= 0x20100) {
		uHeaderLength += sizeof(cdHeader.scatterOffset);
	}
	if (uVersion >= 0x20200) {
		uHeaderLength += sizeof(cdHeader.teamOffset);
	}
	if (uVersion >= 0x20300) {
		uHeaderLength += sizeof(cdHeader.spare3);
		uHeaderLength += sizeof(cdHeader.codeLimit64);
	}
	if (uVersion >= 0x20400) {
		uHeaderLength += sizeof(cdHeader.execSegBase);
		uHeaderLength += sizeof(cdHeader.execSegLimit);
		uHeaderLength += sizeof(cdHeader.execSegFlags);
	}

	uint32_t uBundleIDLength = (uint32_t)strBundleId.size() + 1;
	uint32_t uTeamIDLength = (uint32_t)strTeamId.size() + 1;
	uint32_t uSpecialSlotsLength = (uint32_t)arrSpecialSlots.size() * cdHeader.hashSize;
	uint32_t uCodeSlotsLength = uCodeSlots * cdHeader.hashSize;

	uint32_t uSlotLength = uHeaderLength + uBundleIDLength + uSpecialSlotsLength + uCodeSlotsLength;
	strOutput.reserve(uSlotLength + uTeamIDLength); // pre-allocate to avoid reallocations
	if (uVersion >= 0x20100) {
		//todo
	}
	if (uVersion >= 0x20200 && !strTeamId.empty()) {
		uSlotLength += uTeamIDLength;
	}

	cdHeader.length = BE(uSlotLength);
	cdHeader.identOffset = BE(uHeaderLength);
	cdHeader.nSpecialSlots = BE((uint32_t)arrSpecialSlots.size());
	cdHeader.nCodeSlots = BE(uCodeSlots);

	uint32_t uHashOffset = uHeaderLength + uBundleIDLength + uSpecialSlotsLength;
	if (uVersion >= 0x20100) {
		//todo
	}
	// `strTeamId` may be empty for ad-hoc signature; in that case, `cdHeader.teamOffset == 0` and string
	// data is not serialized below.
	if (uVersion >= 0x20200 && !strTeamId.empty()) {
		uHashOffset += uTeamIDLength;
		cdHeader.teamOffset = BE(uHeaderLength + uBundleIDLength);
	}
	cdHeader.hashOffset = BE(uHashOffset);

	strOutput.append((const char*)&cdHeader, uHeaderLength);
	strOutput.append(strBundleId.data(), strBundleId.size() + 1);
	if (uVersion >= 0x20100) {
		//todo
	}
	if (uVersion >= 0x20200 && !strTeamId.empty()) {
		strOutput.append(strTeamId.data(), strTeamId.size() + 1);
	}

	for (uint32_t i = 0; i < LE(cdHeader.nSpecialSlots); i++) {
		strOutput.append(arrSpecialSlots[i].data(), arrSpecialSlots[i].size());
	}

	if (NULL != pCodeSlotsData && (uCodeSlotsDataLength == uCodeSlots * cdHeader.hashSize)) { //use exists
		strOutput.append((const char*)pCodeSlotsData, uCodeSlotsDataLength);
	} else {
		uint8_t hash[32]; // large enough for both SHA1 (20) and SHA256 (32)
		for (uint32_t i = 0; i < uPages; i++) {
			if (1 == cdHeader.hashType) {
				::SHA1(pCodeBase + uPageSize * i, uPageSize, hash);
				strOutput.append((const char*)hash, 20);
			} else {
				::SHA256(pCodeBase + uPageSize * i, uPageSize, hash);
				strOutput.append((const char*)hash, 32);
			}
		}
		if (uRemain > 0) {
			if (1 == cdHeader.hashType) {
				::SHA1(pCodeBase + uPageSize * uPages, uRemain, hash);
				strOutput.append((const char*)hash, 20);
			} else {
				::SHA256(pCodeBase + uPageSize * uPages, uRemain, hash);
				strOutput.append((const char*)hash, 32);
			}
		}
	}

	return true;
}

bool ZSign::SlotParseCMSSignature(uint8_t* pSlotBase, CS_BlobIndex* pbi)
{
	uint32_t uSlotLength = SlotParseGeneralHeader("CSSLOT_SIGNATURESLOT", pSlotBase, pbi);
	if (uSlotLength < 8) {
		return false;
	}

	jvalue jvInfo;
	ZSignAsset::GetCMSInfo(pSlotBase + 8, uSlotLength - 8, jvInfo);
	//ZLog::PrintV("%s\n", jvInfo.styleWrite().c_str());

	ZLog::Print("\tCertificates: \n");
	for (size_t i = 0; i < jvInfo["certs"].size(); i++) {
		ZLog::PrintV("\t\t\t%s\t<=\t%s\n", jvInfo["certs"][i]["Subject"]["CN"].as_cstr(), jvInfo["certs"][i]["Issuer"]["CN"].as_cstr());
	}

	ZLog::Print("\tSignedAttrs: \n");
	if (jvInfo["attrs"].has("ContentType")) {
		ZLog::PrintV("\t  ContentType: \t%s => %s\n", jvInfo["attrs"]["ContentType"]["obj"].as_cstr(), jvInfo["attrs"]["ContentType"]["data"].as_cstr());
	}

	if (jvInfo["attrs"].has("SigningTime")) {
		ZLog::PrintV("\t  SigningTime: \t%s => %s\n", jvInfo["attrs"]["SigningTime"]["obj"].as_cstr(), jvInfo["attrs"]["SigningTime"]["data"].as_cstr());
	}

	if (jvInfo["attrs"].has("MessageDigest")) {
		ZLog::PrintV("\t  MsgDigest: \t%s => %s\n", jvInfo["attrs"]["MessageDigest"]["obj"].as_cstr(), jvInfo["attrs"]["MessageDigest"]["data"].as_cstr());
	}

	if (jvInfo["attrs"].has("CDHashes")) {
		string strData = jvInfo["attrs"]["CDHashes"]["data"].as_cstr();
		ZUtil::StringReplace(strData, "\n", "\n\t\t\t\t");
		ZLog::PrintV("\t  CDHashes: \t%s => \n\t\t\t\t%s\n", jvInfo["attrs"]["CDHashes"]["obj"].as_cstr(), strData.c_str());
	}

	if (jvInfo["attrs"].has("CDHashes2")) {
		ZLog::PrintV("\t  CDHashes2: \t%s => \n", jvInfo["attrs"]["CDHashes2"]["obj"].as_cstr());
		for (size_t i = 0; i < jvInfo["attrs"]["CDHashes2"]["data"].size(); i++) {
			ZLog::PrintV("\t\t\t\t%s\n", jvInfo["attrs"]["CDHashes2"]["data"][i].as_cstr());
		}
	}

	for (size_t i = 0; i < jvInfo["attrs"]["unknown"].size(); i++) {
		jvalue& jvAttr = jvInfo["attrs"]["unknown"][i];
		ZLog::PrintV("\t  UnknownAttr: \t%s => %s, type: %d, count: %d\n", jvAttr["obj"].as_cstr(), jvAttr["name"].as_cstr(), jvAttr["type"].as_int(), jvAttr["count"].as_int());
	}
	ZLog::Print("\n");

	SlotParseGeneralTailer(pSlotBase, uSlotLength);

	if (ZLog::IsDebug()) {
		ZFile::WriteFile("./.zsign_debug/CMSSignature.slot", (const char*)pSlotBase, uSlotLength);
		ZFile::WriteFile("./.zsign_debug/CMSSignature.der", (const char*)pSlotBase + 8, uSlotLength - 8);
	}
	return true;
}

bool ZSign::SlotBuildCMSSignature(ZSignAsset* pSignAsset,
	const string& strCodeDirectorySlot,
	const string& strAltnateCodeDirectorySlot,
	string& strOutput)
{
	strOutput.clear();
	if (pSignAsset->m_bAdhoc) { // The empty CSSLOT_SIGNATURESLOT
		uint8_t ldid[] = { 0xfa, 0xde, 0x0b, 0x01, 0x00, 0x00, 0x00, 0x08 };
		strOutput.append((const char*)ldid, sizeof(ldid));
		return true;
	}

	// The CMS "cdhashes" plist (attr 1.2.840.113635.100.9.1) and the "CDHashes2"
	// attribute (1.2.840.113635.100.9.2) must only contain hashes of CodeDirectory
	// blobs that actually exist in the signature. When only one CodeDirectory is
	// serialized (SHA256-only mode), emitting hashes for a non-existent alternate
	// CD breaks Apple's code signature verification with errSecCSSignatureFailed
	// (`codesign --verify` reports "code or signature have been modified"):
	// verification computes hashes of the real CDs and finds the second entry
	// doesn't match any actual CD.
	//
	// Format rules derived from Apple codesign output:
	// - cdhashes plist: one entry per CD, value = first 20 bytes of the CD's
	//   "best" hash. For dual-hash builds (SHA1+SHA256) the primary CD uses SHA1
	//   directly (20 bytes) and the alternate uses SHA256 truncated to 20.
	//   For SHA256-only builds the single entry uses SHA256 truncated to 20.
	// - CDHashes2 attribute: full hash (32 bytes for SHA256) of each CD wrapped
	//   in `SEQUENCE { OID sha256, OCTET STRING hash }`. The current
	//   GenerateCMS() implementation consumes a single `strAltnateCodeDirectorySlot256`
	//   string for this attribute — when there is no alternate CD we pass the
	//   SHA256 of the primary CD instead of SHA256(empty).
	const bool bHasAlternate = !strAltnateCodeDirectorySlot.empty();

	jvalue jvHashes;
	string strCDHashesPlist;
	string strCodeDirectorySlotSHA1;   // SHA1 of primary CD (used by CMS detached content & dual-hash plist[0])
	string strPrimaryCD_SHA256;        // SHA256 of primary CD (used in SHA256-only mode)
	string strAltnateCD_SHA256;        // SHA256 of alternate CD (dual-hash mode only)
	ZSHA::SHA1(strCodeDirectorySlot, strCodeDirectorySlotSHA1);
	ZSHA::SHA256(strCodeDirectorySlot, strPrimaryCD_SHA256);
	if (bHasAlternate) {
		ZSHA::SHA256(strAltnateCodeDirectorySlot, strAltnateCD_SHA256);
	}

	// 20-byte (truncated) hashes for the CDHashes plist.
	const size_t kPlistHashLen = 20;
	if (bHasAlternate) {
		jvHashes["cdhashes"][0].assign_data(strCodeDirectorySlotSHA1.data(), kPlistHashLen);
		jvHashes["cdhashes"][1].assign_data(strAltnateCD_SHA256.data(), kPlistHashLen);
	} else {
		// SHA256-only: single CD, use its SHA256 truncated to 20 bytes.
		jvHashes["cdhashes"][0].assign_data(strPrimaryCD_SHA256.data(), kPlistHashLen);
	}
	jvHashes.style_write_plist(strCDHashesPlist);

	// Full SHA256 hash to embed in the CDHashes2 signed attribute. In SHA256-only
	// mode this must be the SHA256 of the primary CD, not SHA256 of an empty
	// alternate — the latter is rejected by Apple's verifier.
	const string& strCDHashes2 = bHasAlternate ? strAltnateCD_SHA256 : strPrimaryCD_SHA256;

	string strCMSData;
	if (!pSignAsset->GenerateCMS(strCodeDirectorySlot, strCDHashesPlist, strCodeDirectorySlotSHA1, strCDHashes2, strCMSData)) {
		return false;
	}

	uint32_t uMagic = BE((uint32_t)CSMAGIC_BLOBWRAPPER);
	uint32_t uLength = BE((uint32_t)strCMSData.size() + 8);

	strOutput.append((const char*)&uMagic, sizeof(uMagic));
	strOutput.append((const char*)&uLength, sizeof(uLength));
	strOutput.append(strCMSData.data(), strCMSData.size());
	return true;
}

uint32_t ZSign::GetCodeSignatureLength(uint8_t* pCSBase)
{
	CS_SuperBlob* psb = (CS_SuperBlob*)pCSBase;
	if (NULL != psb && CSMAGIC_EMBEDDED_SIGNATURE == LE(psb->magic)) {
		return LE(psb->length);
	}
	return 0;
}

bool ZSign::ParseCodeSignature(uint8_t* pCSBase)
{
	CS_SuperBlob* psb = (CS_SuperBlob*)pCSBase;
	if (NULL == psb || CSMAGIC_EMBEDDED_SIGNATURE != LE(psb->magic)) {
		return false;
	}

	ZLog::PrintV("\n>>> CodeSignature Segment: \n");
	ZLog::PrintV("\tmagic: \t\t0x%x\n", LE(psb->magic));
	ZLog::PrintV("\tlength: \t%d\n", LE(psb->length));
	ZLog::PrintV("\tslots: \t\t%d\n", LE(psb->count));

	CS_BlobIndex* pbi = (CS_BlobIndex*)(pCSBase + sizeof(CS_SuperBlob));
	for (uint32_t i = 0; i < LE(psb->count); i++, pbi++) {
		uint8_t* pSlotBase = pCSBase + LE(pbi->offset);
		switch (LE(pbi->type)) {
		case CSSLOT_CODEDIRECTORY:
			SlotParseCodeDirectory(pSlotBase, pbi);
			break;
		case CSSLOT_REQUIREMENTS:
			SlotParseRequirements(pSlotBase, pbi);
			break;
		case CSSLOT_ENTITLEMENTS:
			SlotParseEntitlements(pSlotBase, pbi);
			break;
		case CSSLOT_DER_ENTITLEMENTS:
			SlotParseDerEntitlements(pSlotBase, pbi);
			break;
		case CSSLOT_ALTERNATE_CODEDIRECTORIES:
			SlotParseCodeDirectory(pSlotBase, pbi);
			break;
		case CSSLOT_SIGNATURESLOT:
			SlotParseCMSSignature(pSlotBase, pbi);
			break;
		case CSSLOT_IDENTIFICATIONSLOT:
			SlotParseGeneralHeader("CSSLOT_IDENTIFICATIONSLOT", pSlotBase, pbi);
			break;
		case CSSLOT_TICKETSLOT:
			SlotParseGeneralHeader("CSSLOT_TICKETSLOT", pSlotBase, pbi);
			break;
		default:
			SlotParseGeneralTailer(pSlotBase, SlotParseGeneralHeader("CSSLOT_UNKNOWN", pSlotBase, pbi));
			break;
		}
	}

	if (ZLog::IsDebug()) {
		ZFile::WriteFile("./.zsign_debug/CodeSignature.blob", (const char*)pCSBase, LE(psb->length));
	}
	return true;
}

bool ZSign::SlotGetCodeSlotsData(uint8_t* pSlotBase, uint8_t*& pCodeSlots, uint32_t& uCodeSlotsLength)
{
	uint32_t uSlotLength = LE(*(((uint32_t*)pSlotBase) + 1));
	if (uSlotLength < 8) {
		return false;
	}
	CS_CodeDirectory cdHeader = *((CS_CodeDirectory*)pSlotBase);
	pCodeSlots = pSlotBase + LE(cdHeader.hashOffset);
	uCodeSlotsLength = LE(cdHeader.nCodeSlots) * cdHeader.hashSize;
	return true;
}

bool ZSign::GetCodeSignatureExistsCodeSlotsData(uint8_t* pCSBase,
	uint8_t*& pCodeSlots1Data,
	uint32_t& uCodeSlots1DataLength,
	uint8_t*& pCodeSlots256Data,
	uint32_t& uCodeSlots256DataLength)
{
	pCodeSlots1Data = NULL;
	pCodeSlots256Data = NULL;
	uCodeSlots1DataLength = 0;
	uCodeSlots256DataLength = 0;
	CS_SuperBlob* psb = (CS_SuperBlob*)pCSBase;
	if (NULL == psb || CSMAGIC_EMBEDDED_SIGNATURE != LE(psb->magic)) {
		return false;
	}

	CS_BlobIndex* pbi = (CS_BlobIndex*)(pCSBase + sizeof(CS_SuperBlob));
	for (uint32_t i = 0; i < LE(psb->count); i++, pbi++) {
		uint8_t* pSlotBase = pCSBase + LE(pbi->offset);
		switch (LE(pbi->type)) {
		case CSSLOT_CODEDIRECTORY:
		{
			CS_CodeDirectory cdHeader = *((CS_CodeDirectory*)pSlotBase);
			if (LE(cdHeader.length) > 8) {
				pCodeSlots1Data = pSlotBase + LE(cdHeader.hashOffset);
				uCodeSlots1DataLength = LE(cdHeader.nCodeSlots) * cdHeader.hashSize;
			}
		}
		break;
		case CSSLOT_ALTERNATE_CODEDIRECTORIES:
		{
			CS_CodeDirectory cdHeader = *((CS_CodeDirectory*)pSlotBase);
			if (LE(cdHeader.length) > 8) {
				pCodeSlots256Data = pSlotBase + LE(cdHeader.hashOffset);
				uCodeSlots256DataLength = LE(cdHeader.nCodeSlots) * cdHeader.hashSize;
			}
		}
		break;
		default:
			break;
		}
	}

	return ((NULL != pCodeSlots1Data) && (NULL != pCodeSlots256Data) && uCodeSlots1DataLength > 0 && uCodeSlots256DataLength > 0);
}
