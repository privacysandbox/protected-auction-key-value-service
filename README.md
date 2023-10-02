> FLEDGE has been renamed to Protected Audience API. To learn more about the name change, see the
> [blog post](https://privacysandbox.com/intl/en_us/news/protected-audience-api-our-new-name-for-fledge).

# ![Privacy Sandbox Logo](docs/assets/privacy_sandbox_logo.png) FLEDGE Key/Value service

# Background

FLEDGE API is a proposal to serve remarketing and other custom-audience ads without third-party
cookies. FLEDGE executes the ad auction between the buyers (DSP) and the sellers (SSP) locally, and
receives real-time signals from the FLEDGE K/V servers. To learn more about

-   FLEDGE for the Web: [explainer](https://developer.chrome.com/en/docs/privacy-sandbox/fledge/)
    and the [developer guide](https://developer.chrome.com/blog/fledge-api/).
-   FLEDGE on Android:
    [design proposal](https://developer.android.com/design-for-safety/privacy-sandbox/fledge) and
    the
    [developer guide](https://developer.android.com/design-for-safety/privacy-sandbox/guides/fledge).

When the auction is executed, separate
[FLEDGE K/V servers](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md)
are queried for the buyers and sellers. When a buyer is making a bid, the DSP K/V server can be
queried to receive real-time information to help determine the bid. To help the seller pick an
auction winner, the SSP K/V server can be queried to receive any information about the creative to
help score the ad.

# State of the project

The current codebase represents the initial implementation and setup of the Key/Value server. It can
be integrated with Chrome and Android with the
[Privacy Sandbox unified origin trial](https://developer.chrome.com/blog/expanding-privacy-sandbox-testing/)
and
[Privacy Sandbox on Android Developer Preview](https://developer.android.com/design-for-safety/privacy-sandbox/program-overview).
Our goal is to present the foundation of the project in a publicly visible way for early feedback.
This feedback will help us shape the future versions.

The implementation, and in particular the APIs, are in rapid development and may change as new
versions are released. The query API conforms to the
[API explainer](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md). At the
moment, to load data, instead of calling the mutation API, you would place the data as files into a
location that can be directly read by the server. See more details in the
[data loading guide](/docs/loading_data.md).

The query API only supports the newer version of response format (accompanied by response header
`X-fledge-bidding-signals-format-version: 2`) as described in the
[FLEDGE main explainer](https://github.com/WICG/turtledove/blob/main/FLEDGE.md#31-fetching-real-time-data-from-a-trusted-server).
Chrome's support of this format is expected to be available in Chrome version 105.

Currently, this service can be deployed to 1 region of your choice with more regions to be added
soon. Monitoring and alerts are currently unavailable.

> **Attention**: The Key/Value Server is publicly queryable. It does not authenticate callers. That
> is also true for the product end state. It is recommended to only store data you do not mind seen
> by other entities.

## How to use this repo

The `main` branch hosts live code with latest changes. It is unstable and is used for development.
It is suitable for contribution and inspection of the latest code. The `release-*` branches are
stable releases that can be used to build and deploy the system.

## Breaking changes

This codebase right now is in a very early stage. We expect frequent updates that may not be fully
backward compatible.

The release version follows the `[major change]-[minor change]-[patch]` scheme. All 0.x.x versions
may contain breaking changes without notice. Refer to the [release changelog](/CHANGELOG.md) for the
details of the breaking changes.

Once the codebase is in a more stable state that is version 1.0.0, we will establish additional
channels for announcing breaking changes and major version will always be incremented for breaking
changes.

# Key documents

-   [FLEDGE services overview](https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md)
-   [FLEDGE K/V server API explainer](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md)
-   [FLEDGE K/V server trust model](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_trust_model.md)
-   [Local server quickstart guide](/docs/developing_the_server.md)
-   [AWS server user deployment documentation](/docs/deploying_on_aws.md)
-   [Integrating the K/V server with FLEDGE](/docs/integrating_with_fledge.md)
-   [FLEDGE K/V server sharding explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_sharding.md)
-   Operating documentation
    -   [Data loading API and operations](/docs/loading_data.md)
    -   [Generating and loading UDF files](/docs/generating_udf_files.md)
    -   Error handling explainer (_to be published_)
-   Developer guide
    -   [Codebase structure](/docs/repo_layout.md)
    -   [Working with Terraform](/production/terraform/README.md)
    -   [Contributing to the codebase](/docs/CONTRIBUTING.md)
-   [Code of conduct](/docs/CODE_OF_CONDUCT.md)
-   [Change log](/CHANGELOG.md)

# Contribution

Contributions are welcome, and we will publish more detailed guidelines soon. In the meantime, if
you are interested,
[open a new Issue](https://github.com/privacysandbox/fledge-key-value-service/issues) in the GitHub
repository.

# Feedback

The FLEDGE K/V feature set, API and system design are under active discussion and subject to change
in the future. If you try this API and have feedback, we'd love to hear it:

-   **GitHub**: For questions or feedback that are relevant to this proposal, open a new Issue in
    this repository. For questions that are purely relevant to browser-side logic,
    [open an Issue in the Turtledove repository](https://github.com/WICG/turtledove/issues).
-   **W3C:** You are welcome to
    [attend the FLEDGE WICG meeting](https://github.com/WICG/turtledove/issues/88).
-   **Developer support**: Ask questions and join discussions on
    -   [Privacy Sandbox for the Web Developer Support repo](https://github.com/GoogleChromeLabs/privacy-sandbox-dev-support)
    -   [Privacy Sandbox on Android issue tracker](https://issuetracker.google.com/issues/new?component=1116743&template=1642575)

# Announcements

## General announcements

General announcements will be made on
[the FLEDGE mailing list](https://groups.google.com/a/chromium.org/g/fledge-api-announce/) on
chromium.org and the
[Privacy Sandbox progress update page](https://developer.android.com/design-for-safety/privacy-sandbox/progress-updates/latest)
on the Android developer site, if needed, an accompanying article will be published on the
[Chrome Developers blog](https://developer.chrome.com/tags/privacy/) and
[Android Developers blog](https://android-developers.googleblog.com/).

---

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/privacysandbox/fledge-key-value-service/badge)](https://securityscorecards.dev/viewer/?uri=github.com/privacysandbox/fledge-key-value-service)
