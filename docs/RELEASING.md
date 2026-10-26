# Releases

`master` is the development branch. Push commits to run the Linux tests and Windows build. Successful **Build and test** runs contain a `Quickslot-…` artifact with installable ZIP packages. Development filenames include the commit ID; artifacts are retained for 30 days.

## Publish a version

1. Set the version in `src/Version.h`. Match its base version in `CMakeLists.txt` and `vcpkg.json`, and keep the packed SKSE version increasing.
2. Add the matching heading to `CHANGELOG.md`, such as `## 0.10.0`.
3. Complete the relevant [gameplay checks](TESTING.md), commit, push and check the build.
4. Create and push the matching tag:

   ```sh
   git tag -a v0.10.0 -m "Quickslot 0.10.0"
   git push origin v0.10.0
   ```

The tag run builds that commit and publishes after both test jobs pass. Alpha, beta and RC tags create prereleases. A plain version tag, such as `v0.11.0`, creates a regular release. Publish subsequent changes under a new version.

Each release contains the mod, complete source, symbols, menu API, build information and SHA-256 checksums. The build information identifies the commit and Windows test results. GitHub Releases retain these files. Nexus uploads use the separate manual workflow below.

## Upload a published release to Nexus Mods

The **Upload release to Nexus** GitHub Actions workflow is manually triggered. It downloads the runtime ZIP from a published standard GitHub release, verifies the tag, successful build/test run, checksums and tested DLL, then uses the [official Nexus upload action](https://github.com/Nexus-Mods/upload-action) to add a version to the existing main file. It updates the mod version and changelog, sets the main mod-manager download, and archives the previous file version. Edit the main page description and screenshots on Nexus Mods.

One-time setup in **Settings → Environments → nexus**:

- Add the secret `NEXUSMODS_API_KEY` using your Nexus Mods API key. Keep the key out of repository files, issues and chat.
- Add variables `NEXUSMODS_FILE_ID` and `NEXUSMODS_MOD_ID`. Use the API IDs shown by **Advanced** in the public mod page's Files tab or the edit menu in Manage Files; these identify the existing main file and mod page.

After the GitHub release build finishes, open **Actions → Upload release to Nexus → Run workflow**, select `master`, and enter the release tag, such as `v0.10.0`. Choose a published standard release with a successful build/test run. If an upload fails partway through, check the Nexus Files and Changelog tabs before retrying.

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
