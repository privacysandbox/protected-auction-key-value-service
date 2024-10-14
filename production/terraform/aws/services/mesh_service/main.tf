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

data "aws_appmesh_mesh" "existing_app_mesh" {
  name = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-app-mesh"
}

data "aws_service_discovery_dns_namespace" "existing_cloud_map_private_dns_namespace" {
  name = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-cloud-map-private-dns-namespace"
  type = "DNS_PRIVATE"
}


resource "aws_service_discovery_service" "cloud_map_service" {
  name = "${var.service}-${var.environment}-cloud-map-service.${var.root_domain}"

  dns_config {
    namespace_id = data.aws_service_discovery_dns_namespace.existing_cloud_map_private_dns_namespace.id

    dns_records {
      ttl  = 10
      type = "A"
    }
  }
  health_check_custom_config {
    failure_threshold = 1
  }

  # Ensure all cloud map entries are deleted.
  force_destroy = true
}

resource "aws_appmesh_virtual_node" "appmesh_virtual_node" {
  name      = "${var.service}-${var.environment}-appmesh-virtual-node"
  mesh_name = data.aws_appmesh_mesh.existing_app_mesh.id
  spec {
    listener {
      port_mapping {
        port     = var.service_port
        protocol = "grpc"
      }

      health_check {
        protocol            = "grpc"
        healthy_threshold   = var.healthcheck_healthy_threshold
        unhealthy_threshold = var.healthcheck_unhealthy_threshold
        timeout_millis      = var.healthcheck_timeout_sec * 1000
        interval_millis     = var.healthcheck_interval_sec * 1000
      }
    }

    service_discovery {
      aws_cloud_map {
        service_name   = aws_service_discovery_service.cloud_map_service.name
        namespace_name = data.aws_service_discovery_dns_namespace.existing_cloud_map_private_dns_namespace.name
      }
    }
  }
}

resource "aws_appmesh_virtual_service" "appmesh_virtual_service" {
  name      = "${var.service}-${var.environment}-appmesh-virtual-service.${var.root_domain}"
  mesh_name = data.aws_appmesh_mesh.existing_app_mesh.name
  spec {
    provider {
      virtual_node {
        virtual_node_name = aws_appmesh_virtual_node.appmesh_virtual_node.name
      }
    }
  }
}

resource "aws_route53_record" "mesh_node_record" {
  name = aws_appmesh_virtual_service.appmesh_virtual_service.name
  type = "A"
  // In seconds
  ttl     = 300
  zone_id = var.root_domain_zone_id
  // Any non-loopback IP will do, this record just needs to exist, not go anywhere (should be overrifed by appmesh).
  records = ["10.10.10.10"]
}

data "aws_iam_policy_document" "virtual_node_policy_document" {
  statement {
    actions = [
      "appmesh:StreamAggregatedResources"
    ]
    resources = [
      aws_appmesh_virtual_node.appmesh_virtual_node.arn
    ]
  }
}

resource "aws_iam_policy" "app_mesh_node_policy" {
  name   = format("%s-%s-virtualNodePolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.virtual_node_policy_document.json
}

resource "aws_iam_role_policy_attachment" "app_mesh_node_policy_to_ec2_attachment" {
  role       = var.server_instance_role_name
  policy_arn = aws_iam_policy.app_mesh_node_policy.arn
}

resource "aws_iam_role_policy_attachment" "amazon_ec2_container_registry_read_only_to_ec2_attachment" {
  role       = var.server_instance_role_name
  policy_arn = "arn:aws:iam::aws:policy/AmazonEC2ContainerRegistryReadOnly"
}
