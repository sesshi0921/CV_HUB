# Repository Rules

## Branch Strategy

`main` is the release/stable branch. `develop` is the integration branch for normal development.

Expected flow:

```text
feature/* or fix/*
  -> pull request
  -> develop
  -> pull request
  -> main
```

`main` must only be updated by a pull request whose source branch is `develop`. GitHub branch protection cannot directly inspect the source of a direct push, so direct pushes to `main` must be blocked and a required check must enforce that `main` pull requests come from `develop`.

`develop` should also be updated through pull requests once CI exists. A branch cannot be protected by "CI before direct push" because CI runs after the push has already happened. To guarantee CI has passed before code enters `develop`, direct pushes to `develop` must be blocked and required status checks must pass before merge.

## GitHub Rulesets

Prefer GitHub rulesets over legacy branch protection rules when available, because rulesets can protect branch creation, update, deletion, and bypass behavior consistently.

### `main` Ruleset

Target:

- Branch name pattern: `main`

Rules:

- Restrict deletions
- Block force pushes
- Require a pull request before merging
- Require conversation resolution before merge
- Require at least one approval when the project has multiple contributors
- Require status checks before merge
- Require branches to be up to date before merge

Required status checks:

- `main-source-branch` after the workflow exists
- Common build/test checks after CI exists

Bypass:

- No regular user bypass
- Repository administrators may keep emergency bypass only if it is audited and used rarely

Source branch enforcement:

- Add a CI check named `main-source-branch`.
- The check must pass only when `github.base_ref == 'main'` and `github.head_ref == 'develop'` for pull requests targeting `main`.
- Mark `main-source-branch` as required in the `main` ruleset.

### `develop` Ruleset

Target:

- Branch name pattern: `develop`

Rules:

- Restrict deletions
- Block force pushes
- Require a pull request before merging once CI exists
- Require status checks before merge once CI exists
- Require conversation resolution before merge
- Require branches to be up to date before merge when CI runtime is acceptable

Required status checks after CI is introduced:

- CMake configure
- CMake build
- Unit tests
- Formatting or lint checks when introduced

Interim behavior before CI exists:

- Keep force pushes and deletion blocked.
- Allow PR merges into `develop` without required checks only until the first CI workflow is merged.
- After the first CI workflow is available, immediately mark the CI checks as required.

## Commit Policy

- Keep commits focused and descriptive.
- Include architecture documentation updates with architecture-sensitive code changes.
- Do not commit generated build outputs, local caches, or dependency download directories.
- Prefer small vertical slices over large unreviewable changes.

## Pull Request Policy

Each PR should state:

- Purpose
- Main files changed
- Validation performed
- Known limitations

For implementation PRs, include tests or a clear reason tests are not yet possible.

## Architecture Guardrails

- Keep README limited to installation and execution instructions.
- Keep canonical architecture in `docs/ARCHITECTURE.md`.
- Keep Codex development guidance in `AGENTS.md` and `.codex/skills/cv-hub-dev`.
- Core interfaces must not depend on OpenCV, PyTorch, ImGui, or other concrete external library types.
- Plugin-specific behavior belongs inside `plugins/<name>/`.
- UI rendering must depend on descriptors and runtime state, not concrete plugin implementations.
- Node graph execution must validate type compatibility before running.

## Initial Checkpoint

The current architecture documentation can be pushed directly to `main` as the initial checkpoint. After that checkpoint, use PR-based changes for implementation work.
