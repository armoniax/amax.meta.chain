#pragma once
#include <fc/bloom_filter.hpp>
#include <eosio/chain/block.hpp>

namespace eosio{ namespace chain{

class block_contribution
{
   private:
        signed_block_ptr main_block;
        bloom_filter     bf;
        uint32_t         common_txs = 0;

   public:
        block_contribution() {
            bloom_parameters p;
            p.compute_optimal_parameters();
            bf =  bloom_filter(p);
        }

        uint32_t calculate( signed_block_ptr main_block , signed_block_ptr backup_block ){
            EOS_ASSERT( main_block, producer_exception, "main block is NULL" );
            EOS_ASSERT( backup_block, producer_exception, "backup block is NULL" );
            bf.clear();
            common_txs = 0;
            
            if( main_block->transactions.size()==0 ) return config::percent_100;

            for (auto &receipt : main_block->transactions)
            {
                bf.insert(receipt.get_transaction_id());
            }
                
            for (auto &receipt : backup_block->transactions)
            {    
                if(bf.contains(receipt.get_transaction_id())){
                    common_txs++;
                }
            }
                
            fc_dlog(_backup_block_trace_log, "[ADD_BACKUP_BLOCK_TXS] common size: ${size}",("size",common_txs));
            fc_dlog(_backup_block_trace_log, "[ADD_BACKUP_BLOCK_TXS] main size: ${size}",("size",main_block->transactions.size()));
            uint32_t percent = common_txs * config::percent_100 / main_block->transactions.size();
            return percent;
        }
    };
}}