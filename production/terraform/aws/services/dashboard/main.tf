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
            "height": 9,
            "width": 9,
            "y": 0,
            "x": 9,
            "type": "metric",
            "properties": {
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} CacheKey', 'Average', 300)", "id": "e1", "period": 300, "label": "$${PROP('Dim.event')} $${PROP('Dim.service.instance.id')}" } ]
                ],
                "title": "Cache hits",
                "period": 300,
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "height": 9,
            "width": 9,
            "y": 0,
            "x": 0,
            "type": "metric",
            "properties": {
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,status,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} GetValuesSuccess', 'Average', 300)", "id": "e1", "period": 300, "label": "$${PROP('Dim.service.instance.id')}" } ]
                ],
                "title": "GetValuesSuccess",
                "period": 300,
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "height": 9,
            "width": 9,
            "y": 9,
            "x": 0,
            "type": "metric",
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} GetValuesV1Latency', 'Average', 1)", "id": "e1", "label": "$${PROP('Dim.service.instance.id')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 300,
                "title": "GetValuesV1Latency (nanoseconds)",
                "yAxis": {
                    "left": {
                        "label": "",
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 9,
            "y": 9,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} ConcurrentStreamRecordReader', 'Average', 300)", "id": "e1", "period": 300 } ],
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} AwsSqsReceiveMessageLatency', 'Average', 300)", "id": "e2", "period": 300 } ],
                    [ { "expression": "SEARCH('{KV-Server,OTelLib,deployment.environment,event,host.arch,service.instance.id,service.name,service.version,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} SeekingInputStreambuf', 'Average', 300)", "id": "e3", "period": 300 } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 300,
                "title": "Read latency",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 0,
            "y": 18,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} total cores', 'Average', 60)", "id": "e1", "period": 60 } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 60,
                "title": "system.cpu.total_cores[MEAN]",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 9,
            "y": 18,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} main process utilization', 'Average', 60)", "id": "e1", "period": 60 } ],
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} total utilization', 'Average', 60)", "id": "e2", "period": 60 } ],
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} total load', 'Average', 60)", "id": "e3", "period": 60 } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 60,
                "title": "system.cpu.percent[MEAN]",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 0,
            "y": 27,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} system.memory.usage main process', 'Average', 60)", "id": "e1", "period": 60, "label": "$${PROP('Dim.service.instance.id')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 60,
                "title": "system.memory.usage_kb for main process[MEAN]",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 9,
            "y": 27,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                    [ { "expression": "SEARCH('{KV-Server,Noise,deployment.environment,host.arch,label,service.instance.id,service.name,service.version,shard_number,telemetry.sdk.language,telemetry.sdk.name,telemetry.sdk.version} ${var.environment} system.memory.usage MemAvailable', 'Average', 60)", "id": "e1", "period": 60, "label": "$${PROP('Dim.service.instance.id')}" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "stat": "Average",
                "period": 60,
                "title": "system.memory.usage_kb for MemAvailable[MEAN]",
                "yAxis": {
                    "left": {
                        "showUnits": false,
                        "min": 0
                    }
                }
            }
        },
        {
            "type": "metric",
            "x": 0,
            "y": 36,
            "width": 9,
            "height": 9,
            "properties": {
                "metrics": [
                     [ { "expression": "SELECT COUNT(ChangeNotifierErrors) FROM SCHEMA(\"KV-Server\", Noise,\"deployment.environment\",error_code,\"host.arch\",\"service.instance.id\",\"service.name\",\"service.version\",shard_number,\"telemetry.sdk.language\",\"telemetry.sdk.name\",\"telemetry.sdk.version\") WHERE \"deployment.environment\" = '${var.environment}' GROUP BY error_code, \"service.instance.id\"", "label": "error rate", "id": "m1", "stat": "Average", "visible": false } ],
                     [ { "expression": "DIFF(m1)", "label": "error count", "id": "e1" } ]
                ],
                "view": "timeSeries",
                "stacked": false,
                "region": "us-east-1",
                "title": "Change notifier error count",
                "period": 60,
                "stat": "Average",
                "yAxis": {
                    "left": {
                        "min": 0,
                        "showUnits": false
                    }
                }
            }
        }
    ]
}
EOF
}
