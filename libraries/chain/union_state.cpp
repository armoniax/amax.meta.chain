#include <eosio/chain/union_state.hpp>

/** 
*@Description: codes is used to construct pending block on main/backup chain mode.
*/
namespace eosio{
    namespace chain{
        bool union_state::upgrade(){
              return false;
        }
        pending_block_header_state union_state::next(){
               pending_block_header_state result;
               return result;
        }
    }
}