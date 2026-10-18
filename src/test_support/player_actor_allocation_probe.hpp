#pragma once
#include <cstdlib>
#include <limits>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
// Include once in a standalone test executable; never linked into the runtime.
// Test-only, thread-local one-shot allocation failure. Runtime sources have no
// test hook. Only operator-new allocations in the guarded invocation are counted.
namespace allocation_probe {
thread_local bool enabled=false;
thread_local std::size_t calls=0,fail_at=std::numeric_limits<std::size_t>::max();
void count(){if(enabled&&++calls==fail_at){enabled=false;throw std::bad_alloc{};}}
void* plain(std::size_t n){count();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void* aligned(std::size_t n,std::size_t alignment){count();void* p=nullptr;
#ifdef _WIN32
 p=_aligned_malloc(n?n:1,alignment);
#else
 if(posix_memalign(&p,alignment,n?n:1)!=0)p=nullptr;
#endif
 if(p)return p;throw std::bad_alloc{};
}
void release_aligned(void* p)noexcept{
#ifdef _WIN32
 _aligned_free(p);
#else
 std::free(p);
#endif
}
struct Scope{explicit Scope(std::size_t ordinal=std::numeric_limits<std::size_t>::max()){calls=0;fail_at=ordinal;enabled=true;}~Scope(){enabled=false;}};
}
void* operator new(std::size_t n){return allocation_probe::plain(n);}
void* operator new[](std::size_t n){return allocation_probe::plain(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){return allocation_probe::aligned(n,static_cast<std::size_t>(a));}
void* operator new[](std::size_t n,std::align_val_t a){return allocation_probe::aligned(n,static_cast<std::size_t>(a));}
void operator delete(void* p,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete[](void* p,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
