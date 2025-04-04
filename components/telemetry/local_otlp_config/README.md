# Viewing Telemetry locally

The server can be run locally with 2 differt compile time flags.

-   default: `--@google_privacysandbox_servers_common//src/telemetry:local_otel_export=ostream`
-   alternative: `--@google_privacysandbox_servers_common//src/telemetry:local_otel_export=otlp`

## OTLP

Follow this [quick start](https://opentelemetry.io/docs/collector/quick-start/) to bring up the Otel
Collector to run with the otlp option.

To bring up the collector and its dependencies you can run:
`COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose up` from this directory.

The Otel Collector in turn, can export to locally running Zipkin, Jaeger, and Prometheus instances.
These tools can be viewed from the browser.

-   Traces
    -   Zipkin: <http://localhost:9411>
-   Metrics
    -   Prometheus: <http://localhost:9090>
