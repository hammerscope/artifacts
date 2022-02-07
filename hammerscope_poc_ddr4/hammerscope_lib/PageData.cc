#include "PageData.h"

namespace HammerScope
{
    PageData::PageData(uint64_t* _virtual_address)
    {
        virtual_address = _virtual_address;
        page_frame_number = GetPageFrameNumber(pagemap_fd, reinterpret_cast<uint8_t*>(virtual_address) + PageData::pagemap_delta);
        offset = -1;
        assert(page_frame_number != 0);
    }
    PageData::PageData(uint64_t* _virtual_address, uint64_t _offset)
    {
        virtual_address = _virtual_address;
        page_frame_number = reinterpret_cast<uint64_t>(virtual_address) / PAGE_SIZE;
        offset = _offset;
    }
    uint64_t* PageData::get_virtual_address(){ return virtual_address; }
    uint64_t PageData::get_page_frame_number(){ return page_frame_number; }
    uint64_t PageData::get_physical_address(){ return get_page_frame_number() * PAGE_SIZE; }
    //uint64_t PageData::get_page_row_index(){ return get_physical_address()/ROW_SIZE; }
    uint64_t PageData::get_page_index(){
        DRAMPhysicalLocation physical_location = {0};
        int dram_func[8][16] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                                {2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13},
                                {4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11},
                                {6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9},
                                {8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7},
                                {10, 11, 8, 9, 14, 15, 12, 13, 2, 3, 0, 1, 6, 7, 4, 5},
                                {12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3},
                                {14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1}};
        if (offset != -1)
        {  
          physical_location.bank = dram_func[(offset/(ROW_SIZE*16)) % 8][(offset/ROW_SIZE) % 16];
          physical_location.row  = (offset/(ROW_SIZE*16)) % (1ull << 16);
          physical_location.col  = (offset >> 3) % ROW_SIZE;
          //printf("[DEBUG] BANK - %lld, ROW - %lld\n", physical_location.bank, physical_location.row );
          return (physical_location.data);
        } else {
          uint64_t addr = get_physical_address();
          physical_location = phys_2_dram(addr, PageData::dram_layout);
          //*CHANNEL - 1 bit
          //*DIMM    - 2 bit
	        //*RANK    - 1 bit
          //*BANK    - 3 bit
          //*ROW     - 16 bit
          //*COL     - 10 bit
          return (physical_location.data);
        }
    }
    uint64_t PageData::get_page_row_index(){
        return get_page_index() >> 10;
    }
}
