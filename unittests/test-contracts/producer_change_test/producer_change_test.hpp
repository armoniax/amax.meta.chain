#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <map>

using namespace eosio;
using namespace std;

namespace test {
   enum class producer_change_operation: uint8_t {
      add       = 0,
      modify    = 1,
      del       = 2,
   };

   #define producer_authority_change(operation) \
      struct producer_authority_##operation { \
         static constexpr producer_change_operation change_operation = producer_change_operation::operation; \
         std::optional<block_signing_authority> authority; \
         EOSLIB_SERIALIZE( producer_authority_##operation, (authority) ) \
      };

   producer_authority_change(add)
   producer_authority_change(modify)
   producer_authority_change(del)

   using producer_change_record = std::variant<
      producer_authority_add,     // add
      producer_authority_modify,  // modify
      producer_authority_del      // delete
   >;

   struct producer_change_map {
      uint32_t  producer_count = 0; // the total producer count after change
      map<name, producer_change_record> changes;

      EOSLIB_SERIALIZE( producer_change_map, (producer_count)(changes) )
   };

   inline std::optional<uint64_t> set_proposed_producers( const producer_change_map& changes ) {
      auto packed_changes = eosio::pack( changes );
      int64_t ret = internal_use_do_not_use::set_proposed_producers_ex(2, (char*)packed_changes.data(), packed_changes.size());
      if (ret >= 0)
        return static_cast<uint64_t>(ret);
      return {};
   }
}

// #define producer_change_map int
using producer_change_map = test::producer_change_map;

class [[eosio::contract]] producer_change_test : public eosio::contract {
public:
   using eosio::contract::contract;

   [[eosio::action]] void change(const producer_change_map& changes);
};
