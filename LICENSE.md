# License Terms

Copyright 2020-2021 Arm Limited. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Note

Individual files contain the following tag instead of the full license text.

SPDX-License-Identifier: Apache-2.0

This enables machine processing of license information based on the SPDX License Identifiers that are here available: http://spdx.org/licenses/

## Third Party Licenses

All files listed in the path `test/packs/ARM/RteTest_DFP/0.1.1/Device` are licensed under
[Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0) and are attributed to ARM Limited. The files originate
from https://github.com/ARM-software/CMSIS_5/tree/develop/Device/ARM.

## External Dependencies

The components listed below are not redistributed with the project but are used internally for building, development,
or testing purposes.

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

| Component | Version | License | Origin | Usage |
| --------- | ------- | ------- | ------ | ----- |
|Google C++ Testing Framework|1.11.0|[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)|https://github.com/google/googletest.git| Testing |
|cxxopts|2.2.1|[MIT](https://opensource.org/licenses/MIT)|https://github.com/jarro2783/cxxopts.git| packgen |
|yaml-cpp|0.7.0|[MIT](https://opensource.org/licenses/MIT)|https://github.com/jbeder/yaml-cpp.git| packgen |
|[GetGitRevisionDescription.cmake](./cmake/GetGitRevisionDescription.cmake)||[Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt)||Continuous integration|
|[GetGitRevisionDescription.cmake.in](./cmake/GetGitRevisionDescription.cmake.in)||[Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt)||Continuous integration|
|json|3.10.5|[MIT](https://opensource.org/licenses/MIT)|https://github.com/nlohmann/json| yml-schema-checker |
|json-schema-validator|2.1.0|[MIT](https://opensource.org/licenses/MIT)|https://github.com/pboettch/json-schema-validator| yml-schema-checker |

<!-- markdownlint-restore -->
