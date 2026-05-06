# Project Decisions

Use this file to record important planning and design decisions that should survive across sessions.

## Decisions

### 2026-05-06 - Use repository files for long-term project memory

- Decision: Track roadmap, active status, issues, and planning decisions in repository markdown files instead of relying on chat history.
- Why: This project is too large to manage reliably within a single conversation.
- Files:
- `TODO.md`
- `STATUS.md`
- `KNOWN_ISSUES.md`
- `DECISIONS.md`
- Consequence: Every substantial work session should update the relevant tracking files before commit/push.

### 2026-05-07 - Treat build and sample execution as existing baseline capability

- Decision: Consider successful Debug / Release / GPU execution and at least one working topology optimization sample as already-established baseline capability.
- Why: This prevents repeating low-value validation work and keeps `Phase 0` focused on documenting current behavior and identifying actual gaps for tutorialization.
- Consequence:
- `Phase 0` should now emphasize feature inventory, file workflow verification, STL/OBJ interaction verification, and issue logging.
- Tutorial-oriented development can begin as soon as those remaining baseline checks are documented clearly.
