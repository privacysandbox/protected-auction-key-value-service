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


###############################################################
#
#                         Collector LB
#
# The internal lb uses HTTP/2 (gRPC) with no TLS.
###############################################################

resource "google_compute_backend_service" "collector" {
  name     = "${var.environment}-${var.collector_service_name}-service"
  provider = google-beta

  port_name             = "otlp"
  protocol              = "TCP"
  load_balancing_scheme = "INTERNAL_MANAGED"
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
  backend_service = google_compute_backend_service.collector.id
}

resource "google_compute_global_forwarding_rule" "collectors" {
  for_each = var.subnets

  name = "${var.environment}-${var.collector_service_name}-${each.value.region}-ilb-rule"

  ip_protocol           = "TCP"
  port_range            = var.collector_service_port
  load_balancing_scheme = "INTERNAL_MANAGED"
  target                = google_compute_target_tcp_proxy.collector.id
  subnetwork            = each.value.id

  labels = {
    service = var.collector_service_name
    region  = each.value.region
  }

  depends_on = [var.proxy_subnets]
}

resource "google_dns_record_set" "collector" {
  name         = "${var.environment}-${var.collector_service_name}.${var.collector_domain_name}."
  managed_zone = var.collector_dns_zone
  type         = "A"
  ttl          = 10
  routing_policy {
    dynamic "geo" {
      for_each = google_compute_global_forwarding_rule.collectors
      content {
        location = geo.value.labels.region
        rrdatas  = [geo.value.ip_address]
      }
    }
  }
}

resource "google_compute_health_check" "collector" {
  name = "${var.environment}-${var.collector_service_name}-lb-hc"

  grpc_health_check {
    port = var.collector_service_port
  }

  timeout_sec         = 3
  check_interval_sec  = 3
  healthy_threshold   = 2
  unhealthy_threshold = 4

  log_config {
    enable = true
  }
}
