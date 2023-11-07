# Repository layout

This is a high level overview of the repository structure.

-   [builders](/builders/): Build environment and tools.
-   [components](/components/): Source code of all components in the system. The content can be
    considered system internal implementation. For public API and libraries, see [public](/public/).
-   [docs](/docs/): User facing documentation.
-   [infrastructure](/infrastructure/): Source code of infrastructure logic that can be potentially
    reused by similar systems.
-   [production](/production/): Configuration for packaging and deployment of the system into cloud
    platforms.
-   [public](/public/): APIs and client libraries provided to users of this system.
-   [third_party_deps](/third_party_deps/): Configuration to allow the use of third party dependencies.
-   [tools](/tools/): Convenient tools provided to users of the system for common operations.
