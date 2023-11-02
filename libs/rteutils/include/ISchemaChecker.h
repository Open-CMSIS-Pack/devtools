/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ISCHEMA_CHECKER_H
#define ISCHEMA_CHECKER_H

#include "RteError.h"
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
