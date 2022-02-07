#include "hammer.h"
#include "PageData.h"

namespace HammerScope
{
    typedef typename std::vector<PageData>::iterator iterator;
    typedef typename std::vector<PageData>::const_iterator const_iterator;

    class RowData
    {
            vector<PageData> row;
        public:
            RowData();
            void push_back(PageData);
            uint64_t size();
            PageData& operator[] (int);
            iterator begin() noexcept;
            const_iterator cbegin() const noexcept;
            iterator end() noexcept;
            const_iterator cend() const noexcept;
            void clear();
            uint64_t* get_page_by_physical_address(uint64_t);
    };
}
