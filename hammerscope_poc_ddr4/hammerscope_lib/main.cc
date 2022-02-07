#include "hammer.h"
#include "MemoryDB.h"
#include "HammerDB.h"

using namespace HammerScope;

MemoryDB db;
HammerDB hammer_db;

uint64_t TestIndex(uint64_t row_index)
{
  if (row_index >= db.size() || db[row_index].size() == 0)
    return BAD_INDEX;
  return OK;
}

uint64_t GetTestNumber(uint64_t shm_addr, uint64_t shm_offset, uint64_t* test_num)
{
  uint64_t* buf = reinterpret_cast<uint64_t*>(shm_addr);
  *test_num = buf[shm_offset];
  return 0;
}

uint64_t WriteTestNumber(uint64_t shm_addr, uint64_t shm_offset, uint64_t test_num)
{
  uint64_t* buf = reinterpret_cast<uint64_t*>(shm_addr);
  buf[shm_offset] = test_num;
  return 0;
}

uint64_t MapSharedMem(uint64_t dram_index, uint64_t page_num, uint64_t* fd, uint64_t* shm_addr)
{
  if(dram_index == MAX_BNK_CNT)
  {
    *fd = memfd_create("shared_mem", 0);
    if (*fd == -1)
    {
      printf("[!] memfd_create() - error");
      return BAD_INDEX;
    }
    int ret = ftruncate(*fd, PAGE_SIZE*page_num);
    if (ret == -1)
    {
      printf("[!] ftruncate() - error");
      return BAD_INDEX;
    }
    void* ptr = mmap(NULL, PAGE_SIZE*page_num, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_SHARED, *fd, 0);
    if (ptr == MAP_FAILED)
    {
      printf("[!] mmap() - error");
      return BAD_INDEX;
    }
    *shm_addr = reinterpret_cast<uint64_t>(ptr);
    return OK;
  }
  
  int    fds[0x1000] = {0};
  void* ptrs[0x1000] = {0};
  uint64_t i;
  uint64_t res = BAD_INDEX;
  for(i = 0; i < 0x1000; i++)
  {
    fds[i] = memfd_create("shared_mem", 0);
    if (fds[i] == -1)
    {
      printf("[!] memfd_create() - error");
      return BAD_INDEX;
    }
    int ret = ftruncate(fds[i], PAGE_SIZE*page_num);
    if (ret == -1)
    {
      printf("[!] ftruncate() - error");
      return BAD_INDEX;
    }
    ptrs[i] = mmap(NULL, PAGE_SIZE*page_num, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_SHARED, fds[i], 0);
    if (ptrs[i] == MAP_FAILED)
    {
      printf("[!] mmap() - error");
      return BAD_INDEX;
    }
    uint64_t* virtual_address_ptr = reinterpret_cast<uint64_t*>(ptrs[i]); 
    PageData pd(virtual_address_ptr);
    uint64_t row_index = pd.get_page_row_index();
    //printf("[!] pfn - %p \t row_index - %p\n", pd.get_page_frame_number(), row_index >> 16); //& (0xFull<<19));
    
    if ((row_index >> 16) == (dram_index))
    {
      *fd = fds[i];
      *shm_addr = reinterpret_cast<uint64_t>(virtual_address_ptr);
      res = OK;
      
      printf("[!] pfn - 0x%llx \t found_index - 0x%llx \t requested_index - 0x%llx\n", pd.get_page_frame_number(), (row_index >> 16), (dram_index));
      
      for(uint64_t j = 0; j < page_num; j++)
      {
        memset(virtual_address_ptr , 0xFF, PAGE_SIZE);
        virtual_address_ptr[0x1FF] = (uint64_t)(-4);
        PageData pd(virtual_address_ptr);
        uint64_t row_index = pd.get_page_index();
        //*CHANNEL - 1 bit
        //*DIMM    - 2 bit
        //*RANK    - 1 bit
        //*BANK    - 3 bit
        //*ROW     - 16 bit
        //*COL     - 10 bit
        printf("[!] page index: %lld \t BANK: %lld \t ROW: %lld \t COL: %lld\n", 
                j, 
                GET_BITS(row_index, 38, 26),
                GET_BITS(row_index, 16, 10),
                GET_BITS(row_index, 10, 0 ));
        
        virtual_address_ptr = reinterpret_cast<uint64_t*>(reinterpret_cast<uint64_t>(virtual_address_ptr) + PAGE_SIZE);
      }
      break;
    }
    close(fds[i]);
  }
  
  for(uint64_t j = 0; j < i; j++)
  {
    int ret = munmap(ptrs[j], PAGE_SIZE);
    if (ret == -1)
    {
        printf("[!] munmap() - error");
        return BAD_INDEX;
    }
  }
  
  return res;
}

uint64_t ReadTimeTestNS(uint64_t row_index, uint64_t number_of_rounds, uint64_t number_of_round_reads, uint64_t* out_buf, uint64_t* time_buf)
{
  if (row_index >= db.size() || db[row_index].size() == 0)
    return BAD_INDEX;

  volatile uint64_t* p = reinterpret_cast<uint64_t*>(db[row_index][0].get_virtual_address());
  
  std::chrono::time_point<std::chrono::high_resolution_clock> now;

  for (uint64_t i = 0; i < number_of_rounds; i++)
  {
    now = std::chrono::high_resolution_clock::now();
    time_buf[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    for(uint64_t j = 0; j < number_of_round_reads; j++)
    {
      uint64_t ns;

      now = std::chrono::high_resolution_clock::now();
      ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

			*(volatile char *)p;
			__asm__ volatile ("clflushopt (%0)\n"::"r" (p):"memory");

      now = std::chrono::high_resolution_clock::now();
      ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() - ns;

      if (ns > out_buf[i])
      {
        out_buf[i] = ns;
      }
    }
  }
  return OK;
}

uint64_t ReadTimeTest(uint64_t row_index, uint64_t number_of_reads, uint64_t* out_buf)
{
  if (row_index >= db.size() || db[row_index].size() == 0)
    return BAD_INDEX;

  volatile uint64_t* p = reinterpret_cast<uint64_t*>(db[row_index][0].get_virtual_address());
  
  for (uint64_t i = 0; i < number_of_reads; i++)
  {
			*(volatile char *)p;
			__asm__ volatile ("clflushopt (%0)\n"::"r" (p):"memory");
      out_buf[i] = rdtscp();
  }
  return OK;
}

uint64_t HammerRowAllReachablePages(uint64_t row_index, uint64_t n_sides, uint64_t threshold, uint64_t idle) 
{
    void* memory_mapping = db.get_memory_mapping();
    uint64_t memory_mapping_size = db.get_memory_mapping_size();

    if(memory_mapping == NULL || memory_mapping_size == 0)
      return UNINIT_MEMORY_DB;

    uint64_t total_bitflips = 0;
    if(row_index == 0 || row_index >= db.size() || n_sides > MAX_AGGR_NUM)
    {
      return CANT_HAMMER_ROW;
    }

    for(uint64_t counter = 0; counter < n_sides*2; counter++)
    {
      if((row_index+counter) >= db.size() || db[row_index+counter].size() != PAGES_IN_ROW){
        return CANT_HAMMER_ROW;
      }
    }

    /*printf("[!] Hammering rows %ld/%ld/%ld of %ld (got %ld/%ld/%ld pages)\n", 
        row_index, row_index+1, row_index+2, db.size(), 
        db[row_index].size(), db[row_index+1].size(), 
        db[row_index+2].size());*/
    
    //check 0->1 flips and 1->0 flips
    for(uint64_t mod = 0; mod < 2; ++mod)
    {
      //0->1 flips     
      uint8_t  aggr_value  = 0xff;
      uint8_t  vict_value  = 0x00;
      uint64_t valid_value = 0x00;

      //1->0 flips 
      if(mod == 1)
      {
	      aggr_value  = 0x00;
	      vict_value  = 0xff;
	      valid_value = MAX_UINT64;
      }

      vector<uint64_t*>rows_vaddr;
      rows_vaddr.resize(n_sides);
      uint64_t rows_paddr[MAX_AGGR_NUM] = {0};

      //fill_data
      for(uint64_t counter = 0; counter < n_sides*2; counter+=2)
      {
        for (PageData vict_page : db[row_index+counter]) {
          memset(vict_page.get_virtual_address(), vict_value, PAGE_SIZE);
          MemFlush(vict_page.get_virtual_address(), PAGE_SIZE);
        }

        for (PageData aggr_page : db[row_index+counter+1]) {
          memset(aggr_page.get_virtual_address(), aggr_value, PAGE_SIZE);
          MemFlush(aggr_page.get_virtual_address(), PAGE_SIZE);
        }
        rows_vaddr[counter/2] = db[row_index+counter+1][0].get_virtual_address();
        rows_paddr[counter/2] = db[row_index+counter+1][0].get_physical_address();
      }

      // Now hammer
      NSideHammer(rows_vaddr, NUMBER_OF_READS, threshold, idle);

      // check if the attack succeed
      uint64_t number_of_bitflips_in_target = 0;
      for(uint64_t counter = 0; counter < n_sides*2; counter+=2)
      {
        for (PageData vict_page : db[row_index+counter]) {
          MemFlush(vict_page.get_virtual_address(), PAGE_SIZE);
          for (uint32_t index = 0; index < QWORDS_IN_PAGE; ++index) {
            if (vict_page.get_virtual_address()[index] != valid_value) {
              //yay we found flip
              total_bitflips++;

              HammerLocation hl = {0};
              memcpy(hl.pages_physical_addresses, rows_paddr, sizeof(rows_paddr));
              hl.n_sides = n_sides;
              hl.row_index = row_index + counter;
              hl.base_index = row_index;
              hl.dram_index = vict_page.get_page_index();
	            hl.physical_address = vict_page.get_physical_address() + (index*sizeof(uint64_t));
	            hl.qword_value = vict_page.get_virtual_address()[index];
              hl.mod = mod;
              hl.qword_index = index;
              hammer_db.push_back(hl);
            }
          }
        }
      }
    }  
    return total_bitflips;
}

uint64_t TestHammerBitflip(HammerLocation hl, uint64_t number_of_reads, uint64_t threshold, uint64_t idle, TimeResult* res) 
{
    if(hl.row_index == 0 || hl.row_index > (db.size() - (hl.n_sides*2)) || hl.n_sides > MAX_AGGR_NUM)
    {
      return CANT_HAMMER_ROW;
    }

    for(uint64_t counter = 0; counter < hl.n_sides*2; counter++)
    {
      if((hl.base_index+counter) >= db.size() || db[hl.base_index+counter].size() != PAGES_IN_ROW){
        return CANT_HAMMER_ROW;
      }
    }
    //printf("[!] Hammering rows %ld/%ld/%ld of %ld (got %ld/%ld/%ld pages)\n", 
    //  row_index-1, row_index, row_index+1, db.size(), 
    //  db[row_index-1].size(), db[row_index].size(), 
    //  db[row_index+1].size());
    //

    //0->1 flips     
    uint8_t  aggr_value  = 0xff;
    uint8_t  vict_value  = 0x00;
    uint64_t valid_value = 0x00;

    //1->0 flips 
    if(hl.mod == 1)
    {
	    aggr_value  = 0x00;
	    vict_value  = 0xff;
	    valid_value = MAX_UINT64;
    }

    vector<uint64_t*>rows_vaddr;
    rows_vaddr.resize(hl.n_sides);

    //fill_data
    for(uint64_t counter = 0; counter < hl.n_sides*2; counter+=2)
    {
      for (PageData vict_page : db[hl.base_index+counter]) {
        memset(vict_page.get_virtual_address(), vict_value, PAGE_SIZE);
        MemFlush(vict_page.get_virtual_address(), PAGE_SIZE);
      }

      for (PageData aggr_page : db[hl.base_index+counter+1]) {
        memset(aggr_page.get_virtual_address(), aggr_value, PAGE_SIZE);
        MemFlush(aggr_page.get_virtual_address(), PAGE_SIZE);
      }

      rows_vaddr[counter/2] = db[hl.base_index+counter+1][0].get_virtual_address();
    }

    //save start time
    std::chrono::time_point<std::chrono::high_resolution_clock> now;
    now = std::chrono::high_resolution_clock::now();
    uint64_t start = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    res->start_time = start;

    // Now hammer 
    NSideHammer(rows_vaddr, number_of_reads, threshold, idle);
    
    //save end time
    now = std::chrono::high_resolution_clock::now();
    uint64_t end = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    res->end_time = end; 

    // Now check bitflip.
    uint64_t* page_address = db[hl.row_index].get_page_by_physical_address(hl.physical_address & (~(PAGE_MASK)));
    uint64_t value = page_address[hl.qword_index];
    if (value != valid_value) {
      res->flip = true; 
    } 
    else {
      res->flip = false;
    }
    res->value = value;
    return OK;
}

uint64_t TestHammerBinBitflip(HammerLocation hl, uint64_t number_of_reads, uint64_t cycle_time, uint64_t bin_index, TimeResult* res) 
{
    if(hl.row_index == 0 || hl.row_index > (db.size() - (hl.n_sides*2)) || hl.n_sides > MAX_AGGR_NUM)
    {
      return CANT_HAMMER_ROW;
    }

    for(uint64_t counter = 0; counter < hl.n_sides*2; counter++)
    {
      if((hl.base_index+counter) >= db.size() || db[hl.base_index+counter].size() != PAGES_IN_ROW){
        return CANT_HAMMER_ROW;
      }
    }
    //printf("[!] Hammering rows %ld/%ld/%ld of %ld (got %ld/%ld/%ld pages)\n", 
    //  row_index-1, row_index, row_index+1, db.size(), 
    //  db[row_index-1].size(), db[row_index].size(), 
    //  db[row_index+1].size());
    //

    //0->1 flips     
    uint8_t  aggr_value  = 0xff;
    uint8_t  vict_value  = 0x00;
    uint64_t valid_value = 0x00;

    //1->0 flips 
    if(hl.mod == 1)
    {
	    aggr_value  = 0x00;
	    vict_value  = 0xff;
	    valid_value = MAX_UINT64;
    }

    vector<uint64_t*>rows_vaddr;
    rows_vaddr.resize(hl.n_sides);

    //fill_data
    for(uint64_t counter = 0; counter < hl.n_sides*2; counter+=2)
    {
      for (PageData vict_page : db[hl.base_index+counter]) {
        memset(vict_page.get_virtual_address(), vict_value, PAGE_SIZE);
        MemFlush(vict_page.get_virtual_address(), PAGE_SIZE);
      }

      for (PageData aggr_page : db[hl.base_index+counter+1]) {
        memset(aggr_page.get_virtual_address(), aggr_value, PAGE_SIZE);
        MemFlush(aggr_page.get_virtual_address(), PAGE_SIZE);
      }

      rows_vaddr[counter/2] = db[hl.base_index+counter+1][0].get_virtual_address();
    }

    // Now hammer 
    BinNSideHammer(rows_vaddr, number_of_reads, cycle_time, bin_index, res);

    // Now check bitflip.
    uint64_t* page_address = db[hl.row_index].get_page_by_physical_address(hl.physical_address & (~(PAGE_MASK)));
    uint64_t value = page_address[hl.qword_index];
    if (value != valid_value) {
      res->flip = true; 
    } 
    else {
      res->flip = false;
    }
    res->value = value;
    return OK;
}

uint64_t GetHammerDBSize()
{
  return hammer_db.size();
}

void GetHammerDB(uint8_t* arr_ptr)
{
  hammer_db.copy(arr_ptr);
}

void ClearHammerDB()
{
  hammer_db.clear();
}

uint64_t GetTotalRow()
{
  return GetPhysicalMemorySize()/ROW_SIZE;
}

uint64_t GetAllocatedRow()
{
  return db.GetAllocatedRow();
}

int MapMemory(double fraction_of_physical_memory, uint64_t cfg)
{
  uint64_t mapping_size;
  void* mapping;
  int hugetlb_fd;
  if(SetupMapping(&mapping_size, &mapping, &hugetlb_fd, fraction_of_physical_memory) == -1)
  {
  #ifdef DEBUG_PRINT
    printf("[!] Mapping Failed\n");
    printf("[!] Abort\n");
  #endif
    return MAPPING_FAILED;
  }
  return db.InitDB(mapping, hugetlb_fd, mapping_size, cfg, 0, 0);
}

uint64_t MapSharedHugeMem(double fraction_of_physical_memory, uint64_t* _hugetlb_fd, uint64_t* shm_addr)
{
  uint64_t mapping_size;
  void* mapping;
  int hugetlb_fd;
  
  if(SetupSharedMapping(&mapping_size, &mapping, &hugetlb_fd, fraction_of_physical_memory) == -1)
  {
  #ifdef DEBUG_PRINT
    printf("[!] Mapping Failed\n");
    printf("[!] Abort\n");
  #endif
    return MAPPING_FAILED;
  }
  *shm_addr = reinterpret_cast<uint64_t>(mapping);
  *_hugetlb_fd = hugetlb_fd;
  return OK;
}

uint64_t InitDB(uint64_t cfg, uint64_t hugetlb_fd, uint64_t shm_addr, uint64_t mapping_size, uint64_t pid, uint64_t shm_base_addr)
{
  void* mapping = reinterpret_cast<void*>(shm_addr);
  //printf("[!] cfg=%llx hugetlb_fd=%llx shm_addr=%llx mapping_size=%llx\n", cfg, hugetlb_fd, shm_addr, mapping_size);
  return db.InitDB(mapping, hugetlb_fd, mapping_size, cfg, pid, shm_base_addr);
}

uint64_t InitDBFunc(uint64_t shm_addr, uint64_t mapping_size)
{
  void* mapping = reinterpret_cast<void*>(shm_addr);
  //printf("[!] cfg=%llx hugetlb_fd=%llx shm_addr=%llx mapping_size=%llx\n", cfg, hugetlb_fd, shm_addr, mapping_size);
  return db.InitDBFunc(mapping, mapping_size);
}

uint64_t InitDBUserAllocHugepage(uint64_t* addr_arr, uint64_t arr_len)
{
  return db.InitDBUserHugepage(addr_arr, arr_len);
}

uint64_t ReadQWORDTimeTestNS(uint64_t address1, uint64_t address2, uint64_t number_of_rounds, uint64_t* out_buf)
{
  volatile uint64_t* p = reinterpret_cast<uint64_t*>(address1);
  volatile uint64_t* q = reinterpret_cast<uint64_t*>(address2);
  
  uint64_t start, end;
  std::chrono::time_point<std::chrono::high_resolution_clock> now;
  now = std::chrono::high_resolution_clock::now();
  start = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  for (uint64_t i = 0; i < number_of_rounds; i++)
  { 
    __asm__ volatile ("clflushopt (%0)\n"::"r" (p):"memory");
    __asm__ volatile ("clflushopt (%0)\n"::"r" (q):"memory");
    *p;
    *q;
    
    now = std::chrono::high_resolution_clock::now();
    end = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    out_buf[i] = end - start;
    start = end;
  }
  return OK;
}

uint64_t alloc_1MB_block(uint64_t* block_addr)
{
  uint64_t block_size = ONE_MEGA;
  void* ptr = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  if (ptr == MAP_FAILED)
  {
    printf("[!] mmap() - error\n");
    return BAD_INDEX;
  }
  memset(ptr, 0, block_size);
  *block_addr = reinterpret_cast<uint64_t>(ptr);
  return OK;
}

uint64_t alloc_2MB_block(uint64_t* block_addr)
{
  void* ptr = NULL;
  uint64_t block_size = 2*ONE_MEGA;
  int ret = posix_memalign(&ptr, block_size, block_size);
  if (ret != 0)
  {
    printf("[!] posix_memalign() - error\n");
    return BAD_INDEX;
  }
  madvise(ptr, block_size, MADV_HUGEPAGE);
  if (ptr == MAP_FAILED)
  {
    printf("[!] madvise() - error\n");
    return BAD_INDEX;
  }
  memset(ptr, 0, block_size);
  *block_addr = reinterpret_cast<uint64_t>(ptr);
  return OK;
}

uint64_t free_block(uint64_t block_addr, uint64_t free_type)
{
  void* ptr = reinterpret_cast<void*>(block_addr);
  if(free_type == 1){
    munmap(ptr, ONE_MEGA);
  } else {
    madvise(ptr, 2*ONE_MEGA, MADV_FREE);
  }
  return OK;
}

/*int main (int argc, char** argv)
{
  MapMemory(0.5);
  FastHammerAllReachablePages(db.get_memory_mapping(),db.get_memory_mapping_size());
*/
