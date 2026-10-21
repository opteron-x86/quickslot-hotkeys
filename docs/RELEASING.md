# Releases

`master` is the development branch. Push commits to run the Linux tests and Windows build. Successful **Build and test** runs contain a `Quickslot-…` artifact with installable ZIP packages. Development filenames include the commit ID; artifacts are retained for 30 days.

## Publish a version

1. Set the version in `src/Version.h`. Match its base version in `CMakeLists.txt` and `vcpkg.json`, and keep the packed SKSE version increasing.
2. Add the matching heading to `CHANGELOG.md`, such as `## 0.9.0-rc20`.
3. Complete the relevant [gameplay checks](TESTING.md), commit, push and check the build.
4. Create and push the matching tag:

   ```sh
   git tag -a v0.9.0-rc20 -m "Quickslot 0.9.0 RC20"
   git push origin v0.9.0-rc20
   ```

The tag run builds that commit and publishes after both test jobs pass. Alpha, beta and RC tags create prereleases. A version without a suffix creates a regular release. Published versions are fixed; changes receive a new version.

Each release contains the mod, complete source, symbols, menu API, build information and SHA-256 checksums. The build information identifies the commit and Windows test results. GitHub Releases retain these files. Nexus uploads are handled separately.

## Version numbers

- Development: ongoing work, identified by commit.
- `0.10.0-beta.1`: features under testing.
- `0.10.0-rc.1`: settled features undergoing final testing and fixes.
- `0.10.0`: regular release.
- `0.10.1`: compatible fixes.
- `0.11.0`: compatible feature additions.

`1.0.0` establishes the stable public API. Subsequent incompatible API changes increment the major version. See [Semantic Versioning](https://semver.org/).

## Local packages

From a committed checkout, build the DLL and run:

```sh
python tools/test-windows.py --build build/windows --output build/test-results
python tools/package-release.py --build-dir build/windows --validation build/test-results/windows-tests.json --output-dir dist
```

For Linux cross-builds, use `build/windows-local`. Add `--tag vVERSION` for a numbered package. Packaging requires a clean checkout, passing Windows probes and an empty output directory. It verifies the tested DLL, dependency hashes and archive contents. Outputs go to `dist/`.
