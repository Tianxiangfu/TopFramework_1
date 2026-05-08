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

### 2026-05-07 - Phase 1 first pass uses one active lesson and two announced lesson slots

- Decision: The first tutorial-home implementation should ship with one real loadable lesson and two clearly marked preparation slots, instead of inventing incomplete example projects.
- Why: The repository currently contains only one confirmed example case suitable for immediate integration, and forcing fake lesson files would create misleading product behavior.
- Consequence:
- The tutorial home can still present the intended course structure immediately.
- Only the cantilever lesson is loadable in this pass.
- Additional lesson projects can be activated incrementally without redesigning the home screen.

### 2026-05-07 - Keep the current theme and improve typography first

- Decision: Do not replace the existing dark theme yet; improve readability first by introducing title/body/small font hierarchy and safer font loading fallback.
- Why: The current UI weakness is primarily visual hierarchy and oversized uniform text, not the base color theme itself.
- Consequence:
- Theme-color refactoring is deferred.
- Future visual polish should build on this font hierarchy instead of scaling the whole window font uniformly.

### 2026-05-08 - Use a standalone workflow-state model as the base for Phase 2

- Decision: Implement Phase 2.1 as a dedicated `TutorialWorkflow` model that owns ordered lesson steps, step status, required node types, and per-step issues.
- Why: The wizard panel, validation layer, and execution gating all need the same source of truth; embedding this logic directly into UI code would create rework.
- Consequence:
- Phase 2.2 should render from the workflow model instead of hardcoding step order in the panel.
- Phase 2.3 and 2.4 should update step status and issues through the workflow model instead of inventing a second validation structure.

### 2026-05-08 - First workflow panel version should reflect the current graph immediately

- Decision: The first Phase 2.2 workflow panel should scan the currently loaded graph and mark steps completed automatically, instead of forcing all lesson cases to start from step 1.
- Why: The repository already includes complete teaching examples, and showing them as entirely unfinished would be misleading.
- Consequence:
- Opening a complete example should show most or all core steps as completed.
- Empty or partial graphs should still surface pending or configuration-error states.
