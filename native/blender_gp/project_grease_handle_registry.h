// Generation-checked JNI handles. Kotlin holds an opaque token, never a raw pointer: tokens come
// from one process-wide counter (shared by every handle type) and are never reused, so a call with a
// freed handle, a handle whose address was reused by a later object, or a handle of the wrong type
// finds nothing and is refused instead of touching freed memory.
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace pg_handles {

inline std::atomic<uint64_t> &token_counter()
{
  static std::atomic<uint64_t> c{0};
  return c;
}

template<class T> class Registry {
 public:
  static int64_t add(T *p)
  {
    if (!p) return 0;
    std::lock_guard<std::mutex> l(inst().m);
    const int64_t token = static_cast<int64_t>(token_counter().fetch_add(1) + 1);
    inst().live[token] = p;
    return token;
  }
  /* nullptr for 0, an unknown, a freed or a wrong-type token */
  static T *get(int64_t token)
  {
    std::lock_guard<std::mutex> l(inst().m);
    auto it = inst().live.find(token);
    return it == inst().live.end() ? nullptr : it->second;
  }
  /* Invalidates the token and returns its object (nullptr if it was not live: a double free). */
  static T *remove(int64_t token)
  {
    std::lock_guard<std::mutex> l(inst().m);
    auto it = inst().live.find(token);
    if (it == inst().live.end()) return nullptr;
    T *p = it->second;
    inst().live.erase(it);
    return p;
  }

 private:
  std::mutex m;
  std::unordered_map<int64_t, T *> live;
  static Registry &inst() { static Registry r; return r; }
};

}  // namespace pg_handles
