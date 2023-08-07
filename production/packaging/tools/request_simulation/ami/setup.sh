#!/bin/bash
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


set -o errexit
set -o pipefail

chmod 500 /home/ec2-user/request_simulation_docker_image.tar

sudo mkdir /opt/privacysandbox

sudo cp /home/ec2-user/request_simulation_docker_image.tar /opt/privacysandbox/request_simulation_docker_image.tar

# Install necessary dependencies
sudo yum update -y
sudo yum install -y docker

sudo usermod -a -G docker ec2-user
sudo systemctl start docker
sudo systemctl enable docker
