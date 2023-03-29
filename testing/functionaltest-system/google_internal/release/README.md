# Releases

## Repositories

-   [Gerrit/Git-on-Borg](https://team.git.corp.google.com/potassium-engprod-team/functionaltest-system)
    Source of Truth, primary development repository

## Step 1: Cut a Release

The following instructions are for creation of a release from the `main` branch (ie. release from
`HEAD`).

1. Execute the `cut_release` script:

    ```sh
    google_internal/release/cut_release
    ```

    On successful execution, this will create a local branch named `prerelease-a.b.c` where `a.b.c`
    is the new version number.

1. Push the commit to gerrit: `git push origin refs/for/main`.

1. Review and submit the CL. You'll need to edit the CL description to link to the bug that you
   created, with `Bug: b/bug_id`.

1. After refreshing your local repo, switch to the `main` branch then execute:

    ```sh
    google_internal/release/tag_release
    ```

    This creates a tag for the release. Follow the instructions to push the tag to the remote.
