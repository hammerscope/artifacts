#include "hammer.h"

namespace HammerScope
{
  class PageData
  {
        uint64_t* virtual_address;
        uint64_t page_frame_number;
        uint64_t offset;
    public:
        inline static int pagemap_fd;
        inline static DRAMLayout dram_layout;
        inline static int64_t pagemap_delta;
        PageData(uint64_t*);
        PageData(uint64_t*, uint64_t);
        uint64_t* get_virtual_address();
        uint64_t get_page_frame_number();
        uint64_t get_physical_address();
        uint64_t get_page_index();
        uint64_t get_page_row_index();
  };
}
