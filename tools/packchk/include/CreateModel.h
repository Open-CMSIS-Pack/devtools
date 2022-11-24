/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CREATEMODEL_H
#define CREATEMODEL_H

#include "Validate.h"
#include "RteModelReader.h"
#include "PackChk.h"
#include <list>
#include <string>
#include <set>

#define PDSC_FEXT     _T(".pdsc")
#define PDSC_FEXT_LEN 5

class CreateModel
{
public:
  CreateModel(RteGlobalModel& rteModel);
  ~CreateModel();

  bool CheckForOtherPdscFiles(const std::string& pdscFullPath);
  bool AddPdsc(const std::string& pdscFile, bool bSkipCheckForOtherPdsc = false);
  bool AddRefPdsc(const std::set<std::string>& pdscRefFiles);
  bool SetPackXsd(const std::string& packXsdFile);
  bool ReadAllPdsc();
  bool PrintPdscFiles(std::list<std::string>& pdscFiles);

private:
  RteModelReader m_reader;
  std::string m_schemaFile;
  bool m_validatePdsc = false;

};

#endif // CREATEMODEL_H
