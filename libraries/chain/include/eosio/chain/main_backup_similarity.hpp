#pragma once
#include <fc/bloom_filter.hpp>
#include <eosio/chain/block.hpp>

namespace eosio{ namespace chain{

class main_backup_similarity
   {
   private:
      signed_block_ptr main_block;
      bloom_filter     bf;
      uint32_t         common_txs = 0;

   public:
      main_backup_similarity() {
          bloom_parameters p;
          p.compute_optimal_parameters();
          bf =  bloom_filter(p);
      }
      
      void add_main_block_txs(signed_block_ptr block)
      {
        main_block = block;
        bf.clear();
        common_txs = 0;
        for (auto &receipt : block->transactions)
        {
            if (receipt.trx.contains<transaction_id_type>())
            {
                bf.insert(receipt.trx.get<transaction_id_type>());
            }
            else
            {
                bf.insert(receipt.trx.get<packed_transaction>().id());
            }
        }
        
      }

      void add_backup_block_txs(signed_block_ptr block)
      {
        EOS_ASSERT(block, producer_exception, "added backup block is NULL");
        for (auto &receipt : block->transactions)
        {
            if (receipt.trx.contains<transaction_id_type>())
            {
                if(bf.contains(receipt.trx.get<transaction_id_type>())){
                   common_txs++;
                }
            }
            else
            {
                if(bf.contains(receipt.trx.get<packed_transaction>().id())){
                   common_txs++;
                }
            }
        }
      }

      uint32_t get_similarity_degree()
      {
         if( main_block->transactions.size()==0 ) return config::percent_100;
         fc_dlog(_backup_block_trace_log, "[ADD_BACKUP_BLOCK_TXS] common size: ${size}",("size",common_txs));
         fc_dlog(_backup_block_trace_log, "[ADD_BACKUP_BLOCK_TXS] main size: ${size}",("size",main_block->transactions.size()));
         uint32_t degree = common_txs * config::percent_100 / main_block->transactions.size();
         return degree;
      }
   };
}}