/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


resource "aws_cloudwatch_dashboard" "environment_dashboard" {
  dashboard_name = "${var.environment}-metrics"

  # https://docs.aws.amazon.com/AmazonCloudWatch/latest/APIReference/CloudWatch-Dashboard-Body-Structure.html
  dashboard_body = <<EOF
{
    "widgets": [
        {
            "height": 10,
            "width": 12,
            "y": 0,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"request.count\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]

                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "request.count per second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 0,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"SecureLookupRequestCount\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Secure lookup request count per second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 10,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"request.failed_count_by_status\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.error_code')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "request.failed_count_by_status per second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 10,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"request.duration_ms\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "request.duration_ms [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 20,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"request.size_bytes\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "request.size_bytes [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 20,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"response.size_bytes\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "response.size_bytes [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 30,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"KVUdfRequestError\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.error_code')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Request Errors Per Second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 30,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"InternalLookupRequestError\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.error_code')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Internal Request Errors Per Second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 40,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"KVServerError\" Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.error_code')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Server Non-request Errors [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 40,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=(\"ShardedLookupGetKeyValuesLatencyInMicros\" OR \"ShardedLookupGetKeyValueSetLatencyInMicros\" OR \"ShardedLookupRunQueryLatencyInMicros\") Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Sharded Lookup Latency Microseconds [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 50,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"ShardedLookupKeyCountByShard\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.key_shard_num')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Sharded Lookup Key Count By Shard Per Second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 50,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=(\"InternalGetKeyValuesLatencyInMicros\" OR \"InternalGetKeyValueSetLatencyInMicros\" OR \"InternalRunQueryLatencyInMicros\") Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Internal Lookup Latency Microseconds [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 60,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=(\"GetValuePairsLatencyInMicros\" OR \"GetKeyValueSetLatencyInMicros\") Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Cache Query Latency Microseconds [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 60,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"CacheAccessEventCount\" Noise=(\"Raw\" OR \"Noised\") generation_id=(\"consented\" OR \"not_consented\")', 'Sum', 60))", "id": "e1", "visible": false, "label": "$${PROP('Dim.Noise')} $${PROP('Dim.cache_access')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')} $${PROP('Dim.generation_id')}" } ],
                      [ { "expression": "e1 / 60"} ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Cache Access Event Count Per Second [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 70,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} status Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.status')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Server Retryable Operation Status Count [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 70,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} data_source data_source=(NOT \"realtime\") Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.data_source')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "File Update Stats [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 80,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=(\"ConcurrentStreamRecordReaderReadShardRecordsLatency\" OR \"ConcurrentStreamRecordReaderReadStreamRecordsLatency\" OR \"ConcurrentStreamRecordReaderReadByteRangeLatency\")  Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Data Reader Latency Microseconds[MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 80,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=(\"UpdateKeyValueLatency\" OR \"UpdateKeyValueSetLatency\" OR \"DeleteKeyLatency\" OR \"DeleteValuesInSetLatency\" OR \"RemoveDeletedKeyLatency\") Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('MetricName')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Cache Update Latency Microseconds[MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 90,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} data_source data_source=\"realtime\" Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Realtime Update Stats [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 90,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                      [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=${var.environment} MetricName=\"ReceivedLowLatencyNotifications\"  Noise=(\"Raw\" OR \"Noised\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "region": "${var.region}",
                "view": "timeSeries",
                "stacked": false,
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                },
                "title": "Realtime Message Processing Latency Microseconds[MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 100,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                    [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=\"${var.environment}\" Noise=(\"Raw\" OR \"Noised\") MetricName=\"system.cpu.percent\" label=(\"total utilization\" OR \"main process utilization\" OR \"total load\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.label')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "${var.region}",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                },
                 "title": "system.cpu.percent [MEAN]"
            }
        },
        {
            "height": 10,
            "width": 12,
            "y": 100,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                     [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=\"${var.environment}\" Noise=(\"Raw\" OR \"Noised\") MetricName=\"system.memory.usage_kb\" label=(\"MemTotal:\" OR \"main process\")', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.label')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "${var.region}",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                },
                "title": "system.memory.usage_kb [MEAN]"
            }
        },
        {
           "height": 10,
            "width": 12,
            "y": 110,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                    [ { "expression": "REMOVE_EMPTY(SEARCH('service.name=\"kv-server\" deployment.environment=\"${var.environment}\" Noise=(\"Raw\" OR \"Noised\") MetricName=\"system.cpu.percent\" label=\"total cpu cores\"', 'Average', 60))", "id": "e1", "label": "$${PROP('Dim.Noise')} $${PROP('Dim.label')} $${PROP('Dim.service.instance.id')} $${PROP('Dim.shard_number')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "${var.region}",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                },
                "title": "system.cpu.total_cores [MEAN]"
            }
        },
        {
           "height": 10,
            "width": 12,
            "y": 110,
            "x": 12,
            "type": "metric",
            "properties": {
                "metrics": [
                    [ "AWS/Lambda", "Errors", "FunctionName", "kv-server-${var.environment}-sqs-cleanup", { "id": "errors", "stat": "Sum", "color": "#d13212" } ],
                    [ ".", "Invocations", ".", ".", { "id": "invocations", "stat": "Sum", "visible": false } ],
                    [ { "expression": "100 - 100 * errors / MAX([errors, invocations])", "label": "Success rate (%)", "id": "availability", "yAxis": "right" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "${var.region}",
                "yAxis": {
                  "right": {
                      "max": 100
                  }
                },
                "title": "Sqs cleanup job error count and success rate (%)"
            }
        }
    ]
}
EOF
}
