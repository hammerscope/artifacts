#include "hammer.h"

namespace HammerScope
{
    #pragma pack(1)
    struct HammerLocation
    {
        uint64_t pages_physical_addresses[MAX_AGGR_NUM]; 
        uint64_t row_index;
        uint64_t base_index;
        uint64_t n_sides;
        uint64_t dram_index;
	    uint64_t physical_address;
	    uint64_t qword_value;
        uint64_t qword_index;
	    uint64_t mod;
    };
    

    class HammerDB
    {
            vector<HammerLocation> hammer_locations;
        public:
            HammerDB();
            ~HammerDB();
            HammerLocation& operator[] (int);
            uint64_t size();
            void clear();
            void push_back(HammerLocation);
            void copy(uint8_t*);
    };
}
