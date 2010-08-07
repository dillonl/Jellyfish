#ifndef __ALLOCATORS_MMAP_HPP__
#define __ALLOCATORS_MMAP_HPP__

#include <sys/types.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdio.h>

namespace allocators {
  class mmap
  {
    void        *ptr;
    size_t      size;
   
  public:
    mmap() : ptr(MAP_FAILED), size(0) {}
    mmap(size_t _size) : ptr(MAP_FAILED), size(0) {
      realloc(_size);
      fast_zero();
    }
    ~mmap() {
      if(ptr != MAP_FAILED)
        ::munmap(ptr, size);
    }

    void *get_ptr() const { return ptr != MAP_FAILED ? ptr : NULL; }
    size_t get_size() const { return size; }

    void *realloc(size_t new_size) {
      void *new_ptr = MAP_FAILED;
      if(ptr == MAP_FAILED) {
        new_ptr = ::mmap(NULL, new_size, PROT_WRITE|PROT_READ, 
			 MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      } 
// mremap is Linux specific
#ifdef MREMAP_MAYMOVE
      else {
        new_ptr = ::mremap(ptr, size, new_size, MREMAP_MAYMOVE);
      }
#endif
      if(new_ptr == MAP_FAILED)
        return NULL;
      size = new_size;
      ptr = new_ptr;
      return ptr;
    }
    
  private:
    static const int nb_threads = 4;
    struct tinfo {
      pthread_t  thid;
      char      *start, *end;
      size_t     pgsize;
    };
    void fast_zero() {
      tinfo info[nb_threads];
      size_t pgsize = (size_t)getpagesize();
      size_t nb_pages = size / pgsize + (size % pgsize != 0);

      for(int i = 0; i < nb_threads; i++) {
        info[i].start  = (char *)ptr + pgsize * ((i * nb_pages) / nb_threads);
        info[i].end    = (char *)ptr + pgsize * (((i + 1) * nb_pages) / nb_threads);
        
        info[i].pgsize = pgsize;
        pthread_create(&info[i].thid, NULL, _fast_zero, &info[i]);
      }
      for(int i = 0; i < nb_threads; i++)
        pthread_join(info[i].thid, NULL);
    }

    static void * _fast_zero(void *_info) {
      tinfo *info = (tinfo *)_info;
      
      for(char *cptr = info->start; cptr < info->end; cptr += info->pgsize)
        *cptr = 0;

      return NULL;
    }
  };
};
#endif