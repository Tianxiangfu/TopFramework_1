# Known Issues

Use this file for confirmed issues only.

## Open Issues

- Title: `ModulePanel` is still a static placeholder resource browser
- Status: `open`
- Scope: left-side resource browsing / preview workflow
- Reproduction: open the `Browse` tab and inspect the file tree and preview panel
- Impact: cannot be used as a real course case browser or project asset manager
- Workaround: use existing sample files and direct project loading instead

- Title: `Console` tab is placeholder-only
- Status: `open`
- Scope: execution feedback and solver progress display
- Reproduction: open the `Console` tab in the main UI
- Impact: students cannot inspect detailed runtime progress in a dedicated console view
- Workaround: use the log panel

- Title: `domain-from-mesh` is not implemented
- Status: `open`
- Scope: mesh-to-FE domain workflow
- Reproduction: execute a graph using the `domain-from-mesh` node
- Impact: arbitrary surface-mesh-based tutorial cases cannot rely on this node yet
- Workaround: use `domain-box` or `domain-lshape`

- Title: `domain-import` FE mesh import is not implemented
- Status: `open`
- Scope: imported FE mesh workflow
- Reproduction: execute a graph using the `domain-import` node
- Impact: imported FE tutorial cases cannot rely on this node yet
- Workaround: use built-in domain generators

- Title: export nodes are placeholder implementations
- Status: `open`
- Scope: result and file export workflows
- Reproduction: execute `output-export` or `post-export`
- Impact: tutorial reporting and artifact export are not production-ready
- Workaround: rely on in-app visualization and manual screenshots for now

- Title: `topo-beso` is not a full independent BESO implementation
- Status: `open`
- Scope: topology method coverage
- Reproduction: inspect or execute the `topo-beso` path
- Impact: BESO should not yet be presented as fully implemented course functionality
- Workaround: use SIMP as the primary teaching method

- Title: `topo-constraint` is currently placeholder pass-through behavior
- Status: `open`
- Scope: design-constraint teaching workflows
- Reproduction: execute a graph with `topo-constraint`
- Impact: design-constraint lessons cannot rely on real constraint processing yet
- Workaround: avoid making it part of early course content

- Title: `VDB` loading is not implemented
- Status: `open`
- Scope: volumetric asset loading
- Reproduction: attempt to load a `.vdb` file
- Impact: VDB should not be included in tutorial workflows
- Workaround: use STL/OBJ assets

## Recording Format

- Title:
- Status: `open` / `mitigated` / `resolved`
- Scope:
- Reproduction:
- Impact:
- Workaround:
