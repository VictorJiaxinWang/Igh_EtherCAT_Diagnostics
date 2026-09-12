#include "ethercat_diag/cli/daemon_options.h"

#include <cassert>
#include <iostream>

int main()
{
    assert((parseMasterList("0") == std::vector<int>{0}));
    assert((parseMasterList("1") == std::vector<int>{1}));
    assert((parseMasterList("0,1") == std::vector<int>{0, 1}));
    assert(parseMasterList("").empty());
    assert(parseMasterList("0,0").empty());
    assert(parseMasterList("-1").empty());
    assert(parseMasterList("0,x").empty());
    std::cout << "All daemon option tests passed\n";
}
