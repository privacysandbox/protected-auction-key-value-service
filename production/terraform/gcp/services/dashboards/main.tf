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
        "height": 20,
        "widget": {
          "title": "request.count [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/request.count\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "height": 20,
        "widget": {
          "title": "Secure lookup request count [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/SecureLookupRequestCount\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "height": 20,
        "widget": {
          "title": "request.failed_count_by_status [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/request.failed_count_by_status\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 20
      },
      {
        "height": 20,
        "widget": {
          "title": "request.duration_ms [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/request.duration_ms\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 20
      },
      {
        "height": 20,
        "widget": {
          "title": "request.size_bytes [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/request.size_bytes\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 40
      },
      {
        "height": 20,
        "widget": {
          "title": "response.size_bytes [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                       "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/response.size_bytes\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 40
      },
      {
        "height": 20,
        "widget": {
          "title": "Request Errors [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/KVUdfRequestError\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"error_code\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 60
      },
      {
        "height": 20,
        "widget": {
          "title": "Internal Request Errors [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/InternalLookupRequestError\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"error_code\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 60
      },
      {
        "height": 20,
        "widget": {
          "title": "Server Non-Request Errors [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/KVServerError\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"error_code\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 80
      },
      {
        "height": 20,
        "widget": {
          "title": "Sharded Lookup Key Count By Shard [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ShardedLookupKeyCountByShard\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 80
      },
      {
        "height": 20,
        "widget": {
          "title": "Sharded Lookup GetKeyValues Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ShardedLookupGetKeyValuesLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 100
      },
      {
        "height": 20,
        "widget": {
          "title": "Sharded Lookup GetKeyValueSet Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ShardedLookupGetKeyValueSetLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 100
      },
      {
        "height": 20,
        "widget": {
          "title": "Sharded Lookup RunQuery Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ShardedLookupRunQueryLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 120
      },
      {
        "height": 20,
        "widget": {
          "title": "Internal GetKeyValues Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/InternalGetKeyValuesLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 120
      },
      {
        "height": 20,
        "widget": {
          "title": "Internal GetKeyValueSet Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/InternalGetKeyValueSetLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 140
      },
      {
        "height": 20,
        "widget": {
          "title": "Internal RunQuery Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/InternalRunQueryLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 140
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache GetKeyValuePairs Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/GetValuePairsLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 160
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache GetKeyValueSet Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/GetKeyValueSetLatencyInMicros\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 160
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache Access Event Count [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/CacheAccessEventCount\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"cache_access\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 180
      },
      {
        "height": 20,
        "widget": {
          "title": "Server Retryable Operation Status Count [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/GetParameterStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/CompleteLifecycleStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/CreateDataOrchestratorStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/StartDataOrchestratorStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/LoadNewFilesStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/GetShardManagerStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/DescribeInstanceGroupInstancesStatus\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"status\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 180
      },
      {
        "height": 20,
        "widget": {
          "title": "File Update Stats [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsUpdatedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"!=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"data_source\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsDeletedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"!=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"data_source\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsDroppedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"!=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"data_source\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 200
      },
      {
        "height": 20,
        "widget": {
          "title": "Data Reader Latency [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ConcurrentStreamRecordReaderReadShardRecordsLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ConcurrentStreamRecordReaderReadStreamRecordsLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 200
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache UpdateKeyValue Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/UpdateKeyValueLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 220
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache UpdateKeyValueSet Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/UpdateKeyValueSetLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 220
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache DeleteKey Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/DeleteKeyLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 240
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache DeleteValuesInSet Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/DeleteValuesInSetLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 240
      },
      {
        "height": 20,
        "widget": {
          "title": "Cache RemoveDeletedKey Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/RemoveDeletedKeyLatency\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 260
      },
      {
        "height": 20,
        "widget": {
          "title": "Realtime Update Stats [MEAN]",
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
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsUpdatedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsDeletedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
                  }
                }
              },
              {
                "minAlignmentPeriod": "60s",
                "plotType": "LINE",
                "targetAxis": "Y1",
                "timeSeriesQuery": {
                  "timeSeriesFilter": {
                    "aggregation": {
                      "alignmentPeriod": "60s",
                      "perSeriesAligner": "ALIGN_RATE"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/TotalRowsDroppedInDataLoading\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\" metric.label.\"data_source\"=\"realtime\"",
                    "secondaryAggregation": {
                      "alignmentPeriod": "60s",
                      "crossSeriesReducer": "REDUCE_MEAN",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    }
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
        "yPos": 260
      },
      {
        "height": 20,
        "widget": {
          "title": "Realtime Message Processing Latency Microseconds [95TH PERCENTILE]",
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
                      "crossSeriesReducer": "REDUCE_PERCENTILE_95",
                      "groupByFields": [
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_DELTA"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/ReceivedLowLatencyNotifications\" resource.type=\"generic_task\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 280
      },
      {
        "height": 20,
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
                        "metric.label.\"label\"",
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
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
        "yPos": 280
      },
      {
        "height": 20,
        "widget": {
          "title": "system.memory.usage_kb [MEAN]",
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
                        "metric.label.\"label\"",
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.memory.usage_kb\" resource.type=\"generic_task\" metric.label.\"label\"=\"main process\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
                  }
                }
              },
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
                        "metric.label.\"label\"",
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
                      ],
                      "perSeriesAligner": "ALIGN_MEAN"
                    },
                    "filter": "metric.type=\"workload.googleapis.com/system.memory.usage_kb\" resource.type=\"generic_task\" metric.label.\"label\"=\"MemTotal:\" metric.label.\"deployment_environment\"=\"${var.environment}\" metric.label.\"service_name\"=\"kv-server\""
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
        "yPos": 300
      },
      {
        "height": 20,
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
                        "metric.label.\"Noise\"",
                        "metric.label.\"shard_number\"",
                        "metric.label.\"service_instance_id\""
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
        "xPos": 24,
        "yPos": 300
      }
    ]
  }
}
EOF
}
