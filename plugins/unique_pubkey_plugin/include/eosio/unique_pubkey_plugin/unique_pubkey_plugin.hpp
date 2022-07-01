#pragma once

#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/producer_plugin/producer_plugin.hpp>
#include <eosio/history_plugin/history_plugin.hpp>

#include <appbase/application.hpp>

namespace eosio {

// using namespace appbase;

class unique_pubkey_plugin : public plugin<unique_pubkey_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES((chain_plugin)(producer_plugin)(history_plugin))

   unique_pubkey_plugin() = default;
   unique_pubkey_plugin(const unique_pubkey_plugin&) = delete;
   unique_pubkey_plugin(unique_pubkey_plugin&&) = delete;
   unique_pubkey_plugin& operator=(const unique_pubkey_plugin&) = delete;
   unique_pubkey_plugin& operator=(unique_pubkey_plugin&&) = delete;
   virtual ~unique_pubkey_plugin() override = default;

   virtual void set_program_options(options_description& cli, options_description& cfg) override {}
   void plugin_initialize(const variables_map& vm);
   void plugin_startup() {}
   void plugin_shutdown() {}
private:
   chain_plugin* chain_plug = nullptr;
   producer_plugin* producer_plug = nullptr;
};

}
