/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SCHEMACHECKERERROR_H
#define SCHEMACHECKERERROR_H

#include "SchemaError.h"

#include <nlohmann/json-schema.hpp>

#include <string>

using nlohmann::json;

class CustomErrorHandler : public nlohmann::json_schema::basic_error_handler
{
public:
  CustomErrorHandler(const std::string& filePath);
  ~CustomErrorHandler();

  /**
   * @brief callback to process error data
   * @param pointer to json erroneous schema
   * @param erroneous instance
   * @param error message
  */
  void error(const nlohmann::json::json_pointer& ptr,
    const json& instance, const std::string& message) override;

  /**
   * @brief get list of all errors
   * @return list of all errors
  */
  SchemaErrors& GetAllErrors();

private:
  std::string  m_yamlFile;
  SchemaErrors m_errList;
};

#endif // SCHEMACHECKERERROR_H
