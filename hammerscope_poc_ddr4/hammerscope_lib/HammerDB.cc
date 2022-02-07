#include "HammerDB.h"
namespace HammerScope
{
    HammerDB::HammerDB(){};
    HammerDB::~HammerDB(){};
    HammerLocation& HammerDB::operator[] (int index){ return hammer_locations[index]; };
    uint64_t HammerDB::size(){ return hammer_locations.size(); };
    void HammerDB::clear(){ hammer_locations.clear(); };
    void HammerDB::push_back(HammerLocation hl){ hammer_locations.push_back(hl); };
    void HammerDB::copy(uint8_t* ptr){ std::copy(hammer_locations.begin(), hammer_locations.end(), reinterpret_cast<HammerLocation*>(ptr)); };
}
