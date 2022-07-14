#include "producer_change_test.hpp"

using namespace eosio;


void producer_change_test::change(const proposed_producer_changes& changes, const std::optional<int64_t> expected) {
    auto ret = test::set_proposed_producers_ex(changes);
    if (expected) {
        check(ret == *expected, "set_proposed_producers_ex result != expected, ret=" + to_string(ret));
    }

}
