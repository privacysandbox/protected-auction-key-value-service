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
"""AWS Lambda hook to triggering an SQS cleanup task."""
import boto3
import sqs_cleanup_manager


def find_and_cleanup(topic, prefix, timeout_secs):
    sns = boto3.resource("sns")
    sqs = boto3.resource("sqs")
    manager = sqs_cleanup_manager.SqsCleanupManager(sns, sqs)
    return manager.find_and_cleanup(topic, prefix, timeout_secs)


def handler(event, context):
    """AWS Lambda hook."""
    sns_topic = event.get("sns_topic")
    queue_prefix = event.get("queue_prefix")
    timeout_secs = int(event.get("timeout_secs"))
    if queue_prefix is None:
        raise Exception("no prefix")
    if sns_topic is None:
        raise Exception("no topic")
    deleted_queues, deleted_subscriptions = find_and_cleanup(
        sns_topic, queue_prefix, timeout_secs
    )
    realtime_sns_topic = event.get("realtime_sns_topic")
    realtime_queue_prefix = event.get("realtime_queue_prefix")
    if realtime_sns_topic is None:
        raise Exception("no realtime topic")
    if realtime_queue_prefix is None:
        raise Exception("no realtime prefix")
    deleted_realtime_queues, deleted_realtime_subscriptions = find_and_cleanup(
        realtime_sns_topic, realtime_queue_prefix, timeout_secs
    )
    logging_verbosity_updates_sns_topic = event.get(
        "logging_verbosity_updates_sns_topic"
    )
    parameter_queue_prefix = event.get("parameter_queue_prefix")
    if logging_verbosity_updates_sns_topic is None:
        raise Exception("no logging verbosity updates topic")
    if parameter_queue_prefix is None:
        raise Exception("no parameter queue prefix")

    (
        deleted_logging_verbosity_parameter_queues,
        deleted_logging_verbosity_parameter_subscriptions,
    ) = find_and_cleanup(
        logging_verbosity_updates_sns_topic, parameter_queue_prefix, timeout_secs
    )

    return {
        "deleted_queues": deleted_queues,
        "deleted_subscriptions": deleted_subscriptions,
        "deleted_realtime_queues": deleted_realtime_queues,
        "deleted_realtime_subscriptions": deleted_realtime_subscriptions,
        "deleted_logging_verbosity_parameter_queues": deleted_logging_verbosity_parameter_queues,
        "deleted_logging_verbosity_parameter_subscriptions": deleted_logging_verbosity_parameter_subscriptions,
    }
