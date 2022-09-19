#pragma once
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/producer_schedule.hpp>
#include <eosio/chain/protocol_feature_activation.hpp>

#include <type_traits>

namespace eosio { namespace chain {

   namespace detail {
      template<typename... Ts>
      struct block_header_extension_types {
         using block_header_extension_t = fc::static_variant< Ts... >;
         using decompose_t = decompose< Ts... >;
      };
   }

   struct backup_block_extension{
      backup_block_extension() = default;
      backup_block_extension(const backup_block_extension&) = default;
      backup_block_extension(block_id_type previous_backup, bool is_backup)
      :previous_backup(previous_backup), is_backup(is_backup){}

      static constexpr uint16_t extension_id() { return 3; }
      static constexpr bool     enforce_unique() { return true; }

      block_id_type previous_backup;
      bool is_backup = false;
   };

   using block_header_extension_types = detail::block_header_extension_types<
      protocol_feature_activation,
      producer_schedule_change_extension,
      producer_schedule_change_extension_v2,
      backup_block_extension
   >;

   using block_header_extension = block_header_extension_types::block_header_extension_t;
   /**
   *@Description: block header for main chain and backup chain
   */
   struct block_header
   {
      block_timestamp_type             timestamp;
      account_name                     producer;

      /**
       *  By signing this block this producer is confirming blocks [block_num() - confirmed, blocknum())
       *  as being the best blocks for that range and that he has not signed any other
       *  statements that would contradict.
       *
       *  No producer should sign a block with overlapping ranges or it is proof of byzantine
       *  behavior. When producing a block a producer is always confirming at least the block he
       *  is building off of.  A producer cannot confirm "this" block, only prior blocks.
       */
      uint16_t                         confirmed = 1;

      block_id_type                    previous;
      checksum256_type                 transaction_mroot; /// mroot of cycles_summary
      checksum256_type                 action_mroot; /// mroot of all delivered action receipts

      /**
       * LEGACY SUPPORT - After enabling the wtmsig-blocks extension this field is deprecated and must be empty
       *
       * Prior to that activation this carries:
       *
       * The producer schedule version that should validate this block, this is used to
       * indicate that the prior block which included new_producers->version has been marked
       * irreversible and that it the new producer schedule takes effect this block.
       */

      using new_producers_type = optional<legacy::producer_schedule_type>;

      uint32_t                          schedule_version = 0;
      new_producers_type                new_producers;
      extensions_type                   header_extensions;


      block_header() = default;

      digest_type       digest()const;
      block_id_type     id() const;
      uint32_t          block_num() const { return num_from_id(previous) + 1; }
      static uint32_t   num_from_id(const block_id_type& id);
      flat_multimap<uint16_t, block_header_extension> validate_and_extract_header_extensions()const;
      //previous_backup block id
      mutable block_id_type                    _previous_backup;
      //flag to main block or backup
      mutable bool                             _is_backup = false;
      mutable bool                             is_extracted = false;

      inline void extract_backup_block_extension() const {
         if( !is_extracted ){
            const auto& header_ext = validate_and_extract_header_extensions();
            if( header_ext.count(backup_block_extension::extension_id()) > 0 ){
               auto& backup_ext = header_ext.lower_bound(backup_block_extension::extension_id())->second.get<backup_block_extension>();
               _is_backup = backup_ext.is_backup;
               _previous_backup = backup_ext.previous_backup;
            }
            is_extracted = true;
         }
      }

      inline bool is_backup() const {
         extract_backup_block_extension();
         return _is_backup;
      }

      inline block_id_type previous_backup() const {
         extract_backup_block_extension();
         return _previous_backup;
      }
   };


   struct signed_block_header : public block_header
   {
      signature_type    producer_signature;
   };

} } /// namespace eosio::chain

FC_REFLECT(eosio::chain::block_header,
           (timestamp)(producer)(confirmed)(previous)
           (transaction_mroot)(action_mroot)
           (schedule_version)(new_producers)(header_extensions))
FC_REFLECT(eosio::chain::backup_block_extension,(previous_backup)(is_backup));
FC_REFLECT_DERIVED(eosio::chain::signed_block_header, (eosio::chain::block_header), (producer_signature))
