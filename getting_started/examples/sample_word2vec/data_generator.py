# Copyright 2023 Google LLC
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

from gensim import downloader
from gensim import models

import nltk
from nltk.corpus import stopwords
from scipy.cluster import hierarchy
from scipy.spatial import distance
from typing import Optional

import argparse
import collections
import csv
import datetime
import json
import math
import os
import re


"""
Generates CSV files that can be used by the data cli.

Generated files will be:
  * categories.csv - Key->Set data of categories of words
  * embeddings.csv - Key->Value data of words to an embedding vector.

All data will be generated from publicly available word2vec embeddings.
"""


def _PickCategoryName(model, words) -> str:
    """Find the word in words closest to the average of the words."""
    average_word = model.get_mean_vector(words)
    closest_val = None
    closest_word = None
    for word in words:
        dist = distance.cosine(average_word, model.get_vector(word))
        if closest_val is None or dist < closest_val:
            closest_val = dist
            closest_word = word
    return closest_word


def _CreateHierarchy(model, linkage_matrix, words, n_clusters) -> dict[str, list[str]]:
    """Cluster words by category and related words."""
    clusters = hierarchy.fcluster(linkage_matrix, n_clusters, criterion="maxclust")

    # Create a dictionary of root word to child words for each group
    word_clusters = collections.defaultdict(set)

    for word, cluster_id in zip(words, clusters):
        word_clusters[cluster_id].add(word)

    # Get the closest to average of every cluster
    hierarchy_dict = {}
    for cluster in word_clusters.values():
        category = _PickCategoryName(model, cluster)
        hierarchy_dict[category.lower()] = [word for word in cluster]
    return hierarchy_dict


def _SelectWords(model, num_words) -> list[str]:
    """Choose words from those available in the model based off of simple criteria."""
    nltk.download("stopwords")
    words = []
    stopwords_set = set(stopwords.words())
    for k in model.key_to_index:
        if len(k) < 3:
            continue
        if k.lower() in stopwords_set:
            continue
        if re.match("^[a-zA-Z]+$", k) is None:
            continue
        # exclude words that are a subset of another word
        should_add = True
        for word in words:
            if k in word or word in k:
                should_add = False
                break
        if should_add:
            words.append(k)
        if len(words) >= num_words:
            break
    return words


def GenerateCategoryDict(
    model: models.KeyedVectors, num_words: int, num_categories: int
) -> dict[str, list[str]]:
    """Create a hiearchy of category to list of related words.

    num_words are selected from the model and organized into num_categories.
    Category names are chosen from the list of words they represent.
    Words are uniquely assigned to a category.
    Any category that only has one associated word is filtered out.

    Args:
      num_words: Number of words to select from the those available in the model.
      num_categories: Number of groups.
    """
    words = _SelectWords(model, num_words)
    vectors = [model.get_vector(word) for word in words]
    linkage_matrix = hierarchy.linkage(vectors, method="average", metric="cosine")
    category_dict = _CreateHierarchy(model, linkage_matrix, words, num_categories)
    # remove any categories that only have 1 entry
    short_lists = []
    for k, v in category_dict.items():
        if len(v) == 1:
            short_lists.append(k)
    for k in short_lists:
        del category_dict[k]
    return category_dict


def WriteCategoryCSV(path: str, categories: dict[str, str]) -> None:
    """Writes the categories dict to a DELTA file CSV.

    The format of the CSV is intended to be used with the data cli to generate a DELTA file that can be ingested by the server.

    Args:
      path: Intended destination of the CSV.
      categories: Dictionary returned by GenerateCategoryDict.
    """
    timestamp = int(datetime.datetime.now().timestamp() * 1000000)
    with open(path, "w") as csvfile:
        writer = csv.writer(csvfile)
        # Write the header row
        writer.writerow(
            ["key", "mutation_type", "logical_commit_time", "value", "value_type"]
        )
        for category, words in categories.items():
            writer.writerow(
                [category, "UPDATE", timestamp, "|".join(words), "string_set"]
            )


def WriteEmbeddingCSV(path: str, model: models.KeyedVectors) -> None:
    """Writes all of the models word embeddings to a DELTA file CSV.

    The format of the CSV is intended to be used with the data cli to generate a DELTA file that can be ingested by the server.

    Args:
      path: Intended destination of the CSV.
      model: KeyedVectors model.
    """
    timestamp = int(datetime.datetime.now().timestamp() * 1000000)
    # Create a CSV writer object
    with open(path, "w") as csvfile:
        writer = csv.writer(csvfile)
        # Write the header row
        writer.writerow(
            ["key", "mutation_type", "logical_commit_time", "value", "value_type"]
        )
        for key, embedding in zip(model.index_to_key, model.vectors):
            writer.writerow(
                [key, "UPDATE", timestamp, json.dumps(embedding.tolist()), "string"]
            )


def LoadModel(
    path: Optional[str] = None, model: str = "word2vec-google-news-300"
) -> models.KeyedVectors:
    """Loads an instance a pretrained word2vec model.

    First we try to the load the model from local disk.
    If unavailable it is loaded by name gensim and saved to disk
    Valid models: https://github.com/RaRe-Technologies/gensim-data#available-data


    Args:
      path: local path to model, ignored if None.
      model: name of the model to load

    Returns:
      KeyedVectors instance.
    """
    if path is not None:
        try:
            model = models.KeyedVectors.load(path)
            return model
        except FileNotFoundError:
            pass

    model = downloader.load(model)
    if path is not None:
        model.save(path)
    return model


def Main():
    parser = argparse.ArgumentParser(
        description="Generate CSV files from word2vec model."
    )
    parser.add_argument(
        "--model-cache",
        dest="model_cache",
        default="/tmp/model_cache.vec",
        help="Path to locally cached KeyedVectors model",
    )
    parser.add_argument(
        "--model-name",
        dest="model_name",
        default="word2vec-google-news-300",
        help="Name of the gensim model to load",
    )
    parser.add_argument(
        "--generate-categories",
        action=argparse.BooleanOptionalAction,
        dest="is_generate_categories",
        default=True,
        help="Generate category CSV containing key->set data",
    )
    parser.add_argument(
        "--categories-path",
        dest="categories_path",
        default="categories.csv",
        type=os.path.expanduser,
        help="Destination for category csv",
    )
    parser.add_argument(
        "--num-category_words",
        dest="num_category_words",
        default=10000,
        help="Number of words to group into categories.",
    )
    parser.add_argument(
        "--num-categories",
        dest="num_categories",
        default=1000,
        help="Number of groups to put category words in.",
    )
    parser.add_argument(
        "--show-categories",
        action=argparse.BooleanOptionalAction,
        dest="is_show_categories",
        default=False,
        help="Print category data to stdout.",
    )
    parser.add_argument(
        "--generate-embeddings",
        action=argparse.BooleanOptionalAction,
        dest="is_generate_embeddings",
        default=True,
        help="Generate embeddings CSV containing word->embedding data for every word in the model",
    )
    parser.add_argument(
        "--embeddings-path",
        dest="embeddings_path",
        default="embeddings.csv",
        type=os.path.expanduser,
        help="Destination for embedding csv",
    )
    args = parser.parse_args()
    model = LoadModel(args.model_cache, args.model_name)
    if args.is_generate_categories:
        categories = GenerateCategoryDict(
            model, args.num_category_words, args.num_categories
        )
        if args.is_show_categories:
            print(json.dumps(categories, indent=4))
        WriteCategoryCSV(args.categories_path, categories)
    if args.is_generate_embeddings:
        WriteEmbeddingCSV(args.embeddings_path, model)


if __name__ == "__main__":
    Main()
