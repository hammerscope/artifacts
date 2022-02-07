#include "MemoryDB.h"

namespace HammerScope
{
    MemoryDB::MemoryDB()
    {
      memory_mapping = NULL;
      memory_mapping_size = 0;
      hugetlb_fd = -1;
    }

    MemoryDB::~MemoryDB()
    {
        Clear();
    }

    void MemoryDB::Clear()
    {
        memory_mapping = NULL;
        memory_mapping_size = 0;
        for(int i=0; i<pages_per_row.size(); i+=1)
        {
            pages_per_row[i].clear();
        }
        pages_per_row.clear();
    }

    int MemoryDB::InitDB(void* _memory_mapping, int _hugetlb_fd, uint64_t _memory_mapping_size, uint64_t cfg, uint64_t pid, uint64_t shm_base_addr)
    {
        //int dram_func[4][8] = {{0, 1, 2, 3, 4, 5, 6, 7}, {2, 3, 0, 1, 6, 7, 4, 5}, {4, 5, 6, 7, 0, 1, 2, 3}, {6, 7, 4, 5, 2, 3, 0, 1}};
        
        memory_mapping = _memory_mapping;
        memory_mapping_size = _memory_mapping_size;
        hugetlb_fd = _hugetlb_fd;
        pages_per_row.resize(memory_mapping_size / ROW_SIZE);
        pages_index.resize(memory_mapping_size / ROW_SIZE);
        
        char s[50] = {0}; 
        if (pid != 0)
        {
          sprintf(s, "/proc/%d/pagemap", pid);
        }
        else
        {
          sprintf(s, "/proc/self/pagemap");
        }
        printf("[!] Open %s.\n", s);
        int pagemap = open(s, O_RDONLY);
        if (pagemap < 0)
        {
          printf("[!] pagemap open failed.\n");
          return PAGEMAP_OPEN_FAIL;
        }
        PageData::pagemap_fd = pagemap;
        if(shm_base_addr){
          PageData::pagemap_delta = shm_base_addr - reinterpret_cast<uint64_t>(_memory_mapping) ; 
        } else {
          PageData::pagemap_delta = 0;
        }
        PageData::dram_layout = getDRAMLayoutCfg(cfg);
        
        for (uint64_t offset = 0; offset < memory_mapping_size; offset += PAGE_SIZE) 
        {
          uint8_t* virtual_address = static_cast<uint8_t*>(memory_mapping) + offset;
	        uint64_t* virtual_address_ptr = reinterpret_cast<uint64_t*>(virtual_address);
          
          PageData pd(virtual_address_ptr);
          
          uint64_t row_index = pd.get_page_row_index();

          if (row_index > pages_per_row.size()) {
            pages_per_row.resize(row_index);
          }
          //TODO: Add pages to specific index using column value
          pages_index[pages_index.size()] = row_index;
          pages_per_row[row_index].push_back(pd);
          /*if ( (row_index & ~((1ull<<16)-1)) >> 16 != dram_func[(offset/(ROW_SIZE*8)) % 4][(offset/ROW_SIZE) % 8]){
            printf("[DEBUG] BANK - %lld, ROW - %lld\n", (row_index & ~((1ull<<16)-1)) >> 16, row_index & ((1ull<<16)-1) );
          }*/
        }
        sort(pages_index.begin(), pages_index.end());
        printf("[!] Identifying rows for accessible pages ... Done\n");
        
        if (pid != 0)
        {
          close(PageData::pagemap_fd);
          PageData::pagemap_delta = 0;
          pagemap = open("/proc/self/pagemap", O_RDONLY);
          if (pagemap < 0)
          {
            printf("[!] pagemap open failed.\n");
            return PAGEMAP_OPEN_FAIL;
          }
          PageData::pagemap_fd = pagemap;
        }
        
        return OK;
    }
    
    int MemoryDB::InitDBFunc(void* _memory_mapping, uint64_t _memory_mapping_size)
    {   
        memory_mapping = _memory_mapping;
        memory_mapping_size = _memory_mapping_size;
        
        pages_per_row.resize(memory_mapping_size / ROW_SIZE);
        pages_index.resize(memory_mapping_size / ROW_SIZE);
        
        for (uint64_t offset = 0; offset < memory_mapping_size; offset += PAGE_SIZE) 
        {
          uint8_t* virtual_address = static_cast<uint8_t*>(memory_mapping) + offset;
	        uint64_t* virtual_address_ptr = reinterpret_cast<uint64_t*>(virtual_address);
          
          PageData pd(virtual_address_ptr, offset);
          
          uint64_t row_index = pd.get_page_row_index();

          if (row_index > pages_per_row.size()) {
            pages_per_row.resize(row_index);
          }
          //TODO: Add pages to specific index using column value
          pages_index[pages_index.size()] = row_index;
          pages_per_row[row_index].push_back(pd);
        }
        sort(pages_index.begin(), pages_index.end());
        printf("[!] Identifying rows for accessible pages ... Done\n");
        
        return OK;
    }
    
    int MemoryDB::InitDBUserHugepage(uint64_t* addr_arr, uint64_t arr_len)
    {   
        memory_mapping = reinterpret_cast<void*>(RAMSES_BADADDR);
        memory_mapping_size = (2*ONE_MEGA)*arr_len;
        
        pages_per_row.resize(memory_mapping_size / ROW_SIZE);
        pages_index.resize(memory_mapping_size / ROW_SIZE);
        
               
        for (uint64_t offset = 0; offset < memory_mapping_size; offset += PAGE_SIZE) 
        {
          uint8_t* virtual_address = reinterpret_cast<uint8_t*>(addr_arr[offset/(2*ONE_MEGA)]) + (offset % (2*ONE_MEGA));
          uint64_t* virtual_address_ptr = reinterpret_cast<uint64_t*>(virtual_address);
          
          PageData pd(virtual_address_ptr, offset);
          
          uint64_t row_index = pd.get_page_row_index();
          //printf("[!] row_index: %x\n", row_index);
          if (row_index > pages_per_row.size()) {
            pages_per_row.resize(row_index);
          }
          //TODO: Add pages to specific index using column value
          pages_index[pages_index.size()] = row_index;
          pages_per_row[row_index].push_back(pd);
        }
        sort(pages_index.begin(), pages_index.end());
        //printf("[!] Identifying rows for accessible pages ... Done\n");
        return OK;
    }
    
    RowData& MemoryDB::operator[] (int row_index){ return pages_per_row[row_index]; }
    void MemoryDB::resize(uint64_t number_of_rows) { pages_per_row.resize(number_of_rows); }
    uint64_t MemoryDB::size(){ return pages_per_row.size(); }
    void* MemoryDB::get_memory_mapping(){return memory_mapping;};
    uint64_t MemoryDB::get_memory_mapping_size(){return memory_mapping_size;};
    uint64_t MemoryDB::GetAllocatedRow()
    {
      //printf("[DEBUG] inside GetAllocatedRow\n");
      uint64_t full_rows = 0;
      for (RowData target_row : pages_per_row) 
      {
        //printf("[DEBUG] %lld\n", target_row.size());
        if(target_row.size() == PAGES_IN_ROW)
        {
          full_rows++;
        } 
      }
      return full_rows;
    }
    
    uint64_t get_banks_cnt()
    {
      return 1 << PageData::dram_layout.hash_functions.len;
    }
}
