#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/contract_types.hpp>

namespace eosio { namespace chain {

   class apply_context;

   /**
    * @defgroup native_action_handlers Native Action Handlers
    */
   ///@{
   void apply_amax_newaccount(apply_context&);
   void apply_amax_updateauth(apply_context&);
   void apply_amax_deleteauth(apply_context&);
   void apply_amax_linkauth(apply_context&);
   void apply_amax_unlinkauth(apply_context&);

   /*
   void apply_amax_postrecovery(apply_context&);
   void apply_amax_passrecovery(apply_context&);
   void apply_amax_vetorecovery(apply_context&);
   */

   void apply_amax_setcode(apply_context&);
   void apply_amax_setabi(apply_context&);

   void apply_amax_canceldelay(apply_context&);
   
   void apply_amax_activate(apply_context& context);
   ///@}  end action handlers

} } /// namespace eosio::chain
