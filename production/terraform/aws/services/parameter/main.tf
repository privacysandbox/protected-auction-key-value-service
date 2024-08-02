/**
 * Copyright 2022 Google LLC
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

resource "aws_ssm_parameter" "s3_bucket_parameter" {
  name      = "${var.service}-${var.environment}-data-bucket-id"
  type      = "String"
  value     = var.s3_bucket_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "bucket_update_sns_arn_parameter" {
  name      = "${var.service}-${var.environment}-data-loading-file-channel-bucket-sns-arn"
  type      = "String"
  value     = var.bucket_update_sns_arn_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "realtime_sns_arn_parameter" {
  name      = "${var.service}-${var.environment}-data-loading-realtime-channel-sns-arn"
  type      = "String"
  value     = var.realtime_sns_arn_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "launch_hook_parameter" {
  name      = "${var.service}-${var.environment}-launch-hook"
  type      = "String"
  value     = "${var.service}-${var.environment}-launch-hook"
  overwrite = true
}

resource "aws_ssm_parameter" "backup_poll_frequency_secs_parameter" {
  name      = "${var.service}-${var.environment}-backup-poll-frequency-secs"
  type      = "String"
  value     = var.backup_poll_frequency_secs_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "use_external_metrics_collector_endpoint" {
  name      = "${var.service}-${var.environment}-use-external-metrics-collector-endpoint"
  type      = "String"
  value     = var.use_external_metrics_collector_endpoint
  overwrite = true
}

resource "aws_ssm_parameter" "metrics_collector_endpoint" {
  count     = (var.use_external_metrics_collector_endpoint) ? 1 : 0
  name      = "${var.service}-${var.environment}-metrics-collector-endpoint"
  type      = "String"
  value     = var.metrics_collector_endpoint
  overwrite = true
}


resource "aws_ssm_parameter" "metrics_export_interval_millis_parameter" {
  name      = "${var.service}-${var.environment}-metrics-export-interval-millis"
  type      = "String"
  value     = var.metrics_export_interval_millis_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "metrics_export_timeout_millis_parameter" {
  name      = "${var.service}-${var.environment}-metrics-export-timeout-millis"
  type      = "String"
  value     = var.metrics_export_timeout_millis_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "telemetry_config_parameter" {
  name      = "${var.service}-${var.environment}-telemetry-config"
  type      = "String"
  value     = var.telemetry_config
  overwrite = true
}

resource "aws_ssm_parameter" "realtime_updater_num_threads_parameter" {
  name      = "${var.service}-${var.environment}-realtime-updater-num-threads"
  type      = "String"
  value     = var.realtime_updater_num_threads_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "data_loading_num_threads_parameter" {
  name      = "${var.service}-${var.environment}-data-loading-num-threads"
  type      = "String"
  value     = var.data_loading_num_threads_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "s3client_max_connections_parameter" {
  name      = "${var.service}-${var.environment}-s3client-max-connections"
  type      = "String"
  value     = var.s3client_max_connections_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "s3client_max_range_bytes_parameter" {
  name      = "${var.service}-${var.environment}-s3client-max-range-bytes"
  type      = "String"
  value     = var.s3client_max_range_bytes_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "num_shards_parameter" {
  name      = "${var.service}-${var.environment}-num-shards"
  type      = "String"
  value     = var.num_shards_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "udf_num_workers_parameter" {
  name      = "${var.service}-${var.environment}-udf-num-workers"
  type      = "String"
  value     = var.udf_num_workers_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "route_v1_requests_to_v2_parameter" {
  name      = "${var.service}-${var.environment}-route-v1-to-v2"
  type      = "String"
  value     = var.route_v1_requests_to_v2_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "add_missing_keys_v1_parameter" {
  name      = "${var.service}-${var.environment}-add-missing-keys-v1"
  type      = "String"
  value     = var.add_missing_keys_v1_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "use_real_coordinators_parameter" {
  name      = "${var.service}-${var.environment}-use-real-coordinators"
  type      = "String"
  value     = var.use_real_coordinators_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "primary_coordinator_account_identity_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-primary-coordinator-account-identity"
  type      = "String"
  value     = var.primary_coordinator_account_identity_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "secondary_coordinator_account_identity_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-secondary-coordinator-account-identity"
  type      = "String"
  value     = var.secondary_coordinator_account_identity_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "primary_coordinator_private_key_endpoint_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-primary-coordinator-private-key-endpoint"
  type      = "String"
  value     = var.primary_coordinator_private_key_endpoint_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "secondary_coordinator_private_key_endpoint_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-secondary-coordinator-private-key-endpoint"
  type      = "String"
  value     = var.secondary_coordinator_private_key_endpoint_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "primary_coordinator_region_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-primary-coordinator-region"
  type      = "String"
  value     = var.primary_coordinator_region_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "secondary_coordinator_region_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-secondary-coordinator-region"
  type      = "String"
  value     = var.secondary_coordinator_region_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "public_key_endpoint_parameter" {
  count     = (var.use_real_coordinators_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-public-key-endpoint"
  type      = "String"
  value     = var.public_key_endpoint_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "data_loading_file_format_parameter" {
  name      = "${var.service}-${var.environment}-data-loading-file-format"
  type      = "String"
  value     = var.data_loading_file_format_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "logging_verbosity_level_parameter" {
  name      = "${var.service}-${var.environment}-logging-verbosity-level"
  type      = "String"
  value     = var.logging_verbosity_level_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "logging_verbosity_update_sns_arn_parameter" {
  name      = "${var.service}-${var.environment}-logging-verbosity-update-sns-arn"
  type      = "String"
  value     = var.logging_verbosity_update_sns_arn_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "logging_verbosity_backup_poll_frequency_secs_parameter" {
  name      = "${var.service}-${var.environment}-logging-verbosity-backup-poll-frequency-secs"
  type      = "String"
  value     = var.logging_verbosity_backup_poll_frequency_secs_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "use_sharding_key_regex_parameter" {
  name      = "${var.service}-${var.environment}-use-sharding-key-regex"
  type      = "String"
  value     = var.use_sharding_key_regex_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "sharding_key_regex_parameter" {
  count     = (var.use_sharding_key_regex_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-sharding-key-regex"
  type      = "String"
  value     = var.sharding_key_regex_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "udf_timeout_millis_parameter" {
  name      = "${var.service}-${var.environment}-udf-timeout-millis"
  type      = "String"
  value     = var.udf_timeout_millis_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "udf_update_timeout_millis_parameter" {
  name      = "${var.service}-${var.environment}-udf-update-timeout-millis"
  type      = "String"
  value     = var.udf_update_timeout_millis_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "udf_min_log_level_parameter" {
  name      = "${var.service}-${var.environment}-udf-min-log-level"
  type      = "String"
  value     = var.udf_min_log_level_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "enable_otel_logger_parameter" {
  name      = "${var.service}-${var.environment}-enable-otel-logger"
  type      = "String"
  value     = var.enable_otel_logger_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "data_loading_blob_prefix_allowlist" {
  name      = "${var.service}-${var.environment}-data-loading-blob-prefix-allowlist"
  type      = "String"
  value     = var.data_loading_blob_prefix_allowlist
  overwrite = true
}

resource "aws_ssm_parameter" "consented_debug_token_parameter" {
  count     = (var.enable_consented_log_parameter_value) ? 1 : 0
  name      = "${var.service}-${var.environment}-consented-debug-token"
  type      = "String"
  value     = var.consented_debug_token_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "enable_consented_log_parameter" {
  name      = "${var.service}-${var.environment}-enable-consented-log"
  type      = "String"
  value     = var.enable_consented_log_parameter_value
  overwrite = true
}
