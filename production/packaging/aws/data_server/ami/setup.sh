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
OTEL_COL_CONF=/opt/aws/aws-otel-collector/etc/otel_collector_config.yaml
sudo mkdir -p "$(dirname "${OTEL_COL_CONF}")"
sudo cp /home/ec2-user/otel_collector_config.yaml "${OTEL_COL_CONF}"
sudo cp /home/ec2-user/envoy_networking.sh /opt/privacysandbox/envoy_networking.sh
sudo cp /home/ec2-user/hc.bash /opt/privacysandbox/hc.bash
sudo cp /home/ec2-user/health.proto /opt/privacysandbox/health.proto
sudo chmod 555 /opt/privacysandbox/envoy_networking.sh
sudo chmod 555 /opt/privacysandbox/hc.bash
sudo chmod 555 /opt/privacysandbox/health.proto

# Install necessary dependencies
sudo yum update -y
sudo yum install -y docker
sudo yum localinstall -y /home/ec2-user/aws-otel-collector.rpm
sudo amazon-linux-extras install -y aws-nitro-enclaves-cli

sudo usermod -a -G docker ec2-user
sudo usermod -a -G ne ec2-user
sudo systemctl start docker
sudo systemctl enable docker
sudo docker pull envoyproxy/envoy-distroless:v1.24.1

sudo mkdir /etc/envoy
sudo chown ec2-user:ec2-user /etc/envoy

# Install grpcurl
cd /tmp
wget -q https://github.com/fullstorydev/grpcurl/releases/download/v1.9.1/grpcurl_1.9.1_linux_amd64.rpm
sudo rpm -i grpcurl_1.9.1_linux_amd64.rpm
