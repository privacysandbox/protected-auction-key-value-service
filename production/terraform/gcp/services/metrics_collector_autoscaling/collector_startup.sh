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


# Add an OpenTelemetry collector
wget https://github.com/open-telemetry/opentelemetry-collector-releases/releases/download/v0.81.0/otelcol-contrib_0.81.0_linux_amd64.deb
sudo dpkg -i otelcol-contrib_0.81.0_linux_amd64.deb
systemctl stop otelcol-contrib.service

# Configure OpenTelemetry collector for GCP monitory
# shellcheck disable=SC2154
cat > /etc/otelcol-contrib/config.yaml << COLLECTOR_CONFIG
receivers:
  otlp:
    protocols:
      grpc:
        endpoint: :${collector_port}

processors:
  batch:
  resourcedetection:
    detectors: [gcp]
    timeout: 10s

exporters:
  googlecloud:
    metric:
      resource_filters:
        # configures all resources to be passed on to GCP
        # https://github.com/open-telemetry/opentelemetry-collector-contrib/blob/main/exporter/googlecloudexporter/README.md
        - regex: .*
    log:
      default_log_name: kv-server-logs

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [googlecloud]
    metrics:
      receivers: [otlp]
      processors: [batch]
      exporters: [googlecloud]
    logs:
      receivers: [otlp]
      processors: [batch]
      exporters: [googlecloud]
COLLECTOR_CONFIG

systemctl start otelcol-contrib.service
