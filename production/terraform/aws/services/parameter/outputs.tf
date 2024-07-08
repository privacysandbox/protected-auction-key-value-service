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

output "s3_bucket_parameter_arn" {
  value = aws_ssm_parameter.s3_bucket_parameter.arn
}

output "bucket_update_sns_arn_parameter_arn" {
  value = aws_ssm_parameter.bucket_update_sns_arn_parameter.arn
}

output "realtime_sns_arn_parameter_arn" {
  value = aws_ssm_parameter.realtime_sns_arn_parameter.arn
}

output "launch_hook_parameter_arn" {
  value = aws_ssm_parameter.launch_hook_parameter.arn
}

output "launch_hook_parameter_value" {
  value = aws_ssm_parameter.launch_hook_parameter.value
}

output "backup_poll_frequency_secs_parameter_arn" {
  value = aws_ssm_parameter.backup_poll_frequency_secs_parameter.arn
}

output "use_external_metrics_collector_endpoint_arn" {
  value = aws_ssm_parameter.use_external_metrics_collector_endpoint.arn
}

output "metrics_collector_endpoint_arn" {
  value = (var.use_external_metrics_collector_endpoint) ? aws_ssm_parameter.metrics_collector_endpoint[0].arn : ""
}

output "metrics_export_interval_millis_parameter_arn" {
  value = aws_ssm_parameter.metrics_export_interval_millis_parameter.arn
}

output "metrics_export_timeout_millis_parameter_arn" {
  value = aws_ssm_parameter.metrics_export_timeout_millis_parameter.arn
}

output "telemetry_config_parameter_arn" {
  value = aws_ssm_parameter.telemetry_config_parameter.arn
}

output "realtime_updater_num_threads_parameter_arn" {
  value = aws_ssm_parameter.realtime_updater_num_threads_parameter.arn
}

output "data_loading_num_threads_parameter_arn" {
  value = aws_ssm_parameter.data_loading_num_threads_parameter.arn
}

output "s3client_max_connections_parameter_arn" {
  value = aws_ssm_parameter.s3client_max_connections_parameter.arn
}

output "s3client_max_range_bytes_parameter_arn" {
  value = aws_ssm_parameter.s3client_max_range_bytes_parameter.arn
}

output "num_shards_parameter_arn" {
  value = aws_ssm_parameter.num_shards_parameter.arn
}

output "udf_num_workers_parameter_arn" {
  value = aws_ssm_parameter.udf_num_workers_parameter.arn
}

output "route_v1_requests_to_v2_parameter_arn" {
  value = aws_ssm_parameter.route_v1_requests_to_v2_parameter.arn
}

output "add_missing_keys_v1_parameter_arn" {
  value = aws_ssm_parameter.add_missing_keys_v1_parameter.arn
}

output "use_real_coordinators_parameter_arn" {
  value = aws_ssm_parameter.use_real_coordinators_parameter.arn
}

output "primary_coordinator_account_identity_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.primary_coordinator_account_identity_parameter[0].arn : ""
}

output "secondary_coordinator_account_identity_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.secondary_coordinator_account_identity_parameter[0].arn : ""
}

output "primary_coordinator_private_key_endpoint_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.primary_coordinator_private_key_endpoint_parameter[0].arn : ""
}

output "secondary_coordinator_private_key_endpoint_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.secondary_coordinator_private_key_endpoint_parameter[0].arn : ""
}

output "primary_coordinator_region_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.primary_coordinator_region_parameter[0].arn : ""
}

output "secondary_coordinator_region_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.secondary_coordinator_region_parameter[0].arn : ""
}

output "public_key_endpoint_parameter_arn" {
  value = (var.use_real_coordinators_parameter_value) ? aws_ssm_parameter.public_key_endpoint_parameter[0].arn : ""
}

output "data_loading_file_format_parameter_arn" {
  value = aws_ssm_parameter.data_loading_file_format_parameter.arn
}

output "logging_verbosity_level_parameter_arn" {
  value = aws_ssm_parameter.logging_verbosity_level_parameter.arn
}

output "use_sharding_key_regex_parameter_arn" {
  value = aws_ssm_parameter.use_sharding_key_regex_parameter.arn
}

output "sharding_key_regex_parameter_arn" {
  value = (var.use_sharding_key_regex_parameter_value) ? aws_ssm_parameter.sharding_key_regex_parameter[0].arn : ""
}

output "udf_timeout_millis_parameter_arn" {
  value = aws_ssm_parameter.udf_timeout_millis_parameter.arn
}

output "udf_update_timeout_millis_parameter_arn" {
  value = aws_ssm_parameter.udf_update_timeout_millis_parameter.arn
}

output "udf_min_log_level_parameter_arn" {
  value = aws_ssm_parameter.udf_min_log_level_parameter.arn
}

output "enable_otel_logger_parameter_arn" {
  value = aws_ssm_parameter.enable_otel_logger_parameter.arn
}

output "data_loading_blob_prefix_allowlist_parameter_arn" {
  value = aws_ssm_parameter.data_loading_blob_prefix_allowlist.arn
}

output "consented_debug_token_parameter_arn" {
  value = (var.enable_consented_log_parameter_value) ? aws_ssm_parameter.consented_debug_token_parameter[0].arn : ""
}

output "enable_consented_log_parameter_arn" {
  value = aws_ssm_parameter.enable_consented_log_parameter.arn
}
