# TopOptFrame Baseline Inventory

Date: 2026-05-07

## Purpose

This document records the current baseline of the application before tutorial-oriented product changes.

It answers three questions:

1. What is already usable today
2. What works but is not tutorial-ready yet
3. What is still placeholder or incomplete

## Confirmed Baseline

- The project can run at least one topology optimization sample successfully.
- `Debug` and `Release` desktop builds can run successfully.
- GPU builds can run successfully.

This means the project is already beyond a prototype shell. The current gap is productization for teaching, not basic executability.

## A. Features Already Usable For Teaching

These features can already support instructor demos or guided student exercises with limited extra explanation.

### 1. Node-based workflow editing

- Node canvas exists and is interactive.
- Nodes can be created from the node library.
- Nodes can be drag-dropped into the canvas.
- Connections and graph execution order are managed by the editor/executor stack.
- Basic delete / clear / selection workflows already exist.

### 2. Topology optimization execution path

- The graph executor supports a full workflow from domain, material, boundary conditions, solver, topology optimization, and post-processing nodes.
- SIMP execution is implemented.
- Density-field results and FE results are cached and reused for preview.
- Step execution and full-graph execution are both available.

### 3. FEA and optimization-related node categories

The node registry already exposes a meaningful teaching-oriented set of categories:

- `Input`
- `Output`
- `Data`
- `Domain`
- `FEA`
- `Topology`
- `PostProcess`

This is already close to a course structure and does not need conceptual redesign.

### 4. 3D visualization baseline

- The 3D view supports orbit, pan, zoom, camera reset, standard views, wireframe toggle, grid, and axes.
- STL and OBJ loading are supported.
- Execution results can be pushed directly into the view as triangles.
- Density playback controls already exist for `post-density-view`.

### 5. Project file workflow baseline

- New / Open / Save / Save As entry points already exist.
- Windows file dialogs are connected.
- Project serialization stores nodes, connections, editor state, split ratios, and camera state.

### 6. Logging and runtime feedback

- The log panel is usable.
- Execution logs, warnings, and errors are visible in the UI.
- The status bar exposes node count, connection count, current file, and active tool.

## B. Features Usable But Not Yet Tutorial-ready

These features work technically, but they still reflect a developer-oriented tool rather than a student-facing teaching product.

### 1. Entry experience

- The application opens directly into the workbench.
- There is no welcome screen.
- There is no teaching mode entry.
- There is no guided experiment selection flow.

### 2. Node interaction model

- Users can build graphs, but they still need to understand the node system first.
- There is no guided workflow generator.
- There is no "build a standard topology optimization case" shortcut.

### 3. Property editing

- Parameters are editable, but they are not explained.
- There are no teaching hints, recommended ranges, or unit descriptions.
- Advanced and beginner parameters are not separated.

### 4. Result interpretation

- Results can be produced, but the application does not yet explain what they mean for a learner.
- There is no dedicated result interpretation panel for course use.
- There is no integrated experiment instruction or "what to observe" guidance.

### 5. Teaching content management

- Example assets exist, but there is no formal tutorial case library or lesson metadata system.
- There is no distinction between free exploration and course exercises.

## C. Placeholder / Incomplete / Not Yet Implemented

These items should not be treated as finished capabilities in course planning.

### 1. Resource browser

- The current `ModulePanel` is still a placeholder-style resource browser.
- Its tree and preview content are static UI scaffolding, not a real project content manager.

### 2. Console tab

- The `Console` tab currently shows only placeholder text.
- It is not yet a real execution console or solver progress panel.

### 3. Domain generation and import gaps

- `domain-from-mesh` volumetric meshing is not implemented.
- `domain-import` FE mesh import is not implemented.

### 4. Export gaps

- `output-export` is currently placeholder behavior.
- `post-export` file export is also placeholder behavior.

### 5. Algorithm completeness gaps

- `topo-beso` currently falls back to a SIMP-like implementation and is not a fully independent BESO path.
- `topo-constraint` is currently pass-through placeholder behavior.

### 6. File/format gaps

- `VDB` loading is not implemented.

## D. Tutorial-readiness Classification

### Ready for immediate guided demo

- Standard node-based topology optimization demonstration
- Guided instructor demo with existing sample projects
- Simple student reproduction under supervision

### Needs product-layer work before student self-use

- Course home page
- Case selection and lesson descriptions
- Parameter explanations
- Workflow guidance
- Better result interpretation

### Must not be advertised as complete yet

- Real project resource browser
- Console/solver progress workspace
- Mesh-to-FE conversion from arbitrary surface mesh
- FE mesh import
- Production export pipeline
- Full BESO implementation
- Constraint-processing workflow
- VDB support

## E. Practical Conclusion

The current application is already strong enough to serve as:

- an internal research/demo tool
- an instructor-guided teaching demo
- a foundation for a tutorial application

It is not yet strong enough to serve as:

- a self-guided student tutorial product
- a polished classroom exercise platform
- a complete topology optimization teaching suite

## F. Recommended Next Focus

The next highest-value steps are:

1. Add tutorial-facing entry flow and case selection
2. Add workflow guidance for standard topology optimization tasks
3. Add teaching-oriented parameter explanations
4. Replace placeholder resource and console panels with real teaching panels
5. Keep placeholder capabilities clearly documented so they are not mistaken for finished features
