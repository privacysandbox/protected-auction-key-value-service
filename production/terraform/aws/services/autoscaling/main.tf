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

data "aws_default_tags" "current" {}

resource "aws_launch_template" "instance_launch_template" {
  name          = "${var.service}-${var.environment}-${var.shard_num}-instance-lt"
  image_id      = var.instance_ami_id
  instance_type = var.instance_type

  iam_instance_profile {
    arn = var.instance_profile_arn
  }

  vpc_security_group_ids = [
    var.instance_security_group_id
  ]

  enclave_options {
    enabled = true
  }

  user_data = base64encode(templatefile(
    "${path.module}/instance_init_script.tftpl",
    {
      enclave_memory_mib              = var.enclave_memory_mib,
      enclave_cpu_count               = var.enclave_cpu_count,
      enclave_enable_debug_mode       = "${var.enclave_enable_debug_mode ? "--debug-mode" : " "}"
      server_port                     = var.server_port,
      region                          = var.region,
      prometheus_service_region       = var.prometheus_service_region
      prometheus_workspace_id         = var.prometheus_workspace_id
      run_server_outside_tee          = var.run_server_outside_tee
      cloud_map_service_id            = var.cloud_map_service_id
      app_mesh_name                   = var.app_mesh_name
      virtual_node_name               = var.virtual_node_name
      healthcheck_interval_sec        = var.healthcheck_interval_sec
      healthcheck_timeout_sec         = var.healthcheck_timeout_sec
      healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
      healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold
      healthcheck_grace_period_sec    = var.healthcheck_grace_period_sec

  }))

  # Enforce IMDSv2.
  metadata_options {
    http_endpoint          = "enabled"
    http_tokens            = "required"
    instance_metadata_tags = "enabled"
  }

  tag_specifications {
    resource_type = "instance"
    tags = {
      Name        = "${var.service}-${var.environment}-${var.shard_num}-instance"
      service     = var.service
      environment = var.environment
      shard-num   = var.shard_num
    }
  }
}

# Create auto scaling group for EC2 instances
resource "aws_autoscaling_group" "instance_asg" {
  name                = "${var.service}-${var.environment}-${var.shard_num}-instance-asg"
  max_size            = var.autoscaling_max_size
  min_size            = var.autoscaling_min_size
  desired_capacity    = var.autoscaling_desired_capacity
  health_check_type   = "ELB"
  vpc_zone_identifier = var.autoscaling_subnet_ids
  target_group_arns   = var.target_group_arns

  launch_template {
    id      = aws_launch_template.instance_launch_template.id
    version = aws_launch_template.instance_launch_template.latest_version
  }

  dynamic "tag" {
    for_each = data.aws_default_tags.current.tags
    content {
      key                 = tag.key
      value               = tag.value
      propagate_at_launch = true
    }
  }

  instance_refresh {
    strategy = "Rolling"
  }

  initial_lifecycle_hook {
    name                 = var.launch_hook_name
    default_result       = "ABANDON"
    heartbeat_timeout    = 300
    lifecycle_transition = "autoscaling:EC2_INSTANCE_LAUNCHING"
  }

  wait_for_capacity_timeout = var.wait_for_capacity_timeout
}
