# Development Guide

Simply [fork the repository](#1-forking-the-repository) and make a [pull request](#2-making-a-pull-request-pr) when you're done. You are free to add README translations to other languages too. If you found some cool seeds also add results to `README.md`.

## 0. TODO

### 0.1. Sister (shadow) seeds

Minecraft actually uses only lower 48 bits of the seed for structure generation and full 64 bits for biomes. This means It is actually more effective to first find lower 48 bits where you get quad temple and later search through remaining sister seeds to find where all temples became desert pyramids and with high % of swamp. I did not implement this logic yet, so contributions are welcome (see [issue#2](https://github.com/KK-mp4/witch-temple-finder/issues/2)).

```math
\text{"base seeds":}\quad 2^{48} = 281474976710656
```

```math
\text{"sister seeds":}\quad 2^{16} = 65536
```

### 0.2. Optimization

I'm not using [cubiomes](https://github.com/Cubitect/cubiomes) library to it's full potential. As their `README.md` states it is more efficient to get biome range instead of block by block (see [issue#3](https://github.com/KK-mp4/witch-temple-finder/issues/3)):

```cpp
    int *biomeIds = allocCache(&g, r);
    genBiomes(&g, biomeIds, r);
```

You can also do first call with low resolution to +- tell % of swamp, and then if % is high do a high resolution pass.

### 0.3. Witch hut and jungle temple rotation

Since they are asymmetrical, their bounding box can be rotated. Currently by code does not implement those rotations and you make actually get false positives (see [issue#1](https://github.com/KK-mp4/witch-temple-finder/issues/1)).

## 1. Forking the repository

Fork this Github repo and clone your fork locally. Then make changes in a local branch to the fork.

See [creating a pull request from a fork](https://docs.github.com/en/github/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork) for more information.

To fork this repository simply [click "Fork" button](https://github.com/KK-mp4/witch-temple-finder/fork).

```bash
# 1. Fork via GitHub UI
# 2. Clone your fork locally (replace your-username)
git clone git@github.com:your-username/witch-temple-finder.git
cd witch-temple-finder

# Add upstream remote (original repo)
git remote add upstream git@github.com:KK-mp4/witch-temple-finder.git
git fetch upstream
```

## 2. Making a pull request (PR)

### 2.1. PR checklist

A quick list of things to keep in mind as you're making changes:

- As you make changes
  - Make your changes in a forked repo (instead of making a branch on the main repo)
  - [Sign your commits](#23-signing-off-commits) as you go
  - Rebase from master instead of using `git pull` on your PR branch
- When you make the PR
  - Make a PR from the forked repo you made
  - Ensure the title of the PR matches [semantic release conventions](https://gist.github.com/qoomon/5dfcdf8eec66a051ecd85625518cfd13) (e.g. start with `feat:` or `fix:` or `chore:` or `docs:`)
  - Ensure you leave a release note for any user facing changes in the PR
  - Try to keep PRs smaller. This makes them easier to review
  - Assign @KK-mp4 as a reviewer

### 2.2. Good practices to keep in mind

- Fill in the description
  - What this PR does/why it's need
  - Which issue(s) this PR fixes
  - Does this PR introduce a user-facing change
- Add `WIP:` to PR name if more work needs to be done prior to review

### 2.3. Signing off commits

> [!WARNING]
> Using the default integrations with IDEs like VSCode or IntelliJ will not sign commits.

Use [git signoffs](https://docs.github.com/en/github/authenticating-to-github/managing-commit-signature-verification) to sign your commits.

Then, you can sign off commits with the `-s` flag:

```bash
git commit -s -m "My first commit"
```

### 2.4. Incorporating upstream changes from master

Use `git rebase [master]` instead of `git merge` : `git pull -r`.

Note that this means if you are midway through working through a PR and rebase, you'll have to force push:

```bash
git push --force-with-lease origin [branch name]
```

Keep your fork up to date and rebase your branch:

```bash
# Ensure local master is up-to-date with upstream
git checkout master
git fetch upstream
git rebase upstream/master

# Switch back to your branch and rebase from updated master
git checkout my-feature-branch
git rebase master

# Resolve conflicts if any, then push
git push --force-with-lease origin my-feature-branch
```

## 3. Setup with [VSCode](https://code.visualstudio.com/)

If you are using other IDE, you probably know that you are doing and able to compile C++ code yourself. Down below I will provide a simple setup.

This project includes the [`.vscode/extensions.json`](https://github.com/KK-mp4/witch-temple-finder/blob/main/.vscode/extensions.json) file, meaning that when you open project it will prompt you with "*Do you want to install recommended extensions?*" notification. Click yes.

Now click "*Left Control + Shift + P*" to open quick actions tab and search for "*CMake: Configure*". Then scan for kits and if there is none you would have to install some C++ compiler. After that is done you can compile this project for your system and run it.

To run different finders you can use *Run and Debug* tab on the left panel, there in the dropdown you can select different launch options. To modify launch options you can edit [`.vscode/launch.json`](https://github.com/KK-mp4/witch-temple-finder/blob/main/.vscode/launch.json) file.

### 3.1. Project structure

```text
└───witch-temple-finder
    │   CMakeLists.txt
    │
    ├───.vscode - VSCode settings
    ├───assets - images for README
    ├───build - compiled program
    ├───drafts - git-ignored folder for random scaps
    ├───external - cubiomes source code
    ├───include - helper functions like Java random
    ├───logs - finders output
    └───src - main seed finder source code
            location_finder.cpp - finds best temples in one seed
            main.cpp - entry point
            quad_temple_finder.cpp - finds seeds with temple clusters
            seed_finder.cpp - finds seeds with best temples
```
