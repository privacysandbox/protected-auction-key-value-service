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

output "cloud_map_service_id" {
  description = "The ID of the service discovery service"
  value       = aws_service_discovery_service.cloud_map_service.id
}

output "virtual_node_name" {
  description = "The name of the virtual node."
  value       = aws_appmesh_virtual_node.appmesh_virtual_node.name
}

output "virtual_service_name" {
  description = "The name of the virtual service."
  value       = aws_appmesh_virtual_service.appmesh_virtual_service.name
}

output "cloud_map_service_name" {
  description = "The name of the service discovery service"
  value       = aws_service_discovery_service.cloud_map_service.name
}
