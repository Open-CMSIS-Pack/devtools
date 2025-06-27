/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRRPCSERVER_H
#define PROJMGRRPCSERVER_H

#include <map>
#include <string>

/**
  * Forward declarations
*/
class ProjMgr;

/**
  * @brief projmgr rpc server
*/
class ProjMgrRpcServer {
public:
  /**
   * @brief class constructor
  */
  ProjMgrRpcServer(ProjMgr& manager);

  /**
   * @brief class destructor
  */
  ~ProjMgrRpcServer(void);

  /**
   * @brief run rpc server
   * @return true if terminated successfully, otherwise false
  */
  bool Run(void);

  /**
   * @brief get parser object
   * @return reference to m_manager
  */
  ProjMgr& GetManager() { return m_manager; };

  void SetDebug(bool debug) { m_debug = debug; }

  /**
   * @brief set m_shutdown flag
   * @param boolean value
  */
  void SetShutdown(bool value) { m_shutdown = value; }

  /**
   * @brief set m_contextLength flag
   * @param boolean value
  */
  void SetContentLengthHeader(bool value) { m_contextLength = value; }

  /**
   * @brief get request from stdin with content length header
   * @return string request
  */
  const std::string GetRequestFromStdinWithLength(void);

  /**
   * @brief get request from stdin
   * @param string request
  */
  const std::string GetRequestFromStdin(void);

protected:
  ProjMgr& m_manager;
  bool m_debug = false;
  bool m_shutdown = false;
  bool m_contextLength = false;
};

#endif  // PROJMGRRPCSERVER_H
