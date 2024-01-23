/**
 * Copyright 2024 Google LLC
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
resource "google_compute_backend_service" "default" {
  name                  = "${var.service}-${var.environment}-xlb-backend-service"
  provider              = google-beta
  port_name             = "envoy"
  protocol              = "HTTP2"
  load_balancing_scheme = "EXTERNAL_MANAGED"
  locality_lb_policy    = "ROUND_ROBIN"
  timeout_sec           = 10

  health_checks = [google_compute_health_check.grpc_non_tls.id]
  dynamic "backend" {
    for_each = var.instance_groups
    content {
      group           = backend.value
      balancing_mode  = "UTILIZATION"
      max_utilization = 0.80
      capacity_scaler = 1.0
    }
  }
  log_config {
    enable      = true
    sample_rate = 0.1
  }

  depends_on = [var.internal_load_balancer, var.grpc_route]
}

resource "google_compute_url_map" "default" {
  name            = "${var.service}-${var.environment}-xlb-grpc-map"
  default_service = google_compute_backend_service.default.id
}

resource "google_compute_target_https_proxy" "default" {
  name    = "${var.service}-${var.environment}-https-lb-proxy"
  url_map = google_compute_url_map.default.id
  ssl_certificates = [
    var.server_domain_ssl_certificate_id
  ]
}

resource "google_compute_global_forwarding_rule" "xlb_https" {
  name     = "${var.service}-${var.environment}-xlb-https-forwarding-rule"
  provider = google-beta

  ip_protocol           = "TCP"
  port_range            = "443"
  load_balancing_scheme = "EXTERNAL_MANAGED"
  target                = google_compute_target_https_proxy.default.id
  ip_address            = var.server_ip_address

  labels = {
    environment = var.environment
    service     = var.service
  }
}

resource "google_dns_record_set" "default" {
  name         = "${var.server_url}."
  managed_zone = var.server_dns_zone
  type         = "A"
  ttl          = 10
  rrdatas = [
    var.server_ip_address
  ]
}

resource "google_compute_health_check" "grpc_non_tls" {
  name = "${var.service}-${var.environment}-xlb-hc"
  grpc_health_check {
    port = var.service_port
  }

  timeout_sec         = 2
  check_interval_sec  = 2
  healthy_threshold   = 1
  unhealthy_threshold = 2

  log_config {
    enable = true
  }
}
