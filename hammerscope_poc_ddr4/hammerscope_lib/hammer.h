#pragma once
#include <asm/unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/kernel-page-flags.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <iterator>
#include <random>
#include <chrono>
#include <ctime>
#include <bits/stdc++.h>

#define DEBUG_PRINT

#define ONE_KILO         (1<<10)
#define ONE_MEGA         (1ULL<<20)
#define PAGE_SIZE        (1ULL<<12)
#define _4G              (1ULL<<32)
//#define ROW_SIZE (256*ONE_KILO)
#define ROW_SIZE         (8*ONE_KILO)
#define PAGES_IN_ROW     (ROW_SIZE/PAGE_SIZE)
#define NUMBER_OF_READS  (128*1024)
#define MAX_UINT64       ((uint64_t)-1)
#define QWORDS_IN_PAGE   (PAGE_SIZE/sizeof(uint64_t))
#define CH_DIMM_RNK_MASK (0xFFFFull<<16)

//errors
#define OK                    ( 0)
#define BAD_INDEX             (-1)
#define NOT_ALLOCATED_ROWS    (-2)
#define UNMAP_FAILED          (-3)
#define UNINIT_MEMORY_DB      (-4)
#define MAPPING_FAILED        (-5)
#define REMOVE_MAPPING_FAILED (-6)
#define CANT_HAMMER_ROW       (-7)
#define DRAM_GEOMETRY_NOT_FIT (-8)
#define PAGEMAP_OPEN_FAIL     (-9)
#define RAMSES_BADADDR        ((uint64_t)-1)

#define LS_BITMASK(n) ((1ull << (n)) - 1)
#define BIT(n,x)      (((x) >> (n)) & 1)
#define POP_BIT(n,x)  (((x) & LS_BITMASK(n)) + (((x) >> ((n)+1)) << (n)))

#define GET_BITS(num, width, offset) ((num & (LS_BITMASK(width) << offset)) >> offset)

#define MW_BITS  (3)
#define COL_BITS (10)
#define ROW_BITS (16)
#define BANK_OFF (MW_BITS + COL_BITS)
	
#define INTEL_DUALCHAN_MASK (1 << 2)
#define INTEL_DUALDIMM_MASK (1 << 1)
#define INTEL_DUALRANK_MASK (1 << 0)

#define HASH_FN_CNT (6)
#define MAX_BNK_MASK ((1ull<<38)-1)
#define HUGETLB_PATH "/mnt/huge/buff"

#define _2MB_BITS (21)
#define _2MB_MASK ((1ull << _2MB_BITS)-1)
#define MAP_HUGE_2MB (_2MB_BITS << MAP_HUGE_SHIFT)
#define MAP_HUGETLB_2MB (MAP_HUGETLB | MAP_HUGE_2MB)

#define _1GB_BITS (30)
#define _1GB_MASK ((1ull << _1GB_BITS)-1)
#define MAP_HUGE_1GB (_1GB_BITS << MAP_HUGE_SHIFT)
#define MAP_HUGETLB_1GB (MAP_HUGETLB | MAP_HUGE_1GB)

#define MAX_AGGR_NUM (20)
#define PAGE_MASK ((PAGE_SIZE)-1)

#define MAX_BNK_CNT (16)

namespace HammerScope
{ 
  struct AddrFns{
	  uint64_t lst[HASH_FN_CNT];
	  uint64_t len;
  };

  struct DRAMLayout{
	  AddrFns hash_functions;
	  uint64_t row_mask;
	  uint64_t col_mask;
  };

  struct TimeResult{
    uint64_t start_time;
    uint64_t end_time;
    uint64_t flip;
    uint64_t value;
  };

  struct DRAMPhysicalLocation{
    union{
      struct{
	      uint64_t col  :10;
	      uint64_t row  :16;
	      uint64_t bank :38;
	      //uint64_t rank :1;
	      //uint64_t dimm :2;
	      //uint64_t chan :1;
      };
      uint64_t data;
    };
  };

  uint64_t GetPhysicalMemorySize();
  uint64_t GetCurrentTimeNS();
  uint64_t GetPageFrameNumber(int, uint8_t*);
  void DoubleSideHammer(uint64_t*, uint64_t*, uint64_t);
  void NSideHammer(std::vector<uint64_t*>, uint64_t, uint64_t, uint64_t);
  void BinNSideHammer(std::vector<uint64_t*>, uint64_t, uint64_t, uint64_t, TimeResult*);
  void BinNSideHammerUTRR(std::vector<uint64_t*>, uint64_t, uint64_t, uint64_t, uint64_t, TimeResult*);
  uint64_t SetupMapping(uint64_t*, void**, int*, double);
  uint64_t SetupSharedMapping(uint64_t*, void**, int*, double);
  int RemoveMapping(uint64_t, void*, int);
  void MemFlush(void *, size_t);
  uint64_t rdtsc();
  uint64_t get_dram_row(uint64_t, uint64_t);
  uint64_t get_dram_col(uint64_t, uint64_t);
  DRAMPhysicalLocation phys_2_dram(uint64_t, DRAMLayout);
  uint64_t dram_2_phys(DRAMPhysicalLocation, DRAMLayout);
  DRAMLayout getDRAMLayoutCfg(uint64_t);
  uint64_t rdtscp();
}

using namespace HammerScope;
using namespace std;
