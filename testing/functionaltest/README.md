# Functional testing the K/V server

## Requirements

You must have the [Docker Compose v2](https://github.com/docker/compose#where-to-get-docker-compose)
plugin installed. You can check the version of the plugin you have installed like this (the full
version string may vary, though the major version must be `v2`):

```sh
$ docker compose version
Docker Compose version v2.17.2
```

## Build the server

```sh
builders/tools/bazel-debian run //production/packaging/aws/data_server:copy_to_dist \
  --config=local_instance \
  --config=local_platform
unzip -d dist/debian -j -u dist/debian/server_artifacts.zip server/bin/server
touch dist/debian/server
docker load -i dist/server_docker_image.tar
```

## Execute the server

The server can be run using either run directly or if using docker containers, using
`run-server-docker`.

### Generate data files

Generate data files for the functional test suites, using:

```sh
builders/tools/bazel-debian run //testing/functionaltest:copy_to_dist \
  --config=local_instance \
  --config=local_platform
```

### Direct execution mode

To run the server with these delta files, you should specify both the `--delta_directory` and
`--realtime_directory` flags. Both of these directories must exist. The `--delta_directoy` should
contain the files

```sh
dist/debian/server \
  --port 50051 \
  --delta_directory "${delta_directory}" \
  --realtime_directory "${realtime_directory}"
```

When running the test suite, use the following values for the script flags:

-   `<network>`: `host`
-   `<endpoint>`: `localhost:50051`

### run-server-docker

This script creates a docker network, and runs the server container on that network. The script
prints the name of the docker network, which is needed when invoking the test suite. The server will
be accessible at `kvserver:50051` on this docker network.

```sh
testing/functionaltest/run-server-docker
```

When running the test suite, use the following values for the script flags:

-   `<network>`: the network name printed by the script
-   `<endpoint>`: `kvserver:50051`

## Run the test suite

Use the appropriate network and endpoint values, which will vary according to the method used to run
the server.

```sh
testing/functionaltest/run-tests \
  --network <network-name> \
  --endpoint <endpoint>
```
