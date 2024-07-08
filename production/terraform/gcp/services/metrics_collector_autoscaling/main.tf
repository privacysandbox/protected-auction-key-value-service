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

#################################################################
#
#                         Collector
#
# The collector receives and forwards gRPC OpenTelemetry traffic.
#################################################################

resource "google_compute_instance_template" "collector" {
  for_each = var.subnets

  region      = each.value.region
  name        = "${var.environment}-${var.collector_service_name}-${each.value.region}-it-${substr(replace(uuid(), "/-/", ""), 0, 8)}"
  provider    = google-beta
  description = "This template is used to create an opentelemetry collector for the region."
  tags        = ["allow-otlp", "allow-hc", "allow-all-egress", ]

  disk {
    auto_delete  = true
    boot         = true
    device_name  = "persistent-disk-0"
    disk_type    = "pd-standard"
    mode         = "READ_WRITE"
    source_image = "debian-cloud/debian-11"
    type         = "PERSISTENT"
  }

  labels = {
    environment = var.environment
    service     = var.collector_service_name
  }

  network_interface {
    network    = var.vpc_id
    subnetwork = each.value.id
  }

  machine_type = var.collector_machine_type

  service_account {
    email  = var.service_account_email
    scopes = ["https://www.googleapis.com/auth/cloud-platform"]
  }
  metadata = {
    startup-script = templatefile(var.collector_startup_script_path, {
      collector_port = var.collector_service_port,
    })
  }

  lifecycle {
    create_before_destroy = true
    ignore_changes        = [name]
  }
}

resource "google_compute_region_instance_group_manager" "collector" {
  for_each = google_compute_instance_template.collector

  region = each.value.region
  name   = "${var.environment}-collector-${each.value.region}-mig"
  version {
    instance_template = each.value.id
    name              = "primary"
  }

  named_port {
    name = "otlp"
    port = var.collector_service_port
  }

  base_instance_name = "${var.collector_service_name}-${var.environment}"

  auto_healing_policies {
    health_check      = google_compute_health_check.collector.id
    initial_delay_sec = var.vm_startup_delay_seconds
  }

  update_policy {
    minimal_action  = "REPLACE"
    type            = "PROACTIVE"
    max_surge_fixed = max(10, var.max_replicas_per_service_region)
  }

  timeouts {
    create = "1h"
    delete = "1h"
    update = "1h"
  }
}

resource "google_compute_region_autoscaler" "collector" {
  for_each = google_compute_region_instance_group_manager.collector
  name     = "${var.environment}-${var.collector_service_name}-${each.value.region}-as"
  region   = each.value.region
  target   = each.value.id

  autoscaling_policy {
    max_replicas    = var.max_collectors_per_region
    min_replicas    = 1
    cooldown_period = var.vm_startup_delay_seconds

    cpu_utilization {
      target = var.cpu_utilization_percent
    }
  }
}


resource "google_compute_health_check" "collector" {
  name = "${var.environment}-${var.collector_service_name}-auto-heal-hc"

  tcp_health_check {
    port_name = "otlp"
    port      = var.collector_service_port
  }

  timeout_sec         = 3
  check_interval_sec  = 3
  healthy_threshold   = 2
  unhealthy_threshold = 4

  log_config {
    enable = true
  }
}
