#pragma once
#include <eosio/chain/config.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/authority.hpp>

namespace eosio { namespace chain {  namespace producer_change_merger {

   inline void merge(const producer_change_map& change_map, flat_map<name, block_signing_authority> &producers) {

      for (const auto& change : change_map.changes) {
         const auto producer_name = change.first;
         change.second.visit( [&producer_name, &producers](const auto& c ) {
            switch(c.change_operation) {
               case producer_change_operation::add:
               case producer_change_operation::modify:
                  EOS_ASSERT( c.authority, producer_schedule_exception,
                              "producer authority can not be empty for change operation ${op}",
                              ("op", (uint32_t)c.change_operation) );
                  producers[producer_name] = *c.authority;
                  break;
               case producer_change_operation::del:
                  producers.erase(producer_name);
                  break;
            }
         });
      }
      EOS_ASSERT( producers.size() == change_map.producer_count, producer_schedule_exception,
                  "new producer count ${count} mismatch with expected ${expected}",
                  ("count", producers.size())("expected", change_map.producer_count) );
   }

   /**
    * main_producers should be sorted by producer_name
    */
   inline void validate(const proposed_producer_changes& changes, const vector<producer_authority>& main_producers, flat_map<name, block_signing_authority>& backup_producers)
   {
      auto exist_in_main_producers = [&main_producers](const name &producer_name) {
         return std::find_if(main_producers.begin(), main_producers.end(), [&producer_name](const auto& item) {
                  return item.producer_name == producer_name;
               }) != main_producers.end();
      };
      const auto& main_changes = changes.main_changes;
      const auto& backup_changes = changes.backup_changes;
      // check main producer changes
      uint32_t main_producer_count = main_producers.size();
      for (const auto& change : main_changes.changes)
      {
         const auto& producer_name = change.first;
         bool existed = (!main_changes.clear_existed) && exist_in_main_producers(producer_name);
         auto op = (producer_change_operation)change.second.which();

         switch(op) {
            case producer_change_operation::add: {
               main_producer_count++;
               EOS_ASSERT( !existed, producer_schedule_exception, "the added main producer:${p} already exists", ("p", producer_name ) );

               auto change_itr = backup_changes.changes.find(producer_name);
               if (change_itr != backup_changes.changes.end()) {
                  auto bop = (producer_change_operation)change_itr->second.which();
                  EOS_ASSERT( bop == producer_change_operation::del, producer_schedule_exception, "the added main producer:${p} also exist in backup change", ("p", producer_name ) );
               } else if (!backup_changes.clear_existed) {
                  auto prod_itr = backup_producers.find(producer_name);
                  EOS_ASSERT( prod_itr == backup_producers.end(), producer_schedule_exception, "the added main producer:${p} also exist in backup producers", ("p", producer_name ) );
               }

               break;
            }
            case producer_change_operation::modify:
               EOS_ASSERT( !backup_changes.clear_existed, producer_schedule_exception, "can not modify main producer:${p} when clear_existed is true", ("p", producer_name ) );
               EOS_ASSERT( existed, producer_schedule_exception, "the modified main producer:${p} not exists", ("p", producer_name ) );
               break;
            case producer_change_operation::del:
               EOS_ASSERT( !backup_changes.clear_existed, producer_schedule_exception, "can not delete main producer:${p} when clear_existed is true", ("p", producer_name ) );
               EOS_ASSERT( existed, producer_schedule_exception, "the deleted main producer:${p} not exists", ("p", producer_name ) );
               EOS_ASSERT( main_producer_count > 0, producer_schedule_exception, "the main producer count is 0 when delete producer:${p}", ("p", producer_name ) );
               main_producer_count--;
               break;
         }
      }

      // check backup producer changes
      uint32_t backup_producer_count = backup_producers.size();
      for (const auto& change : backup_changes.changes)
      {
         const auto& producer_name = change.first;
         bool existed = (!backup_changes.clear_existed) && ( backup_producers.find(producer_name) != backup_producers.end()) ;
         auto op = (producer_change_operation)change.second.which();

         switch(op) {
            case producer_change_operation::add: {
               backup_producer_count++;
               EOS_ASSERT( !existed, producer_schedule_exception, "the added backup producer:${p} already exists", ("p", producer_name ) );

               // new backup producer must not exist in main producers
               auto change_itr = changes.main_changes.changes.find(producer_name);
               if (change_itr != changes.main_changes.changes.end()) {
                  auto bop = (producer_change_operation)change_itr->second.which();
                  EOS_ASSERT( bop == producer_change_operation::del,
                     producer_schedule_exception, "the added backup producer:${p} also exist in main producer change", ("p", producer_name ) );
               } else if (!main_changes.clear_existed) {
                  EOS_ASSERT( !exist_in_main_producers(producer_name), producer_schedule_exception, "the added backup producer:${p} also exist in main producers", ("p", producer_name ) );
               }

               break;
            }
            case producer_change_operation::modify:
               EOS_ASSERT( !backup_changes.clear_existed, producer_schedule_exception, "can not modify backup producer:${p} when clear_existed is true", ("p", producer_name ) );
               EOS_ASSERT( existed, producer_schedule_exception, "the modified backup producer:${p} not exists", ("p", producer_name ) );
               break;
            case producer_change_operation::del:
               EOS_ASSERT( !backup_changes.clear_existed, producer_schedule_exception, "can not delete main producer:${p} when clear_existed is true", ("p", producer_name ) );
               EOS_ASSERT( existed, producer_schedule_exception, "the deleted backup producer:${p} not exists", ("p", producer_name ) );
               EOS_ASSERT( backup_producer_count > 0, producer_schedule_exception, "the backup producer count is 0 when delete producer:${p}", ("p", producer_name ) );
               backup_producer_count--;
               break;
         }
      }

   }


}}}
