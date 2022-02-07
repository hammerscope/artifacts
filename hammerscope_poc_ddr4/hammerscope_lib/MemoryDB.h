#include "hammer.h"
#include "RowData.h"

namespace HammerScope
{
    class MemoryDB
    {
            vector<RowData> pages_per_row;
            vector<uint64_t> pages_index;
            void* memory_mapping;
            uint64_t memory_mapping_size;
            int hugetlb_fd;
        public:
            MemoryDB();
            ~MemoryDB();
            int InitDB(void*, int, uint64_t, uint64_t, uint64_t, uint64_t);
            int InitDBFunc(void*, uint64_t);
            int InitDBUserHugepage(uint64_t* addr_arr, uint64_t arr_len);
            RowData& operator[] (int);
            void resize(uint64_t);
            uint64_t size();
            void Clear();
            void* get_memory_mapping();
            uint64_t get_memory_mapping_size();
            uint64_t GetAllocatedRow();
            uint64_t get_banks_cnt();
    };
}
