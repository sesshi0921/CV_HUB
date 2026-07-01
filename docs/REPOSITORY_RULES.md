# Repository Rules

## Branch Protection

`main` is the protected branch. Direct pushes to `main` should be avoided after the initial architecture checkpoint.

Recommended GitHub branch protection:

- Require pull request before merging
- Require at least one approval
- Require status checks before merge once CI exists
- Require branches to be up to date before merge
- Block force pushes
- Block branch deletion
- Require conversation resolution before merge

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
