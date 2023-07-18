/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SvdOptions_H
#define SvdOptions_H

#include <cstdint>
#include <string>
#include <set>


class SvdOptions {
public:
  SvdOptions();
  ~SvdOptions();


  bool SetWarnLevel(const uint32_t warnLevel);
  bool SetLogFile(const std::string& m_logFile);
  bool AddDiagSuppress(const std::string& suppress);
  bool SetVerbose(bool bVerbose);
  bool ConfigureProgramName(const std::string& programPath);
  bool HaltProgramExecution();
  const std::string& GetSvdFullpath();
  std::string GetSvdFileName();
  bool SetFileUnderTest(const std::string& filename);
  bool SetOutputDirectory(const std::string& filename);
  const std::string& GetOutputDirectory();
  void SetQuietMode(bool bQuiet = true);
  void SetAllowSuppressError(bool bSuppress = true);
  void SetSuppressWarnings(bool bSuppress = true);
  void SetStrict(bool bStrict = true);
  bool MakeSurePathExists(const std::string& path);
  bool SetOutFilenameOverride(const std::string& filename);
  const std::string& GetOutFilenameOverride() const;

  std::string GetCurrentDateTime();
  std::string GetHeader();
  std::string GetVersion();
  const std::string& GetProgramName();
  const std::string& GetLogPath();


  void SetGenerateHeader        (bool bGenerateHeader           = true)   { m_bGenerateHeader         = bGenerateHeader         ; }
  void SetGeneratePartition     (bool bGeneratePartition        = true)   { m_bGeneratePartition      = bGeneratePartition      ; }
  void SetGenerateSfd           (bool bGenerateSfd              = true)   { m_bGenerateSfd            = bGenerateSfd            ; }
  void SetGenerateSfr           (bool bGenerateSfr              = true)   { m_bGenerateSfr            = bGenerateSfr            ; }
  void SetCreateFields          (bool bCreateFields             = true)   { n_bCreateFields           = bCreateFields           ; }
  void SetCreateFieldsAnsiC     (bool bCreateFieldsAnsiC        = true)   { n_bCreateFieldsAnsiC      = bCreateFieldsAnsiC      ; }
  void SetCreateMacros          (bool bCreateMacros             = true)   { m_bCreateMacros           = bCreateMacros           ; }
  void SetCreateEnumValues      (bool bCreateEnumValues         = true)   { m_bCreateEnumValues       = bCreateEnumValues       ; }
  void SetSuppressPath          (bool bSuppressPath             = true)   { m_bSuppressPath           = bSuppressPath           ; }
  void SetCreateFolder          (bool bCreateFolder             = true)   { m_bCreateFolder           = bCreateFolder           ; }
  void SetShowMissingEnums      (bool bShowMissingEnums         = true)   { m_bShowMissingEnums       = bShowMissingEnums       ; }
  void SetUnderTest             (bool bUnderTest                = true)   { m_bUnderTest              = bUnderTest              ; }
  void SetNoCleanup             (bool bNoCleanup                = true)   { m_bNoCleanup              = bNoCleanup              ; }
  void SetDebugStruct           (bool bDebugStruct              = true)   { m_bDebugStruct            = bDebugStruct            ; }
  void SetDebugHeaderfile       (bool bDebugHeaderfile          = true)   { m_bDebugHeaderfile        = bDebugHeaderfile        ; }
  void SetDebugSfd              (bool bDebugSfd                 = true)   { m_bDebugSfd               = bDebugSfd               ; }
  void SetGenerateMapPeripheral (bool bGenerateMapPeripheral    = true)   { m_bGenerateMapPeripheral  = bGenerateMapPeripheral  ; }
  void SetGenerateMapRegister   (bool bGenerateMapRegister      = true)   { m_bGenerateMapRegister    = bGenerateMapRegister    ; }
  void SetGenerateMapField      (bool bGenerateMapField         = true)   { m_bGenerateMapField       = bGenerateMapField       ; }


  bool IsGenerateHeader         () const  { return m_bGenerateHeader         ; }
  bool IsGeneratePartition      () const  { return m_bGeneratePartition      ; }
  bool IsGenerateSfd            () const  { return m_bGenerateSfd            ; }
  bool IsGenerateSfr            () const  { return m_bGenerateSfr            ; }
  bool IsCreateFields           () const  { return n_bCreateFields           ; }
  bool IsCreateFieldsAnsiC      () const  { return n_bCreateFieldsAnsiC      ; }
  bool IsCreateMacros           () const  { return m_bCreateMacros           ; }
  bool IsCreateEnumValues       () const  { return m_bCreateEnumValues       ; }
  bool IsSuppressPath           () const  { return m_bSuppressPath           ; }
  bool IsCreateFolder           () const  { return m_bCreateFolder           ; }
  bool IsShowMissingEnums       () const  { return m_bShowMissingEnums       ; }
  bool IsUnderTest              () const  { return m_bUnderTest              ; }
  bool IsNoCleanup              () const  { return m_bNoCleanup              ; }
  bool IsDebugStruct            () const  { return m_bDebugStruct            ; }
  bool IsDebugHeaderfile        () const  { return m_bDebugHeaderfile        ; }
  bool IsDebugSfd               () const  { return m_bDebugSfd               ; }
  bool IsGenerateMapPeripheral  () const  { return m_bGenerateMapPeripheral  ; }
  bool IsGenerateMapRegister    () const  { return m_bGenerateMapRegister    ; }
  bool IsGenerateMapField       () const  { return m_bGenerateMapField       ; }

  bool IsGenerateMap() const;


private:
  bool m_bGenerateMapPeripheral = false;
  bool m_bGenerateMapRegister = false;
  bool m_bGenerateMapField = false;
  bool m_bGenerateHeader = false;
  bool m_bGeneratePartition = false;
  bool m_bGenerateSfd = false;
  bool m_bGenerateSfr = false;
  bool n_bCreateFields = false;
  bool n_bCreateFieldsAnsiC = false;
  bool m_bCreateMacros = false;
  bool m_bCreateEnumValues = false;
  bool m_bSuppressPath = false;
  bool m_bCreateFolder = false;
  bool m_bShowMissingEnums = false;
  bool m_bUnderTest = false;
  bool m_bNoCleanup = false;
  bool m_bDebugStruct = false;
  bool m_bDebugHeaderfile = false;
  bool m_bDebugSfd = false;

  std::string m_svdToCheck;
  std::string m_logPath;
  std::string m_programName;
  std::string m_outputDir;
  std::string m_outfileOverride;
};

#endif // PACKOPTIONS_H
