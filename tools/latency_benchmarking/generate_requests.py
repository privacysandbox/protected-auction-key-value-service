# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import csv
import argparse
import json
import base64

from pathlib import Path
from typing import Any, Iterator

"""
Generates JSON requests to be sent to the KV server V2 API.

For each N in a number-of-keys-list, it will
  * select N keys from the given snapshot.csv file
  * generate a request body with the list of keys
  * save request under <output_dir>/n=<N>/request.json

"""


def _BuildRequest(data: list[str]) -> dict[str, Any]:
    """Build the HTTP body that contains the base64 encoded request body as data."""
    arguments = []
    argument = {"tags": ["custom", "keys"], "data": data}
    arguments.append(argument)

    body = {
        "metadata": {"hostname": "example.com"},
        "partitions": [{"id": 0, "compressionGroupId": 0, "arguments": arguments}],
    }
    body_base64_string = base64.b64encode(json.dumps(body).encode()).decode()
    http_body = {"raw_body": {"data": body_base64_string}}
    return json.dumps(http_body)


def WriteRequests(
    keys: list[str],
    number_of_keys_list: list[int],
    output_dir: str,
) -> None:
    """Writes the requests to JSON files.

    Args:
      keys: List of all keys.
      number_of_keys_list: List of number of keys to use in request. One request will generated per item.
      output_dir: Base output dir to write to.
    """
    for n in number_of_keys_list:
        if n > len(keys):
            print(
                f"Warning: List of provided lookup keys ({len(keys)}) is smaller than number of keys to be included in request ({n}). Skipping...\n"
            )
            continue

        request = _BuildRequest(keys[:n])

        # Write to an output file at <output_dir>/n=<n>/request.json
        output_dir_n = os.path.join(output_dir, f"{n=}")
        Path(output_dir_n).mkdir(parents=True, exist_ok=True)
        with open(os.path.join(output_dir_n, "request.json"), "w") as f:
            f.write(request)


def _ReadCsv(snapshot_csv_file: str) -> Iterator[str]:
    with open(snapshot_csv_file, "r") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            yield row


def ReadKeys(snapshot_csv_file: str, max_number_of_keys: int) -> list[str]:
    """Read keys from CSV file. Only include update and string type mutations.

    Args:
      snapshot_csv_file: Path to snapshot CSV file.
      max_number_of_keys: Maximum number of keys to read.

    Returns:
      List of unique set of keys.
    """
    keys = set()
    for row in _ReadCsv(snapshot_csv_file):
        if row["value_type"].lower() != "string":
            continue
        if row["mutation_type"].lower() == "update":
            keys.add(row["key"])
            if len(keys) >= max_number_of_keys:
                break
    return list(keys)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--number-of-keys-list",
        dest="number_of_keys_list",
        type=int,
        nargs="+",
        help="List of number of keys to include in ghz request",
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        type=str,
        default="/tmp/latency_benchmarking",
        help="Output directory for benchmarks",
    )
    parser.add_argument(
        "--snapshot-csv-file",
        dest="snapshot_csv_file",
        default="snapshot.csv",
        help="Snapshot CSV file with KVMutation update entries.",
    )
    args = parser.parse_args()
    keys = ReadKeys(args.snapshot_csv_file, max(args.number_of_keys_list))
    WriteRequests(keys, args.number_of_keys_list, args.output_dir)


if __name__ == "__main__":
    Main()
