# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""TODO(pmeric): Write module docstring."""

_ALLOWED_ENDPOINT_TYPES = ("grpc", "http")

def endpoint(
        host,
        port,
        endpoint_type,
        docker_network = "",
        **kwargs):
    """Defines an SUT endpoint"""
    if endpoint_type not in _ALLOWED_ENDPOINT_TYPES:
        fail("endpoint type [", endpoint_type, "] not in", _ALLOWED_ENDPOINT_TYPES)
    return struct(
        host = host,
        port = port,
        docker_network = docker_network,
        endpoint_type = endpoint_type,
        **kwargs
    )
