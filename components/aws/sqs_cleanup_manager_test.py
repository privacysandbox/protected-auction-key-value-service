# Copyright 2022 Google LLC
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
"""Tests for sqs_cleanup_manager."""
import dataclasses
import sqs_cleanup_manager
import time
import unittest


@dataclasses.dataclass
class FakeQueue:
    url: str


class FakeClient(object):
    def __init__(self):
        self.tags = {}
        self.attributes = {}

    def list_queue_tags(self, QueueUrl):
        return self.tags

    def get_queue_attributes(self, QueueUrl, AttributeNames):
        return self.attributes


@dataclasses.dataclass
class Meta:
    client: FakeClient


class FakeSqsResource(object):
    def __init__(self, client):
        self._meta = Meta(client)

    @property
    def meta(self):
        return self._meta


class SqsCleanupManagerTest(unittest.TestCase):

    TIMEOUT_SECS = 600

    def setUp(self):
        self._client = FakeClient()
        self._fake_sqs_resource = FakeSqsResource(self._client)
        self._manager = sqs_cleanup_manager.SqsCleanupManager(
            None, self._fake_sqs_resource
        )

    def test_expired_no_tag(self):
        queue = FakeQueue("http://foobar.com")
        now = time.time()
        self._client.attributes = {
            "Attributes": {"CreatedTimestamp": str(int(now - 1000))}
        }
        self.assertTrue(
            self._manager._is_expired(queue, SqsCleanupManagerTest.TIMEOUT_SECS)
        )

    def test_is_not_expired_no_tag(self):
        queue = FakeQueue("http://foobar.com")
        now = time.time()
        self._client.attributes = {
            "Attributes": {"CreatedTimestamp": str(int(now - 100))}
        }
        self.assertFalse(
            self._manager._is_expired(queue, SqsCleanupManagerTest.TIMEOUT_SECS)
        )

    def test_is_not_expired(self):
        queue = FakeQueue("http://foobar.com")
        now = time.time()
        self._client.tags = {"Tags": {"last_updated": str(int(now - 100))}}
        self.assertFalse(
            self._manager._is_expired(queue, SqsCleanupManagerTest.TIMEOUT_SECS)
        )

    def test_is_expired(self):
        queue = FakeQueue("http://foobar.com")
        now = time.time()
        self._client.tags = {"Tags": {"last_updated": str(int(now - 1000))}}
        self.assertTrue(
            self._manager._is_expired(queue, SqsCleanupManagerTest.TIMEOUT_SECS)
        )

    def test_is_expired_invalid(self):
        queue = FakeQueue("http://foobar.com")
        now = time.time()
        self._client.tags = {"Tags": {"last_updated": "abc"}}
        self.assertTrue(
            self._manager._is_expired(queue, SqsCleanupManagerTest.TIMEOUT_SECS)
        )


if __name__ == "__main__":
    unittest.main()
