#pragma once

#include <eosio/chain/block_header_state.hpp>

namespace eosio{
    namespace chain{
        struct union_state{
           block_header_state backup_state;
           block_header_state main_state;
           bool is_ready = false;
           void set_backup_state(block_header_state_ptr ptr){
               backup_state = *ptr;
           }
           void set_main_state(block_header_state_ptr ptr){
               main_state = *ptr;
           }
           /** 
           *@Description: when a backup block upgrade to a main block, this fuction will work right now!
           *@todo paragraph describing what is to be done
           */
           bool upgrade();
           /** 
           *@Description:
           *@todo paragraph describing what is to be done
           */
           pending_block_header_state next();
        };
        using union_state_ptr = std::shared_ptr<union_state>;     
    }
}

FC_REFLECT(eosio::chain::union_state,(backup_state)(main_state)(is_ready))