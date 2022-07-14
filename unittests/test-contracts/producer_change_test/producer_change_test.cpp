#include "producer_change_test.hpp"

using namespace eosio;


void producer_change_test::change(const proposed_producer_changes& changes) {
    test::set_proposed_producers(changes);
}
