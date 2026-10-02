# Independent reference for the Offset (random mode, translation only) and Noise (position factor)
# deformStroke() bodies of Blender 3.6.23, written from the pinned MOD_gpencil_legacy_offset.c /
# MOD_gpencil_legacy_noise.c with the canvas mapping documented in project_grease_modifier_stack.h.
# Prints the expected canvas coordinates embedded in tests/test_modifier_stack.cc.
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from gen_rng_golden import f32, hash_int_2d, hash_int_01, hash_string, halton_3d

def i32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v

def fmodf(a, b): return f32(math.fmod(f32(a), f32(b)))

def offset_point(seed_param, stroke_index, rnd_offset, pt):
    seed = i32(seed_param + hash_string("Project Grease") + hash_string("Offset"))
    rand_offset = hash_int_01(seed & 0xFFFFFFFF)
    r = halton_3d([2, 3, 7], [0.0, 0.0, 0.0], stroke_index)
    rand0 = []
    for i in range(3):
        v = fmodf(r[i] * 2.0 - 1.0 + rand_offset, 1.0)
        v = fmodf(math.sin(f32(f32(v) * f32(12.9898)) + 0.0) * f32(43758.5453), 1.0)
        rand0.append(v)
    x, y, z = pt[0], -pt[1], pt[2]                 # canvas -> object space (y up)
    x += rnd_offset[0] * rand0[0]; y += rnd_offset[1] * rand0[1]; z += rnd_offset[2] * rand0[2]
    return (x, -y, z)                              # back to canvas

def noise_points(seed_param, stroke_index, cfra, step, factor, pts):
    # two-point stroke: normal is (1,1,1); noise_scale 1, noise_offset 0, use_random on
    seed = i32(seed_param + stroke_index + hash_string("Project Grease") + hash_string("Noise") + cfra // step)
    useed = (seed + 2) & 0xFFFFFFFF
    table = [hash_int_01(hash_int_2d(useed, i + 0 + 1)) for i in range(4)]
    s = 100.0
    p = [[c[0] / s, -c[1] / s, c[2] / s] for c in pts]
    v1 = [p[0][k] - p[1][k] for k in range(3)]
    n = [1.0, 1.0, 1.0]
    v2 = [v1[1] * n[2] - v1[2] * n[1], v1[2] * n[0] - v1[0] * n[2], v1[0] * n[1] - v1[1] * n[0]]
    l = math.sqrt(sum(c * c for c in v2)); v2 = [c / l for c in v2]
    out = []
    for i in range(2):
        d = (table[i] * 2.0 - 1.0) * factor * 0.1
        q = [p[i][k] + v2[k] * d for k in range(3)]
        out.append((q[0] * s, -q[1] * s, q[2] * s))
    return out

if __name__ == '__main__':
    for k in (0, 1, 2):
        print('offset seed 7 stroke', k, offset_point(7, k, (30.0, 30.0, 0.0), (100.0, 200.0, 0.0)))
    print('noise seed 3 stroke 0 cfra 4 step 4', noise_points(3, 0, 4, 4, 1.0, [(100.0, 200.0, 0.0), (160.0, 260.0, 0.0)]))
    print('noise seed 3 stroke 1 cfra 4 step 4', noise_points(3, 1, 4, 4, 1.0, [(100.0, 200.0, 0.0), (160.0, 260.0, 0.0)]))
    print('noise seed 3 stroke 0 cfra 20 step 4', noise_points(3, 0, 20, 4, 1.0, [(100.0, 200.0, 0.0), (160.0, 260.0, 0.0)]))
