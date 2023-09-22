/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Validate.h"
#include "ValidateSemantic.h"
#include "GatherCompilers.h"

#include "RteModel.h"
#include "RteProject.h"
#include "RteFsUtils.h"
#include "ErrLog.h"

using namespace std;

/**
 * @brief class constructor
 * @param rteModel RTE Model to run on
 * @param packOptions options configured for program
*/
ValidateSemantic::ValidateSemantic(RteGlobalModel& rteModel, CPackOptions& packOptions) :
  Validate(rteModel, packOptions)
{
}

/**
 * @brief class destructor
*/
ValidateSemantic::~ValidateSemantic()
{
}

/**
 * @brief base function to run through all tests
 * @return passed / failed
*/
bool ValidateSemantic::Check()
{
  for(auto packItem : GetModel().GetChildren()) {
    RtePackage* pKg = dynamic_cast<RtePackage*>(packItem);
    if(!pKg) {
      continue;
    }

    string fileName = pKg->GetPackageFileName();
    if(GetOptions().IsSkipOnPdscTest(fileName)) {
      continue;
    }

    BeginTest(fileName);

    GatherCompilers(pKg);
    TestMcuDependencies(pKg);

    EndTest();
  }

  TestComponentDependencies();

  return true;
}

/**
 * @brief setup a test run
 * @param packName name of tested package
 * @return passed / failed
 */
bool ValidateSemantic::BeginTest(const std::string& packName)
{
  ErrLog::Get()->SetFileName(packName);

  return true;
}

/**
 * @brief clean up after test run ended
 * @return passed / failed
 */
bool ValidateSemantic::EndTest()
{
  ErrLog::Get()->SetFileName("");

  return true;
}

/**
 * @brief search for compilers referenced in pack
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSemantic::GatherCompilers(RtePackage* pKg)
{
  if(!pKg) {
    return false;
  }

  GatherCompilersVisitor compilersVisitor;
  pKg->AcceptVisitor(&compilersVisitor);

  string comps;
  m_compilers = compilersVisitor.GetCompilerList();
  for(auto& [key, compiler] : m_compilers) {
    if(!comps.empty()) {
      comps += ", ";
    }
    string compilerName = compilersVisitor.GetCompilerName(compiler);
    comps += compilerName;
  }

  if(comps.empty()) {
    comps = "<no compiler dependency found>";
  }

  LogMsg("M079", VAL("COMPILER", comps));

  if(m_compilers.empty()) {
    compiler_s compiler;
    m_compilers[""] = compiler;   // add "empty" compiler as default
  }

  return true;
}


/**
 * @brief RTE Model messages
*/
const map<RteItem::ConditionResult, string> conditionResultTxt = {
  { RteItem::UNDEFINED            , "not evaluated yet"                                                                     },
  { RteItem::R_ERROR              , "error evaluating condition ( recursion detected, condition is missing)"                },
  { RteItem::FAILED               , "HW or compiler not match"                                                              },
  { RteItem::MISSING              , "no component is installed"                                                             },
  { RteItem::MISSING_API          , "no required API is installed"                                                          },
  { RteItem::MISSING_API_VERSION  , "no API with the required or compatible version is installed"                           },
  { RteItem::UNAVAILABLE          , "component is installed, but filtered out"                                              },
  { RteItem::UNAVAILABLE_PACK     , "component is installed, pack is not selected"                                          },
  { RteItem::INCOMPATIBLE         , "incompatible component is selected"                                                    },
  { RteItem::INCOMPATIBLE_VERSION , "incompatible version of component is selected"                                         },
  { RteItem::INCOMPATIBLE_VARIANT , "incompatible variant of component is selected"                                         },
  { RteItem::CONFLICT             , "several exclusive or incompatible components selected"                            },
  { RteItem::INSTALLED            , "matching components are installed, but not selectable because not in active bundle"    },
  { RteItem::SELECTABLE           , "matching components are installed, but not selected"                                   },
  { RteItem::FULFILLED            , "required component selected or no dependency exist"                                    },
  { RteItem::IGNORED              , "condition/expression is irrelevant for the current context"                            },
};

/**
 * @brief output dependency result for error reporting
 * @param dependencyResult result
 * @param inRecursion flag if in recursion
 * @return passed / failed
 */
bool ValidateSemantic::OutputDepResults(const RteDependencyResult& dependencyResult, bool inRecursion /*= 0*/)
{
  static int recursionCnt = 0;
  if(!inRecursion) {
    recursionCnt = 0;
  }

  string spaces;
  for(int i = 0; i < recursionCnt; i++) {
    spaces += "  ";
  }

  const string& errNum = dependencyResult.GetErrorNum();
  const string& msgText = dependencyResult.GetMessageText();
  const string& dispName = dependencyResult.GetDisplayName();
  const string& outMsgText = dependencyResult.GetOutputMessage();

  if(!outMsgText.empty()) {
    if(!errNum.empty()) {
      LogMsg("M502", VAL("NUM", errNum), NAME(dispName), MSG(msgText));
      recursionCnt = 0;
    }
    else {
      if(outMsgText.find("missing") != string::npos) {
        LogMsg("M504", SPACE(spaces), NAME(dispName));
      }

      recursionCnt++;
    }
  }

  for(auto &[key, value] : dependencyResult.GetResults()) {
    OutputDepResults(value, true);
  }

  return true;
}

/**
 * @brief check defined memories
 * @param device RteDeviceItem to run on
 * @return passed / failed
 */
bool ValidateSemantic::CheckMemory(RteDeviceItem* device)
{
  string devName = device->GetName();

  list<RteDeviceProperty*> processors;
  device->GetEffectiveProcessors(processors);
  for(auto proc : processors) {
    const string& pName = proc->GetName();
    if(!pName.empty()) {
      devName += ":";
      devName += pName;
    }

    LogMsg("M071", NAME(devName), proc->GetLineNumber());

    const list<RteDeviceProperty*>& propGroup = device->GetEffectiveProperties("memory", pName);
    if(propGroup.empty()) {
      LogMsg("M312", TAG("memory"), NAME(device->GetName()), device->GetLineNumber());
      return false;
    }

    map<const string, RteDeviceProperty*> propNameCheck;
    for(auto prop : propGroup) {
      const string& id = prop->GetEffectiveAttribute("id");
      const string& name = prop->GetEffectiveAttribute("name");
      const string& access = prop->GetEffectiveAttribute("access");
      const string& start = prop->GetEffectiveAttribute("start");
      const string& size = prop->GetEffectiveAttribute("size");
      const string& pname = prop->GetEffectiveAttribute("pname");
      int lineNo = prop->GetLineNumber();

      string key = name.empty() ? id : name;
      if(!pname.empty()) {
        key += ":" + pname;
      }

      LogMsg("M070", NAME(key), NAME2(devName), lineNo);                        // Checking Memory '%NAME%' for device '%NAME2%'

      if(id.empty()) {                                                          // new description, where 'name' is just a string and 'access' describes the permissions
        if(name.empty() && access.empty()) {
          LogMsg("M307", TAG("id"), TAG2("name"), TAG3("access"), lineNo);      // Attribute '%TAG%' or '%TAG2%' + '%TAG3%' must be specified for 'memory'
        }
      }
      else {                                                                    // "classic" way of RAM/ROM description, where RAM/ROM already has access permissions R, RW
        if(!name.empty() && !access.empty()) {
          LogMsg("M399", TAG("id"), TAG2("name"), TAG3("access"), lineNo);      // Attribute '%TAG% = %NAME%' is ignored, because '%TAG2% = %NAME2%' + '%TAG3% = %NAME3%' is specified
        }
      }

      if(name.empty()) {
        if(!access.empty()) {
          LogMsg("M309", TAG("name"), TAG2("memory"), TAG3("access"), lineNo);  // Attribute '%TAG%' missing when specifying '%TAG2%' for 'memory'
        }
      }
      else {
        if(access.empty()) {
          LogMsg("M309", TAG("access"), TAG2("memory"), TAG3("name"), lineNo);  // Attribute '%TAG%' missing when specifying '%TAG2%' for 'memory'
        }
      }

      if(start.empty()) {
        LogMsg("M308", TAG("start"), TAG2("memory"), lineNo);                   // Attribute '%TAG%' missing
      }

      if(size.empty()) {
        LogMsg("M308", TAG("size"), TAG2("memory"), lineNo);                    // Attribute '%TAG%' missing
      }

      if(!key.empty()) {
        auto propNameCheckIt = propNameCheck.find(key);
        if(propNameCheckIt != propNameCheck.end()) {
          RteDeviceProperty* propFound = propNameCheckIt->second;
          if(propFound) {
            LogMsg("M311", TAG("memory"), NAME(key), LINE(propFound->GetLineNumber()), lineNo);
          }
        }
        else {
          propNameCheck[key] = prop;
        }
      }
    }
  }

  return true;
}

/**
 * @brief check for unsupported characters in 'name'
 * @param name string input name
 * @param tag tag for error reporting
 * @param lineNo line number for error reporting
 * @return passed / failed
 */
bool ValidateSemantic::CheckForUnsupportedChars(const string& name, const string& tag, int lineNo)
{
  if(name.empty()) {
    return false;
  }

  const string supportedChars = "[\\-_A-Za-z0-9/]+";
  LogMsg("M065", TAG(tag), NAME(name), CHR(supportedChars), lineNo);

  if(!RteUtils::CheckCMSISName(name)) {
    LogMsg("M383", TAG(tag), NAME(name), CHR(supportedChars), lineNo);
    return false;
  }

  LogMsg("M010");

  return true;
}

/**
 * @brief checks for device description
 * @param device RteDEviceItem to run on
 * @param processorProperty properties set for device
 * @return passed / failed
 */
bool ValidateSemantic::CheckDeviceDescription(RteDeviceItem* device, RteDeviceProperty* processorProperty)
{
  const string& mcuName = device->GetName();
  const string& mcuVendor = device->GetEffectiveAttribute("Dvendor");
  const string& Pname = processorProperty->GetEffectiveAttribute("Pname");
  int lineNo = device->GetLineNumber();

  RteDeviceProperty* descrProp = device->GetSingleEffectiveProperty("description", Pname);
  if(descrProp) {
    const string& descr = descrProp->GetDescription();
    if(descr.empty()) {
      LogMsg("M380", VENDOR(mcuVendor), MCU(mcuName), lineNo);
      return false;
    }
  }

  return true;
}

/**
 * @brief compares name against name and extension
 * @param name string name to check
 * @param searchName string name to search for
 * @param searchExt string extension (e.g. ".h") to search for
 * @return true / false
 */
bool ValidateSemantic::FindName(const string& name, const string& searchName, const string& searchExt)
{
  if(name.find(searchName, 0) != 0) {
    return false;
  }
  if(name.find(searchExt, 0) != name.length() - searchExt.length()) {
    return false;
  }

  return true;
}

/**
 * @brief update RTE model after new options have been set
 * @param target RteTarget selected target
 * @param rteProject RteProject selected project
 * @param component RteComponent selected component
 * @return passed / failed
 */
bool ValidateSemantic::UpdateRte(RteTarget* target, RteProject* rteProject, RteComponent* component)
{
  target->ClearCollections();
  target->ClearSelectedComponents();
  RteComponentAggregate* cmsisComp = target->GetComponentAggregate("ARM::CMSIS.CORE");
  target->SelectComponent(cmsisComp, 1, true);
  target->SelectComponent(component, 1, true, true);
  rteProject->CollectSettings();
  target->CollectFilteredFiles();
  target->EvaluateComponentDependencies();

  return true;
}

/**
 * @brief check RTE Model output for dependency result
 * @param target RteTarget selected target
 * @param component RteComponent selected component
 * @param mcuVendor string mcuVendor for error reporting
 * @param mcuDispName string mcuDispName for error reporting
 * @param compiler compiler_s compiler for error reporting
 * @return passed / failed
 */
bool ValidateSemantic::CheckDependencyResult(RteTarget* target, RteComponent* component, string mcuVendor, string mcuDispName, compiler_s compiler)
{
  string compId = component->GetComponentID(true);

  RteDependencyResult dependencyResult;
  RteItem::ConditionResult res = target->GetDepsResult(dependencyResult.Results(), target);
  switch(res) {
    case RteItem::SELECTABLE: // all dependencies resolved, component can be selected
    case RteItem::FULFILLED:  // all dependencies resolved, component is selected
    case RteItem::IGNORED:    // condition/expression is irrelevant for the current context
      break;
    default: {                // error
      string msg;
      auto resultText = conditionResultTxt.find(res);
      if(resultText != conditionResultTxt.end()) {
        msg = "\nDependency Result: " + resultText->second;
      }

      int lineNo = component->GetLineNumber();
      LogMsg("M351", COMP("Startup"), VAL("COMPID", compId), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), MSG(msg), lineNo);

      OutputDepResults(dependencyResult);
    } break;
  }

  return true;
}

/**
 * @brief skip directories
 * @param systemHeader string name to test
 * @param rteFolder the "RTE" folder path used for placing files
 * @return true / false
 */
bool ValidateSemantic::ExcludeSysHeaderDirectories(const string& systemHeader, const string& rteFolder)
{
  if(systemHeader.empty()) {
    return true;
  }
  if(systemHeader == rteFolder) {
    return true;
  }
  if(systemHeader == "./" + rteFolder) {
    return true;
  }
  if(systemHeader == "./" + rteFolder + "/_Test") {
    return true;
  }

  return false;
}

/**
 * @brief searches for given file
 * @param systemHeader string name to search for
 * @param targFiles list of target files to view
 * @return true / false
 */
bool ValidateSemantic::FindFileFromList(const string& systemHeader, const set<RteFile*>& targFiles)
{
  for(auto findSysH : targFiles) {
    string fNameSysH = RteUtils::BackSlashesToSlashes(RteUtils::ExtractFileName(findSysH->GetName()));
    if(fNameSysH == systemHeader) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Check device dependencies. Tests if all dependencies are solved and a minimum
 *        on support files and configuration has been defined
 * @param device RteDeviceItem to run on
 * @param rteProject  RteProject to run on
 * @return passed / failed
 */
bool ValidateSemantic::CheckDeviceDependencies(RteDeviceItem *device, RteProject* rteProject)
{
  if(!device) {
    return false;
  }

  const string& mcuName = device->GetName();
  const string& mcuVendor = device->GetEffectiveAttribute("Dvendor");
  int lineNo = device->GetLineNumber();

  CheckForUnsupportedChars(mcuName, "Dname", lineNo);
  CheckMemory(device);

  XmlItem deviceStartup;
  deviceStartup.SetAttribute("Cclass", "Device");
  deviceStartup.SetAttribute("Cgroup", "Startup");

  bool bOk = true;
  for(auto &[processorName, processor] : device->GetProcessors()) {
    const string& Pname = processor->GetEffectiveAttribute("Pname");
    lineNo = processor->GetLineNumber();

    CheckDeviceDescription(device, processor);

    string mcuDispName = mcuName;
    if(!processorName.empty()) {
      mcuDispName += ":";
      mcuDispName += Pname;
    }

    list<string> trustZoneList;
    auto trustZone = processor->GetAttribute("Dtz");
    if(trustZone.empty()) {
      trustZoneList.push_back("");
    }
    else {
      trustZoneList.push_back("TZ-disabled");
      trustZoneList.push_back("Secure");
      trustZoneList.push_back("Non-secure");
    }

    for(auto trustZoneMode : trustZoneList) {
      RteItem filter;
      device->GetEffectiveFilterAttributes(processorName, filter);
      filter.AddAttribute("Dname", mcuName);

      for(auto &[compilerKey, compiler] : m_compilers) {
        filter.AddAttribute("Tcompiler", compiler.tcompiler);
        filter.AddAttribute("Toptions", compiler.toptions);

        if(!trustZoneMode.empty()) {
          filter.AddAttribute("Dsecure", trustZoneMode);
        }

        rteProject->Clear();
        rteProject->AddTarget("Test", filter.GetAttributes(), true, true);
        rteProject->SetActiveTarget("Test");
        RteTarget* target = rteProject->GetActiveTarget();
        rteProject->FilterComponents();

        set<RteComponentAggregate*> startupComponents;
        target->GetComponentAggregates(deviceStartup, startupComponents);
        if(startupComponents.empty()) {
          LogMsg("M350", COMP("Startup"), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), lineNo);
          continue;  // error: no startup component found
        }

        for(auto aggregate : startupComponents) {
          ErrLog::Get()->SetFileName(aggregate->GetPackage()->GetPackageFileName());

          for(auto &[componentKey, componentMap] : aggregate->GetAllComponents()) {
            int foundSystemC = 0, foundStartup = 0;
            bool bFoundSystemH = false;
            int lineSystem = 0, lineStartup = 0;

            for(auto& [key, component] : componentMap) {
              string compId = component->GetComponentID(true);
              LogMsg("M091", COMP("Startup"), VAL("COMPID", compId), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), lineNo);

              UpdateRte(target, rteProject, component);
              int lineNo = component->GetLineNumber();

              CheckDependencyResult(target, component, mcuVendor, mcuDispName, compiler);

              const set<RteFile*>& targFiles = target->GetFilteredFiles(component);
              if(targFiles.empty()) {
                LogMsg("M352", COMP("Startup"), VAL("COMPID", compId), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), lineNo);
                continue;
              }

              const string& deviceHeaderfile = target->GetDeviceHeader();
              if(deviceHeaderfile.empty()) {
                LogMsg("M353", VAL("FILECAT", "Device Header-file"), COMP("Startup"), VAL("COMPID", compId), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), lineNo);
                bOk = false;
              }

              const set<string>& incPaths = target->GetIncludePaths();
              if(incPaths.empty()) {
                LogMsg("M355", VAL("FILECAT", "Include"), COMP("Startup"), VAL("COMPID", compId), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), lineNo);
                bOk = false;
              }

              for(auto file : targFiles) {
                const string& category = file->GetAttribute("category");

                if(category == "source" || category == "sourceAsm" || category == "sourceC") {
                  string fileName = RteUtils::BackSlashesToSlashes(RteUtils::ExtractFileName(file->GetName()));
                  if(fileName.empty()) {
                    continue;
                  }
                  const string& attribute = file->GetAttribute("attr");

                  if(FindName(fileName, "system_", ".c")) {
                    foundSystemC++;
                    lineSystem = file->GetLineNumber();
                    if(attribute != "config") {
                      LogMsg("M377", NAME(fileName), TYP(category), lineNo);
                    }

                    string systemHeader = RteUtils::ExtractFileBaseName(fileName);
                    systemHeader += ".h";

                    bFoundSystemH = FindFileFromList(systemHeader, targFiles);
                    if(!bFoundSystemH) {
                      string incPathsMsg;
                      int    incPathsCnt = 0;
                      for(auto& incPath : incPaths) {
                        systemHeader = RteUtils::BackSlashesToSlashes(incPath);
                        if(ExcludeSysHeaderDirectories(systemHeader, rteProject->GetRteFolder())) {
                          continue;
                        }

                        incPathsMsg += "\n  ";
                        incPathsMsg += to_string((unsigned long long) ++incPathsCnt);
                        incPathsMsg += ": ";
                        incPathsMsg += systemHeader;

                        systemHeader += "/";
                        systemHeader += RteUtils::ExtractFileBaseName(fileName);
                        systemHeader += ".h";

                        string sysHeader = RteUtils::ExtractFileName(systemHeader);
                        for (auto f : targFiles) {
                          if(RteUtils::ExtractFileName(f->GetName()) == sysHeader) {
                            systemHeader = f->GetOriginalAbsolutePath();
                            break;
                          }
                        }

                        if(RteFsUtils::Exists(systemHeader)) {
                          bFoundSystemH = true;
                        }
                      }

                      if(!bFoundSystemH) {
                        systemHeader  = RteUtils::ExtractFileBaseName(fileName);
                        systemHeader += ".h";
                        if(incPathsMsg.empty()) {
                          incPathsMsg  = "\n  ";
                          incPathsMsg += to_string((unsigned long long) ++incPathsCnt);
                          incPathsMsg += ": ";
                          incPathsMsg += "<not found any include path>";
                        }
                        LogMsg("M358", VAL("HFILE", RteUtils::ExtractFileName(systemHeader)), VAL("CFILE", fileName), COMP("Startup"), VAL("COMPID", compId),
                               VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions), PATH(incPathsMsg), lineNo);
                        bOk = false;
                      }
                    }
                  }

                  if(fileName.find("startup_", 0) != string::npos) {
                    foundStartup++;
                    lineStartup = file->GetLineNumber();

                    if(attribute != "config") {
                      LogMsg("M377", NAME(fileName), TYP(category), lineNo);
                    }
                  }
                }
              }
            }

            if(foundSystemC != 1 || foundStartup != 1) {    // ignore if generator="..."
              if(HasExternalGenerator(aggregate)) {
                continue;
              }
            }

            if(foundSystemC != 1) {
              LogMsg(foundSystemC ? "M354" : "M353",
                     VAL("FILECAT", "system_*"), COMP("Startup"), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions),
                     foundSystemC ? lineSystem : lineNo);
              bOk = false;
            }

            if(foundStartup != 1) {
              LogMsg(foundStartup ? "M354" : "M353",
                     VAL("FILECAT", "startup_*"), COMP("Startup"), VENDOR(mcuVendor), MCU(mcuDispName), COMPILER(compiler.tcompiler), OPTION(compiler.toptions),
                     foundStartup ? lineStartup : lineNo);
              bOk = false;
            }
          }
        }

      }
      filter.RemoveAttribute("Tcompiler");

      if(bOk) {
        LogMsg("M010");
      }
    }
  }

  return bOk;
}


/**
 * @brief check for MCU dependencies
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSemantic::HasExternalGenerator(RteComponentAggregate* aggregate)
{
  auto bundleName = aggregate->GetCbundleName();
  if(!bundleName.empty()) {
    for(auto &[componentKey, componentMap] : aggregate->GetAllComponents()) {
      for(auto& [key, component] : componentMap) {
        auto parentBundle = component->GetParentBundle();
        if(parentBundle) {
          for(auto bundleComponent : parentBundle->GetChildren()) {
            auto generatorAttrName = bundleComponent->GetAttribute("generator");
            if(!generatorAttrName.empty()) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}


/**
 * @brief check for MCU dependencies
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSemantic::TestMcuDependencies(RtePackage* pKg)
{
  RteGlobalModel& model = GetModel();
  RteProject* rteProject = model.AddProject(1);
  if(!rteProject || !pKg) {
    return false;
  }

  model.GetLatestPackage("ARM.CMSIS");

  list<RteDeviceItem*> devices;
  pKg->GetEffectiveDeviceItems(devices);
  for(auto device : devices) {
    CheckDeviceDependencies(device, rteProject);
  }

  model.DeleteProject(1);

  return true;
}


/**
 * @brief check component dependencies
 * @return passed / failed
 */
bool ValidateSemantic::TestComponentDependencies()
{
  list<RteDevice*> devices;
  string dname, dvendor;
  string pKgFileName;

  RteGlobalModel& model = GetModel();
  RteProject* rteProject = model.AddProject(1);
  if(!rteProject) {
    return true;
  }

  bool bOk = true;
  for(auto &[compKey, component] : model.GetComponentList()) {
    RtePackage* pKg = component->GetPackage();
    if(!pKg) {
      continue;
    }
    const string& packName = pKg->GetPackageFileName();
    if(GetOptions().IsSkipOnPdscTest(packName)) {
      continue;
    }

    ErrLog::Get()->SetFileName(packName);

    const string& compClass = component->GetAttribute("Cclass");
    const string& compGroup = component->GetAttribute("Cgroup");
    const string& compVer = component->GetAttribute("Cversion");
    const string& compSub = component->GetAttribute("Csub");
    const string& apiVer = component->GetAttribute("Capiversion");
    int lineNo = component->GetLineNumber();

    LogMsg("M069", CCLASS(compClass), CGROUP(compGroup), CSUB(compSub), CVER(compVer));

    XmlItem filter;
    rteProject->Clear();
    rteProject->AddTarget("Test", filter.GetAttributes(), true, true);
    rteProject->SetActiveTarget("Test");
    RteTarget* target = rteProject->GetActiveTarget();
    rteProject->FilterComponents();

    CheckSelfResolvedCondition(component, target);

    target->SelectComponent(component, 1, true);

    const string& apiVersion = component->GetAttribute("Capiversion");
    RteApi* api = component->GetApi(target, false);
    if(!apiVersion.empty()) {
      if(!api) {
        LogMsg("M363", CCLASS(compClass), CGROUP(compGroup), CSUB(compSub), CVER(compVer), APIVER(apiVer), lineNo);
        continue;     // skip Model based test, if API is missing, test will fail
      }
    }
    else if(api) {
      RtePackage* apiPkg = api->GetPackage();
      if(apiPkg) {
        const string& packN = apiPkg->GetPackageFileName();
        LogMsg("M378", CCLASS(compClass), CGROUP(compGroup), CSUB(compSub), CVER(compVer), NAME(packN), lineNo);
      }
    }

    RteDependencyResult dependencyResult;
    RteItem::ConditionResult result = target->GetDepsResult(dependencyResult.Results(), target);

    switch(result) {
      case RteItem::SELECTABLE: // all dependencies resolved, component can be selected
      case RteItem::INSTALLED:
      case RteItem::IGNORED:    // condition/expression is irrelevant for the current context           (same as FULFILLED)
      case RteItem::FULFILLED:  // all dependencies resolved, component is selected
        break;
      default:     // error
        bOk = false;
        break;
    }

    if(!bOk) {
      string msg;
      auto resultText = conditionResultTxt.find(result);
      if(resultText != conditionResultTxt.end()) {
        msg = "\nDependency Result: " + resultText->second;
      }

      LogMsg("M362", CCLASS(compClass), CGROUP(compGroup), CSUB(compSub), CVER(compVer), APIVER(apiVer), MSG(msg), lineNo);
      OutputDepResults(dependencyResult);
    }
    else {
      LogMsg("M010");
    }
  }

  model.DeleteProject(1);
  ErrLog::Get()->SetFileName("");

  return bOk;
}


bool ValidateSemantic::TestDepsResult(const map<const RteItem*, RteDependencyResult>& results, RteComponent* component)
{
  bool success = true;

  const string& compName = component->GetID();

  for(auto& [item, dRes] : results) {
    RteItem::ConditionResult r = dRes.GetResult();
    if(r < RteItem::INSTALLED) {
      continue;
    }

    if(!TestDepsResult(dRes.GetResults(), component)) {
      success = false;
    }

    const string& itemName = item->GetID();
    auto aggrs = dRes.GetComponentAggregates();
    for(auto aggr : aggrs) {
      if(aggr->HasComponent(component)) {
        const string errExprStr = item->GetParent()->GetID();
        const int compLineNo = item->GetLineNumber();
        // "The component '%NAME%' has dependency '%NAME2%' : '%EXPR%' that is be resolved by the component itself."
        LogMsg("M389", NAME(compName), LINE(compLineNo), NAME2(itemName), VAL("EXPR", errExprStr), item->GetLineNumber());
        success = false;
      }
    }
  }

  return success;
}


bool ValidateSemantic::CheckSelfResolvedCondition(RteComponent* component, RteTarget* target)
{
  RteDependencySolver* depSolver = target->GetDependencySolver();
  depSolver->Clear();

  component->Evaluate(depSolver);
  RteDependencyResult depsRes;
  component->GetDepsResult(depsRes.Results(), target);
  TestDepsResult(depsRes.GetResults(), component);

  return true;
}

