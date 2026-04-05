# External Dependencies

`grpcMMO` keeps these local checkouts inside the repository so the default build
path matches the `grpcMUD` layout:

- `external/vcpkg`
- `external/frame`

`external/vcpkg` should stay pinned to a release tag that matches the
`builtin-baseline` recorded in `vcpkg.json` instead of tracking `master`.
The current workspace target is `2026.03.18`.

`grpcMMO-data` remains a separate companion repository because its contents are
expected to be large and primarily LFS-backed terrain assets.
