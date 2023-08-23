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

# TODO(b/298052952): Change "gcp" to "${var.environment}" once the gcp instance client is implemented.

resource "google_secret_manager_secret" "kvs_runtime_flag_secrets" {
  for_each = var.runtime_flags

  secret_id = "${var.service}-gcp-${each.key}"
  replication {
    automatic = true
  }
}

resource "google_secret_manager_secret_version" "kvs_runtime_flag_secret_values" {
  for_each = google_secret_manager_secret.kvs_runtime_flag_secrets

  secret      = each.value.id
  secret_data = var.runtime_flags[split("${var.service}-gcp-", each.value.id)[1]]
}
