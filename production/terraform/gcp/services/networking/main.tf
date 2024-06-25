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

resource "google_compute_network" "kv_server" {
  count                   = (var.use_existing_vpc) ? 0 : 1
  name                    = "${var.service}-${var.environment}-vpc"
  auto_create_subnetworks = false
}

resource "google_compute_subnetwork" "kv_server" {
  for_each = { for index, region in tolist(var.regions) : index => region }

  name          = "${var.service}-${var.environment}-${each.value}-subnet"
  network       = var.use_existing_vpc ? var.existing_vpc_id : google_compute_network.kv_server[0].id
  purpose       = "PRIVATE"
  region        = each.value
  ip_cidr_range = tolist(var.regions_cidr_blocks)[each.key]
}

resource "google_compute_subnetwork" "proxy_subnets" {
  for_each = { for index, region in tolist(var.regions) : index => region }

  ip_cidr_range = "10.${139 + each.key}.0.0/23"
  name          = "${var.service}-${var.environment}-${each.value}-collector-proxy-subnet"
  network       = var.use_existing_vpc ? var.existing_vpc_id : google_compute_network.kv_server[0].id
  purpose       = "GLOBAL_MANAGED_PROXY"
  region        = each.value
  role          = "ACTIVE"
  lifecycle {
    ignore_changes = [ipv6_access_type]
  }
}

resource "google_compute_router" "kv_server" {
  for_each = var.regions

  name    = "${var.service}-${var.environment}-${each.value}-router"
  network = var.use_existing_vpc ? var.existing_vpc_id : google_compute_network.kv_server[0].id
  region  = each.value
}

resource "google_compute_router_nat" "kv_server" {
  for_each = {
    for key, value in google_compute_router.kv_server : key => value
    if !contains(var.regions_use_existing_nat, value.region)
  }

  name                               = "${var.service}-${var.environment}-${each.value.region}-nat"
  router                             = each.value.name
  region                             = each.value.region
  nat_ip_allocate_option             = "AUTO_ONLY"
  source_subnetwork_ip_ranges_to_nat = "ALL_SUBNETWORKS_ALL_IP_RANGES"

  log_config {
    enable = true
    filter = "ERRORS_ONLY"
  }
}

resource "google_compute_global_address" "kv_server" {
  count      = var.enable_external_traffic ? 1 : 0
  name       = "${var.service}-${var.environment}-xlb-ip"
  ip_version = "IPV4"
}
