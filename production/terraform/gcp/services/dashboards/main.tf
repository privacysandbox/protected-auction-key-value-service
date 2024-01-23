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

resource "google_monitoring_dashboard" "environment_dashboard" {
  dashboard_json = <<EOF
{
  "displayName": "${var.environment} KV Server Metrics",
  "mosaicLayout": {
    "columns": 48,
    "tiles": [
      {
        "height": 19,
        "widget": {
          "title": "system.cpu.total_cores [MEAN]",
          "xyChart": {
            "chartOptions": {},
            "dataSets": [
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"service_name\"",
                        "metric.label.\"deployment_environment\"",
                        "metric.label.\"shard_number\"",
                        "resource.label.\"task_id\"",
                        "metric.label.\"service_version\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.cpu.percent\" resource.type=\"generic_task\" metric.label.\"label\"=\"total cpu cores\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              }
            ],
            "yAxis": {
              "scale": "LINEAR"
            }
          }
        },
        "width": 24,
        "xPos": 0,
        "yPos": 0
      },
      {
        "height": 19,
        "widget": {
          "title": "system.cpu.percent [MEAN]",
          "xyChart": {
            "chartOptions": {},
            "dataSets": [
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"service_name\"",
                        "metric.label.\"deployment_environment\"",
                        "metric.label.\"label\"",
                        "metric.label.\"shard_number\"",
                        "resource.label.\"task_id\"",
                        "metric.label.\"service_version\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.cpu.percent\" resource.type=\"generic_task\" metric.label.\"label\"!=\"total cpu cores\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              }
            ],
            "yAxis": {
              "scale": "LINEAR"
            }
          }
        },
        "width": 24,
        "xPos": 24,
        "yPos": 0
      },
      {
        "height": 19,
        "widget": {
          "title": "system.memory.usage_kb for main process [MEAN]",
          "xyChart": {
            "chartOptions": {},
            "dataSets": [
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"service_name\"",
                        "metric.label.\"deployment_environment\"",
                        "metric.label.\"shard_number\"",
                        "resource.label.\"task_id\"",
                        "metric.label.\"service_version\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.memory.usage_kb\" resource.type=\"generic_task\" metric.label.\"label\"=\"main process\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              }
            ],
            "yAxis": {
              "scale": "LINEAR"
            }
          }
        },
        "width": 24,
        "xPos": 0,
        "yPos": 19
      },
      {
        "height": 19,
        "widget": {
          "title": "system.memory.usage_kb for MemAvailable: [MEAN]",
          "xyChart": {
            "chartOptions": {},
            "dataSets": [
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"service_name\"",
                        "metric.label.\"deployment_environment\"",
                        "metric.label.\"label\"",
                        "metric.label.\"shard_number\"",
                        "resource.label.\"task_id\"",
                        "metric.label.\"service_version\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.memory.usage_kb\" resource.type=\"generic_task\" metric.label.\"label\"=\"MemAvailable:\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              }
            ],
            "yAxis": {
              "scale": "LINEAR"
            }
          }
        },
        "width": 24,
        "xPos": 24,
        "yPos": 19
      }
    ]
  }
}
EOF
}
