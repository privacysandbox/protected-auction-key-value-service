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
"""Manages SQS Queue cleanup tasks for expired queues.

An expired queue is one that has not had the last_used tag updated in 10
minutes.  If the last_used tag is not present, the queue creation time is used.
"""

import time


class SqsCleanupManager(object):
  """Deletes expired SQS Queues and associated subscriptions."""

  def __init__(self, sns_resource, sqs_resource):
    self._sns_resource = sns_resource
    self._sqs_resource = sqs_resource

  def _get_creation_time(self, queue) -> int:
    """Get creation time of queue.

    Args:
      queue: SQS Queue.

    Returns:
      Creation time in seconds since epoch or 0 on error.
    """
    client = self._sqs_resource.meta.client
    response = client.get_queue_attributes(
        QueueUrl=queue.url, AttributeNames=['CreatedTimestamp'])
    attributes = response.get('Attributes', {})
    timestamp = attributes.get('CreatedTimestamp', 0)
    try:
      return int(timestamp)
    except ValueError:
      return 0

  def _is_expired(self, queue, timeout_secs) -> bool:
    """Checks if the Queue is no longer in use.

    The queue is checked for the 'last_used' tag.
    If the last usage is beyond some threshold it is considered expired.

    Args:
      queue: SQS Queue.
      timeout_secs: Age that a queue is considered expired.

    Returns:
      True if expired.
    """
    client = self._sqs_resource.meta.client
    response = client.list_queue_tags(QueueUrl=queue.url)
    tags = response.get('Tags', {})
    last_used = tags.get('last_updated')
    if last_used is not None:
      try:
        then = int(last_used)
      except ValueError:
        return True
    else:
      then = self._get_creation_time(queue)
    now = int(time.time())
    return now - then > timeout_secs

  def _find_expired_queues(self, queue_prefix: str, timeout_secs: int) -> list:
    """Searches for all queues with the given name prefix.

    Args:
      queue_prefix: Filtthat a queues starting with prefix.
      timeout_secs: Age that a queue is considered expired.

    Returns:
      List of SQS Queue instances that have expired.
    """
    client = self._sqs_resource.meta.client
    paginator = client.get_paginator('list_queues')
    expired_queues = []
    for response in paginator.paginate(QueueNamePrefix=queue_prefix):
      queue_urls = response.get('QueueUrls', [])
      for queue in queue_urls:
        queue = self._sqs_resource.Queue(queue)
        if self._is_expired(queue, timeout_secs):
          expired_queues.append(queue)
    return expired_queues

  def _get_target_subscriptions(self, topic_arn: str) -> dict:
    """Find all subscriptions to the topic.

    Args:
      topic_arn: SNS topic string.

    Returns:
     Dictionary of arns, from subscription target to subscription.
    """
    client = self._sns_resource.meta.client
    paginator = client.get_paginator('list_subscriptions_by_topic')
    targets_to_subscriptions = {}
    for response in paginator.paginate(TopicArn=topic_arn):
      for subscription in response.get('Subscriptions', []):
        subscription_arn = subscription.get('SubscriptionArn')
        target_arn = subscription.get('Endpoint')
        if subscription is not None and target_arn is not None:
          targets_to_subscriptions[target_arn] = subscription_arn
    return targets_to_subscriptions

  def _cleanup(self, expired_queues: list,
               subscriptions: dict[str, str]) -> tuple[int, int]:
    """Deletes all expired SQS queues and associated subscriptions.

    Args:
      expired_queues: List of expired SQS.Queue instances.
      subscriptions: Subscription target arns to subscription arns.

    Returns:
      tuple of number of queues deleted and number of subscriptions deleted.
    """
    subscriptions_deleted = 0
    for queue in expired_queues:
      queue_arn = queue.attributes.get('QueueArn')
      subscription_arn = subscriptions.get(queue_arn)
      if subscription_arn is not None:
        subscription = self._sns_resource.Subscription(subscription_arn)
        subscription.delete()
        subscriptions_deleted += 1
      queue.delete()
    return len(expired_queues), subscriptions_deleted

  def find_and_cleanup(self, topic_arn: str, queue_prefix: str,
                       timeout_secs: int) -> tuple[int, int]:
    """Deletes all expired Queues and associated subscriptions.

      Returns:
        Number of deleted queues and subscriptions.
    """
    target_subscriptions = self._get_target_subscriptions(topic_arn)
    expired_queues = self._find_expired_queues(queue_prefix, timeout_secs)
    return self._cleanup(expired_queues, target_subscriptions)
