# Releasing pcb2gcode

The project version lives in `CMakeLists.txt`:

```cmake
project(
  pcb2gcode
  VERSION 3.0.4
  ...
)
```

That number is what `pcb2gcode --version` prints, what the MCP server reports, and what G-code headers contain (`( pcb2gcode 3.0.4 )`). The git tag is created only when a draft GitHub Release is published.

## What happens on its own

Merging a pull request to `master` updates the draft release notes. CI attaches Linux tarballs to that draft after a green build. The Windows zip upload steps are turned off, so a release has Linux archives.

That automatic work leaves two things alone:

- `CMakeLists.txt` keeps its current `VERSION`.
- The draft stays unpublished, so no tag is created.

Publish the draft from the [Releases](https://github.com/pcb2gcode/pcb2gcode/releases) page when the notes, version, and archives look right. Publishing creates the tag `vX.Y.Z`.

## How to cut a version

1. Merge the changes you want onto `master` and wait until CI is green. The draft notes should list those pull requests.
2. On GitHub, open **Actions → Update release version → Run workflow**. Select the `master` branch.
3. Choose a component:
   - **auto** reads the open draft and picks major, minor, or patch. Run it again on the same draft and the version stays put.
   - **major**, **minor**, or **patch** adds one of those to the version currently in `CMakeLists.txt`. Each click adds another.
4. The workflow commits `chore: set version to X.Y.Z` as `github-actions[bot]` and starts a CI run on `master`. The push itself does not start CI; the action dispatches that run.
5. Wait for that CI run to finish. It rebuilds the binaries with the new version and replaces the draft archives. The tarball names look like `pcb2gcode-…-X.Y.Z-g<sha>.tar.gz`.
6. Open the draft, check the tag name, the notes, and `pcb2gcode --version` inside an archive, then publish it.

**auto** also leaves the version unchanged when the draft notes contain no pull request. Release Drafter writes `* No changes` in that case and still proposes a patch; the action ignores that proposal.

## How auto picks the number

Pull request titles are labeled by [.github/workflows/release-drafter.yaml](../.github/workflows/release-drafter.yaml) using the rules in [.github/release-drafter.yml](../.github/release-drafter.yml). The highest matching change wins.

| Change since the last published tag | Next component |
| --- | --- |
| A title with `!` (`feat!:`, `fix!:`, …) or the `major` label | major |
| A feature, and nothing breaking | minor |
| Anything else, including a bugfix, docs, tests, or CI | patch |

You can set the `major`, `minor`, or `patch` label by hand when the title does not say it. `minor` and `patch` override the default for that pull request. A breaking title still wins over a feature label.

The action applies that one step to the project version, including when `CMakeLists.txt` is already ahead of the newest `vN.N.N` tag. With `VERSION 3.0.4` and a last published tag of `v3.0.1`:

- a patch since that tag becomes `3.0.5`
- a feature since that tag becomes `3.1.0`
- a breaking change since that tag becomes `4.0.0`

One unpublished draft is one version. Extra features merged before you publish stay inside that same minor bump. A later breaking change raises it to the next major. The baseline is the `VERSION` from before the `chore: set version to …` commits, so a second **auto** run computes the same number.

## Pull request titles

Use a [Conventional Commit](https://www.conventionalcommits.org/en/v1.0.0/) title so the draft is sorted and the version bump is right:

```text
feat: add outline cooldown
fix: keep drill order stable
feat!: drop the legacy outline config
```

| Title prefix | Draft section |
| --- | --- |
| `feat:`, `feature:`, `enhancement:` | Features |
| `perf:` | Performance |
| `fix:`, `bug:`, `bugfix:` | Bug Fixes |
| `build:` | Build |
| `docs:`, `doc:` | Documentation |
| `test:`, `tests:`, `ci:` | Testing |
| `refactor:` | Refactoring |
| `style:` | Style |
| `chore:` | Chores |

A prefix may include a scope, as in `fix(gerber): …`. Put `!` before the colon for a breaking change.

## Version text in tests

Integration tests compare G-code bytes. Generated headers are rewritten to `( pcb2gcode VERSION )` by `tools/fix_up_expected.py` before the comparison, and the files in `tests/data` use that same token. When you refresh an expected file from a local run, keep that token in the header so the next release bump still matches.

## Files

| File | Role |
| --- | --- |
| `CMakeLists.txt` | Source of the version number |
| `.github/release-drafter.yml` | Draft sections and which labels bump major, minor, or patch |
| `.github/workflows/release-drafter.yaml` | Labels a pull request from its title |
| `.github/workflows/ci.yml` | Builds the archives and updates the draft on `master` |
| `.github/workflows/bump-version.yml` | The **Update release version** action |
| `tools/bump_release_version.py` | The script that action runs |

To try a bump locally, from a `master` checkout:

```bash
python3 tools/bump_release_version.py --part patch --write
```

**auto** needs the draft's resolved version and the last published tag, which the GitHub action supplies:

```bash
python3 tools/bump_release_version.py --part auto \
    --resolved-version 3.0.2 --last-tag v3.0.1 --write
```

The action is the path that also updates the draft and rebuilds the archives. `python3 tools/bump_release_version.py --self-test` checks the version rules.
