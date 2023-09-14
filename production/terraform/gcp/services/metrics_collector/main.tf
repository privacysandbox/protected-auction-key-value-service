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

resource "google_compute_backend_service" "mesh_collector" {
  name     = "${var.environment}-${var.collector_service_name}-service"
  provider = google-beta

  port_name             = "otlp"
  protocol              = "TCP"
  load_balancing_scheme = "EXTERNAL"
  timeout_sec           = 10
  health_checks         = [google_compute_health_check.collector.id]

  dynamic "backend" {
    for_each = var.collector_instance_groups
    content {
      group           = backend.value
      balancing_mode  = "UTILIZATION"
      capacity_scaler = 1.0
    }
  }
}

resource "google_compute_target_tcp_proxy" "collector" {
  name            = "${var.environment}-${var.collector_service_name}-lb-proxy"
  backend_service = google_compute_backend_service.mesh_collector.id
}

resource "google_compute_global_forwarding_rule" "collector" {
  name     = "${var.environment}-${var.collector_service_name}-forwarding-rule"
  provider = google-beta

  ip_protocol           = "TCP"
  port_range            = var.collector_service_port
  load_balancing_scheme = "EXTERNAL"
  target                = google_compute_target_tcp_proxy.collector.id
  ip_address            = var.collector_ip_address

  labels = {
    environment = var.environment
    service     = var.collector_service_name
  }
}

resource "google_dns_record_set" "collector" {
  name         = "${var.environment}-${var.collector_service_name}.${var.collector_domain_name}."
  managed_zone = var.dns_zone
  type         = "A"
  ttl          = 10
  rrdatas = [
    var.collector_ip_address
  ]
}

resource "google_compute_health_check" "collector" {
  name = "${var.environment}-${var.collector_service_name}-lb-hc"

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
