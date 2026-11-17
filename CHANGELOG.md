# Changelog

## Unreleased

## v0.1.3 - 2026-11-17

- chore: align CI/CD, release scripts, and contributing docs with cpp-templates (#1)
  * feat: use VCPKG_OVERLAY_PORTS and guard release
  * chore: adopt CI/CD and contributing templates
  * docs: maintainer setup for release App, secrets, rulesets
  * docs: soft alignment marker and release-bot contributing docs
- ci: poll gh run watch every 30s in release
- docs: document developing against unreleased libs

## v0.1.2 - 2026-11-16

- Replace `slimr-curl` with `cpp-httplib[openssl]`.
- Internal: switch macOS builds to `llvm@23` and static `libc++`.
- Internal: align tooling with the cpp-templates library template.
- Split `slimr-bugsink` from `slimr-cpp` into its own vcpkg registry repository.
- Internal: move the repository to `slimr-down-now/bugsink`
