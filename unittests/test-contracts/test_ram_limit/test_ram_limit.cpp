#include <utility>
#include <vector>
#include <string>

#include <eosio/eosio.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wsign-compare"

using namespace eosio;

CONTRACT test_ram_limit : public contract {
   public:
   using contract::contract;

      const uint32_t FIVE_MINUTES = 5*60;

      ACTION setentry( name payer, uint64_t from, uint64_t to, uint64_t size ) {
         const auto self = get_self();
         eosio::print("test_ram_limit::setentry ", eosio::name{self},
            ", payer=", payer,
            ", from=", from,
            ", to=", to,
            ", size=", size,
            "\n");
         test_table table( self, self.value );
         for ( int key = from; key <=to; ++key ) {
            auto itr = table.find(key);
            if ( itr != table.end() ) {
               table.modify( itr, payer, [size](test& t) {
                  t.data.assign( size, (int8_t)size );
               });
            } else {
               table.emplace( payer, [key,size](test& t) {
                  t.key = key;
                  t.data.assign( size, (int8_t)size );
               });
            }
         }
      }

      ACTION rmentry( uint64_t from, uint64_t to ) {
         const auto self = get_self();
         eosio::print("test_ram_limit::rmentry ", eosio::name{self}, "\n");
         test_table table( self, self.value );
         for ( int key = from; key <=to; ++key ) {
            auto itr = table.find(key);
            check( itr != table.end(), "could not find test_table entry" );
            table.erase(itr);
         }
      }

      ACTION printentry( uint64_t from, uint64_t to ) {
         const auto self = get_self();
         eosio::print("test_ram_limit::printout ", eosio::name{self}, ":");
         test_table table( self, self.value );
         for ( int key = from; key <= to; ++key ) {
            auto itr = table.find(key);
            eosio::print("\nkey=", key);
            check( itr != table.end(), "could not find test_table entry" );
            eosio::print(" size=", itr->data.size());
         }
      }

      ACTION getentry( uint64_t from, uint64_t to ) {
         const auto self = get_self();
         eosio::print("test_ram_limit::getentry ", eosio::name{self}, ":");
         test_table table( self, self.value );
         for ( int key = from; key <= to; ++key ) {
            auto itr = table.find(key);
            check( itr != table.end(), "could not find test_table entry" );
         }
      }

      ACTION itrentry( uint64_t from, uint64_t count ) {
         const auto self = get_self();
         eosio::print("test_ram_limit::itrentry ", eosio::name{self}, ":");
         test_table table( self, self.value );
         auto itr = table.find(from);
         for ( size_t i = 0; i < count; ++i ) {
            check( itr != table.end(), "no more entry in test_table entry! from=" + std::to_string(from) + " get_count=" + std::to_string(i) );
            itr++;
         }
      }

   private:
      TABLE test {
         uint64_t            key;
         std::vector<int8_t> data;

         uint64_t primary_key()const { return key; }

         EOSLIB_SERIALIZE( test, (key)(data) )
      };
      typedef eosio::multi_index< "test.table"_n, test> test_table;
};

#pragma clang diagnostic pop

EOSIO_DISPATCH( test_ram_limit, (setentry)(rmentry)(printentry)(getentry)(itrentry) )
