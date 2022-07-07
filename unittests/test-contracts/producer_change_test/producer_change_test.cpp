#include "producer_change_test.hpp"

using namespace eosio;


void producer_change_test::change(const producer_change_map& changes) {
    test::set_proposed_producers(changes);
}
