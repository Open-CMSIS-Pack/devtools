/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file DeviceVendor.cpp
* @brief Device vendor matching
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "DeviceVendor.h"

#include "RteUtils.h"

using namespace std;

// static data members
std::string DeviceVendor::NO_VENDOR("NO_VENDOR:0");
std::string DeviceVendor::NO_MCU("NO_MCU");

map<string, string> DeviceVendor::m_vendorNameToId;
map<string, string> DeviceVendor::m_vendorIdToName;
map<string, string> DeviceVendor::m_vendorIdToId;


bool DeviceVendor::Match(const string& vendor1, const string& vendor2)
{
  if (vendor1 == vendor2)
    return true; // trivial common case

  string suffix1 = RteUtils::GetSuffix(vendor1);
  string suffix2 = RteUtils::GetSuffix(vendor2);
  if (!suffix1.empty() && !suffix2.empty()) {
    if (suffix1 == suffix2 ||
      VendorIDToOfficialID(suffix1) == VendorIDToOfficialID(suffix2)) {
      return true;
    }
  }
  string v1 = GetFullVendorString(vendor1);
  string v2 = GetFullVendorString(vendor2);

  return WildCards::Match(RteUtils::GetPrefix(v1), RteUtils::GetPrefix(v2));
}

const map<string, string>& DeviceVendor::GetVendorIdToIdMap()
{
  if (m_vendorIdToId.empty()) {
    m_vendorIdToId["97"] = "21"; // EnergyMicro -> Silicon Labs
    m_vendorIdToId["100"] = "19"; // Spansion -> Cypress
    m_vendorIdToId["114"] = "19"; // Fujitsu -> Cypress
    m_vendorIdToId["78"] = "11"; // Freescale -> NXP
  }
  return m_vendorIdToId;
}

const string& DeviceVendor::VendorIDToOfficialID(const string& vendorSuffix)
{
  GetVendorIdToIdMap();
  map<string, string>::const_iterator it = m_vendorIdToId.find(vendorSuffix);
  if (it != m_vendorIdToId.end())
    return it->second;
  return vendorSuffix;
}


const map<string, string>& DeviceVendor::GetVendorNameToIdMap()
{
  if (m_vendorNameToId.empty()) {
    m_vendorNameToId["NO_VENDOR"] = "0";
    m_vendorNameToId["3PEAK"] = "177";
    m_vendorNameToId["ABOV Semiconductor"] = "126";
    m_vendorNameToId["ABOV"] = "126";
    m_vendorNameToId["Acer Labs"] = "20";
    m_vendorNameToId["Actel"] = "56";
    m_vendorNameToId["Aeroflex UTMC"] = "34";
    m_vendorNameToId["ALi"] = "50";
    m_vendorNameToId["Altera"] = "85";
    m_vendorNameToId["Altium"] = "65";
    m_vendorNameToId["Ambiq Micro"] = "120";
    m_vendorNameToId["Analog Devices"] = "1";
    m_vendorNameToId["AnchorChips"] = "2";
    m_vendorNameToId["ARM"] = "82";
    m_vendorNameToId["ARM CMSIS"] = "109";
    m_vendorNameToId["ASIX Electronics Corporation"] = "81";
    m_vendorNameToId["Atmel"] = "3";
    m_vendorNameToId["Microchip"] = "3";
    m_vendorNameToId["Atmel Wireless & uC"] = "22";
    m_vendorNameToId["AustriaMicroSystems"] = "84";
    m_vendorNameToId["California Eastern Laboratories"] = "94";
    m_vendorNameToId["CAST, Inc."] = "55";
    m_vendorNameToId["Chipcon"] = "42";
    m_vendorNameToId["Cirrus Logic"] = "83";
    m_vendorNameToId["CML Microcircuits"] = "45";
    m_vendorNameToId["CORERIVER"] = "96";
    m_vendorNameToId["CSR"] = "118";
    m_vendorNameToId["Cybernetic Micro Systems"] = "29";
    m_vendorNameToId["CybraTech"] = "43";
    m_vendorNameToId["Cygnal Integrated Products"] = "60";
    m_vendorNameToId["Cypress"] = "19";
    m_vendorNameToId["Daewoo"] = "27";
    m_vendorNameToId["Dallas Semiconductor"] = "4";
    m_vendorNameToId["Dialog Semiconductor"] = "113";
    m_vendorNameToId["Digi International"] = "87";
    m_vendorNameToId["Digital Core Design"] = "58";
    m_vendorNameToId["Dolphin"] = "57";
    m_vendorNameToId["Domosys"] = "26";
    m_vendorNameToId["easyplug"] = "61";
    m_vendorNameToId["EM Microelectronic"] = "74";
    m_vendorNameToId["Ember"] = "98";
    m_vendorNameToId["Energy Micro"] = "21"; // 97
    m_vendorNameToId["EnOcean"] = "91";
    m_vendorNameToId["Evatronix"] = "64";
    m_vendorNameToId["Freescale"] = "11";// NXP
    m_vendorNameToId["Freescale Semiconductor"] = "11"; // NXP
    m_vendorNameToId["Freescale Semiconductors"] = "11"; // NXP
    m_vendorNameToId["Fujitsu"] = "19"; // now Cypess
    m_vendorNameToId["Fujitsu Semiconductor"] = "19"; // now Cypress
    m_vendorNameToId["Fujitsu Semiconductors"] = "19"; // now Cyrpess
    m_vendorNameToId["Generic"] = "5";
    m_vendorNameToId["Genesis Microchip"] = "53";
    m_vendorNameToId["GigaDevice"] = "123";
    m_vendorNameToId["Goal Semiconductor"] = "77";
    m_vendorNameToId["Goodix"] = "155";
    m_vendorNameToId["Handshake Solutions"] = "71";
    m_vendorNameToId["Hilscher"] = "88";
    m_vendorNameToId["Holtek"] = "106";
    m_vendorNameToId["Honeywell"] = "36";
    m_vendorNameToId["Hynix Semiconductor"] = "6";
    m_vendorNameToId["Hyundai"] = "35";
    m_vendorNameToId["Infineon"] = "7";
    m_vendorNameToId["InnovASIC"] = "38";
    m_vendorNameToId["Intel"] = "8";
    m_vendorNameToId["ISSI"] = "9";
    m_vendorNameToId["Kawasaki"] = "49";
    m_vendorNameToId["Kionix"] = "127";
    m_vendorNameToId["Lapis Semiconductor"] = "10";
    m_vendorNameToId["LAPIS Technology"] = "10";
    m_vendorNameToId["Luminary Micro"] = "76";
    m_vendorNameToId["Maxim"] = "23";
    m_vendorNameToId["MediaTek"] = "129";
    m_vendorNameToId["MegaChips"] = "128";
    m_vendorNameToId["Megawin"] = "70";
    m_vendorNameToId["Mentor Graphics Co."] = "24";
    m_vendorNameToId["Micronas"] = "30";
    m_vendorNameToId["Microsemi"] = "112";
    m_vendorNameToId["Milandr"] = "99";
    m_vendorNameToId["milandr"] = "99";
    m_vendorNameToId["MindMotion"] = "132";
    m_vendorNameToId["MXIC"] = "40";
    m_vendorNameToId["Myson Technology"] = "32";
    m_vendorNameToId["NetSilicon"] = "67";
    m_vendorNameToId["Nordic Semiconductor"] = "54";
    m_vendorNameToId["Nuvoton"] = "18";
    m_vendorNameToId["NXP"] = "11";
    m_vendorNameToId["NXP (founded by Philips)"] = "11";
    m_vendorNameToId["OKI SEMICONDUCTOR CO.,LTD."] = "108";
    m_vendorNameToId["onsemi"] = "141";
    m_vendorNameToId["ONSemiconductor"] = "141";
    m_vendorNameToId["Oregano Systems"] = "44";
    m_vendorNameToId["PalmChip"] = "105";
    m_vendorNameToId["Philips"] = "79";
    m_vendorNameToId["RadioPulse"] = "86";
    m_vendorNameToId["Ramtron"] = "41";
    m_vendorNameToId["Realtek"] = "124";
    m_vendorNameToId["Realtek Semiconductor"] = "124";
    m_vendorNameToId["Redpine Signals"] = "125";
    m_vendorNameToId["RDC Semiconductor"] = "73";
    m_vendorNameToId["ROHM"] = "103";
    m_vendorNameToId["Samsung"] = "47";
    m_vendorNameToId["Sanyo"] = "46";
    m_vendorNameToId["Shanghai Huahong IC"] = "66";
    m_vendorNameToId["Sharp"] = "39";
    m_vendorNameToId["Siemens"] = "25";
    m_vendorNameToId["Sigma Designs"] = "111";
    m_vendorNameToId["Silicon Labs"] = "21";
    m_vendorNameToId["Silicon Laboratories, Inc."] = "21";
    m_vendorNameToId["Siliconians"] = "28";
    m_vendorNameToId["SMSC"] = "33";
    m_vendorNameToId["Socle Technology Corp."] = "95";
    m_vendorNameToId["SONiX"] = "110";
    m_vendorNameToId["Spansion"] = "19"; // new name for Spansion, now Cypress
    m_vendorNameToId["SST"] = "12";
    m_vendorNameToId["ST"] = "13";
    m_vendorNameToId["STMicroelectronics"] = "13";
    m_vendorNameToId["Sunrise Micro Devices"] = "121";
    m_vendorNameToId["SyncMOS"] = "63";
    m_vendorNameToId["Synopsys"] = "37";
    m_vendorNameToId["Syntek Semiconductor Co., Ltd."] = "62";
    m_vendorNameToId["TDK"] = "75";
    m_vendorNameToId["Tekmos"] = "80";
    m_vendorNameToId["Temic"] = "15";
    m_vendorNameToId["Teridian Semiconductor Corp."] = "14";
    m_vendorNameToId["TI"] = "16";
    m_vendorNameToId["Texas Instruments"] = "16";
    m_vendorNameToId["Tezzaron Semiconductor"] = "68";
    m_vendorNameToId["Toshiba"] = "92";
    m_vendorNameToId["Triad Semiconductor"] = "104";
    m_vendorNameToId["Triscend"] = "17";
    m_vendorNameToId["Uniband Electronic Corp."] = "101";
    m_vendorNameToId["Vitesse"] = "72";
    m_vendorNameToId["Winbond"] = "93";
    m_vendorNameToId["WiNEDGE"] = "48";
    m_vendorNameToId["WIZnet"] = "102";
    m_vendorNameToId["Zensys"] = "59";
    m_vendorNameToId["Zilog"] = "89";
    m_vendorNameToId["Zylogic Semiconductor Corp."] = "69";
    m_vendorNameToId["Renesas"] = "117";
    m_vendorNameToId["AutoChips"] = "150";
  }
  return m_vendorNameToId;
}


const string& DeviceVendor::VendorNameToID(const string& vendorPrefix)
{
  GetVendorNameToIdMap(); // ensures m_vendorNameToId is filled
  map<string, string>::const_iterator it = m_vendorNameToId.find(vendorPrefix);
  if (it != m_vendorNameToId.end())
    return it->second;
  return RteUtils::EMPTY_STRING;
}

const map<string, string>& DeviceVendor::GetVendorIdToNameMap()
{
  if (m_vendorIdToName.empty()) {
    m_vendorIdToName["0"] = "NO_VENDOR";
    m_vendorIdToName["177"] = "3PEAK";
    m_vendorIdToName["126"] = "ABOV Semiconductor";
    m_vendorIdToName["20"] = "Acer Labs";
    m_vendorIdToName["56"] = "Actel";
    m_vendorIdToName["34"] = "Aeroflex UTMC";
    m_vendorIdToName["50"] = "ALi";
    m_vendorIdToName["85"] = "Altera";
    m_vendorIdToName["65"] = "Altium";
    m_vendorIdToName["120"] = "Ambiq Micro";
    m_vendorIdToName["1"] = "Analog Devices";
    m_vendorIdToName["2"] = "AnchorChips";
    m_vendorIdToName["82"] = "ARM";
    m_vendorIdToName["109"] = "ARM CMSIS";
    m_vendorIdToName["81"] = "ASIX Electronics Corporation";
    m_vendorIdToName["3"] = "Microchip";
    m_vendorIdToName["22"] = "Atmel Wireless & uC";
    m_vendorIdToName["84"] = "AustriaMicroSystems";
    m_vendorIdToName["94"] = "California Eastern Laboratories";
    m_vendorIdToName["55"] = "CAST, Inc.";
    m_vendorIdToName["42"] = "Chipcon";
    m_vendorIdToName["83"] = "Cirrus Logic";
    m_vendorIdToName["45"] = "CML Microcircuits";
    m_vendorIdToName["96"] = "CORERIVER";
    m_vendorIdToName["118"] = "CSR";
    m_vendorIdToName["29"] = "Cybernetic Micro Systems";
    m_vendorIdToName["43"] = "CybraTech";
    m_vendorIdToName["60"] = "Cygnal Integrated Products";
    m_vendorIdToName["19"] = "Cypress";
    m_vendorIdToName["27"] = "Daewoo";
    m_vendorIdToName["4"] = "Dallas Semiconductor";
    m_vendorIdToName["113"] = "Dialog Semiconductor";
    m_vendorIdToName["87"] = "Digi International";
    m_vendorIdToName["58"] = "Digital Core Design";
    m_vendorIdToName["57"] = "Dolphin";
    m_vendorIdToName["26"] = "Domosys";
    m_vendorIdToName["61"] = "easyplug";
    m_vendorIdToName["74"] = "EM Microelectronic";
    m_vendorIdToName["98"] = "Ember";
    m_vendorIdToName["97"] = "Silicon Labs";
    m_vendorIdToName["91"] = "EnOcean";
    m_vendorIdToName["64"] = "Evatronix";
    m_vendorIdToName["78"] = "NXP"; // former Freescale
    m_vendorIdToName["100"] = "Cypress"; // now Spansion is Cypress
    m_vendorIdToName["114"] = "Cypress"; // now Fujitsu is Cypress (was Spansion)
    m_vendorIdToName["5"] = "Generic";
    m_vendorIdToName["53"] = "Genesis Microchip";
    m_vendorIdToName["123"] = "GigaDevice";
    m_vendorIdToName["77"] = "Goal Semiconductor";
    m_vendorIdToName["155"] = "Goodix";
    m_vendorIdToName["71"] = "Handshake Solutions";
    m_vendorIdToName["88"] = "Hilscher";
    m_vendorIdToName["106"] = "Holtek";
    m_vendorIdToName["36"] = "Honeywell";
    m_vendorIdToName["6"] = "Hynix Semiconductor";
    m_vendorIdToName["35"] = "Hyundai";
    m_vendorIdToName["7"] = "Infineon";
    m_vendorIdToName["38"] = "InnovASIC";
    m_vendorIdToName["8"] = "Intel";
    m_vendorIdToName["9"] = "ISSI";
    m_vendorIdToName["49"] = "Kawasaki";
    m_vendorIdToName["127"] = "Kionix";
    m_vendorIdToName["10"] = "LAPIS Technology";
    m_vendorIdToName["76"] = "Luminary Micro";
    m_vendorIdToName["23"] = "Maxim";
    m_vendorIdToName["129"] = "MediaTek";
    m_vendorIdToName["128"] = "MegaChips";
    m_vendorIdToName["70"] = "Megawin";
    m_vendorIdToName["24"] = "Mentor Graphics Co.";
    m_vendorIdToName["30"] = "Micronas";
    m_vendorIdToName["112"] = "Microsemi";
    m_vendorIdToName["99"] = "Milandr";
    m_vendorIdToName["132"] = "MindMotion";
    m_vendorIdToName["40"] = "MXIC";
    m_vendorIdToName["32"] = "Myson Technology";
    m_vendorIdToName["67"] = "NetSilicon";
    m_vendorIdToName["54"] = "Nordic Semiconductor";
    m_vendorIdToName["18"] = "Nuvoton";
    m_vendorIdToName["11"] = "NXP";
    m_vendorIdToName["108"] = "OKI SEMICONDUCTOR CO.,LTD.";
    m_vendorIdToName["141"] = "onsemi";
    m_vendorIdToName["44"] = "Oregano Systems";
    m_vendorIdToName["105"] = "PalmChip";
    m_vendorIdToName["79"] = "Philips";
    m_vendorIdToName["86"] = "RadioPulse";
    m_vendorIdToName["41"] = "Ramtron";
    m_vendorIdToName["124"] = "Realtek Semiconductor";
    m_vendorIdToName["125"] = "Redpine Signals";
    m_vendorIdToName["73"] = "RDC Semiconductor";
    m_vendorIdToName["103"] = "ROHM";
    m_vendorIdToName["47"] = "Samsung";
    m_vendorIdToName["46"] = "Sanyo";
    m_vendorIdToName["66"] = "Shanghai Huahong IC";
    m_vendorIdToName["39"] = "Sharp";
    m_vendorIdToName["25"] = "Siemens";
    m_vendorIdToName["111"] = "Sigma Designs";
    m_vendorIdToName["21"] = "Silicon Labs";
    m_vendorIdToName["28"] = "Siliconians";
    m_vendorIdToName["33"] = "SMSC";
    m_vendorIdToName["95"] = "Socle Technology Corp.";
    m_vendorIdToName["110"] = "SONiX";
    m_vendorIdToName["12"] = "SST";
    m_vendorIdToName["13"] = "STMicroelectronics";
    m_vendorIdToName["121"] = "Sunrise Micro Devices";
    m_vendorIdToName["63"] = "SyncMOS";
    m_vendorIdToName["37"] = "Synopsys";
    m_vendorIdToName["62"] = "Syntek Semiconductor Co., Ltd.";
    m_vendorIdToName["75"] = "TDK";
    m_vendorIdToName["80"] = "Tekmos";
    m_vendorIdToName["15"] = "Temic";
    m_vendorIdToName["14"] = "Teridian Semiconductor Corp.";
    m_vendorIdToName["16"] = "Texas Instruments";
    m_vendorIdToName["68"] = "Tezzaron Semiconductor";
    m_vendorIdToName["92"] = "Toshiba";
    m_vendorIdToName["104"] = "Triad Semiconductor";
    m_vendorIdToName["17"] = "Triscend";
    m_vendorIdToName["101"] = "Uniband Electronic Corp.";
    m_vendorIdToName["72"] = "Vitesse";
    m_vendorIdToName["93"] = "Winbond";
    m_vendorIdToName["48"] = "WiNEDGE";
    m_vendorIdToName["102"] = "WIZnet";
    m_vendorIdToName["122"] = "WIZnet";
    m_vendorIdToName["59"] = "Zensys";
    m_vendorIdToName["89"] = "Zilog";
    m_vendorIdToName["69"] = "Zylogic Semiconductor Corp.";
    m_vendorIdToName["117"] = "Renesas";
    m_vendorIdToName["150"] = "AutoChips";
  }
  return m_vendorIdToName;
}


const string& DeviceVendor::VendorIDToName(const string& vendorSuffix)
{
  GetVendorIdToNameMap(); // ensures m_vendorNameToId is filled

  map<string, string>::const_iterator it = m_vendorIdToName.find(vendorSuffix);
  if (it != m_vendorIdToName.end())
    return it->second;
  return RteUtils::EMPTY_STRING;
}


bool DeviceVendor::IsCanonicalVendorName(const string& vendorName)
{
  const string& id = VendorNameToID(vendorName);
  if (id.empty())
    return false;
  const string& canonicalName = VendorIDToName(id);
  return vendorName == canonicalName;
}

string DeviceVendor::GetCanonicalVendorName(const string& vendor)
{
  string prefix = RteUtils::GetPrefix(vendor);
  string suffix = RteUtils::GetSuffix(vendor);
  if (suffix.empty()) {
    suffix = VendorNameToID(prefix);
  }
  if (!suffix.empty()) {
    const string& canonicalName = VendorIDToName(suffix);
    if (!canonicalName.empty()) {
      return canonicalName;
    }
  }
  return prefix;
}

string DeviceVendor::GetFullVendorString(const string& vendor)
{
  string prefix = RteUtils::GetPrefix(vendor);
  string suffix = RteUtils::GetSuffix(vendor);
  if (suffix.empty()) {
    suffix = VendorNameToID(prefix);
  } else {
    suffix = VendorIDToOfficialID(suffix);
  }
  if (!suffix.empty()) {
    const string& canonicalName = VendorIDToName(suffix);
    if (!canonicalName.empty()) {
      prefix = canonicalName;
      prefix += ":";
      prefix += suffix;
      return prefix;
    }
  }
  return vendor;
}

// End of DeviceVendor.cpp
