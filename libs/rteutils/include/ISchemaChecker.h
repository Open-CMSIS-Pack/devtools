/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ISCHEMA_CHECKER_H
#define ISCHEMA_CHECKER_H

#include "RteError.h"
#include "RteUtils.h"
#include <list>

/**
 * @brief Abstract schema validator interface class
*/
class ISchemaChecker
{
public:
  /**
   * @brief virtual destructor
  */
  virtual ~ISchemaChecker() {}

  /**
   * @brief Validates a file against supplied schema
   * @param fileName file to validate
   * @param schemaFile file schema
   * @return true if successful
  */
  virtual bool ValidateFile(const std::string& file, const std::string& schemaFile) = 0;

  /**
   * @brief Validates a file against schema obtained by FindSchema() method
   * @param fileName file to validate
   * @return true if successful
  */
  virtual bool Validate(const std::string& file) { return ValidateFile(file, FindSchema(file)); }

  /**
   * @brief Finds schema for given file to validate
   * @param fileName file to validate
   * @return schema file name if found, empty string otherwise
  */
  virtual std::string FindSchema(const std::string& file) const { return RteUtils::EMPTY_STRING; }

  /**
   * @brief returns collection of error messages found during last run of validate
  */
  const std::list<RteError>& GetErrors() const { return m_errors; }

  /**
   * @brief clears error collection
  */
  void ClearErrors() { m_errors.clear(); }
protected:
  std::list<RteError> m_errors;
};

#endif // ISCHEMA_CHECKER_H
