#pragma once
#include "common/common.h"
#include "common/json.h"
#include "openssl.h"
#include <vector>

class ZAppBundle {
public:
  ZAppBundle();

public:
  bool SignFolder(ZSignAsset *pSignAsset, const string &strFolder,
                  const string &strBundleID, const string &strBundleVersion,
                  const string &strDisplayName,
                  const vector<string> &arrDyLibFiles, bool bForce,
                  bool bWeakInject, bool bEnableCache);

private:
  bool SignNode(JValue &jvNode);
  void GetNodeChangedFiles(JValue &jvNode);
  void GetChangedFiles(JValue &jvNode, vector<string> &arrChangedFiles);
  void GetPlugIns(const string &strFolder, vector<string> &arrPlugIns);

private:
  bool FindAppFolder(const string &strFolder, string &strAppFolder);
  bool GetObjectsToSign(const string &strFolder, JValue &jvInfo);
  bool GetSignFolderInfo(const string &strFolder, JValue &jvNode,
                         bool bGetName = false);

private:
  bool GenerateCodeResources(const string &strFolder, JValue &jvCodeRes);
  void GetFolderFiles(const string &strFolder, const string &strBaseFolder,
                      set<string> &setFiles);

private:
  bool m_bForceSign;
  bool m_bWeakInject;
  vector<string> arrDyLibPaths;
  ZSignAsset *m_pSignAsset;

public:
  string m_strAppFolder;
};
