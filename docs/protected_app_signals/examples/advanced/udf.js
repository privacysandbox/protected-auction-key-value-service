/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Get the embedding for each of the words.
 *
 * @param words An array of strings.
 * @returns A dictionary of words to embedding.
 */
function associateEmbeddings(words) {
  const getValuesResult = JSON.parse(getValues(words));
  // getValuesResult returns "kvPairs" when successful and "code" on failure.
  // Ignore failures and only add successful lookups to output.
  wordEmbeddings = {};
  if (getValuesResult.hasOwnProperty('kvPairs')) {
    const kvPairs = getValuesResult.kvPairs;
    const keyValuesOutput = {};
    for (const key in kvPairs) {
      if (kvPairs[key].hasOwnProperty('value')) {
        wordEmbeddings[key] = JSON.parse(kvPairs[key].value);
      }
    }
  }
  return wordEmbeddings;
}

/**
 * Get the embedding for the given word.
 *
 * @returns embedding array.
 */
function getWordEmbedding(word) {
  embeddings = associateEmbeddings([word]);
  if (Object.keys(embeddings).length < 1) {
    return null;
  }
  return Object.values(embeddings)[0];
}

/**
 * Computes the similarity between two embeddings.
 *
 * @param left embedding
 * @param right embedding
 * @returns Scalar similarity between -1 and 1.  1 is most similar.
 */
function cosineSimilarity(left, right) {
  dot = (a, b) => a.map((x, i) => a[i] * b[i]).reduce((m, n) => m + n);
  magnitude = (x) => Math.sqrt(x.reduce((sum, value) => sum + value * value, 0));
  return dot(left, right) / (magnitude(left) * magnitude(right));
}

/**
 * Finds the costine similarity of each word to the embedding.
 *
 * @param wordEmbeddings Dictionary of word to embedding.
 * @param embedding Embedding to compare each word in the dictionary agains.
 * @returns Dictionary from word to cosine similarity to the supplied embedding.
 */
function associateCosineSimilarity(wordEmbeddings, embedding) {
  wordSimilarity = {};
  for (const word in wordEmbeddings) {
    wordSimilarity[word] = cosineSimilarity(wordEmbeddings[word], embedding);
  }
  return wordSimilarity;
}

/**
 * Computes the set union of all `metadata` keys and scores their similarity against the `signal` word.
 * The words and scores of the top 5 most similar words are returned, in order of similarity.
 *
 * @param metadataKeys Keys into the set data which do a UNION of all entries.
 * @param signal Orders unioned data by similarity to signal word.
 * @returns A sorted list of top 5 words and their scores.
 */
function HandleRequest(requestMetadata, protectedSignals, deviceMetadata, contextualSignals, contextualAdIds) {
  const parsedProtectedSignals = JSON.parse(protectedSignals);
  results = [];
  if (parsedProtectedSignals.metadataKeys.length) {
    // Union all of the sets of the given metadata category
    results = runQuery(parsedProtectedSignals.metadataKeys.join('|'));
  }
  wordSimilarity = {};
  embedding = getWordEmbedding(parsedProtectedSignals.signal);
  if (embedding != null) {
    wordSimilarity = associateCosineSimilarity(associateEmbeddings(results), embedding);
  }
  // Sort by relevance and return the top 5
  sortedWords = Object.entries(wordSimilarity);
  sortedWords.sort((a, b) => b[1] - a[1]);
  sortedWords = sortedWords.slice(0, 5);

  return JSON.stringify(sortedWords);
}
