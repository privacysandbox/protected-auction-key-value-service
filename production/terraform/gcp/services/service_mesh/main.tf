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

resource "google_network_services_mesh" "kv_server" {
  provider = google-beta
  name     = "${var.service}-${var.environment}-mesh"
}

resource "google_network_services_grpc_route" "kv_server" {
  provider  = google-beta
  name      = "${var.service}-${var.environment}-grpc-route"
  hostnames = [split("///", var.kv_server_address)[1]]
  meshes    = [google_network_services_mesh.kv_server.id]
  rules {
    action {
      destinations {
        service_name = "projects/${var.project_id}/locations/global/backendServices/${google_compute_backend_service.kv_server.name}"
      }
    }
  }
}

resource "google_compute_backend_service" "kv_server" {
  name                  = "${var.service}-${var.environment}-mesh-backend-service"
  provider              = google-beta
  port_name             = "grpc"
  protocol              = "GRPC"
  load_balancing_scheme = "INTERNAL_SELF_MANAGED"
  locality_lb_policy    = "ROUND_ROBIN"
  timeout_sec           = 10
  health_checks         = [google_compute_health_check.kv_server.id]

  dynamic "backend" {
    for_each = var.instance_groups
    content {
      group           = backend.value
      balancing_mode  = "UTILIZATION"
      max_utilization = 0.80
      capacity_scaler = 1.0
    }
  }
}

resource "google_compute_health_check" "kv_server" {
  name = "${var.service}-${var.environment}-mesh-hc"
  grpc_health_check {
    port_name = "grpc"
    port      = var.service_port
  }

  timeout_sec         = 3
  check_interval_sec  = 3
  healthy_threshold   = 2
  unhealthy_threshold = 4

  log_config {
    enable = true
  }
}
