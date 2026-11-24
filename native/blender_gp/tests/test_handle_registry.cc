// Host test: a freed, reused-address, zero or wrong-type handle is refused.
#include <cstdio>
#include <cstdlib>
#include "project_grease_handle_registry.h"

struct A { int v; };
struct B { int v; };
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main()
{
  using RA = pg_handles::Registry<A>;
  using RB = pg_handles::Registry<B>;
  A *a = new A{1};
  const int64_t t1 = RA::add(a);
  CHECK(t1 != 0 && RA::get(t1) == a);
  CHECK(RA::get(0) == nullptr);
  CHECK(RB::get(t1) == nullptr);                 // wrong type
  CHECK(RA::remove(t1) == a);
  delete a;
  CHECK(RA::get(t1) == nullptr);                 // freed
  CHECK(RA::remove(t1) == nullptr);              // double free refused
  // an address reused by a later object does not revive the old token
  bool reused = false;
  for (int i = 0; i < 64 && !reused; i++) {
    A *b = new A{2};
    const int64_t t2 = RA::add(b);
    CHECK(t2 != t1 && t2 > t1);
    CHECK(RA::get(t1) == nullptr);
    reused = (b == a);
    RA::remove(t2);
    delete b;
  }
  CHECK(RA::add(nullptr) == 0);
  std::puts("handle registry tests passed");
  return 0;
}
