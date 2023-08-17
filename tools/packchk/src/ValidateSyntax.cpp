/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Validate.h"
#include "ValidateSyntax.h"
#include "CheckConditions.h"
#include "CheckComponents.h"
#include "CheckFiles.h"

#include "CrossPlatform.h"
#include "ErrLog.h"
#include "RteGenerator.h"
#include "RteUtils.h"
#include "RteFsUtils.h"

#include <regex>

#include <time.h>
#include <algorithm>

using namespace std;


const std::map<const RteDeviceItem::TYPE, const std::string> ValidateSyntax::m_rteTypeStr = {
  { RteDeviceItem::FAMILY,    "Family"    },
  { RteDeviceItem::SUBFAMILY, "Subfamily" },
  { RteDeviceItem::DEVICE,    "Device"    },
  { RteDeviceItem::VARIANT,   "Variant"   },
};

const std::map<const RteDeviceItem::TYPE, const std::string> ValidateSyntax::m_rteTypeStrHirachy = {
  { RteDeviceItem::FAMILY,    "|- Family:           " },
  { RteDeviceItem::SUBFAMILY, "   |- Subfamily:     " },
  { RteDeviceItem::DEVICE,    "      |- Device:     " },
  { RteDeviceItem::VARIANT,   "         |- Variant: " },
};

/**
 * @brief class constructor
 * @param rteModel RTE Model to run on
 * @param packOptions options configured for program
 */
ValidateSyntax::ValidateSyntax(RteGlobalModel& rteModel, CPackOptions& packOptions) :
  Validate(rteModel, packOptions)
{
  InitFeatures();
}

/**
 * @brief class destructor
*/
ValidateSyntax::~ValidateSyntax() {
}

/**
 * @brief converts RteDeviceItem::TYPE to string
 * @param type RteDeviceItem::TYPE
 * @return string
*/
const string& ValidateSyntax::GetRteTypeString(const RteDeviceItem::TYPE type)
{
  auto find = m_rteTypeStr.find(type);
  if(find != m_rteTypeStr.end()) {
    return find->second;
  }

  return RteUtils::EMPTY_STRING;
}

/**
* @brief base function to run through all tests
* @return passed / failed
*/
bool ValidateSyntax::Check()
{
  for(auto pack : GetModel().GetChildren()) {
    RtePackage* pKg = dynamic_cast<RtePackage*>(pack);
    if(!pKg) {
      continue;
    }
    string fileName = pKg->GetPackageFileName();
    if(GetOptions().IsSkipOnPdscTest(fileName)) {
      continue;
    }

    BeginTest(fileName);

    CheckSchemaVersion(pKg);
    CheckPackageReleaseDate(pKg);
    CheckPackageUrl(pKg);
    CheckInfo(pKg);
    CheckAllFiles(pKg);
    CheckHierarchy(pKg);
    CheckComponents(pKg);
    CheckExamples(pKg);
    CheckBoards(pKg);
    CheckTaxonomy(pKg);
    CheckDeviceProperties(pKg);
    CheckBoardProperties(pKg);
    CheckRequirements(pKg);

    EndTest();
  }

  return true;
}

/**
* @brief setup a test run
* @param packName name of tested package
* @return passed / failed
*/
bool ValidateSyntax::BeginTest(const std::string& packName)
{
  ErrLog::Get()->SetFileName(packName);
  ClearSchemaVersion();

  return true;
}

/**
* @brief clean up after test run ended
* @return passed / failed
*/
bool ValidateSyntax::EndTest()
{
  ErrLog::Get()->SetFileName("");
  ClearSchemaVersion();

  return true;
}

/**
 * @brief checks all files in pack using visitor pattern
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckAllFiles(RtePackage* pKg)
{
  string workDir = pKg->GetAbsolutePackagePath();
  if(workDir.empty()) {
    workDir = "./";
  }

  CheckFilesVisitor fileVisitor(workDir, pKg->GetName());

  CheckLicense(pKg, fileVisitor);
  pKg->AcceptVisitor(&fileVisitor);

  return(true);
}

/**
 * @brief check (for) license file
 * @param pKg package under test
 * @param fileVisitor
 * @return passed / failed
 */
bool ValidateSyntax::CheckLicense(RtePackage* pKg, CheckFilesVisitor& fileVisitor)
{
  const string& licPath = pKg->GetItemValue("license");
  if(licPath.empty()) {
    return true;
  }

  if(XmlValueAdjuster::IsAbsolute(licPath)) {
    LogMsg("M326", PATH(licPath));   // error : absolute paths are not permitted
  }
  else if(licPath.find('\\') != string::npos) {
    if(XmlValueAdjuster::IsURL(licPath)) {
      LogMsg("M370", URL(licPath));  // error : backslash are non permitted in URL
    }
    else {
      LogMsg("M327", PATH(licPath)); // error : backslash are not recommended
    }
  }

  CheckFiles& checkFiles = fileVisitor.GetCheckFiles();
  if(!checkFiles.CheckFileExists(licPath, -1)) {
    return false;
  }

  return checkFiles.CheckCaseSense(licPath, -1);
}

/**
 * @brief check package information (name, releases, description)
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckInfo(RtePackage* pKg)
{
  string fileName = pKg->GetPackageFileName();
  LogMsg("M052", PATH(fileName));

  // Check if PDSC info for file naming is complete
  bool bInfoComplete = true;
  if(pKg->GetAttribute("vendor").empty()) {
    LogMsg("M302");
    bInfoComplete = false;
  }

  string packName = pKg->GetName();
  if(packName.empty() || packName == "package") {
    LogMsg("M303");
    bInfoComplete = false;
  }

  // Get Releases, iterate, getChildren, GetAttrVersion, GetVersionString
  RteItem* release = pKg->GetReleases();
  if(!release) {
    LogMsg("M338");
    return false;
  }

  const Collection<RteItem*> releases = release->GetChildren();
  if(releases.empty()) {
    LogMsg("M305");
    bInfoComplete = false;
  }
  for(auto rel : releases) {
    const string& rVer = rel->GetVersionString();
    const string& rDescr = rel->GetDescription();
    int lineNo = rel->GetLineNumber();

    if(rVer.empty() && !rDescr.empty()) {
      LogMsg("M328", VAL("DESCR", rDescr), lineNo);
    }

    if(!rVer.empty() && rDescr.empty()) {
      LogMsg("M329", VAL("VER", rVer), lineNo);
    }
  }

  if(pKg->GetDescription().empty()) {
    LogMsg("M306");
    bInfoComplete = false;
  }

  string pdscRef = pKg->GetAttribute("vendor") + "." + pKg->GetName();
  const string& pdscPkg = RteUtils::ExtractFileBaseName(fileName);

  if(pdscRef.compare(pdscPkg) != 0) {
    LogMsg("M207", VAL("PDSC1", pdscRef), VAL("PDSC2", pdscPkg));
    bInfoComplete = false;
  }

  if(bInfoComplete) {
    LogMsg("M010");
  }

  return bInfoComplete;
}

/**
 * @brief check package components via visitor pattern
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckComponents(RtePackage* pKg)
{
  string workDir = pKg->GetAbsolutePackagePath();

  CheckConditions checkConditions(GetModel());
  checkConditions.SetWorkingDir(workDir);

  DefinedConditionsVisitor defCondVisitor(&checkConditions);
  RteItem* conditions = pKg->GetConditions();
  if(conditions) {
    conditions->AcceptVisitor(&defCondVisitor);
    UsedConditionsVisitor usedCondVisitor(&checkConditions);
    pKg->AcceptVisitor(&usedCondVisitor);
    checkConditions.CheckForUnused();
  }

  RteItem* components = pKg->GetComponents();
  if(components) {
    CheckComponent checkComponent(GetModel());
    ComponentsVisitor componentsVisitor(checkComponent);
    components->AcceptVisitor(&componentsVisitor);
  }

  return true;
}

/**
 * @brief check if schema version is set
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckSchemaVersion(RtePackage* pKg)
{
  m_schemaVersion = pKg->GetAttribute("schemaVersion");
  if(m_schemaVersion.empty()) {
    LogMsg("M376");
    m_schemaVersion = "0.0";

    return false;
  }

  LogMsg("M096", VAL("VER", m_schemaVersion));

  return true;
}

/**
 * @brief clear local copy of schema version
 * @return passed / failed
 */
bool ValidateSyntax::ClearSchemaVersion()
{
  m_schemaVersion.clear();

  return true;
}

/**
 * @brief check releases section
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckPackageReleaseDate(RtePackage* pKg)
{
  time_t rawtime;
  char buffer[80] = { "0000-00-00" };

  time(&rawtime);
  struct tm* timeinfo = localtime(&rawtime);
  if(timeinfo) {
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
  }

  RteItem* releases = pKg->GetReleases();
  if(!releases) {
    return false;
  }
  auto childs = releases->GetChildren();
  if(childs.empty()) {
    return true;
  }

  string today(buffer);
  string latestVersion;
  string latestDate;
  int latestLineNo = 0;

  for(auto child : childs) {
    string releaseDate = child->GetAttribute("date");
    string releaseVersion = child->GetAttribute("version");
    int lineNo = child->GetLineNumber();

    LogMsg("M098", RELEASEDATE(releaseDate), lineNo);

    // Check Semantic Version with "PackVersionType" from PACK.XSD
    LogMsg("M066", RELEASEVER(releaseVersion), lineNo);
    static regex re("[0-9]+.[0-9]+.[0-9]+((\\-[0-9A-Za-z_\\-\\.]+)|([_A-Za-z][0-9A-Za-z_\\-\\.]*)|())((\\+[\\-\\._A-Za-z0-9]+)|())");
    if(!regex_match(releaseVersion, re)) {
      LogMsg("M394", RELEASEVER(releaseVersion), lineNo);
      continue;
    }

    // search for BUILD version from SemVer string
    string releaseVersionCheckPre = VersionCmp::RemoveVersionMeta(releaseVersion);

    // search for PRE-RELEASE version from SemVer string
    size_t minusPos = releaseVersionCheckPre.find_first_of("-");
    if(minusPos != string::npos) {
      string preReleaseVersion = releaseVersionCheckPre.substr(minusPos);
      LogMsg("M393", VAL("DEVVERSION", preReleaseVersion),  RELEASEVER(releaseVersion), lineNo);
    }
    else if(releaseDate.empty()) {    // no Pre-Release
      LogMsg("M395", RELEASEVER(releaseVersion), lineNo);
    }

    if(!releaseDate.empty()) {
      bool ok = AlnumCmp::Compare(today, releaseDate) >= 0;
      if(!ok) {
        LogMsg("M386", RELEASEDATE(releaseDate), TODAYDATE(today), lineNo);    // Release date is in future
      }
    }

    // Check releases for consistency
    LogMsg("M067", RELEASEVER(releaseVersion), RELEASEDATE(releaseDate), lineNo);

    if(!latestVersion.empty() && !releaseVersion.empty()) {
      int res = VersionCmp::Compare(latestVersion, releaseVersion);
      if(res <= 0) {
        LogMsg("M396", TAG("Version"), RELEASEVER(releaseVersion), RELEASEDATE(releaseDate), LATESTVER(latestVersion), LATESTDATE(latestDate), LINE(latestLineNo), lineNo);
      }
    }

    latestVersion = releaseVersion;
    latestDate = releaseDate;
    latestLineNo = lineNo;

    LogMsg("M010");
  }

  return true;
}

/**
 * @brief check package url string
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckPackageUrl(RtePackage* pKg)
{
  CPackOptions& packOptions = GetOptions();
  const string& pRefUrl = packOptions.GetUrlRef();
  string pUrl = pKg->GetURL();

  if(pUrl.empty()) {
    LogMsg("M304");
    return true;
  }

  if(pUrl.find(":", 1) == 1) {
    LogMsg("M315", VAL("URL", pUrl));
  }

  if(*pUrl.rbegin() != '/') {
    LogMsg("M316", VAL("URL", pUrl));
  }

  if(pRefUrl.empty()) {
    return true;
  }

  size_t nCheckLen = pRefUrl.length();
  while(nCheckLen > 0 && pRefUrl[nCheckLen - 1] == '/') {
    nCheckLen--;
  }

  if(nCheckLen <= 0) {
    return true;   //reference doesn't make sense, don't blame the PDSC file
  }

  if((pUrl.length() < nCheckLen) || (_strnicmp(pUrl.c_str(), pRefUrl.c_str(), (int)nCheckLen) != 0)) {
    LogMsg("M301", VAL("URL1", pRefUrl), VAL("URL2", pUrl));
    return false;
  }

  return true;
}

/**
 * @brief adds type / text to id string
 * @param id string to add to
 * @param type string input
 * @param text string input
*/
void ValidateSyntax::AddToId(string& id, const string type, const string text)
{
  if(text.empty()) {
    return;
  }

  if(!id.empty()) {
    id += ", ";
  }

  id += type;
  id += "=";
  id += "\"";
  id += text;
  id += "\"";
}

/**
 * @brief create feature id string
 * @param prop RteItem property to create id from
 * @return string
*/
string ValidateSyntax::CreateIdFeature(RteItem* prop)
{
  string id;

  AddToId(id, "type", prop->GetAttribute("type"));
  AddToId(id, "m", prop->GetAttribute("m"));
  AddToId(id, "name", prop->GetAttribute("name"));

  return id;
}

/**
 * @brief create feature id string
 * @param prop RteItem property to create id from
 * @param cpuName for error reporting
 * @return string
 */
string ValidateSyntax::CreateId(RteItem* prop, const string& cpuName)
{
  string id;
  string tag = prop->GetTag();
  if(tag == "feature") {
    id = CreateIdFeature(prop);
    if(!id.empty()) {
      LogMsg("M089", MCU(cpuName), VAL("ID", id));
      LogMsg("M010");
    }
  }
  if(tag == "algorithm") {
    id = prop->GetID();
    if(!id.empty()) {
      LogMsg("M089", MCU(cpuName), VAL("ID", id));
      LogMsg("M010");
    }
  }
  else {
    LogMsg("M095", VAL("MCU", cpuName), VAL("TAG", tag));
  }
  // Other properties can't be tested, because they can overwrite in inheritance

  return id;
}

/**
 * @brief initialize list of known features
*/
void ValidateSyntax::InitFeatures()
{
  // device
  m_featureTableDevice["Crypto"] = FeatureEntry("Cryptographic Engine", "<feature type=\"Crypto\" n=\"128.256\" name=\"HW accelerated AES Encryption Engine\"/>", "128/256-bit HW accelerated AES Encryption Engine");
  m_featureTableDevice["NVIC"] = FeatureEntry("NVIC", "<feature type=\"NVIC\" n=\"120\" name=\"NVIC\"/>", "NVIC with 120 interrupt sources");
  m_featureTableDevice["DMA"] = FeatureEntry("DMA", "<feature type=\"DMA\" n=\"16\" name=\"High-Speed DMA\"/>", "16-channel High-Speed DMA");
  m_featureTableDevice["RNG"] = FeatureEntry("Random Number Generator", "<feature type=\"RNG\" name=\"True Random Number Generator\"/>", "True Random Number Generator");
  m_featureTableDevice["CoreOther"] = FeatureEntry("Other Core Feature", "<feature type=\"CoreOther\" n=1 name=\"96-bit Unique Identifier\"/>", "1 x 96-bit Unique Identifier");
  m_featureTableDevice["ExtBus"] = FeatureEntry("External Bus Interface", "<feature type=\"ExtBus\" n=\"16\" name=\"External Bus Interface for SRAM Communication\"/>", "16-bit External Bus Interface for SRAM Communication");
  m_featureTableDevice["Memory"] = FeatureEntry("Memory", "<feature type=\"Memory\" n=\"128\" name=\"EEPROM\"/>", "128 byte EEPROM");
  m_featureTableDevice["MemoryOther"] = FeatureEntry("Other Memory Type", "<feature type=\"MemoryOther\" n=\"1\" name=\"1 kB MRAM\"/>", "1 x 1 kB MRAM");
  m_featureTableDevice["XTAL"] = FeatureEntry("External Crystal Oscillator", "<feature type=\"XTAL\" n=\"4000000\" m=\"25000000\" name=\"External Crystal Oscillator\"/>", "4 MHz .. 25 MHz External Crystal Oscillator");
  m_featureTableDevice["IntRC"] = FeatureEntry("Internal RC Oscillator", "<feature type=\"IntRC\" n=\"16000000\" name=\"Internal RC Oscillator with +/- 1% accuracy\"/>", "16 MHz Internal RC Oscillator with +/- 1% accuracy");
  m_featureTableDevice["PLL"] = FeatureEntry("PLL", "<feature type=\"PLL\" n=\"3\" name=\"Internal PLL\"/>", "3 Internal PLL");
  m_featureTableDevice["RTC"] = FeatureEntry("RTC", "<feature type=\"RTC\" n=\"32000\" name=\"Internal RTC\"/>", "32 kHz Internal RTC");
  m_featureTableDevice["ClockOther"] = FeatureEntry("Other Clock Peripheral", "<feature type=\"ClockOther\" name=\"My special clock feature\"/>", "My special clock feature");
  m_featureTableDevice["PowerMode"] = FeatureEntry("Power Modes", "<feature type=\"Mode\" n=\"3\" name=\"Run, Sleep, Deep-Sleep\"/>", "3 Power Modes: Run, Sleep, Deep-Sleep");
  m_featureTableDevice["VCC"] = FeatureEntry("Operating Voltage", "<feature type=\"VCC\" n=\"1.8\" m=\"3.6\"/>", "1.8 V .. 3.6 V");
  m_featureTableDevice["Consumption"] = FeatureEntry("Power Consumption", "<feature type=\"Consumption\" n=\"0.00004\" m=\"0.002\" name=\"Ultra-Low Power Consumption\"/>", "40 uW/MHz .. 2 mW/MHz Ultra-Low Power Consumption");
  m_featureTableDevice["PowerOther"] = FeatureEntry("Other Power Feature", "<feature type=\"PowerOther\" n=\"1\" name=\"POR\"/>", "1 x POR");
  m_featureTableDevice["BGA"] = FeatureEntry("BGA", "<feature type=\"BGA\" n=\"256\" name=\"Plastic Ball Grid Array\"/>", "256-ball Plastic Ball Grid Array");
  m_featureTableDevice["CSP"] = FeatureEntry("CSP", "<feature type=\"CSP\" n=\"28\" name=\"Wafer-Level Chip-Scale Package\"/>", "28-ball Wafer-Level Chip-Scale Package");
  m_featureTableDevice["PLCC"] = FeatureEntry("PLCC", "<feature type=\"PLCC\" n=\"20\" name=\"PLCC Package\"/>", "20-lead PLCC Package");
  m_featureTableDevice["QFN"] = FeatureEntry("QFN", "<feature type=\"QFN\" n=\"33\" name=\"QFN Package\"/>", "33-pad QFN Package");
  m_featureTableDevice["QFP"] = FeatureEntry("QFP", "<feature type=\"QFP\" n=\"128\" name=\"Low-Profile QFP Package\"/>", "128-lead Low-Profile QFP Package");
  m_featureTableDevice["SON"] = FeatureEntry("SON", "<feature type=\"SON\" n=\"16\" name=\"SSON Package\"/>", "16-no-lead SSON Package");
  m_featureTableDevice["SOP"] = FeatureEntry("SOP", "<feature type=\"SOP\" n=\"16\" name=\"SSOP Package\"/>", "16-lead SSOP Package");
  m_featureTableDevice["DIP"] = FeatureEntry("DIP", "<feature type=\"DIP\" n=\"16\" name=\"Dual In-Line Package\"/>", "16-lead Dual In-Line Package");
  m_featureTableDevice["PackageOther"] = FeatureEntry("Other Package Type", "<feature type=\"PackageOther\" n=\"44\" name=\"My other Package\"/>", "44-contacts My other Package");
  m_featureTableDevice["IOs"] = FeatureEntry("Inputs/Outputs", "<feature type=\"IOs\" n=\"112\" name=\"General Purpose I/Os, 5V tolerant\"/>", "112 General Purpose I/Os, 5V tolerant");
  m_featureTableDevice["ExtInt"] = FeatureEntry("External Interrupts", "<feature type=\"ExtInt\" n=\"12\"/>", "12 External Interrupts");
  m_featureTableDevice["Temp"] = FeatureEntry("Operating Temperature Range", "<feature type=\"Temp\" n=\"-40\" m=\"105\" name=\"Extended Operating Temperature Range\"/>", "-40 C .. +105 C Extended Operating Temperature Range");
  m_featureTableDevice["ADC"] = FeatureEntry("ADC", "<feature type=\"ADC\" n=\"5\" m=\"12\" name=\"High-Performance ADC\"/>", "5-channel x 12-bit High-Performance ADC");
  m_featureTableDevice["DAC"] = FeatureEntry("DAC", "<feature type=\"DAC\" n=\"2\" m=\"10\"/>", "2 x 12-bit DAC");
  m_featureTableDevice["TempSens"] = FeatureEntry("Temperature Sensor", "<feature type=\"TempSens\" n=\"1\"/>", "1 x Temperature Sensor");
  m_featureTableDevice["AnalogOther"] = FeatureEntry("Other Analog Peripheral", "<feature type=\"AnalogOther\" n=\"1\" name=\"My Analog\"/>", "1 x My Analog");
  m_featureTableDevice["PWM"] = FeatureEntry("PWM", "<feature type=\"PWM\" n=\"2\" m=\"16\" name=\"Pulse Width Modulation\"/>", "2 x 16-bit Pulse Width Modulation");
  m_featureTableDevice["Timer"] = FeatureEntry("Timer/Counter Module", "<feature type=\"Timer\" n=\"2\" m=\"32\" name=\"Timer Module with Quadrature Encoding\"/>", "2 x 32-bit Timer Module with Quadrature Encoding");
  m_featureTableDevice["WDT"] = FeatureEntry("Watchdog", "<feature type=\"WDT\" n=\"1\"/>", "1 x Watchdog Timer");
  m_featureTableDevice["TimerOther"] = FeatureEntry("Other Timer Peripheral", "<feature type=\"TimerOther\" n=\"1\" name=\"Quadrature En-/Decoder\"/>", "1 x Quadrature En-/Decoder");
  m_featureTableDevice["MPSerial"] = FeatureEntry("Multi-Purpose Serial Peripheral", "<feature type=\"MPSerial\" n=\"4\" name=\"Multi-Purpose Serial Interface Module: I2C, I2S, SPI, UART\"/>", "4 x Multi-Purpose Serial Interface Module: I2C, I2S, SPI, UART");
  m_featureTableDevice["CAN"] = FeatureEntry("CAN", "<feature type=\"CAN\" n=\"2\" name=\"CAN 2.0b Controller\"/>", "2 x CAN 2.0b Controller");
  m_featureTableDevice["ETH"] = FeatureEntry("Ethernet", "<feature type=\"ETH\" n=\"1\" m=\"10000000\" name=\"Integrated Ethernet MAC with PHY\"/>", "1 x 10 Mbit/s Integrated Ethernet MAC with PHY");
  m_featureTableDevice["I2C"] = FeatureEntry("I2C", "<feature type=\"I2C\" n=\"2\"name=\"Low-Power I2C\"/>", "2 x Low-Power I2C");
  m_featureTableDevice["I2S"] = FeatureEntry("I2S", "<feature type=\"I2S\" n=\"3\"/>", "3 x I2S");
  m_featureTableDevice["LIN"] = FeatureEntry("LIN", "<feature type=\"LIN\" n=\"4\"/>", "4 x LIN");
  m_featureTableDevice["SDIO"] = FeatureEntry("SDIO", "<feature type=\"SDIO\" n=\"1\" m=\"4\" name=\"SDIO Interface\"/>", "1 x 4-bit SDIO Interface");
  m_featureTableDevice["SPI"] = FeatureEntry("SPI", "<feature type=\"SPI\" n=\"2\" m=\"20000000\" name=\"SPI Interface\"/>", "2 x 20 Mbit/s SPI Interface");
  m_featureTableDevice["UART"] = FeatureEntry("UART", "<feature type=\"UART\" n=\"4\" m=\"3000000\" name=\"High-Speed UART Interface\"/>", "4 x 3 Mbit/s High-Speed UART Interface");
  m_featureTableDevice["USART"] = FeatureEntry("USART", "<feature type=\"USART\" n=\"2\" m=\"1000000\" name=\"High-Speed USART Interface\"/>", "2 x 1 Mbit/s High-Speed USART Interface");
  m_featureTableDevice["USBD"] = FeatureEntry("USB Device", "<feature type=\"USBD\" n=\"2\" name=\"Full-Speed USB Device\"/>", "2 x Full-Speed USB Device");
  m_featureTableDevice["USBH"] = FeatureEntry("USB Host", "<feature type=\"USBH\" n=\"2\" name=\"High-Speed USB Host\"/>", "2 x High-Speed USB Host");
  m_featureTableDevice["USBOTG"] = FeatureEntry("USB OTG", "<feature type=\"USBOTG\" n=\"1\" name=\"High-Speed USB OTG with PHY\"/>", "1 x High-Speed USB OTG with PHY");
  m_featureTableDevice["ComOther"] = FeatureEntry("Other Communication Peripheral", "<feature type=\"ComOther\" n=\"1\" name=\"ZigBee\"/>", "1 x ZigBee");
  m_featureTableDevice["Camera"] = FeatureEntry("Camera Interface", "<feature type=\"Camera\" n=\"1\" m=\"8\" name=\"Digital Camera Interface\"/>", "1 x 8-bit Digital Camera Interface");
  m_featureTableDevice["GLCD"] = FeatureEntry("Graphic LCD Controller", "<feature type=\"GLCD\" n=\"1\" m=\"320.240\" name=\"TFT LCD Controller\"/>", "1 x 320 x 480 pixel TFT LCD Controller");
  m_featureTableDevice["LCD"] = FeatureEntry("Segment LCD Controller", "<feature type=\"LCD\" n=\"1\" m=\"16.40\" name=\"Segment LCD Controller\"/>", "1 x 16 x 40 Segment LCD Controller");
  m_featureTableDevice["Touch"] = FeatureEntry("Capacitive Touch Inputs", "<feature type=\"Touch\" n=\"10\" name=\"Capacitive Touch Inputs\"/>", "10 x Capacitive Touch Inputs");
  m_featureTableDevice["Other"] = FeatureEntry("Other Feature", "<feature type=\"Other\" n=\"2\" name=\"My other Interface\"/>", "2 x My other Interface");
  m_featureTableDevice["GPU"] = FeatureEntry("GPU", "<feature type=\"GPU\"/>", "GPU");
  m_featureTableDevice["AI"] = FeatureEntry("AI", "<feature type=\"AI\"/>", "AI");
  m_featureTableDevice["FPGA"] = FeatureEntry("FPGA", "<feature type=\"FPGA\"/>", "FPGA");
  m_featureTableDevice["Application"] = FeatureEntry("Application", "<feature type=\"Application\"/>", "Application");
  m_featureTableDevice["IrDa"] = FeatureEntry("IrDa", "<feature type=\"IrDa\"/>", "IrDa");
  m_featureTableDevice["HDMI"] = FeatureEntry("HDMI", "<feature type=\"HDMI\"/>", "HDMI");
  m_featureTableDevice["MIPI"] = FeatureEntry("MIPI", "<feature type=\"MIPI\"/>", "MIPI");
  m_featureTableDevice["PCIE"] = FeatureEntry("PCIE", "<feature type=\"PCIE\"/>", "PCIE");
  m_featureTableDevice["Bluetooth"] = FeatureEntry("Bluetooth", "<feature type=\"Bluetooth\"/>", "Bluetooth");
  m_featureTableDevice["ZigBee"] = FeatureEntry("ZigBee", "<feature type=\"ZigBee\"/>", "ZigBee");
  m_featureTableDevice["802.15.4"] = FeatureEntry("802.15.4", "<feature type=\"802.15.4\"/>", "802.15.4");
  m_featureTableDevice["LoRa"] = FeatureEntry("LoRa", "<feature type=\"LoRa\"/>", "LoRa");
  m_featureTableDevice["LTE Cat-M"] = FeatureEntry("LTE Cat-M", "<feature type=\"LTE Cat-M\"/>", "LTE Cat-M");
  m_featureTableDevice["NB-IoT"] = FeatureEntry("NB-IoT", "<feature type=\"NB-IoT\"/>", "NB-IoT");
  m_featureTableDevice["NFC"] = FeatureEntry("NFC", "<feature type=\"NFC\"/>", "NFC");
  m_featureTableDevice["WirelessOther"] = FeatureEntry("WirelessOther", "<feature type=\"WirelessOther\"/>", "WirelessOther");
  m_featureTableDevice["I/O"] = FeatureEntry("I/O", "<feature type=\"I/O\"/>", "I/O");
  m_featureTableDevice["D/A"] = FeatureEntry("D/A", "<feature type=\"D/A\"/>", "D/A");
  m_featureTableDevice["A/D"] = FeatureEntry("A/D", "<feature type=\"A/D\"/>", "A/D");
  m_featureTableDevice["Com"] = FeatureEntry("Com", "<feature type=\"Com\"/>", "Com");
  m_featureTableDevice["USB"] = FeatureEntry("USB", "<feature type=\"USB\"/>", "USB");
  m_featureTableDevice["Package"] = FeatureEntry("Package", "<feature type=\"Package\"/>", "Package");
  m_featureTableDevice["Backup"] = FeatureEntry("Backup", "<feature type=\"Backup\"/>", "Backup");

  // Board
  m_featureTableBoard["ODbg"] = FeatureEntry("Integrated Debug Adapter", "<feature type=\"ODbg\" n=\"1\" name=\"Integrated Link on USB Connector J13\"/>", "1 x Integrated Link on USB Connector J13");
  m_featureTableBoard["XTAL"] = FeatureEntry("Crystal Oscillator", "<feature type=\"XTAL\" n=\"8000000\"/>", "8 MHz Crystal Oscillator");
  m_featureTableBoard["PWR"] = FeatureEntry("Power Supply", "<feature type=\"PWR\" n=\"8\" m=\"12\"/>", "8 V - 12 V Power Supply");
  m_featureTableBoard["PWRSock"] = FeatureEntry("Power Socket", "<feature type=\"PWRSock\" n=\"1\" name=\"Coaxial Power Receptacle\"/>", "1 x Coaxial Power Receptacle");
  m_featureTableBoard["Batt"] = FeatureEntry("Battery", "<feature type=\"Batt\" n=\"1\" name=\"CR2032 Battery for RTC\"/>", "1 x CR2032 Battery for RTC");
  m_featureTableBoard["Curr"] = FeatureEntry("Current", "<feature type=\"Curr\" n=\"0.320\" m=\"0.375\"/>", "320 mA (typ), 375 mA (max) Current");
  m_featureTableBoard["CoreOther"] = FeatureEntry("Other Core Feature", "<feature type=\"CoreOther\" n=1 name=\"My Other Core Feature\"/>", "1 x My Other Core Feature");
  m_featureTableBoard["RAM"] = FeatureEntry("RAM", "<feature type=\"RAM\" n=\"1\" name=\"512 kB Static RAM\"/>", "1 x 512 kB Static RAM");
  m_featureTableBoard["ROM"] = FeatureEntry("Flash", "<feature type=\"ROM\" n=\"1\" name=\"4 MB NAND-Flash\"/>", "1 x 4 MB NAND-Flash");
  m_featureTableBoard["Memory"] = FeatureEntry("Memory", "<feature type=\"Memory\" n=\"128\" name=\"EEPROM\"/>", "128 byte EEPROM");
  m_featureTableBoard["MemCard"] = FeatureEntry("SD/microSD/MMC Card Holder", "<feature type=\"MemCard\" n=\"2\" name=\"SD Card Holder\"/>", "2 x SD Card Holder");
  m_featureTableBoard["MemoryOther"] = FeatureEntry("Other Memory Type", "<feature type=\"MemoryOther\" n=\"1\" name=\"1 kB MRAM\"/>", "1 x 1 kB MRAM");
  m_featureTableBoard["DIO"] = FeatureEntry("Digital I/Os", "<feature type=\"DIO\" n=\"26\" name=\"Digital IOs on 2 x 13 pin header (1.27 mm pitch)\"/>", "26 x Digital IOs on 2 x 13 pin header (1.27 mm pitch)");
  m_featureTableBoard["AIO"] = FeatureEntry("Analog I/Os", "<feature type=\"AIO\" n=\"4\" name=\"Analog Inputs on 4 pin header (1.27 mm pitch)\"/>", "4 x Analog Inputs on 4 pin header (1.27 mm pitch)");
  m_featureTableBoard["Proto"] = FeatureEntry("Prototyping Area", "<feature type=\"Proto\" n=\"4\" m=\"7\" name=\"Prototyping Area with 1.00 mm pitch\"/>", "4 x 7 Prototyping Area with 1.00 mm pitch");
  m_featureTableBoard["USB"] = FeatureEntry("USB", "<feature type=\"USB\" n=\"2\" name=\"Full-Speed USB Device, Micro-B receptacle\"/>", "2 x Full-Speed USB Device, Micro-B receptacle");
  m_featureTableBoard["ETH"] = FeatureEntry("Ethernet", "<feature type=\"ETH\" n=\"1\" m=\"10000000\" name=\"RJ45 Receptacle\"/>", "1 x 10 Mbit/s RJ45 Receptacle");
  m_featureTableBoard["SPI"] = FeatureEntry("SPI", "<feature type=\"SPI\" n=\"1\" name=\"4-Pin Header, 1.27 mm Pitch\"/>", "1 x 4-Pin Header, 1.27 mm Pitch");
  m_featureTableBoard["I2C"] = FeatureEntry("I2C", "<feature type=\"I2C\" n=\"1\" name=\"2-Pin Header, 1.27 mm Pitch\"/>", "1 x 2-Pin Header, 1.27 mm Pitch");
  m_featureTableBoard["RS232"] = FeatureEntry("RS232", "<feature type=\"RS232\" n=\"1\" name=\"DB9 Male Connector\"/>", "1 x DB9 Male Connector");
  m_featureTableBoard["RS422"] = FeatureEntry("RS422", "<feature type=\"RS422\" n=\"1\" name=\"4-Pin Header, 1.27 mm Pitch\"/>", "1 x 4-Pin Header, 1.27 mm Pitch");
  m_featureTableBoard["RS485"] = FeatureEntry("RS485", "<feature type=\"RS485\" n=\"1\" name=\"DB9 Male Connector\"/>", "1 x DB9 Male Connector");
  m_featureTableBoard["CAN"] = FeatureEntry("CAN", "<feature type=\"CAN\" n=\"1\" name=\"DB9 Male Connector\"/>", "1 x DB9 Male Connector");
  m_featureTableBoard["IrDA"] = FeatureEntry("Diode", "<feature type=\"IrDA\" n=\"1\" name=\"Diode Transceiver\"/>", "1 x Diode Transceiver");
  m_featureTableBoard["LineIn"] = FeatureEntry("Line In", "<feature type=\"LineIn\" n=\"1\" name=\"TRS Audio Jack\"/>", "1 x TRS Audio Jack");
  m_featureTableBoard["LineOut"] = FeatureEntry("Line Out", "<feature type=\"LineOut\" n=\"1\" name=\"TRS Audio Jack\"/>", "1 x TRS Audio Jack");
  m_featureTableBoard["MIC"] = FeatureEntry("Microphone", "<feature type=\"MIC\" n=\"1\" name=\"TS Audio Jack (Mono)\"/>", "1 x TS Audio Jack (Mono)");
  m_featureTableBoard["Edge"] = FeatureEntry("Edge", "<feature type=\"Edge\" n=\"2\" m=\"24\"/>", "2 x 24 Pin Edge");
  m_featureTableBoard["ConnOther"] = FeatureEntry("Other Connector Type", "<feature type=\"ConnOther\" n=\"1\" name=\"My Other Connector\"/>", "1 x My Other Connector");
  m_featureTableBoard["Button"] = FeatureEntry("Push-buttons", "<feature type=\"Button\" n=\"3\" name=\"Push-buttons: Reset, Wake Up, User\"/>", "3 Push-buttons: Reset, Wake Up, User");
  m_featureTableBoard["Poti"] = FeatureEntry("Potentiometer", "<feature type=\"Poti\" n=\"1\"/>", "1 x Potentiometer");
  m_featureTableBoard["Joystick"] = FeatureEntry("Joystick", "<feature type=\"Joystick\" n=\"1\" name=\"5-position Joystick\"/>", "1 x 5-position Joystick");
  m_featureTableBoard["Touch"] = FeatureEntry("Touch Keys/Area", "<feature type=\"Touch\" n=\"1\"/>", "1 x Touch Keys/Area");
  m_featureTableBoard["ContOther"] = FeatureEntry("Other Control", "<feature type=\"ContOther\" n=1 name=\"My Other Control Feature\"/>", "1 x My Other Control Feature");
  m_featureTableBoard["Accelerometer"] = FeatureEntry("Accelerometer", "<feature type=\"Accelerometer\" n=\"1\" name=\"3-axis digital Accelerometer\"/>", "1 x 3-axis digital Accelerometer");
  m_featureTableBoard["Gyro"] = FeatureEntry("Gyroscope", "<feature type=\"Gyro\" n=\"1\" name=\"3-axis digital Gyroscope\"/>", "1 x 3-axis digital Gyroscope");
  m_featureTableBoard["Compass"] = FeatureEntry("Digital Compass", "<feature type=\"Compass\" n=\"1\" name=\"High-Precision Digital Compass\"/>", "1 x High-Precision Digital Compass");
  m_featureTableBoard["TempSens"] = FeatureEntry("Temperature Sensor", "<feature type=\"TempSens\" n=\"1\"/>", "1 x Temperature Sensor");
  m_featureTableBoard["PressSens"] = FeatureEntry("Pressure Sensor", "<feature type=\"PressSens\" n=\"1\"/>", "1 x Pressure Sensor");
  m_featureTableBoard["LightSens"] = FeatureEntry("Ambient Light Sensor", "<feature type=\"LightSens\" n=\"1\"/>", "1 x Ambient Light Sensor");
  m_featureTableBoard["SensOther"] = FeatureEntry("Other Sensor", "<feature type=\"SensOther\" n=1 name=\"My Other Sensor Feature\"/>", "1 x My Other Sensor Feature");
  m_featureTableBoard["CustomFF"] = FeatureEntry("Custom Formfactor", "<feature type=\"CustomFF\" n=\"54\" m=\"26\" name=\"40-Pin DIP with 0.1-inch Pitch\"/>", "54 mm x 26 mm, 40-Pin DIP with 0.1-inch Pitch");
  m_featureTableBoard["ArduinoFF"] = FeatureEntry("Arduino Formfactor", "<feature type=\"ArduinoFF\" n=\"1\"/>", "Arduino Formfactor");
  m_featureTableBoard["FreedomFF"] = FeatureEntry("Freedom Formfactor", "<feature type=\"FreedomFF\" n=\"1\"/>", "Freedom Formfactor");
  m_featureTableBoard["TowerFF"] = FeatureEntry("Tower Formfactor", "<feature type=\"TowerFF\" n=\"1\"/>", "Tower Formfactor");
  m_featureTableBoard["LED"] = FeatureEntry("LEDs", "<feature type=\"LED\" n=\"3\" name=\"Multicolor LEDs\"/>", "3 x Multicolor LEDs");
  m_featureTableBoard["Camera"] = FeatureEntry("Camera", "<feature type=\"Camera\" n=\"1\" name=\"Digital VGA Camera\"/>", "1 x Digital VGA Camera");
  m_featureTableBoard["LCD"] = FeatureEntry("LCD", "<feature type=\"LCD\" n=\"1\" m=\"16.40\" name=\"Segment LCD Controller\"/>", "1 x 16 x 40 Segment LCD Controller");
  m_featureTableBoard["GLCD"] = FeatureEntry("GLCD", "<feature type=\"GLCD\" n=\"1\" m=\"320.240\" name=\"2.4 inch Color TFT LCD with resistive touchscreen\"/>", "320 x 240 Pixel 2.4 inch Color TFT LCD with resistive touchscreen");
  m_featureTableBoard["Speaker"] = FeatureEntry("Speaker", "<feature type=\"Speaker\" n=\"1\"/>", "1 x Speaker");
  m_featureTableBoard["VirtualHW"] = FeatureEntry("VirtualHW", "<feature type=\"VirtualHW\"/>", "VirtualHW");
  m_featureTableBoard["Other"] = FeatureEntry("Other Feature", "<feature type=\"Other\" n=1 name=\"My Other Feature\"/>", "1 x My Other Feature");

  // OTHER --> other
  for(auto it = m_featureTableDevice.begin(); it != m_featureTableDevice.end(); it++) {
    string lowerName = it->first;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    m_featureTableDeviceLowerCase[lowerName] = it;
  }

  for(auto it = m_featureTableBoard.begin(); it != m_featureTableBoard.end(); it++) {
    string lowerName = it->first;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    m_featureTableBoardLowerCase[lowerName] = it;
  }
}

/**
 * @brief check device features
 * @param prop RteDeviceProperty features
 * @param devName device name for error reporting
 * @return passed / failed
 */
bool ValidateSyntax::CheckFeatureDevice(RteDeviceProperty* prop, const string& devName)
{
  const string& type = prop->GetAttribute("type");
  int lineNo = prop->GetLineNumber();

  string typeLower = type;
  std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

  FeatureEntry* existingFeature = nullptr;
  auto itexisting = m_featureTableDevice.find(type);
  if(itexisting != m_featureTableDevice.end()) {
    existingFeature = &itexisting->second;
  }

  bool ok = true;
  if(existingFeature) {
    LogMsg("M097", MCU(devName), SECTION("Board"), TYP(type), VAL("DESCR", existingFeature->defaultName));
  }
  else {
    ok = false;
    auto itexist = m_featureTableDeviceLowerCase.find(typeLower);
    if(itexist != m_featureTableDeviceLowerCase.end()) {
      existingFeature = &(itexist->second->second);
    }

    if(existingFeature) {
      LogMsg("M372", MCU(devName), SECTION("Device"), VAL("FEATURE", type), VAL("KNOWNFEATURE", itexist->second->first), VAL("DESCR", existingFeature->defaultName), lineNo);
    }
    else {
      LogMsg("M371", MCU(devName), SECTION("Device"), VAL("FEATURE", type), lineNo);
    }
  }

  if(ok) {
    LogMsg("M010");
  }

  return ok;
}

/**
 * @brief check board features
 * @param prop RteDeviceProperty features
 * @param boardName board name for error reporting
 * @return passed / failed
 */
bool ValidateSyntax::CheckFeatureBoard(RteItem* prop, const string& boardName)
{
  const string& type = prop->GetAttribute("type");
  int lineNo = prop->GetLineNumber();

  string typeLower = type;
  std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

  FeatureEntry* existingFeature = nullptr;
  auto itexisting = m_featureTableBoard.find(type);
  if(itexisting != m_featureTableBoard.end()) {
    existingFeature = &itexisting->second;
  }

  bool ok = true;
  if(existingFeature) {
    LogMsg("M097", MCU(boardName), SECTION("Board"), TYP(type), VAL("DESCR", existingFeature->defaultName));
  }
  else {
    ok = false;
    auto itexist = m_featureTableBoardLowerCase.find(typeLower);
    if(itexist != m_featureTableBoardLowerCase.end()) {
      existingFeature = &(itexist->second->second);
    }

    if(existingFeature) {
      LogMsg("M372", MCU(boardName), SECTION("Board"), VAL("FEATURE", type), VAL("KNOWNFEATURE", itexist->second->first), VAL("DESCR", existingFeature->defaultName), lineNo);
    }
    else {
      LogMsg("M371", MCU(boardName), SECTION("Board"), VAL("FEATURE", type), lineNo);
    }
  }

  if(ok) {
    LogMsg("M010");
  }

  return ok;
}

/**
 * @brief check property and add to list
 * @param prop RteDeviceProperty to add
 * @param propertiesMaps map to add to
 * @param devN device name for error reporting
 * @return passed / failed
 */
bool ValidateSyntax::CheckAddProperty(RteDeviceProperty* prop, map<string, map<string, RteDeviceProperty*>>& propertiesMaps, const string& devN)
{
  if(!prop) {
    return true;
  }

  string devName = devN;
  string Pname = prop->GetAttribute("Pname");
  int lineNo = prop->GetLineNumber();

  string id = CreateId(prop, devName);
  if(id.empty()) {
    return true;
  }

  if(!Pname.empty()) {
    devName += "::";
    devName += Pname;
  }

  // -----------  Common Property  -----------
  bool ok = true;
  auto propsExistingCommon = propertiesMaps.find(COMMON_PROCESSORS_STR);
  if(propsExistingCommon != propertiesMaps.end()) {
    map<string, RteDeviceProperty*>& propertiesCommon = propsExistingCommon->second;
    RteItem* existingPropCommon = 0;
    auto itexistingCommon = propertiesCommon.find(id);
    if(itexistingCommon != propertiesCommon.end()) existingPropCommon = itexistingCommon->second;
    if(existingPropCommon) {
      int line = existingPropCommon->GetLineNumber();
      LogMsg("M348", MCU(devName), LINE(line), VAL("PROP", id), lineNo);
      ok = false;
    }

    if(ok && Pname.empty()) {   // Add a common property
      propertiesCommon[id] = prop;
    }
  }

  // -----------  Pname Property  -----------
  if(!Pname.empty()) {
    auto propsExisting = propertiesMaps.find(Pname);
    if(propsExisting != propertiesMaps.end()) {
      map<string, RteDeviceProperty*>& properties = propsExisting->second;
      RteItem* existingProp = 0;
      auto itexisting = properties.find(id);
      if(itexisting != properties.end()) existingProp = itexisting->second;
      if(existingProp) {
        int line = existingProp->GetLineNumber();
        LogMsg("M348", MCU(devName), LINE(line), VAL("PROP", id), lineNo);
        ok = false;
      }

      if(ok) {        // Add a Pname property
        properties[id] = prop;
      }
    }
    else {
      LogMsg("M374", MCU(devN), VAL("CPU", Pname), lineNo);   // Pname not found
    }
  }

  // search in all lists for common feature
  if(Pname.empty()) {
    for(auto itPropList = propertiesMaps.begin(); itPropList != propertiesMaps.end(); itPropList++) {
      if(itPropList == propsExistingCommon) {
        continue;
      }

      auto propList = itPropList->second;
      RteItem* existProp = 0;
      auto itexist = propList.find(id);
      if(itexist != propList.end()) {
        existProp = itexist->second;
      }

      if(existProp) {
        int line = existProp->GetLineNumber();
        LogMsg("M348", MCU(devName), LINE(line), VAL("PROP", id), lineNo);
        ok = false;
      }
    }
  }

  int compare = VersionCmp::Compare(m_schemaVersion, "1.1");
  if(compare > 0) {
    const string& tag = prop->GetTag();
    if(tag == "feature") {
      if(!CheckFeatureDevice(prop, devName)) {
        ok = false;
      }
    }
  }

  return ok;
}

/**
 * @brief check and add all found properties to map (not just inheritance)
 * @param prop RteDeviceProperty to add
 * @param propertiesMaps map to add to
 * @param devN device name for error reporting
 * @return passed / failed
 */
bool ValidateSyntax::CheckAddPropertyAll(RteDeviceProperty* prop, map<string, map<string, RteDeviceProperty*>>& propertiesMaps, const string& devN)
{
  if(!prop) {
    return true;
  }

  string devName = devN;
  string Pname = prop->GetAttribute("Pname");
  int lineNo = prop->GetLineNumber();

  string id = CreateId(prop, devName);
  if(id.empty()) {
    return true;
  }

  if(!Pname.empty()) {
    devName += "::";
    devName += Pname;
  }

  // -----------  Common Property  -----------
  bool ok = true;
  auto propsExistingCommon = propertiesMaps.find(COMMON_PROCESSORS_STR);
  if(propsExistingCommon != propertiesMaps.end()) {
    map<string, RteDeviceProperty*>& propertiesCommon = propsExistingCommon->second;
    RteItem* existingPropCommon = 0;
    auto itexistingCommon = propertiesCommon.find(id);
    if(itexistingCommon != propertiesCommon.end()) {
      existingPropCommon = itexistingCommon->second;
    }

    if(existingPropCommon) {
      int line = existingPropCommon->GetLineNumber();
      LogMsg("M369", MCU(devName), LINE(line), VAL("PROP", id), lineNo);
      ok = false;
    }

    if(ok && Pname.empty()) {   // Add a common property
      propertiesCommon[id] = prop;
    }
  }

  // -----------  Pname Property  -----------
  if(!Pname.empty()) {
    auto propsExisting = propertiesMaps.find(Pname);
    if(propsExisting != propertiesMaps.end()) {
      map<string, RteDeviceProperty*>& properties = propsExisting->second;
      RteItem* existingProp = nullptr;

      auto itExisting = properties.find(id);
      if(itExisting != properties.end()) {
        existingProp = itExisting->second;
      }

      if(existingProp) {
        int lNo = existingProp->GetLineNumber();
        LogMsg("M369", MCU(devName), LINE(lNo), VAL("PROP", id), lineNo);
        ok = false;
      }

      if(ok && !Pname.empty()) {        // Add a Pname property
        properties[id] = prop;
      }
    }
  }

  // search in all lists for common feature
  if(Pname.empty()) {
    for(auto itPropList = propertiesMaps.begin(); itPropList != propertiesMaps.end(); itPropList++) {
      if(itPropList == propsExistingCommon) {
        continue;
      }

      auto propList = itPropList->second;
      RteItem* existProp = nullptr;
      auto itexist = propList.find(id);
      if(itexist != propList.end()) {
        existProp = itexist->second;
      }

      if(existProp) {
        int line = existProp->GetLineNumber();
        LogMsg("M369", MCU(devName), LINE(line), VAL("PROP", id), lineNo);
        ok = false;
      }
    }
  }

  return ok;
}

/**
 * @brief check and add found properties to map (using inheritance)
 * @param deviceItem RteDeviceItem to search for properties
 * @param prevPropertiesMaps map to add to
 * @return passed / failed
 */
bool ValidateSyntax::CheckDeviceProperties(RteDeviceItem* deviceItem, map<string, map<string, RteDeviceProperty*>>& prevPropertiesMaps)
{
  if(!deviceItem) {
    return true;
  }

  map<string, map<string, RteDeviceProperty*>> properties;
  map<string, map<string, RteDeviceProperty*>> allProperties = prevPropertiesMaps;   // copy from previous recursion step
  const string& devName = deviceItem->GetName();

  // Add property
  properties[COMMON_PROCESSORS_STR];
  if(allProperties.find(COMMON_PROCESSORS_STR) == allProperties.end()) {
    allProperties[COMMON_PROCESSORS_STR];
  }

  for (auto &[kProc, vProc] : deviceItem->GetProcessors()) {
    properties[kProc];

    if(allProperties.find(kProc) == allProperties.end()) {
      allProperties[kProc];
    }
  }

  bool ok = true;
  for(auto &[kProp, pg] : deviceItem->GetProperties()) {
    if(!pg) {
      continue;
    }

    for(auto propItem : pg->GetChildren()) {
      RteDeviceProperty* prop = dynamic_cast<RteDeviceProperty*>(propItem);
      if(!prop) {
        continue;
      }

      bool ret = CheckAddProperty(prop, properties, devName);
      if(!ret) {
        ok = false;
      }
      if(!CheckAddPropertyAll(prop, allProperties, devName)) {
        ok = false;
      }
    }
  }

  // sub items
  for(auto item : deviceItem->GetDeviceItems()) {
    if(!CheckDeviceProperties(item, allProperties)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief main function to check device properties
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckDeviceProperties(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteDeviceFamilyContainer* devices = pKg->GetDeviceFamiles();
  if(!devices) {
    return true;
  }

  bool ok = true;
  for(auto deviceItem : devices->GetChildren()) {
    RteDeviceItem* device = dynamic_cast<RteDeviceItem*>(deviceItem);
    if(!device) {
      continue;
    }

    map<string, map<string, RteDeviceProperty*>> allProperties;
    if(!CheckDeviceProperties(device, allProperties)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief check and add board properties
 * @param prop RteItem property to add
 * @param properties map to add to
 * @param boardName board name for error reporting
 * @return passed / failed
 */
bool ValidateSyntax::CheckAddBoardProperty(RteItem* prop, map<string, RteItem*>& properties, const string& boardName)
{
  if(!prop) {
    return true;
  }

  string id = CreateId(prop, boardName);
  if(id.empty()) {
    return true;
  }

  RteItem* existingProp = nullptr;
  auto itexisting = properties.find(id);
  if(itexisting != properties.end()) {
    existingProp = itexisting->second;
  }

  bool ok = true;
  if(existingProp) {
    LogMsg("M348", MCU(boardName), LINE(existingProp->GetLineNumber()), VAL("PROP", id), prop->GetLineNumber());
    ok = false;
  }

  if(VersionCmp::Compare(m_schemaVersion, "1.1") > 0) {
    const string& tag = prop->GetTag();
    if(tag == "feature") {
      if(!CheckFeatureBoard(prop, boardName)) {
        ok = false;
      }
    }
  }

  if(ok) {
    properties[id] = prop;
  }

  return ok;
}

/**
 * @brief check and add board properties
 * @param boardItem RteItem board to search for properties
 * @param prevProperties map to add to
 * @return passed / failed
 */
bool ValidateSyntax::CheckBoardProperties(RteItem* boardItem, map<string, RteItem*>& prevProperties)
{
  if(!boardItem) {
    return true;
  }

  map<string, RteItem*> properties;
  map<string, RteItem*> allProperties = prevProperties;   // copy from previous recursion step

  for(auto item : boardItem->GetChildren()) {
    if(!item) {
      continue;
    }

    const string& tag = item->GetTag();
    if(tag == "feature") {
      CheckAddBoardProperty(item, properties, item->GetName());
    }
  }

  // sub items
  bool ok = true;
  for(auto item : boardItem->GetChildren()) {
    if(!CheckBoardProperties(item, allProperties)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief check and add board properties
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckBoardProperties(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteBoardContainer* boards = dynamic_cast<RteBoardContainer*>(pKg->GetBoards());
  if(!boards) {
    return true;
  }

  bool ok = true;
  map<string, RteItem*> allProperties;
  for(auto item : boards->GetChildren()) {
    RteItem* boardItem = dynamic_cast<RteItem*>(item);
    if(!boardItem) {
      continue;
    }

    if(!CheckBoardProperties(boardItem, allProperties)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief check all devices
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckDevices(RtePackage* pKg)
{
  RteDeviceFamilyContainer* devices = pKg->GetDeviceFamiles();
  if(!devices) {
    return true;
  }

  map<string, RteItem*> properties;
  map<string, RteDeviceItem*> devicesList;

  bool ok = true;
  for(auto deviceItem : devices->GetChildren()) {
    RteDeviceItem* device = dynamic_cast<RteDeviceItem*>(deviceItem);
    if(!device) {
      continue;
    }

    if(!CheckDevicesMultiple(device, devicesList)) {
      ok = false;
    }
  }

  return ok;
}

/* Same family can be redefined, entries will be merged
 * Same subfamily can be redefined, entries will be merged
 * Same device or variant (if leaf) cannot be redefined
 */

/**
 * @brief check device hierarchy
 * @param parentItem
 * @param treeItems
 * @return passed / failed
 */
bool ValidateSyntax::CheckHierarchy(RteItem* parentItem, map<string, RteDeviceItem*>& treeItems)
{
  if(!parentItem) {
    return true;
  }

  map<string, RteDeviceItem*> hierarchyItems = treeItems;
  map<string, RteDeviceItem*> localItems;

  bool ok = true;
  for(auto childItem : parentItem->GetChildren()) {
    RteDeviceItem* item = dynamic_cast<RteDeviceItem*>(childItem);
    if(!item) {
      continue;
    }

    const string& devName = item->GetName();
    if(devName.empty()) {
      continue;
    }

    int lineNo = item->GetLineNumber();
    RteDeviceItem::TYPE type = item->GetType();

    // --------------------  Check Family and Subfamily for being empty  --------------------
    if(type == RteDeviceItem::FAMILY || type == RteDeviceItem::SUBFAMILY) {
      if(!item->GetDeviceItemCount()) {
        if(type == RteDeviceItem::FAMILY) {
          LogMsg("M359", VAL("FAMILY", devName), lineNo);
        }
        else if(type == RteDeviceItem::SUBFAMILY) {
          LogMsg("M360", VAL("SUBFAMILY", devName), lineNo);
        }
      }
    }

    m_allItems[devName].push_back(item);
    if(type == RteDeviceItem::DEVICE || type == RteDeviceItem::VARIANT) {
      m_allDevices[devName].push_back(item);
    }

    // --------------------  Check local items  --------------------
    auto itexisting = localItems.find(devName);
    if(itexisting != localItems.end()) {
      RteDeviceItem* existing = itexisting->second;
      RteDeviceItem::TYPE typeExisting = existing->GetType();
      LogMsg("M367", TYP(GetRteTypeString(typeExisting)), NAME(devName), LINE(existing->GetLineNumber()), lineNo);
      ok = false;
    }
    else {
      localItems[devName] = item;
    }

    // --------------------  enter Recursion  --------------------
    if(!CheckHierarchy(item, hierarchyItems)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief Global check of all items
 *        - subfamily shall be unique (global)
 *        - family, subfamily, device/variant (leaf) have the same name (global)
 *        - device and variant shall be unique (global)
 *        - something redefined
 * @return passed / failed
 */
bool ValidateSyntax::CheckAllItems()
{
  bool ok = true;

  for(auto &[name, itemsList] : m_allItems) {
    if(itemsList.size() > 1) {
      string text;
      for(auto item : itemsList) {
        RteDeviceItem::TYPE type = item->GetType();
        int lineNo = item->GetLineNumber();
        text += "\n  as '" + GetRteTypeString(type) + "' (Line " + to_string((unsigned long long)lineNo) + ")";
      }

      if(text.length()) {
        LogMsg("M391", NAME(name), MSG(text));
      }
    }
  }

  return ok;
}

/**
 * @brief check all devices gathered on list
 *        - subfamily shall be unique (global)
 *        - family, subfamily, device/variant (leaf) have the same name (global)
 *        - device and variant shall be unique (global)
 *        - something redefined
 * @return passed / failed
 */
bool ValidateSyntax::CheckAllDevices()
{
  bool ok = true;

  for(auto &[name, itemsList] : m_allDevices) {
    if(itemsList.size() > 1) {
      string text;
      for(auto item : itemsList) {
        RteDeviceItem::TYPE type = item->GetType();
        int lineNo = item->GetLineNumber();
        text += "\n  as '" + GetRteTypeString(type) + "' (Line " + to_string((unsigned long long)lineNo) + ")";
      }

      LogMsg("M392", NAME(name), MSG(text));
    }
  }

  return ok;
}

/**
 * @brief check devices hierarchy
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckHierarchy(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteItem* devices = pKg->GetDeviceFamiles();
  if(!devices) {
    return true;
  }

  bool ok = true;
  m_allItems.clear();
  m_allDevices.clear();
  map<string, RteDeviceItem*> treeIitems;

  if(!CheckHierarchy(devices, treeIitems)) {
    ok = false;
  }

  if(!CheckAllDevices()) {
    ok = false;
  }

  if(!CheckAllItems()) {
    ok = false;
  }

  return ok;
}

/**
 * @brief check if name is url string
 * @param filename name to check
 * @return true / false
 */
bool ValidateSyntax::IsURL(const string& filename)
{
  size_t pos = filename.find("://");
  if(pos == string::npos) {
    return false;
  }

  string prefix = filename.substr(0, pos);

  if(!AlnumCmp::CompareLen(prefix, "http", false)) {
    return true;
  }

  if(!AlnumCmp::CompareLen(prefix, "https", false)) {
    return true;
  }

  if(!AlnumCmp::CompareLen(prefix, "ftp", false)) {
    return true;
  }

  if(!AlnumCmp::CompareLen(prefix, "ftps", false)) {
    return true;
  }

  return false;
}

/**
 * @brief check, if referenced board is defined
 * @param example RteExample example project to test
 * @return passed / failed
 */
bool ValidateSyntax::CheckForBoard(RteExample* example)
{
  if(!example) {
    return true;
  }

  const RteItem* boardInfo = example->GetBoardInfoItem();
  if (!boardInfo) {
    return true;
  }
  const string& boardName = boardInfo->GetAttribute("name");
  const string& boardVendor = boardInfo->GetAttribute("vendor");
  const string& exampleName = example->GetName();
  const RteBoardMap& boardMap = GetModel().GetBoards();

  LogMsg("M062", VAL("EXAMPLE", exampleName), VAL("BOARD", boardName), VAL("VENDOR", boardVendor));

  bool ok = false;
  for(auto &[kBoard, vBoard] : boardMap) {
    RteBoard* board = dynamic_cast<RteBoard*>(vBoard);
    if(!board) {
      continue;
    }

    string name = board->GetName();
    string vendor = board->GetAttribute("vendor");

    if(!boardName.compare(name) && !boardVendor.compare(vendor)) {
      ok = true;
      break;
    }
  }

  if(!ok) {
    LogMsg("M324", VAL("EXAMPLE", exampleName), VAL("BOARD", boardName), VAL("VENDOR", boardVendor), example->GetLineNumber());
  }
  else {
    LogMsg("M010");
  }

  return ok;
}

/**
 * @brief check examples
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckExamples(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteItem* examples = pKg->GetExamples();
  if(!examples) {
    return true;
  }

  bool ok = true;
  for(auto child : examples->GetChildren()) {
    RteExample* example = dynamic_cast<RteExample*>(child);
    if(!example) {
      continue;
    }

    CheckForBoard(example);
  }

  return ok;
}

/**
 * @brief check and add a board to list
 * @param board RteBoard to add
 * @return passed / failed
 */
bool ValidateSyntax::CheckAddBoard(RteBoard* board)
{
  if(!board) {
    return true;
  }

  int lineNo = board->GetLineNumber();
  const string& name = board->GetID();

  bool ok = false;
  auto it = boardsFound.find(name);
  if(it != boardsFound.end()) {         // board already exists
    RteBoard* board2 = it->second;
    int lineNo2 = board2->GetLineNumber();
    const string& path = board2->GetPackage()->GetPackageFileName();

    LogMsg("M325", NAME(name), LINE(lineNo), PATH(path), lineNo2);
    ok = false;
  }
  else {
    boardsFound[name] = board;
  }

  return ok;
}

/**
 * @brief find examples for a board
 * @param board RteBoard to test
 * @return passed / failed
 */
bool ValidateSyntax::BoardFindExamples(RteBoard* board)
{
  if(!board) {
    return false;
  }

  int lineNo = board->GetLineNumber();
  const string& name = board->GetName();
  const string& vendor = board->GetAttribute("vendor");

  bool ok = false;
  for(auto packItem : GetModel().GetChildren()) {
    RtePackage* pKg = dynamic_cast<RtePackage*>(packItem);
    if(pKg == nullptr) {
      continue;
    }

    RteItem* examples = pKg->GetExamples();
    if(!examples) {
      continue;
    }

    for(auto child : examples->GetChildren()) {
      RteExample* example = dynamic_cast<RteExample*>(child);
      if(!example ||!example->GetBoardInfoItem()) {
        continue;
      }

      const RteItem* boardInfo = example->GetBoardInfoItem();
      const string& boardName = boardInfo->GetAttribute("name");
      const string& boardVendor = boardInfo->GetAttribute("vendor");

      if(!boardName.compare(name) && !boardVendor.compare(vendor)) {
        ok = true;
        break;
      }
    }
  }

  if(!ok) {
    LogMsg("M379", VAL("BOARD", name), VAL("VENDOR", vendor), lineNo);
  }

  return ok;
}

/**
 * @brief check all boards from package
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckBoards(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  int lineNo = -1;

  RteItem* boards = pKg->GetBoards();
  if(!boards) {
    return true;
  }

  bool ok = true;
  for(auto boardChild : boards->GetChildren()) {
    RteBoard* board = dynamic_cast<RteBoard*>(boardChild);
    if(!board) {
      continue;
    }

    // Check if board has already been defined
    CheckAddBoard(board);
    BoardFindExamples(board);

    const string& boardName = board->GetName();

    // ----------  mounted device(s)  --------------
    Collection<RteItem*> mountedDevices;
    board->GetMountedDevices(mountedDevices);

    if(mountedDevices.empty()) {
      LogMsg("M375", VAL("BOARD", boardName));
    }

    for(auto mountItem : mountedDevices) {
      RteItem* dev = dynamic_cast<RteItem*>(mountItem);
      if(!dev) {
        continue;
      }

      const string& dvendor = dev->GetAttribute("Dvendor");
      string dname = dev->GetAttribute("Dname");
      lineNo = dev->GetLineNumber();
      if(dname.empty()) {
        dname = dev->GetAttribute("DsubFamily");

        if(dname.empty()) {
          dname = dev->GetAttribute("Dfamily");
        }

        if(dname.empty()) {
          continue;
        }
      }

      LogMsg("M060", VAL("BOARD", boardName), VAL("DEVICE", dname));

      list<RteDevice*> devices;
      GetModel().GetDevices(devices, dname, dvendor);
      if(devices.empty()) {
        continue;
      }

      RteDevice* foundDevice = *devices.begin();
      const string foundDName = foundDevice->GetName();
      const string foundDVendor = foundDevice->GetVendorString();
      int lNo = foundDevice->GetLineNumber();
      LogMsg("M063", VENDOR(dvendor), MCU(dname), VENDOR2(foundDVendor), MCU2(foundDName), lNo);

      if(foundDVendor != dvendor) {
        LogMsg("M381", VENDOR(dvendor), MCU(dname), VENDOR2(foundDVendor), MCU2(foundDName), LINE(lNo), board->GetLineNumber());
      }

      if(devices.empty()) {
        LogMsg("M346", VAL("BOARD", boardName), VAL("DEVICE", dname), lineNo);
      }
      else {
        LogMsg("M010");
      }
    }

    // ------------  compatible devices  ------------------
    Collection<RteItem*> compatibleDevices;
    board->GetCompatibleDevices(compatibleDevices);

    for(auto device : compatibleDevices) {
      if(!device) {
        continue;
      }

      const string& dvendor = device->GetAttribute("Dvendor");
      string dname = device->GetAttribute("Dname");
      if(dname.empty()) {
        continue;
      }

      LogMsg("M060", VAL("BOARD", boardName), VAL("DEVICE", dname));

      list<RteDevice*> devices;
      GetModel().GetDevices(devices, dname, dvendor);
      if(devices.empty()) {
        LogMsg("M346", VAL("BOARD", boardName), VAL("DEVICE", dname), device->GetLineNumber());
        continue;
      }

      RteDevice* foundDevice = *devices.begin();
      const string foundDName = foundDevice->GetName();
      const string foundDVendor = foundDevice->GetVendorString();
      int lNo = foundDevice->GetLineNumber();
      LogMsg("M063", VENDOR(dvendor), MCU(dname), VENDOR2(foundDVendor), MCU2(foundDName), lNo);

      if(foundDVendor != dvendor) {
        LogMsg("M381", VENDOR(dvendor), MCU(dname), VENDOR2(foundDVendor), MCU2(foundDName), LINE(lNo), board->GetLineNumber());
      }

      LogMsg("M010");
    }
  }

  return ok;
}

/**
 * @brief check taxonomy section
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckTaxonomy(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteItem* taxonomy = pKg->GetTaxonomy();
  if(!taxonomy) {
    return true;
  }

  bool ok = true;
  string taxonomyId = taxonomy->GetGeneratorName();
  if(!taxonomyId.empty()) {
    for(auto description : taxonomy->GetChildren()) {
      if(!description) {
        continue;
      }

      const string& descClass = description->GetAttribute("Cclass");
      const string& descGroup = description->GetAttribute("Cgroup");
      const string& generatorId = description->GetAttribute("generator");

      // Check Generator ID if found
      if(!generatorId.empty()) {
        bool genFound = false;
        LogMsg("M093", VAL("GENID", generatorId), CCLASS(descClass), CGROUP(descGroup));

        auto genContainer = pKg->GetGenerators();
        if(!genContainer) {
          return false;
        }

        for(auto genChild : genContainer->GetChildren()) {
          RteGenerator* generator = dynamic_cast<RteGenerator*>(genChild);
          if(!generator) {
            continue;
          }

          const string& genName = generator->GetName();
          if(taxonomyId == genName) {
            genFound = true;
            break;
          }
        }

        if(!genFound) {
          int lineNo = taxonomy->GetLineNumber();
          LogMsg("M347", VAL("GENID", generatorId), CCLASS(descClass), CGROUP(descGroup), lineNo);
          ok = false;
        }
        if(ok) {
          LogMsg("M010");
        }
      }
    }
  }

  return ok;
}

/**
 * @brief check and add devices
 *        devices and Variants: if a device is a leaf (no variants), it gets added to list.
 *        devices(leafs) and variants can only occur once
 * @param deviceItem RteDeviceItem to check
 * @param devicesList map to add to
 * @param devName device name
 * @return passed / failed
 */
bool ValidateSyntax::CheckAddDevice(RteDeviceItem* deviceItem, map<string, RteDeviceItem*>& devicesList, const string& devName)
{
  if(!deviceItem) {
    return true;
  }

  bool ok = true;
  string id = devName;
  if(id.empty()) {
    return true;
  }

  RteDeviceItem* existingDeviceItem = 0;
  auto itexisting = devicesList.find(id);
  if(itexisting != devicesList.end()) {
    existingDeviceItem = itexisting->second;
  }

  RteDeviceItem::TYPE type = deviceItem->GetType();
  if(type == RteDeviceItem::DEVICE) {                 // check if there are variants, ignore if
    const list<RteDeviceItem*>& subItems = deviceItem->GetDeviceItems();
    if(!subItems.empty()) {
      return ok;
    }
  }

  if(existingDeviceItem) {
    int line = existingDeviceItem->GetLineNumber();
    RteDeviceItem::TYPE existingType = existingDeviceItem->GetType();

    string devType;
    if(type == RteDeviceItem::DEVICE) {
      devType = "Device";
    }
    else if(type == RteDeviceItem::VARIANT) {
      devType = "Variant";
    }

    string devTypeExisting;
    if(existingType == RteDeviceItem::DEVICE) {
      devTypeExisting = "Device";
    }
    else if(existingType == RteDeviceItem::VARIANT) {
      devTypeExisting = "Variant";
    }

    int lineNo = deviceItem->GetLineNumber();
    if(type == existingType) {
      LogMsg("M365", MCU(devName), VAL("DEVTYPE", devType), LINE(line), lineNo);
    }
    else {
      LogMsg("M366", MCU(devName), VAL("DEVTYPEEXIST", devTypeExisting), VAL("DEVTYPE", devType), LINE(line), lineNo);
    }
    ok = false;
  }
  else {
    devicesList[id] = deviceItem;
  }

  return ok;
}

/**
 * @brief check for multiple defined devices
 * @param deviceItem RteDeviceItem to test
 * @param newDevicesList list of devices
 * @return passed / failed
 */
bool ValidateSyntax::CheckDevicesMultiple(RteDeviceItem* deviceItem, const map<string, RteDeviceItem*>& newDevicesList)
{
  if(!deviceItem) {
    return true;
  }

  bool ok = true;
  for(auto item : deviceItem->GetDeviceItems()) {
    RteDeviceItem* devItem = item;
    if(!devItem) {
      continue;
    }

    const string& devName = devItem->GetName();
    RteDeviceItem::TYPE type = devItem->GetType();

    if(type == RteDeviceItem::DEVICE || type == RteDeviceItem::VARIANT) {
      CheckAddDevice(devItem, m_allDevicesList, devName);
    }

    if(!CheckDevicesMultiple(devItem, m_allDevicesList)) {
      ok = false;
    }
  }

  return ok;
}

/**
 * @brief check requirements section
 * @param pKg package under test
 * @return passed / failed
 */
bool ValidateSyntax::CheckRequirements(RtePackage* pKg)
{
  if(!pKg) {
    return true;
  }

  RteItem* requirements = pKg->GetRequirements();
  if(!requirements) {
    return true;
  }

  for(auto requirement : requirements->GetChildren()) {
    const string& tag = requirement->GetTag();
    if(tag == "packages") {
      CheckRequirements_Packages(requirement);
    }
  }

  return true;
}

/**
 * @brief check requirements for packages
 * @param requirement RteItem to test
 * @return passed / failed
 */
bool ValidateSyntax::CheckRequirements_Packages(RteItem* requirement)
{
  if(!requirement) {
    return true;
  }

  for(auto package : requirement->GetChildren()) {
    const string& name = package->GetName();
    const string& vendor = package->GetAttribute("vendor");
    const string& version = package->GetVersionString();

    bool found = false;
    Collection<RteItem*> foundPackages;
    for(auto pack : GetModel().GetChildren()) {
      RtePackage* pKg = dynamic_cast<RtePackage*>(pack);
      if(!pKg) {
        continue;
      }

      const string& pKgName = pKg->GetName();
      const string& pKgVendor = pKg->GetAttribute("vendor");
      const string& pKgVersion = pKg->GetVersionString();

      if(name != pKgName) {
        continue;
      }

      if(vendor != pKgVendor) {
        continue;
      }

      foundPackages.push_back(pKg);

      if(!VersionCmp::RangeCompare(pKgVersion, version)) {
        found = true;
        break;
      }
    }

    if(!found) {
      int lineNo = package->GetLineNumber();
      string msg;
      if(!foundPackages.empty()) {
        msg += "\n  Found Packs where version does not match:";

        int i = 0;
        for(auto foundPack : foundPackages) {
          RtePackage* pk = dynamic_cast<RtePackage*>(foundPack);
          if(!pk) {
            continue;
          }

          const string& pkName = pk->GetName();
          const string& pkVendor = pk->GetAttribute("vendor");
          const string& pkVersion = pk->GetVersionString();

          msg += "\n    ";
          msg += to_string(i++);
          msg += ": [";
          msg += pkVendor;
          msg += "] ";
          msg += pkName;
          msg += " ";
          msg += pkVersion;
        }
      }

      LogMsg("M382", TAG("package"), VENDOR(vendor), NAME(name), VAL("VER", version), MSG(msg), lineNo);
    }
  }

  return true;
}

