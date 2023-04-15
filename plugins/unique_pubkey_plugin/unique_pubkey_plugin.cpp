#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <eosio/unique_pubkey_plugin/unique_pubkey_plugin.hpp>
#include <eosio/chain/contract_types.hpp>
#include <eosio/history_plugin/public_key_history_object.hpp>

namespace eosio {

static appbase::abstract_plugin& _unique_pubkey_plugin = app().register_plugin<unique_pubkey_plugin>();

using namespace eosio;
using namespace eosio::chain;

void unique_pubkey_plugin::plugin_initialize(const variables_map& vm) {
   try {
      chain_plug = app().find_plugin<chain_plugin>();
      EOS_ASSERT(chain_plug, chain::missing_chain_plugin_exception, "Failed to find chain_plugin");
      producer_plug = app().find_plugin<producer_plugin>();
      EOS_ASSERT(producer_plug, plugin_exception, "Failed to find producer_plugin");

      producer_plug->get_before_incoming_transaction().connect([&](const packed_transaction_ptr& trx) {
         auto& chain = chain_plug->chain();
         chainbase::database& db = const_cast<chainbase::database&>( chain.db() ); // Override read-only access to state DB (highly unrecommended practice!)
         const transaction& t = trx->get_transaction();
         for ( const auto& act : t.actions) {
            if (act.account == config::system_account_name && act.name == N(newaccount)) {
               fc::datastream<const char*> ds( act.data.data(), act.data.size() );
               chain::newaccount nc;
               fc::raw::unpack( ds, nc );
               const auto& pub_key_idx = db.get_index<public_key_history_multi_index, by_pub_key>();
               if (!nc.owner.keys.empty()) {
                  const auto& owner_pubkey = nc.owner.keys.at(0).key;
                  auto owner_range = pub_key_idx.equal_range( owner_pubkey );
                  EOS_ASSERT(owner_range.first == owner_range.second, key_exist_exception,
                     "owner public key ${key} has been used by another account", ("key", owner_pubkey));
               }
               if (!nc.owner.keys.empty()) {
                  const auto& active_pubkey = nc.active.keys.at(0).key;
                  auto active_range = pub_key_idx.equal_range( active_pubkey );
                  EOS_ASSERT(active_range.first == active_range.second, key_exist_exception,
                     "active public key ${key} has been used by another account", ("key", active_pubkey));
               }
            }
         }
      });
   } FC_LOG_AND_RETHROW()
}

}
