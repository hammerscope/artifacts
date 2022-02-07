#include "hammer.h"

namespace HammerScope
{
  DRAMLayout getDRAMLayoutCfg(uint64_t cfg)
  {
    DRAMLayout mem_layout = {0};
    switch (cfg)
    {
    case 1:
      mem_layout = DRAMLayout({{{0x4080,0x48000,0x90000,0x120000,0x1b300}, 5}, 0xffffc0000, ROW_SIZE-1});
      break;
    case 2:
      printf("[!] DRAMLayout #2\n");
      mem_layout = DRAMLayout({{{0x2000,0x14000,0x28000}, 3}, 0xfffff0000, ROW_SIZE-1});
      break;
    
    default:
      printf("[!] unsupported DRAM config\n");
      assert(0);
      break;
    }
    return mem_layout;
  }

  uint64_t rdtscp()
  {
	  uint64_t lo, hi;
	  __asm__ volatile ("rdtscp\n":"=a" (lo), "=d"(hi) ::"%rcx");
	  return (hi << 32) | lo;
  }

  void MemFlush(void *buffer, size_t buflen)
  {
	  const size_t CACHE_LINE_SIZE = 64;
	  for (size_t i = 0; i < buflen; i += CACHE_LINE_SIZE) {
		  __asm__ volatile ("clflushopt (%0)\n\t" : : "r" ((char *)buffer + i) : "memory");
	  }
  }

  uint64_t GetPhysicalMemorySize() 
  {
    struct sysinfo info = {0};
    sysinfo( &info );
#ifdef DEBUG_PRINT
    //printf("[+] Total Ram Size: 0x%lx\n", info.totalram);
    //printf("[+] Memory Units: 0x%lx\n", info.mem_unit);
    //printf("[+] Physical Memory Size: 0x%lx\n", info.totalram * info.mem_unit);
#endif
    return info.totalram * info.mem_unit;
  }

  uint64_t GetCurrentTimeNS() 
  {
    std::chrono::time_point<std::chrono::high_resolution_clock> now;
    now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  }

  uint64_t GetPageFrameNumber(int pagemap, uint8_t* virtual_address)
  {
    // Read the entry in the pagemap.
    uint64_t value;
    int got = pread(pagemap, &value, 8, (reinterpret_cast<uintptr_t>(virtual_address) / PAGE_SIZE) * 8);
    assert(got == 8);
    uint64_t page_frame_number = value & ((1ULL << 54)-1);
    return page_frame_number;
  }

  void DoubleSideHammer(uint64_t* first_row, uint64_t* second_row, uint64_t numbre_of_read)
  {
    volatile uint64_t* p = first_row;
    volatile uint64_t* q = second_row;

    while (numbre_of_read-- > 0) {
        __asm__ volatile("clflushopt (%0);\n\t"
                         "clflushopt (%1);\n\t" 
                         : : "r" (p), "r" (q) : "memory");
        *p; *q;
    }
  }

  void BinNSideHammer(vector<uint64_t*> rows_vaddr, uint64_t numbre_of_rounds, uint64_t cycle_time, uint64_t bin_index, TimeResult* res)
  {
    sched_yield();
    
    uint64_t bin_width = ceil(cycle_time/ceil(cycle_time/1e6));
    uint64_t current_time = GetCurrentTimeNS();
		while ( ((current_time % cycle_time) / bin_width) != bin_index) {
          for (volatile int z = 0; z < 10; z++) {}
			    *(volatile char *)rows_vaddr[0];
			    __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[0]):"memory");
			    current_time = GetCurrentTimeNS();
		}
    
    res->start_time = current_time;
	  for ( int i = 0; i < numbre_of_rounds;  i++) {
		  __asm__ volatile ("mfence":::"memory");
		  for (size_t j = 0; j < rows_vaddr.size(); j++) {
			  *(volatile char*) rows_vaddr[j];
		  }
		  for (size_t j = 0; j < rows_vaddr.size(); j++) {
			  __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[j]):"memory");
		  }
	  }
    res->end_time = GetCurrentTimeNS();
  }
  
  void BinNSideHammerUTRR(vector<uint64_t*> rows_vaddr, uint64_t vict_index, uint64_t numbre_of_rounds, uint64_t cycle_time, uint64_t bin_index, TimeResult* res)
  {
    sched_yield();
    
    uint64_t bin_width = ceil(cycle_time/ceil(cycle_time/1e6));
    uint64_t current_time = GetCurrentTimeNS();
		while ( ((current_time % cycle_time) / bin_width) != bin_index) {
      for ( int j = 0; j < rows_vaddr.size();  j++) {
        if (j != vict_index && j != vict_index+1)
        {
          for ( int i = 0; i < 6;  i++) {
			        __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[j]):"memory");
              *(volatile char *)rows_vaddr[j];
          }
        }
      }
      current_time = GetCurrentTimeNS();
		}
    
    res->start_time = current_time;
	  for ( int i = 0; i < numbre_of_rounds;  i++) {
		  __asm__ volatile ("mfence":::"memory");
      
      for ( int j = 0; j < rows_vaddr.size();  j++) {
        if (j != vict_index && j != vict_index+1)
        {
          for ( int k = 0; k < 6;  k++) {
              __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[j]):"memory");
			        *(volatile char *)rows_vaddr[j];
          }
        } 
      }
      
      DoubleSideHammer(rows_vaddr[vict_index], rows_vaddr[vict_index+1], 24);
	  }
    res->end_time = GetCurrentTimeNS();
  }

  void NSideHammer(vector<uint64_t*> rows_vaddr, uint64_t numbre_of_rounds, uint64_t threshold, uint64_t idle)
  {
    sched_yield();
		
		// Threshold value depends on your system
    if(threshold != MAX_UINT64){
      uint64_t t0 = 0, t1 = 0, t2 = 0;
      t2 = rdtscp();
		  while ( ( ( max(t0, t1) - min(t0, t1) ) < threshold ) &&
              ( ( max(t0, t2) - min(t0, t2) ) > idle ) ) {
			    t0 = rdtscp();
			    *(volatile char *)rows_vaddr[0];
			    __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[0]):"memory");
			    t1 = rdtscp();
		  }
    }

	  for ( int i = 0; i < numbre_of_rounds;  i++) {
		  __asm__ volatile ("mfence":::"memory");
		  for (size_t j = 0; j < rows_vaddr.size(); j++) {
			  *(volatile char*) rows_vaddr[j];
		  }
		  for (size_t j = 0; j < rows_vaddr.size(); j++) {
			  __asm__ volatile ("clflushopt (%0)\n"::"r" (rows_vaddr[j]):"memory");
		  }
	  }
  }

  uint64_t SetupMapping(uint64_t* mapping_size, void** mapping, int* hugetlb_fd, double fraction_of_physical_memory) 
  {
    *mapping_size = static_cast<uint64_t>((static_cast<double>(GetPhysicalMemorySize()) * fraction_of_physical_memory)) & ~_1GB_MASK;
  #ifdef DEBUG_PRINT
    printf("[!] Mapping size: 0x%lx KB\n", *mapping_size/ONE_KILO);
  #endif
    //*mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED /*| MAP_HUGETLB*/, -1, 0);
    *hugetlb_fd = open(HUGETLB_PATH, O_CREAT | O_RDWR, 0777);
    if (*hugetlb_fd == -1){
      printf("[!] failed to open %s\n", HUGETLB_PATH);
      return -1;
    }
    /*check Hugepagesize
    ╰─$ cat /proc/meminfo | grep size                                                                                                                             2 ↵
    Hugepagesize:       2048 kB 
    */
    *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, 
                    MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED | MAP_HUGETLB_1GB, *hugetlb_fd, 0);
    if (*mapping == (void*)-1)
      return -1;

    // Initialize the mapping so that the pages are non-empty.
    printf("[!] Initializing large memory mapping...");
    for (uint64_t index = 0; index < *mapping_size; index += PAGE_SIZE) 
    {
      uint64_t* temporary = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(*mapping) + index);
      temporary[0] = index;
    }
    printf("done\n");
    return 0;
  }
  
  uint64_t SetupSharedMapping(uint64_t* mapping_size, void** mapping, int* hugetlb_fd, double fraction_of_physical_memory) 
  {
    //*mapping_size = static_cast<uint64_t>((static_cast<double>(GetPhysicalMemorySize()) * fraction_of_physical_memory)) & ~_1GB_MASK;
    *mapping_size = static_cast<uint64_t>((static_cast<double>(GetPhysicalMemorySize()) * fraction_of_physical_memory)) & ~_2MB_MASK;
  #ifdef DEBUG_PRINT
    printf("[!] Mapping size: 0x%lx KB\n", *mapping_size/ONE_KILO);
  #endif
    //*mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED /*| MAP_HUGETLB*/, -1, 0);
    *hugetlb_fd = open(HUGETLB_PATH, O_CREAT | O_RDWR, 0777);
    if (*hugetlb_fd == -1){
      printf("[!] failed to open %s\n", HUGETLB_PATH);
      return -1;
    }
    
    int ret = ftruncate(*hugetlb_fd, *mapping_size);
    if (ret == -1)
    {
      printf("[!] ftruncate() - error\n");
      return BAD_INDEX;
    }
    
    /*check Hugepagesize
    ╰─$ cat /proc/meminfo | grep size                                                                                                                             2 ↵
    Hugepagesize:       2048 kB 
    */
    //*mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, 
    //                MAP_POPULATE | /*MAP_ANONYMOUS |*/ MAP_SHARED | MAP_HUGETLB_1GB, *hugetlb_fd, 0);
    *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, 
                    MAP_POPULATE | /*MAP_ANONYMOUS |*/ MAP_SHARED | MAP_HUGETLB_2MB, *hugetlb_fd, 0);
    if (*mapping == (void*)-1)
      return -1;

    // Initialize the mapping so that the pages are non-empty.
    printf("[!] Initializing large memory mapping...");
    for (uint64_t index = 0; index < *mapping_size; index += PAGE_SIZE) 
    {
      uint64_t* temporary = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(*mapping) + index);
      temporary[0] = index;
    }
    printf("done\n");
    return 0;
  }
  
  int RemoveMapping(uint64_t mapping_size, void* mapping, int hugetlb_fd)
  {
    close(hugetlb_fd);
    return munmap(mapping ,mapping_size);

  }

  /*https://github.com/vusec/trrespass/blob/master/hammersuite/src/dram-address.c*/

  uint64_t get_dram_row(uint64_t p_addr, uint64_t row_mask)
  {
	  return (p_addr & row_mask) >> __builtin_ctzl(row_mask);
  }

  uint64_t get_dram_col(uint64_t p_addr, uint64_t col_mask)
  {
	  return (p_addr & col_mask) >> __builtin_ctzl(col_mask);
  }

  DRAMPhysicalLocation phys_2_dram(uint64_t p_addr, DRAMLayout layout)
  {
	  DRAMPhysicalLocation res = {0};
	  for (int i = 0; i < layout.hash_functions.len; i++) {
		  res.bank |= (__builtin_parityl(p_addr & layout.hash_functions.lst[i]) << i);
	  }

    assert(res.bank <= MAX_BNK_MASK);

	  res.row = get_dram_row(p_addr, layout.row_mask);
	  res.col = get_dram_col(p_addr, layout.col_mask);

	  return res;
  }

  uint64_t dram_2_phys(DRAMPhysicalLocation d_addr, DRAMLayout layout)
  {
	  uint64_t p_addr = 0;
	  uint64_t col_val = 0;

	  p_addr = (d_addr.row << __builtin_ctzl(layout.row_mask));	// set row bits
	  p_addr |= (d_addr.col << __builtin_ctzl(layout.col_mask));	// set col bits

	  for (int i = 0; i < layout.hash_functions.len; i++) {
		  uint64_t masked_addr = p_addr & layout.hash_functions.lst[i];
		  // if the address already respects the h_fn then just move to the next func
		  if (__builtin_parity(masked_addr) == ((d_addr.bank >> i) & 1L)) {
			  continue;
		  }
		  // else flip a bit of the address so that the address respects the dram h_fn
		  // that is get only bits not affecting the row.
		  uint64_t h_lsb = __builtin_ctzl((layout.hash_functions.lst[i]) &
						  ~(layout.col_mask) &
						  ~(layout.row_mask));
		  p_addr ^= 1 << h_lsb;
	  }

	  return p_addr;
  }
}
