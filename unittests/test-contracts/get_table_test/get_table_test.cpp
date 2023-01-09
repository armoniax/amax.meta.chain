/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#include "get_table_test.hpp"


namespace db {

   template<typename table, typename Lambda>
   inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
         const eosio::name& modified_payer, Lambda&& setter )
   {
      if (itr == tbl.end()) {
         tbl.emplace(emplaced_payer, [&]( auto& row ) {
            setter(row, true);
         });
      } else {
         print("modify .... ", itr != tbl.end());
         tbl.modify(itr, modified_payer, [&]( auto& row ) {
            setter(row, false);
         });
      }
   }

   template<typename table, typename Lambda>
   inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
            Lambda&& setter )
   {
      set(tbl, itr, emplaced_payer, eosio::same_payer, setter);
   }

}// namespace db

void get_table_test::addnumobj(uint64_t input) {
   numobjs numobjs_table( _self, _self.value );
      numobjs_table.emplace(_self, [&]( auto& obj ) {
         obj.key = numobjs_table.available_primary_key();
         obj.sec64 = input;
         obj.sec128 = input;
         obj.secdouble = input;
         obj.secldouble = input;
      });
}

void get_table_test::addhashobj(std::string hashinput) {
   hashobjs hashobjs_table( _self, _self.value );
      hashobjs_table.emplace(_self, [&]( auto& obj ) {
         obj.key = hashobjs_table.available_primary_key();
         obj.hash_input = hashinput;
         obj.sec256 = sha256(hashinput.data(), hashinput.size());
         obj.sec160 = ripemd160(hashinput.data(), hashinput.size());
      });
}

void get_table_test::addreward(const name& owner, const eosio::asset& rewards) {
   require_auth(owner);
   producer::table producer_tbl(get_self(), get_self().value);
   auto prod_itr = producer_tbl.find(owner.value);
   db::set(producer_tbl, prod_itr, owner, [&]( auto& p, bool is_new ) {
      if (is_new) {
         p.owner = owner;
      }
      if (p.total_votes.amount > 0) {
         /* code */
         auto new_rewards = rewards + p.unallocated_rewards;
         p.allocating_rewards += new_rewards;
         p.rewards_per_vote += new_rewards.amount / (double)p.total_votes.amount;
         p.unallocated_rewards.amount = 0;
      } else {
         p.unallocated_rewards += rewards;
      }
   });

}

void get_table_test::vote(const name& owner, const name& producer_name, const eosio::asset& votes) {
   require_auth(owner);
   voter::table voter_tbl(get_self(), get_self().value);
   producer::table producer_tbl(get_self(), get_self().value);
   uint64_t amount = 0;
   double rewards_per_vote_paid = 0;

   auto voter_itr = voter_tbl.find(owner.value);
   if (voter_itr != voter_tbl.end()) {
      print("the voted producer:", voter_itr->producer_name, "\n");
      check(bool(voter_itr->producer_name), "the voted producer invalid");
      const auto& prod = producer_tbl.get(voter_itr->producer_name.value, "producer not found");
      print("found voted producer:", voter_itr->producer_name, "\n");
      rewards_per_vote_paid = prod.rewards_per_vote;

      producer_tbl.modify( prod, eosio::same_payer, [&]( auto& p ) {
         print("modifying voted producer:", p.owner, "\n");
         if (voter_itr->rewards_per_vote_paid > p.rewards_per_vote) {
            amount = (voter_itr->rewards_per_vote_paid - p.rewards_per_vote) * voter_itr->votes.amount;
            check(p.allocating_rewards.amount >= amount, "producer allocating rewards insufficient");
            p.allocating_rewards.amount -= amount;
         }
         p.total_votes += votes - voter_itr->votes;
      });
      print("success to modify voted producer:", prod.owner, "\n");
   } else {
      auto prod_itr = producer_tbl.find(producer_name.value);
      if (prod_itr != producer_tbl.end()) {
         rewards_per_vote_paid = prod_itr->rewards_per_vote;
      }
      db::set(producer_tbl, prod_itr, owner, [&]( auto& p, bool is_new ) {
         print("new voter setting voted producer:", p.owner, "\n");
         if (is_new) {
            p.owner = producer_name;
         }
         p.total_votes += votes;
      });
      print("new voter success to set voted producer:", prod_itr->owner, "\n");
   }

   db::set(voter_tbl, voter_itr, owner, [&]( auto& v, bool is_new ) {
      print("setting voter:", owner, "\n");
      if (is_new) {
         v.owner = owner;
      }
      v.producer_name = producer_name;
      v.votes = votes;
      v.rewards.amount += amount;
   });
   print("success to set voter:", owner, "\n");
}
