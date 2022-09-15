/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdCpu_H
#define SvdCpu_H

#include "SvdTypes.h"

#define MAXNUM_SAU_REGIONS   255



class SvdInterrupt;
class SvdSauRegionsConfig;

class SvdCpu : public SvdItem
{
public:
  SvdCpu(SvdItem* parent);
  virtual ~SvdCpu();

  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool CopyItem(SvdItem *from);
  virtual bool Calculate();
  virtual bool CheckItem();

  bool CreateInterruptList ();
  std::map<uint32_t, SvdInterrupt*>& GetInterruptList() { return m_interruptList; }

  const std::string&        GetRevisionStr          ()  { return m_revisionStr         ; }
  uint32_t                  GetRevision             ()  { return m_revision            ; }
  uint32_t                  GetNvicPrioBits         ()  { return m_nvicPrioBits        ; }
  bool                      GetMpuPresent           ()  { return m_mpuPresent          ; }
  bool                      GetFpuPresent           ()  { return m_fpuPresent          ; }
  bool                      GetVendorSystickConfig  ()  { return m_vendorSystickConfig ; }
  bool                      GetFpuDP                ()  { return m_fpuDP               ; }
  bool                      GetIcachePresent        ()  { return m_icachePresent       ; }
  bool                      GetDcachePresent        ()  { return m_dcachePresent       ; }
  bool                      GetItcmPresent          ()  { return m_itcmPresent         ; }
  bool                      GetDtcmPresent          ()  { return m_dtcmPresent         ; }
  bool                      GetVtorPresent          ()  { return m_vtorPresent         ; }
  bool                      GetDspPresent           ()  { return m_dspPresent          ; }
  bool                      GetPmuPresent           ()  { return m_pmuPresent          ; }
  bool                      GetMvePresent           ()  { return m_mvePresent          ; }
  bool                      GetMveFP                ()  { return m_mveFP               ; }
  SvdTypes::Endian          GetEndian               ()  { return m_endian              ; }
  SvdTypes::CpuType         GetType                 ()  { return m_type                ; }
  uint32_t                  GetSauNumRegions        ()  { return m_sauNumRegions       ; }
  SvdSauRegionsConfig*      GetSauRegionsConfig     ()  { return m_sauRegionsConfig    ; }
  uint32_t                  GetDeviceNumInterrupts  ()  { return m_deviceNumInterrupts ; }
  uint32_t                  GetPmuNumEventCounters  ()  { return m_pmuNumEventCnt      ; }
  SvdTypes::CmsisCfgForce&  GetCmsisCfgForce        ()  { return m_cmsisCfgForce       ; }


  bool SetRevisionStr          (const std::string&    revisionStr        )  { m_revisionStr         = revisionStr        ; return true; }
  bool SetRevision             (uint32_t              revision           )  { m_revision            = revision           ; return true; }
  bool SetNvicPrioBits         (uint32_t              nvicPrioBits       )  { m_nvicPrioBits        = nvicPrioBits       ; return true; }
  bool SetMpuPresent           (bool                  mpuPresent         )  { m_mpuPresent          = mpuPresent         ; return true; }
  bool SetFpuPresent           (bool                  fpuPresent         )  { m_fpuPresent          = fpuPresent         ; return true; }
  bool SetPmuPresent           (bool                  pmuPresent         )  { m_pmuPresent          = pmuPresent         ; return true; }
  bool SetMvePresent           (bool                  mvePresent         )  { m_mvePresent          = mvePresent         ; return true; }
  bool SetVendorSystickConfig  (bool                  vendorSystickConfig)  { m_vendorSystickConfig = vendorSystickConfig; return true; }
  bool SetFpuDP                (bool                  fpuDP              )  { m_fpuDP               = fpuDP              ; return true; }
  bool SetMveFP                (bool                  mveFP              )  { m_mveFP               = mveFP              ; return true; }
  bool SetIcachePresent        (bool                  icachePresent      )  { m_icachePresent       = icachePresent      ; return true; }
  bool SetDcachePresent        (bool                  dcachePresent      )  { m_dcachePresent       = dcachePresent      ; return true; }
  bool SetItcmPresent          (bool                  itcmPresent        )  { m_itcmPresent         = itcmPresent        ; return true; }
  bool SetDtcmPresent          (bool                  dtcmPresent        )  { m_dtcmPresent         = dtcmPresent        ; return true; }
  bool SetVtorPresent          (bool                  vtorPresent        )  { m_vtorPresent         = vtorPresent        ; return true; }
  bool SetDspPresent           (bool                  dspPresent         )  { m_dspPresent          = dspPresent         ; return true; }
  bool SetEndian               (SvdTypes::Endian      endian             )  { m_endian              = endian             ; return true; }
  bool SetType                 (SvdTypes::CpuType     type               )  { m_type                = type               ; return true; }
  bool SetSauNumRegions        (uint32_t              sauNumRegions      )  { m_sauNumRegions       = sauNumRegions      ; return true; }




protected:


private:
  SvdTypes::CpuType             m_type;
  uint32_t                      m_revision;
  SvdTypes::Endian              m_endian;
  bool                          m_bRevision;
  bool                          m_mpuPresent;
  bool                          m_fpuPresent;
  bool                          m_fpuDP;
  bool                          m_icachePresent;
  bool                          m_dcachePresent;
  bool                          m_itcmPresent;
  bool                          m_dtcmPresent;
  bool                          m_vtorPresent;
  bool                          m_dspPresent;
  bool                          m_pmuPresent;
  bool                          m_mvePresent;
  bool                          m_mveFP;
  bool                          m_vendorSystickConfig;
  uint32_t                      m_nvicPrioBits;
  uint32_t                      m_sauNumRegions;
  uint32_t                      m_pmuNumEventCnt;
  uint32_t                      m_deviceNumInterrupts;
  SvdSauRegionsConfig*          m_sauRegionsConfig;
  SvdTypes::CmsisCfgForce       m_cmsisCfgForce;
  std::string                   m_revisionStr;
  std::map<uint32_t, SvdInterrupt*>  m_interruptList;
};

#endif // SvdCpu_H
