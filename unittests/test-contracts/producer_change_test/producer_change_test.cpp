#include "producer_change_test.hpp"

using namespace eosio;


void producer_change_test::change(const proposed_producer_changes& changes) {
    print("main producer_count=", changes.main_changes.producer_count, "\n");
    print("backup producer_count=", changes.backup_changes.producer_count, "\n");
    test::set_proposed_producers(changes);
}
