#!/usr/bin/env bash
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

# Example usage:
# bash ./hc.bash -p /usr/local/google/home/akundla/bidding-auction-server/production/packaging/aws/common/ami -n health.proto -a localhost:50051 -i 10 -t 5 -h 2 -u 10 -e i-08756c3a64a78711a -g 2 -r us-west-1 -s srv-ajxdkksp7d5wpmou

# https://docs.aws.amazon.com/cli/v1/userguide/cli-configure-retries.html
export AWS_RETRY_MODE="standard"
export AWS_MAX_ATTEMPTS=4

GRPC_STATUS_SERVING="SERVING"

# Input parameter validation values.
healthcheck_min_frequency_sec=1
healthcheck_max_frequency_sec=90
min_checks_threshold=2
max_checks_threshold=10

while getopts "p:n:a:i:t:h:u:e:g:r:s:" flag; do
 case $flag in
   p)
     path_to_folder_of_healthcheck_proto_file=$OPTARG
     ;;
   n)
     healthcheck_proto_file_name=$OPTARG
     ;;
   a)
     address_and_port_of_service=$OPTARG
     ;;
   i)
     if [[ $OPTARG -ge $healthcheck_min_frequency_sec ]] && [[ $OPTARG -le $healthcheck_max_frequency_sec ]]
     then
       interval_between_hc_sec=$OPTARG
     else
        echo "Invalid value for flag -i (interval_between_hc_sec): value must be between ${healthcheck_min_frequency_sec} and ${healthcheck_max_frequency_sec}, inclusive."
        exit 1
     fi
     ;;
   t)
     if [[ $OPTARG -ge $healthcheck_min_frequency_sec ]] && [[ $OPTARG -le $healthcheck_max_frequency_sec ]]
     then
       hc_timeout_sec=$OPTARG
     else
        echo "Invalid value for flag -t (hc_timeout_sec): value must be between ${healthcheck_min_frequency_sec} and ${healthcheck_max_frequency_sec}, inclusive."
        exit 1
     fi
     ;;
   h)
     if [[ $OPTARG -ge $min_checks_threshold ]] && [[ $OPTARG -le $max_checks_threshold ]]
     then
       healthy_threshold=$OPTARG
     else
        echo "Invalid value for flag -h (healthy_threshold): value must be between ${min_checks_threshold} and ${max_checks_threshold}, inclusive."
        exit 1
     fi
     ;;
   u)
     if [[ $OPTARG -ge $min_checks_threshold ]] && [[ $OPTARG -le $max_checks_threshold ]]
     then
       unhealthy_threshold=$OPTARG
     else
        echo "Invalid value for flag -u (unhealthy_threshold): value must be between ${min_checks_threshold} and ${max_checks_threshold}, inclusive."
        exit 1
     fi
     ;;
   e)
     instance_id=$OPTARG
     ;;
   g)
     startup_grace_period=$OPTARG
     ;;
   r)
     region=$OPTARG
     ;;
   s)
     cloud_map_service_id=$OPTARG
     ;;
   \?)
   # Handle invalid options
   echo "Invalid option detected"
   exit 1
   ;;
 esac
done

input_param_names=("path_to_folder_of_healthcheck_proto_file" "healthcheck_proto_file_name" "address_and_port_of_service" "interval_between_hc_sec" "hc_timeout_sec" "healthy_threshold" "unhealthy_threshold" "instance_id" "startup_grace_period" "region" "cloud_map_service_id")

input_param_values=("${path_to_folder_of_healthcheck_proto_file}" "${healthcheck_proto_file_name}" "${address_and_port_of_service}" "${interval_between_hc_sec}" "${hc_timeout_sec}" "${healthy_threshold}" "${unhealthy_threshold}" "${instance_id}" "${startup_grace_period}" "${region}" "${cloud_map_service_id}")

any_params_missing=false

for index in "${!input_param_names[@]}";
do
  if [[ -z "${input_param_values[$index]}" ]]
  then
    echo "${input_param_names[$index]} is missing!"
    any_params_missing=true
  fi
done

if [[ ${any_params_missing} =~ "true" ]]
then
  echo "All params required, exiting."
  exit 1
fi

# Holds the last n statuses, where n = max(healthy_threshold, unhealthy_threshold)
healthcheck_status_queue=()
hc_stat_queue_max_len=$(( healthy_threshold > unhealthy_threshold ? healthy_threshold : unhealthy_threshold ))

last_set_status_healthy=true

echo "Custom health checking script initialized, waiting for grace period before beginning health checks."

sleep "${startup_grace_period}"

echo "Custom health checking script beginning healthchecks now."

while true
do
    current_hc_response=$(grpcurl --plaintext -connect-timeout="${hc_timeout_sec}" -max-time="${hc_timeout_sec}" -import-path="${path_to_folder_of_healthcheck_proto_file}" -proto="${healthcheck_proto_file_name}" "${address_and_port_of_service}" grpc.health.v1.Health/Check)

    if [[ $current_hc_response == *$GRPC_STATUS_SERVING* ]]
    then
      healthcheck_status_queue+=(true)
      # if the server has been set to UNEAHTLHY in the cloud map, it can be set to HEALTHY again. But if it has been condemned in the ASG, it is shutting down; even if the server recovers it will still shut down.
      if [[ "${last_set_status_healthy}" =~ true ]]
      then
        aws servicediscovery update-instance-custom-health-status --service-id "$cloud_map_service_id" --instance-id "$instance_id" --region "$region" --status HEALTHY
      fi
    else
      healthcheck_status_queue+=(false)
      echo "Server is not serving; fails custom health check running on machine!"
      echo "Last status between the quotes if present: '$($current_hc_response)'"
      aws servicediscovery update-instance-custom-health-status --service-id "$cloud_map_service_id" --instance-id "$instance_id" --region "$region" --status UNHEALTHY
      echo "Set to UNHEALTHY in cloud map"
    fi

    # Keep queue at max length and no larger.
    if [[ "${#healthcheck_status_queue[@]}" -gt hc_stat_queue_max_len ]]
    then
      # Remove first element
      healthcheck_status_queue=("${healthcheck_status_queue[@]:1}")
    fi

    current_hc_stat_queue_len=${#healthcheck_status_queue[@]}

    if [[ ${healthcheck_status_queue[-1]} =~ "true" ]] && [[ current_hc_stat_queue_len -ge $healthy_threshold ]]
    then
      start_i="$(( current_hc_stat_queue_len - healthy_threshold ))"
      end_i="$(( current_hc_stat_queue_len - 1 ))"
      can_set_server_as_healthy=true
      for i in $(seq $start_i $end_i)
      do
        if [[ ${healthcheck_status_queue[$i]} =~ "false" ]]
        then
          can_set_server_as_healthy=false
        fi
      done
      if [[ ${can_set_server_as_healthy} =~ "true" ]] && [[ "${last_set_status_healthy}" =~ "false" ]]
      then
        aws autoscaling set-instance-health --instance-id "$instance_id" --region "$region" --health-status Healthy
        last_set_status_healthy=true
        echo "Just made ASG HC Call to set server to healthy"
      fi
    fi

    if [[ ${healthcheck_status_queue[-1]} =~ "false" ]] && [[ current_hc_stat_queue_len -ge $unhealthy_threshold ]]
    then
      echo "Can check if server is un-healthy."
      start_i="$(( current_hc_stat_queue_len - unhealthy_threshold ))"
      end_i="$(( current_hc_stat_queue_len - 1 ))"
      can_set_server_as_unhealthy=true
      for i in $(seq $start_i $end_i)
      do
        if [[ ${healthcheck_status_queue[$i]} =~ "true" ]]
        then
          can_set_server_as_unhealthy=false
        fi
        echo "healthcheck_status_queue[$i]: ${healthcheck_status_queue[$i]}"
      done
      echo "Can set server as unhealthy: ${can_set_server_as_unhealthy}"
      if [[ ${can_set_server_as_unhealthy} =~ "true" ]] && [[ "${last_set_status_healthy}" =~ "true" ]]
      then
        # We're about to flag this instance to be killed by ASG, so it must be de-registered from the cloud map - and we need to de-register it NOW, before the machine is shut down.
        aws servicediscovery deregister-instance --instance-id "$instance_id" --service-id "$cloud_map_service_id" --region "$region"
        echo "Just made Cloud Map Call to de-register instance"
        # Now set the instance to be grought down and replaced by ASG.
        aws autoscaling set-instance-health --instance-id "$instance_id" --region "$region" --health-status Unhealthy
        last_set_status_healthy=false
        echo "Just made ASG HC Call to set server to Unhealthy"
      fi
    fi

    # Sleep no matter what.
    sleep "${interval_between_hc_sec}s"
done
