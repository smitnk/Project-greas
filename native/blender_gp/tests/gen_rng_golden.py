# Independent reference for the Blender 3.6.23 RNG/hash/halton code, written from the pinned source
# (BLI_rand.hh RandomNumberGenerator, BLI_hash.h, rand.cc halton_ex). Does not run Blender.
import struct, math
M32 = 0xFFFFFFFF
def f32(x): return struct.unpack('f', struct.pack('f', x))[0]
def rot(x,k): return ((x << k) | (x >> (32-k))) & M32
def final(a,b,c):
    c ^= b; c = (c - rot(b,14)) & M32
    a ^= c; a = (a - rot(c,11)) & M32
    b ^= a; b = (b - rot(a,25)) & M32
    c ^= b; c = (c - rot(b,16)) & M32
    a ^= c; a = (a - rot(c,4)) & M32
    b ^= a; b = (b - rot(a,14)) & M32
    c ^= b; c = (c - rot(b,24)) & M32
    return a,b,c
def hash_int_2d(kx,ky):
    a=b=c=(0xdeadbeef + (2<<2) + 13) & M32
    a=(a+kx)&M32; b=(b+ky)&M32
    return final(a,b,c)[2]
def hash_int_01(k): return f32(f32(float(hash_int_2d(k,0))) * f32(1.0/f32(float(0xFFFFFFFF))))
def hash_string(s):
    i=0
    for ch in s.encode(): i=(i*37+ch)&M32
    return i
class Rng:
    def __init__(s,seed): s.x=((seed&M32)<<16)|0x330E
    def step(s): s.x=(0x5DEECE66D*s.x+0xB)&0x0000FFFFFFFF_FFFF if False else (0x5DEECE66D*s.x+0xB)&0xFFFFFFFFFFFF
    def get_int(s): s.step(); return (s.x>>17)&0x7FFFFFFF
    def get_float(s): return f32(f32(float(s.get_int()))/f32(float(0x80000000)))
def halton_ex(inv, off):
    e = abs((1.0-off[0])-1e-10)
    if inv >= e:
        h=inv
        while True:
            last=h; h*=inv
            if not (h>=e): break
        off[0] += ((last+h)-1.0)
    else:
        off[0] += inv
    return off[0]
def halton_3d(primes, offset, n):
    inv=[1.0/p for p in primes]; r=[0.0,0.0,0.0]
    for s in range(n):
        for i in range(3):
            o=[offset[i]]; r[i]=halton_ex(inv[i],o); offset[i]=o[0]
    return r
if __name__=='__main__':
    for seed in (0,1,12345):
        g=Rng(seed); print('rng seed',seed,'ints',[g.get_int() for _ in range(6)])
        g=Rng(seed); print('rng seed',seed,'floats',[repr(g.get_float()) for _ in range(4)])
    print('hash_int_2d',[(a,b,hash_int_2d(a,b)) for a,b in ((0,0),(1,0),(7,3),(12345,99),(4294967295,1))])
    print('hash_int_01',[(k,repr(hash_int_01(k))) for k in (0,1,2,12345)])
    print('hash_string',[(s,hash_string(s)) for s in ('','Noise','Offset','GPencil')])
    for n in (1,2,5,9):
        print('halton_3d n=%d'%n, [repr(v) for v in halton_3d([2,3,7],[0.0,0.0,0.0],n)])
