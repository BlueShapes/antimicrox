# TODO

## CI and release maintenance

- [x] Remove deprecated Node.js 20 Actions by updating pinned dependencies to their Node.js 24 releases.
  - Completed for `actions/checkout`, `TheMrMilchmann/setup-msvc-dev`, `github/codeql-action`, and `actions/upload-artifact`.
  - GitHub currently supports only `node20` and `node24` in JavaScript Action metadata.
- [ ] Migrate GitHub Actions to Node.js 26 after GitHub-hosted runners and all pinned third-party actions officially support it.
  - Keep every third-party action pinned to a full commit SHA.
  - Update all Node-based actions to releases whose metadata declares the future Node.js 26 runtime.
  - Confirm that no workflow relies on a deprecated Node.js runtime or a temporary compatibility override.
  - Run the complete Build workflow and verify the Release workflow before publishing a release.
