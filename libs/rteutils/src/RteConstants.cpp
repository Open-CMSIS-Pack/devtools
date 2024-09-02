/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteConstants.cpp
  * @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteConstants.h"
#include "RteUtils.h"

using namespace std;

const StrMap RteConstants::DeviceAttributesKeys = {
  { RTE_DFPU       , YAML_FPU               },
  { RTE_DDSP       , YAML_DSP               },
  { RTE_DMVE       , YAML_MVE               },
  { RTE_DENDIAN    , YAML_ENDIAN            },
  { RTE_DSECURE    , YAML_TRUSTZONE         },
  { RTE_DBRANCHPROT, YAML_BRANCH_PROTECTION },
};

const StrPairVecMap RteConstants::DeviceAttributesValues = {
  { RTE_DFPU       , {{ RTE_DP_FPU       , YAML_FPU_DP         },
                      { RTE_SP_FPU       , YAML_FPU_SP         },
                      { RTE_NO_FPU       , YAML_OFF            }}},
  { RTE_DDSP       , {{ RTE_DSP          , YAML_ON             },
                      { RTE_NO_DSP       , YAML_OFF            }}},
  { RTE_DMVE       , {{ RTE_FP_MVE       , YAML_MVE_FP         },
                      { RTE_MVE          , YAML_MVE_INT        },
                      { RTE_NO_MVE       , YAML_OFF            }}},
  { RTE_DENDIAN    , {{ RTE_ENDIAN_BIG   , YAML_ENDIAN_BIG     },
                      { RTE_ENDIAN_LITTLE, YAML_ENDIAN_LITTLE  }}},
  { RTE_DSECURE    , {{ RTE_SECURE       , YAML_TZ_SECURE      },
                      { RTE_SECURE_ONLY  , YAML_TZ_SECURE_ONLY },
                      { RTE_NON_SECURE   , YAML_TZ_NON_SECURE  },
                      { RTE_TZ_DISABLED  , YAML_OFF            }}},
  { RTE_DBRANCHPROT, {{ RTE_BTI          , YAML_BP_BTI         },
                      { RTE_BTI_SIGNRET  , YAML_BP_BTI_SIGNRET },
                      { RTE_NO_BRANCHPROT, YAML_OFF            }}},
};

const string& RteConstants::GetDeviceAttribute(const string& key, const string& value) {
  auto it = DeviceAttributesValues.find(key);
  if(it != DeviceAttributesValues.end()) {
    for(const auto& [rte, yaml] : it->second) {
      if(value == rte) {
        return yaml;
      } else if(value == yaml) {
        return rte;
      }
    }
  }
  return RteUtils::EMPTY_STRING;
}

// end of RteConstants.cpp
