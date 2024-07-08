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


output "app_mesh_name" {
  description = "The name of the app mesh."
  value       = data.aws_appmesh_mesh.existing_app_mesh.name
}

output "app_mesh_id" {
  description = "The ID of the app mesh."
  value       = data.aws_appmesh_mesh.existing_app_mesh.id
}

output "cloud_map_private_dns_namespace_id" {
  description = "ID of the cloud map namespace"
  value       = data.aws_service_discovery_dns_namespace.existing_cloud_map_private_dns_namespace.id
}

output "cloud_map_private_dns_namespace_name" {
  description = "Name of the cloud map namespace"
  value       = data.aws_service_discovery_dns_namespace.existing_cloud_map_private_dns_namespace.name
}
