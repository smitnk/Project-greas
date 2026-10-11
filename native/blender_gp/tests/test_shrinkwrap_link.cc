/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BKE_shrinkwrap.h"
#include "DNA_gpencil_modifier_types.h"

int main()
{
  /* Link-time reachability probe: force the real Blender 3.6.23 Legacy GP
   * Shrinkwrap entry point into the test binary. This does not execute it
   * without a real evaluated Object/Mesh fixture. */
  using ShrinkwrapFn = void (*)(ShrinkwrapGpencilModifierData *,
                                Object *,
                                MDeformVert *,
                                int,
                                float (*)[3],
                                int);
  ShrinkwrapFn fn = &shrinkwrapGpencilModifier_deform;
  return fn != nullptr ? 0 : 1;
}
