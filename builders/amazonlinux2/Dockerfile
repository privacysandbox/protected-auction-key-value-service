# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# build command: `docker build builders/amazonlinux2 --tag builders/amazonlinux2:build_image`

FROM amazonlinux@sha256:c9ce7208912b7897c9a4cb273f20bbfd54fd745d1dd64f5e625fff6778469e69

COPY /install_yum_apps /scripts/

RUN  \
  /scripts/install_yum_apps --with-golang --with-gcc && \
  /scripts/install_go_apps --with-golang --with-gcc && \
  rm -rf /scripts

ENV PATH="${PATH}:/usr/local/go/bin"
