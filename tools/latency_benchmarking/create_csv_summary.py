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
import argparse
import json
import pandas as pd

"""
Iterates through ghz_output.json files and collects relevant data in a CSV file.
"""


def _ExtractGhzInfo(ghz_output_df: pd.DataFrame) -> pd.DataFrame:
    keys = ["date", "count", "total", "average", "fastest", "slowest", "rps"]
    prefixes = ("tags", "statusCodeDistribution")
    return ghz_output_df.loc[
        :,
        ghz_output_df.columns.str.startswith(prefixes)
        | ghz_output_df.columns.isin(keys),
    ]


def JsonToDataFrame(ghz_result_dir: str) -> pd.DataFrame:
    """Reads a ghz_output.json files and outputs a dataframe with relevant columns.

    The dataframe will contain:
        - metadata: date, user tags added to the ghz command
        - overall stats: # requests, total time spent, rps, fastest, slowest
        - status code distributions

    Example:
                           date  count       total   average  fastest    slowest          rps  statusCodeDistribution.OK  statusCodeDistribution.Unavailable
        0  2024-01-30T21:27:06Z  13130  5000416406  37281592  6313913  100341707  2625.781322                      13021                                 109
        0  2024-01-30T21:27:06Z  13130  5000416406  37281592  6313913  100341707  2625.781322                      13021                                 109
        0  2024-01-30T21:27:06Z  13130  5000416406  37281592  6313913  100341707  2625.781322                      13021                                 109
    """
    output_dfs = []
    for root, dirs, files in os.walk(ghz_result_dir):
        for name in files:
            if name == "ghz_output.json":
                fp = os.path.join(root, name)
                try:
                    with open(fp, "r") as f:
                        json_data = json.loads(f.read())
                        ghz_output_df = pd.json_normalize(json_data)
                        output_dfs.append(_ExtractGhzInfo(ghz_output_df))
                except FileNotFoundError:
                    print(f"File not found: {fp}")
    return pd.concat(output_dfs)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ghz-result-dir",
        dest="ghz_result_dir",
        type=str,
        help="Directory containing subdirectories with ghz.json output files.",
    )
    args = parser.parse_args()
    output_df = JsonToDataFrame(args.ghz_result_dir)
    output_df.to_csv(os.path.join(args.ghz_result_dir, "summary.csv"), index=False)


if __name__ == "__main__":
    Main()
