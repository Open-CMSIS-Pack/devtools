/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdModel_H
#define SvdModel_H

#include "SvdItem.h"
#include <string>
#include <list>


class SvdDevice;
class XMLTreeElement;

class SvdModel : public SvdItem {
public:
  SvdModel(SvdItem* parent);
  virtual ~SvdModel();

  bool          Construct   (XMLTreeElement* xmlTree);
  bool          CalculateModel   ();
  virtual bool  Validate();
  virtual bool  CopyItem(SvdItem *from) { return 0; }
  virtual bool  CheckItem();

  const std::string&  GetInputFileName    ()                                  { return m_inputFileName; }
  bool                SetInputFileName    (const std::string& inputFileName)  { m_inputFileName = inputFileName;  return true; }
  bool                SetShowMissingEnums ()                                  { m_showMissingEnums = true;        return true; }
  bool                GetShowMissingEnums ()                                  { return m_showMissingEnums; }
  SvdDevice*          GetDevice           () const                            { return m_device; }

protected:

private:
  SvdDevice       *m_device;
  bool             m_showMissingEnums;
  std::string      m_inputFileName;
};


class SvdModelCalculate : public SvdVisitor
{
public:
  SvdModelCalculate() { };
  ~SvdModelCalculate() {};
  virtual VISIT_RESULT Visit(SvdItem* item);
  
private:
};

#endif // SvdModel_H
