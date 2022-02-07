#include "RowData.h"

namespace HammerScope
{
    RowData::RowData(){}
    void RowData::push_back(PageData page_data){row.push_back( page_data );}
    uint64_t RowData::size(){ return row.size(); }
    PageData& RowData::operator[] (int page_index){return row[page_index];}
    iterator RowData::begin() noexcept { return row.begin(); }
    const_iterator RowData::cbegin() const noexcept { return row.cbegin(); }
    iterator RowData::end() noexcept { return row.end(); }
    const_iterator RowData::cend() const noexcept { return row.cend(); }
    void RowData::clear() { row.clear(); }
    uint64_t* RowData::get_page_by_physical_address(uint64_t physical_address)
    {
        for (PageData page : row) 
        {
          if (page.get_physical_address() == physical_address)
          {
              return page.get_virtual_address(); 
          }
        }
        return NULL;
    }
}
