# Project Grease L1 blocker scope

## Locked exclusions

- Armature modifier is excluded from the required Project Grease L1 feature scope. Do not add Armature to the L1 completion gate, dependency closure, tests, or CI.
- D1 remains .blend reader only; no .blend writer.
- D2 remains full evaluated-mesh parity for Shrinkwrap. A base-mesh-only implementation is not acceptable.
- Do not replace Blender 3.6.23 algorithms with custom approximations where a required Blender Legacy GP algorithm can be ported.

## Active L1 blockers

1. Shrinkwrap: real Blender 3.6.23 Legacy GP Shrinkwrap using an evaluated target mesh and BVH.
2. Line Art: close the scoped Blender 3.6.23 Legacy GP Line Art parity defined by the dependency audit; do not equate Scene-lite with the final implementation.
3. Animation: close the L1-required frame evaluation/hold/interpolation/onion/multiframe behavior defined by the existing project specification.
4. ARM64/x86_64: add behavioral parity evidence; native link closure alone is insufficient.
5. Android runtime: perform real runtime validation; APK compilation alone is insufficient.

## First implementation unit: Shrinkwrap

Pinned Blender source commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca (Blender 3.6.23).

Imported verbatim source units in this directory:
- blender_3_6_23_shrinkwrap.cc — BKE shrinkwrap core.
- BKE_shrinkwrap.h — public shrinkwrap API/types.
- MOD_gpencil_legacy_shrinkwrap.c — authoritative Legacy GP modifier entry path.

These files are source-port inputs, not a claim that Shrinkwrap is complete. The next engineering step is to close their minimum Android-compatible dependency closure, then wire the real evaluated-mesh provider and modifier stack, followed by reference tests.
