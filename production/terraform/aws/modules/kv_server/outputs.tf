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

output "kv_server_url" {
  value = "${var.enable_external_traffic ? "External url: ${module.load_balancing[0].kv_server_url}\n" : ""}${var.use_existing_vpc ? "Mesh virtual service name: ${module.mesh_service[0].virtual_service_name}" : ""}"
}
