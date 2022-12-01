#pragma once
#include <eosio/chain/types.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/contract_types.hpp>

#include "multi_index_includes.hpp"

namespace eosio { namespace chain {

   class reversible_block_object : public chainbase::object<reversible_block_object_type, reversible_block_object> {
      OBJECT_CTOR(reversible_block_object,(packedblock) )

      id_type        id;
      uint32_t       blocknum = 0; //< blocknum should not be changed within a chainbase modifier lambda
      shared_string  packedblock;

      void set_block( const full_block_ptr& b ) {
         auto temp = b->pack();
         packedblock.resize( temp.size() );
         fc::datastream<char*> ds( packedblock.data(), packedblock.size() );
         ds.write(temp.data(), temp.size());
      }

      signed_block_ptr get_block()const {
         auto result = get_full_block();
         return result->main_block;
      }

      signed_block_ptr get_backup_block()const {
         auto result = get_full_block();
         return result->backup_block ;
      }
      
      full_block_ptr get_full_block()const {
         fc::datastream<const char*> ds( packedblock.data(), packedblock.size() );
         auto result = std::make_shared<full_block>();
         result->unpack(ds);
         return result;
      }

      block_id_type get_block_id()const {
         auto result = get_full_block();
         return result->main_block->id();
      }

      block_id_type get_previous_backup_id()const {
         auto result = get_full_block();
         return result->main_block->previous_backup();
      }
   };

   struct by_num;
   using reversible_block_index = chainbase::shared_multi_index_container<
      reversible_block_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<reversible_block_object, reversible_block_object::id_type, &reversible_block_object::id>>,
         ordered_unique<tag<by_num>, member<reversible_block_object, uint32_t, &reversible_block_object::blocknum>>
      >
   >;

} } // eosio::chain

CHAINBASE_SET_INDEX_TYPE(eosio::chain::reversible_block_object, eosio::chain::reversible_block_index)
