/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Cbuild.h"
#include "CbuildKernel.h"
#include "CbuildLayer.h"

using namespace std;


bool CreateRte(const CbuildRteArgs& args) {
  return CbuildKernel::Get()->Construct(args);
}

bool RunLayer(const int &cmd, const CbuildLayerArgs& args) {
  CbuildLayer clayer;
  switch (cmd) {
    case L_EXTRACT: return clayer.Extract (args);
    case L_COMPOSE: return clayer.Compose (args);
    case L_ADD:     return clayer.Add     (args);
    case L_REMOVE:  return clayer.Remove  (args);
  }
  return false;
}
