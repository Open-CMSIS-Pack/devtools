/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include <string>

struct TraceEvent;

namespace CsvRowMapper {

std::string header();
std::string row(const TraceEvent& event);

} // namespace CsvRowMapper
