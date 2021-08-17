#ifndef RteExample_H
#define RteExample_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteExample.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteItem.h"
#include "RteFile.h"

/**
 * @brief class presenting an example project with related files and properties
 * Children contain project files for different environments
*/
class RteExample : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteExample(RteItem* parent);
  /**
   * @brief virtual destructor
  */
  virtual ~RteExample() override;

  /**
   * @brief get collection of keywords in example meta-data
   * @return set of strings
  */
  const std::set<std::string>& GetKeywords() const { return m_keywords; }
  /**
   * @brief get collection of categories in example meta-data
   * @return set of strings
  */
  const std::set<std::string>& GetCategories() const { return m_categories; }
  /**
   * @brief get information about board for which the example is created
   * @return board information as about RteAttributes
  */
  const RteAttributes& GetBoardInfo() const { return m_board; }

public:
  /**
   * @brief check if example meta-data contain the specified keyword
   * @param keyword keyword string to search for
   * @return true if found
  */
  bool HasKeyword(const std::string& keyword) const;
  /**
   * @brief check if example meta-data contain all specified keywords
   * @param keywords collections of keyword to search for
   * @return true if all supplied keywords are found
  */
  bool HasKeywords(const std::set<std::string>& keywords) const;
  /**
   * @brief check if example meta-data contain the specified category string
   * @param category category string to search for
   * @return true if found
  */
  bool HasCategory(const std::string& category) const;
  /**
   * @brief check if example meta-data contain all specified categories
   * @param categories collections of categories to search for
   * @return true if all supplied categories are found
  */
  bool HasCategories(const std::set<std::string>& categories) const;

  /**
   * @brief clear internal data and call base class implementation
  */
  virtual void Clear() override;

  /**
   * @brief validate item after construction
   * @return true if valid
  */
  virtual bool Validate() override;

  /**
   * @brief get name to display to user
   * @return example name
  */
  virtual std::string GetDisplayName() const override { return GetName(); }

  /**
   * @brief get example vendor
   * @return vendor string
  */
  virtual const std::string& GetVendorString() const override;
  /**
   * @brief get example version
   * @return version string
  */
  virtual const std::string& GetVersionString() const override;

  /**
   * @brief get project pathname to load in the specified development environment
   * @param env development environment name
   * @return project pathname for specified environment if supported, empty string otherwise
  */
  virtual const std::string& GetLoadPath(const std::string& env);

  /**
   * @brief get attribute value specified for given development environment
   * @param env development environment name
   * @param attribute attribute name
   * @return attribute value if found, empty string otherwise
  */
  virtual const std::string& GetEnvironmentAttribute(const std::string& env, const std::string& attribute); // Get attribute std::string of an environment

protected:
  /**
   * @brief construct example ID
   * @return example ID string constructed out of name, board name and board vendor
  */
  virtual std::string ConstructID() override;

  /**
    * @brief process a single XMLTreeElement during construction
    * @param xmlElement pointer to XMLTreeElement to process
    * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;

  RteAttributes m_board; // development board the example refers to
  std::set<std::string> m_keywords; // example keywords
  std::set<std::string> m_categories; // example categories
  std::list<RteAttributes*> m_componentAttributes; // component attributes of a component this example refers
  // Note: RteItem::m_children contain project files for different environments
};
/**
 * @brief alpha-numerically sorted map of name to RteExample pointer pairs
*/
typedef std::map<std::string, RteExample*, AlnumCmp::LenLessNoCase > RteExampleMap;

/**
 * @brief class representing <examples> section of pdsc file
*/
class RteExampleContainer : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteExampleContainer(RteItem* parent);
  /**
    * @brief process a single XMLTreeElement during construction
    * @param xmlElement pointer to XMLTreeElement to process
    * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
};

#endif // RteExample_H
