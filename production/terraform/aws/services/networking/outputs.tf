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

output "vpc_id" {
  value = var.use_existing_vpc ? data.aws_vpc.existing_vpc[0].id : aws_vpc.vpc[0].id
}

output "public_subnet_ids" {
  value = var.use_existing_vpc ? data.aws_subnets.existing_public_subnet.ids : [for subnet in aws_subnet.public_subnet : subnet.id]
}

output "private_subnet_ids" {
  value = var.use_existing_vpc ? data.aws_subnets.existing_private_subnet.ids : [for subnet in aws_subnet.private_subnet : subnet.id]
}

output "private_route_table_ids" {
  value = var.use_existing_vpc ? data.aws_route_tables.existing_private_rt.ids : [for rt in aws_route_table.private_rt : rt.id]
}
