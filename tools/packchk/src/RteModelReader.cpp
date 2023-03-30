/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteModelReader.h"

#include "CrossPlatformUtils.h"
#include "RteUtils.h"
#include "XMLTreeSlim.h"
#include "ErrLog.h"

using namespace std;

/**
 * @brief visitor pattern
 * @param rteItem RteItem to run on
 * @return VISIT_RESULT
*/
VISIT_RESULT RteModelReaderErrorVistior::Visit(RteItem* rteItem)
{
  if(rteItem->IsValid()) {
    return VISIT_RESULT::SKIP_CHILDREN;
  }

  const list<string>& errors = rteItem->GetErrors();
  if(errors.empty()) {
    return VISIT_RESULT::CONTINUE_VISIT;
  }

  for(auto msg : errors) {
    if(msg.find("error #") != string::npos) {
      LogMsg("M505", MSG(msg));
    }
    else if(msg.find("warning #") != string::npos) {
      LogMsg("M506", MSG(msg));
    }
    else {
      LogMsg("M500", MSG(msg));
    }
  }

  return VISIT_RESULT::CONTINUE_VISIT;
}

/**
 * @brief check paths
 * @param fileName string filename to check
 * @param lineNo line number for error reporting
 * @return passed / failed
*/
bool ValueAdjuster::CheckPath(const string& fileName, int lineNo)
{
  if(IsAbsolute(fileName)) {
    LogMsg("M326", PATH(fileName), lineNo);   // error : absolute paths are not permitted
  }
  else if(fileName.find('\\') != string::npos) {
    if(IsURL(fileName)) {
      LogMsg("M370", URL(fileName), lineNo);  // error : backslash are non permitted in URL
    }
    else {
      LogMsg("M327", PATH(fileName), lineNo); // error : backslash are not recommended
    }
  }

  return true;
}

/**
 * @brief hook into XmlReader to pre-check paths
 * @param fileName fileName string filename to check
 * @param lineNo line number for error reporting
 * @return passed / failed
 */
string ValueAdjuster::AdjustPath(const string& fileName, int lineNo)
{
  CheckPath(fileName, lineNo);
  return RteValueAdjuster::AdjustPath(fileName, lineNo);
}

/**
 * @brief class constructor
 * @param rteModel Rte Model to run on
*/
RteModelReader::RteModelReader(RteGlobalModel& rteModel) :
  m_rteModel(rteModel),
  m_rteItemBuilder(&rteModel),
  m_xmlTree(&m_rteItemBuilder)
{
  m_xmlTree.SetXmlValueAdjuster(&m_valueAdjuster);
  m_xmlTree.Init();
}

/**
 * @brief class destructor
*/
RteModelReader::~RteModelReader()
{
}

/**
 * @brief adds a file to XmlReader
 * @param fileName
 * @return
*/
bool RteModelReader::AddFile(const string& fileName)
{
  if(fileName.empty()) {
    return false;
  }

  return m_xmlTree.AddFileName(fileName);
}

/**
 * @brief read all xml files, construct & validate model
 * @return passed / failed
*/
bool RteModelReader::ReadAll()
{
  // ----------------------  Read XML  ----------------------
  uint32_t t1 = CrossPlatformUtils::ClockInMsec();
  bool bOk = m_xmlTree.ParseAll();
  uint32_t t2 = CrossPlatformUtils::ClockInMsec() - t1;
  LogMsg("M075", TIME(t2));

  if(!bOk) {
    LogMsg("M108");
    return false;
  }

  // ----------------------  Construct Model  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
  m_rteModel.InsertPacks(m_rteItemBuilder.GetPacks());
  t2 = CrossPlatformUtils::ClockInMsec() - t1;
  LogMsg("M076", TIME(t2));

  if(!bOk) {
    LogMsg("M109");
  }

  // ----------------------  Validate Model  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
  m_rteModel.ClearErrors();
  bOk = m_rteModel.Validate();
  t2 = CrossPlatformUtils::ClockInMsec() - t1;
  LogMsg("M077", TIME(t2));

  if(!bOk) {
    LogMsg("M110");
  }

  RteModelReaderErrorVistior visitor;
  m_rteModel.AcceptVisitor(&visitor);

  return true;
}
