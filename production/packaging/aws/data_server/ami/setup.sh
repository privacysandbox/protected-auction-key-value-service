#!/bin/bash
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

set -o errexit
set -o pipefail

chmod 500 /home/ec2-user/proxy
chmod 500 /home/ec2-user/server_enclave_image.eif

sudo mkdir /opt/privacysandbox
mkdir /tmp/proxy

sudo cp /home/ec2-user/vsockproxy.service /etc/systemd/system/vsockproxy.service
sudo cp /home/ec2-user/proxy /opt/privacysandbox/proxy
sudo cp /home/ec2-user/server_enclave_image.eif /opt/privacysandbox/server_enclave_image.eif

# Install necessary dependencies
sudo yum update -y
sudo yum install -y docker
sudo amazon-linux-extras install -y aws-nitro-enclaves-cli

sudo usermod -a -G docker ec2-user
sudo usermod -a -G ne ec2-user

# Install Socat which needs gcc
sudo yum install -y gcc
wget http://www.dest-unreach.org/socat/download/socat-1.7.4.3.tar.gz -O /tmp/socat.tar.gz
readonly EXPECTED_CHECKSUM=d697245144731423ddbbceacabbd29447089ea223e9a439b28f9ff90d0dd216e
echo 'Checking checksum of the socat download'
if ! echo "${EXPECTED_CHECKSUM} /tmp/socat.tar.gz" |sha256sum --check --status
then
  echo 'Downloaded socat with incorrect checksum'
  exit 1
fi
tar xzf /tmp/socat.tar.gz -C /tmp
cd /tmp/socat-1.7.4.3
./configure
make
sudo make install
