# NavMesh + BT Minimum Implementation Plan

## Goal
Implement a production-safe minimum set for AI navigation and behavior control that integrates with the existing Component architecture, Editor, Scene serialization, and test pipeline.

## Scope (Minimum)
- NavMesh generation from static collider geometry (offline bake).
- Runtime path query API (find path, raycast, nearest point).
- Agent movement component (follow path, stop, repath).
- Behavior Tree runtime (Selector/Sequence/Condition/Action, Blackboard).
- Editor tools to author BT assets and assign to GameObject.
- Scene/Prefab serialization support for AI components and BT references.
- Basic debugging overlays and automated tests.

## Non-Goals (Phase 1)
- Dynamic obstacle carving.
- Crowd simulation / avoidance at high density.
- GOAP/Utility AI.
- Networked AI determinism.

## Architecture Fit
- Keep existing lifecycle: awake/start/update/lateUpdate.
- New components must follow existing addComponent/getComponent and inspectGUI pattern.
- Register systems with manager singletons where needed.

## Proposed Modules
1. DX12.AI
- NavMesh service, BT runtime, blackboard, planners.

2. DX12.Component
- NavAgentComponent, BehaviorTreeComponent.

3. DX12.Editor
- NavMesh bake window, BT graph editor window, debug overlays.

4. DX12.Scene
- Serialization support for AI components and BT asset references.

## Data Model
### NavMesh Asset
- File: Data/NavMesh/*.navmesh
- Header:
  - version (uint32)
  - buildSettingsHash (uint64)
  - sourceSceneGuid (string)
- Payload:
  - vertices
  - polygons
  - adjacency
  - area type/cost

### BT Asset
- File: Data/AI/*.btree (or FlatBuffer binary)
- Header:
  - version (uint32)
  - nodeCount
- Payload:
  - node graph (typed nodes)
  - blackboard key schema
  - root node id

## Runtime API (Minimum)
### NavMeshSystem
- bool loadNavMesh(path)
- bool hasNavMesh(sceneId)
- bool findPath(start, goal, outPath)
- bool findNearestPoint(pos, outPoint)
- bool raycast(start, end, outHit)

### NavAgentComponent
- setDestination(vec3)
- hasPath()
- stop()
- update(): path follow + repath interval

### BehaviorTreeRuntime
- tick(entity, deltaTime)
- supports:
  - Composite: Selector, Sequence
  - Decorator: Inverter, Cooldown
  - Leaf: Condition, Action
- Result enum: Success, Failure, Running

### Blackboard
- typed key-value store (bool/int/float/vector3/objectId)
- per-agent instance + optional shared tree scope

## Serialization Additions
- Scene.fbs:
  - NavAgentComponentData (speed, acceleration, stoppingDistance, repathInterval)
  - BehaviorTreeComponentData (asset_path, enabled)
- Prefab/Scene version increment with compatibility checks.
- Loader behavior:
  - Missing AI assets -> warn and disable component, not hard crash.

## Editor UX (Minimum)
1. NavMesh Bake Window
- Input layers/tags, agent radius/height, slope, step height.
- Bake button -> writes .navmesh asset.
- Show bake stats + errors.

2. BT Editor Window
- Graph node add/remove/connect.
- Node property inspector.
- Save/Load asset.

3. Debug Overlay
- Draw navmesh polygons, path lines, next waypoint.
- Show BT current active node per selected agent.

## Testing Strategy
1. Unit
- A* path query correctness.
- BT node state transitions.
- Blackboard type safety.

2. Integration
- Scene load with AI components and BT assets.
- Agent reaches destination in sample scene.

3. Regression
- AI smoke test for 3000 ticks headless.
- No crash when missing/corrupt AI assets.

## Milestones
### M1 (Week 1-2)
- Create DX12.AI module and core interfaces.
- Implement NavMesh asset I/O and basic path query.
- Add NavAgentComponent with move-to-point.

### M2 (Week 3-4)
- Implement BT runtime + Blackboard.
- BehaviorTreeComponent that ticks runtime.
- Add minimal BT asset loader.

### M3 (Week 5-6)
- Editor windows for NavMesh bake and BT graph (minimum viable).
- Serialization wiring for Scene/Prefab.
- Debug overlays.

### M4 (Week 7-8)
- Unit/integration tests and CI smoke route.
- Performance pass and stability fixes.

## Acceptance Criteria
- Agent can navigate from A to B around static obstacles.
- BT controls agent actions using conditions and blackboard values.
- Scene save/load preserves AI setup.
- Missing assets do not crash runtime/editor.
- Tests pass in Debug x64 CI.

## Risks and Mitigations
- Risk: Pathfinding perf spikes with many agents.
  - Mitigation: query budget per frame + staggered repath.
- Risk: BT graph corruption from editor operations.
  - Mitigation: transactional edits + schema version checks.
- Risk: Serialization incompatibility.
  - Mitigation: explicit version gates and migration path.
