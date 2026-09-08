# TODO

## CI and release maintenance

- [ ] Migrate GitHub Actions to Node.js 26 after GitHub-hosted runners and all pinned third-party actions officially support it.
  - Keep every third-party action pinned to a full commit SHA.
  - Update Node-based actions such as `actions/checkout` and `TheMrMilchmann/setup-msvc-dev` to Node.js 26-compatible releases.
  - Confirm that no workflow relies on a deprecated Node.js runtime or a temporary compatibility override.
  - Run the complete Build workflow and verify the Release workflow before publishing a release.
