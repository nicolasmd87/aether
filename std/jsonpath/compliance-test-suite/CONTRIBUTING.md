# Contributing to jsonpath-compliance-test-suite

The jsonpath-compliance-test-suite project team welcomes contributions from the community.

## Contribution Flow

This is a rough outline of what a contributor's workflow looks like:

- Create a topic branch from where you want to base your work
- Make commits of logical units
- Make sure your commit messages are in the proper format (see below)
- Push your changes to a topic branch in your fork of the repository
- Submit a pull request

Example:

``` shell
git remote add upstream https://github.com/jsonpath-standard/jsonpath-compliance-test-suite.git
git checkout -b my-new-feature main
git commit -a
git push origin my-new-feature
```

### Making changes to the Test Suite

You need to have [Node.js](https://nodejs.org/en/) (v18 or higher) installed to build the Test Suite.

To add or modify tests:
- add/edit the corresponding file(s) in the `tests` directory
- [optional] run the `build.sh` or `build.ps1` script located in the root folder\
  (this will be performed automatically by GitHub CI after the pull request has been merged to `main`)
- commit the changes to `tests` and `cts.json`

Do not modify `cts.json` directly!


### Tags

When adding a new test, please tag it with the appropriate tags. The full list of tags
is printed (to stderr) when running the build script. Please use the
existing tags where possible, and add new tags as needed.

You can build a subset of the test suite by specifying the tags you want to include.

_Example 1._ Build only the tests tagged with `index` OR `whitespace` (or both):

``` shell
node build.js -t index -t whitespace
```

_Example 2._ Build only the tests tagged with `index` AND `whitespace`:

``` shell
node build.js -t index -t whitespace -a
```

Example 3. Build only tests NOT tagged with `index`:

``` shell
node build.js -t index -e
```

Example 4. Exclude tests tagged with BOTH `index` AND `whitespace`:

``` shell
node build.js -t index -t whitespace -t error -e -a
```

### Checking for potential duplicates

As the test suite grows, it is becoming harder to check for potential duplicates. One way to help with this is to see
which tests are tagged with the same tags.


### Staying In Sync With Upstream

When your branch gets out of sync with the jsonpath-standard/jsonpath-compliance-test-suite/main branch, use the following to update:

``` shell
git checkout my-new-feature
git fetch -a
git pull --rebase upstream main
git push --force-with-lease origin my-new-feature
```

### Updating pull requests

If your PR fails to pass CI or needs changes based on code review, you'll most likely want to squash these changes into
existing commits.

If your pull request contains a single commit or your changes are related to the most recent commit, you can simply
amend the commit.

``` shell
git add .
git commit --amend
git push --force-with-lease origin my-new-feature
```

If you need to squash changes into an earlier commit, you can use:

``` shell
git add .
git commit --fixup <commit>
git rebase -i --autosquash main
git push --force-with-lease origin my-new-feature
```

Be sure to add a comment to the PR indicating your new changes are ready to review, as GitHub does not generate a
notification when you git push.

### Formatting Commit Messages

We follow the conventions on [How to Write a Git Commit Message](https://chris.beams.io/posts/git-commit/).

Be sure to include any related GitHub issue references in the commit message.  See
[GFM syntax](https://guides.github.com/features/mastering-markdown/#GitHub-flavored-markdown) for referencing issues
and commits.

## Reporting Bugs and Creating Issues

When opening a new issue, try to roughly follow the commit message format conventions above.
