#include <eosio/chain/union_state.hpp>

/**
*@Module name: 
*@Description: codes is used to construct pending block on main/backup chain mode.
*@Author: cryptoseeking
*@Modify Time: 2022/06/20 14:15
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