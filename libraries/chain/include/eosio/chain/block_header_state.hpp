#pragma once
#include <eosio/chain/block_header.hpp>
#include <eosio/chain/incremental_merkle.hpp>
#include <eosio/chain/protocol_feature_manager.hpp>
#include <eosio/chain/chain_snapshot.hpp>
#include <future>

extern const fc::string backup_block_trace_logger_name;
extern fc::logger _backup_block_trace_log;
namespace eosio { namespace chain {

namespace legacy {

   /**
    * a fc::raw::unpack compatible version of the old block_state structure stored in
    * version 2 snapshots
    */
   struct snapshot_block_header_state_v2 {
      static constexpr uint32_t minimum_version = 0;
      static constexpr uint32_t maximum_version = 2;
      static_assert(chain_snapshot_header::minimum_compatible_version <= maximum_version, "snapshot_block_header_state_v2 is no longer needed");

      struct schedule_info {
         uint32_t                          schedule_lib_num = 0; /// last irr block num
         digest_type                       schedule_hash;
         producer_schedule_type            schedule;
      };

      /// from block_header_state_common
      uint32_t                             block_num = 0;
      uint32_t                             dpos_proposed_irreversible_blocknum = 0;
      uint32_t                             dpos_irreversible_blocknum = 0;
      producer_schedule_type               active_schedule;
      incremental_merkle                   blockroot_merkle;
      flat_map<account_name,uint32_t>      producer_to_last_produced;
      flat_map<account_name,uint32_t>      producer_to_last_implied_irb;
      public_key_type                      block_signing_key;
      vector<uint8_t>                      confirm_count;

      // from block_header_state
      block_id_type                        id;
      signed_block_header                  header;
      schedule_info                        pending_schedule;
      protocol_feature_activation_set_ptr  activated_protocol_features;
   };


   /**
    * a fc::raw::unpack compatible version of the old block_state structure stored in
    * version 3 snapshots
    */
   struct snapshot_block_header_state_v3 {
      static constexpr uint32_t minimum_version = 3;
      static constexpr uint32_t maximum_version = 3;
      static_assert(chain_snapshot_header::minimum_compatible_version <= maximum_version, "snapshot_block_header_state_v3 is no longer needed");

      struct schedule_info {
         uint32_t                          schedule_lib_num = 0; /// last irr block num
         digest_type                       schedule_hash;
         producer_authority_schedule       schedule;
      };

      /// from block_header_state_common
      uint32_t                          block_num = 0;
      uint32_t                          dpos_proposed_irreversible_blocknum = 0;
      uint32_t                          dpos_irreversible_blocknum = 0;
      producer_authority_schedule       active_schedule;
      incremental_merkle                blockroot_merkle;
      flat_map<account_name,uint32_t>   producer_to_last_produced;
      flat_map<account_name,uint32_t>   producer_to_last_implied_irb;
      block_signing_authority           valid_block_signing_authority;
      vector<uint8_t>                   confirm_count;

      /// from block_header_state
      block_id_type                        id;
      signed_block_header                  header;
      schedule_info                        pending_schedule;
      protocol_feature_activation_set_ptr  activated_protocol_features;
      vector<signature_type>               additional_signatures;
   };
}

using signer_callback_type = std::function<std::vector<signature_type>(const digest_type&)>;

struct block_header_state;

namespace detail {
   struct backup_schedule_index_t {
      backup_producer_schedule_ptr   schedule;

      backup_producer_schedule_ptr   pre_schedule; // only in memory, not persisted

      const backup_producer_schedule_ptr& get_schedule() const {

         return schedule ? schedule : pre_schedule;
      }

      void ensure_pre_schedule(const backup_schedule_index_t& pre_schedule_index) {
         if (!pre_schedule) {
            pre_schedule = pre_schedule_index.get_schedule();
         }
      }

      void ensure_persisted() {
         if (!schedule) {
            schedule = pre_schedule;
         }
      }
   };

   struct block_header_state_common {
      uint32_t                          block_num = 0;
      uint32_t                          dpos_proposed_irreversible_blocknum = 0;
      uint32_t                          dpos_irreversible_blocknum = 0;
      producer_authority_schedule       active_schedule;
      bool                              next_is_backup = false;
      incremental_merkle                blockroot_merkle;
      flat_map<account_name,uint32_t>   producer_to_last_produced;
      flat_map<account_name,uint32_t>   producer_to_last_implied_irb;
      block_signing_authority           valid_block_signing_authority;
      vector<uint8_t>                   confirm_count;
      backup_schedule_index_t           active_backup_schedule;
   };

   struct schedule_info {
      uint32_t                          schedule_lib_num = 0; /// last irr block num
      digest_type                       schedule_hash;
      block_producer_schedule_change    schedule;

      static bool data_empty(const block_producer_schedule_change& schedule) {
         return schedule.which() == 0;
      }

      bool data_empty() const {
         return schedule.which() == 0;
      }

      void data_clear(uint32_t version = 0) {
         schedule = version;
      }

      static uint32_t get_version (const block_producer_schedule_change& schedule) {
         return schedule.visit(schedule_version_visitor());
      }

      uint32_t get_version () const {
         return get_version(schedule);
      }
   };

   bool is_builtin_activated( const protocol_feature_activation_set_ptr& pfa,
                              const protocol_feature_set& pfs,
                              builtin_protocol_feature_t feature_codename );
}

struct pending_block_header_state : public detail::block_header_state_common {
   protocol_feature_activation_set_ptr  prev_activated_protocol_features;
   detail::schedule_info                prev_pending_schedule;
   bool                                 was_pending_promoted = false;
   block_id_type                        previous;
   block_id_type                        previous_backup;
   bool                                 is_backup = false;
   account_name                         producer;
   block_timestamp_type                 timestamp;
   uint32_t                             active_schedule_version = 0;
   uint16_t                             confirmed = 1;

   signed_block_header make_block_header( const checksum256_type& transaction_mroot,
                                          const checksum256_type& action_mroot,
                                          const block_producer_schedule_change& producer_schedule_change,
                                          vector<digest_type>&& new_protocol_feature_activations,
                                          const protocol_feature_set& pfs, bool is_backup = false, block_id_type pre_backup = block_id_type())const;

   block_header_state  finish_next( const signed_block_header& h,
                                    vector<signature_type>&& additional_signatures,
                                    const protocol_feature_set& pfs,
                                    const std::function<void( block_timestamp_type,
                                                              const flat_set<digest_type>&,
                                                              const vector<digest_type>& )>& validator,
                                    bool skip_validate_signee = false )&&;

   block_header_state  finish_next( signed_block_header& h,
                                    const protocol_feature_set& pfs,
                                    const std::function<void( block_timestamp_type,
                                                              const flat_set<digest_type>&,
                                                              const vector<digest_type>& )>& validator,
                                    const signer_callback_type& signer )&&;

protected:
   block_header_state  _finish_next( const signed_block_header& h,
                                     const protocol_feature_set& pfs,
                                     const std::function<void( block_timestamp_type,
                                                               const flat_set<digest_type>&,
                                                               const vector<digest_type>& )>& validator )&&;
};

/**
 *  @struct block_header_state
 *  @brief defines the minimum state necessary to validate transaction headers
 */
struct block_header_state : public detail::block_header_state_common {
   block_id_type                        id;
   signed_block_header                  header;
   detail::schedule_info                pending_schedule;
   protocol_feature_activation_set_ptr  activated_protocol_features;
   vector<signature_type>               additional_signatures;
   bool                                 is_backup = false;
   block_id_type                        pre_backup;
   /// this data is redundant with the data stored in header, but it acts as a cache that avoids
   /// duplication of work
   flat_multimap<uint16_t, block_header_extension> header_exts;

   block_header_state() = default;

   explicit block_header_state( detail::block_header_state_common&& base )
   :detail::block_header_state_common( std::move(base) )
   {}

   explicit block_header_state( legacy::snapshot_block_header_state_v2&& snapshot );
   explicit block_header_state( legacy::snapshot_block_header_state_v3&& snapshot );

   pending_block_header_state  next( block_timestamp_type when, uint16_t num_prev_blocks_to_confirm,bool is_backup = false, block_id_type pre_backup = block_id_type())const;

   block_header_state   next( const signed_block_header& h,
                              vector<signature_type>&& additional_signatures,
                              const protocol_feature_set& pfs,
                              const std::function<void( block_timestamp_type,
                                                        const flat_set<digest_type>&,
                                                        const vector<digest_type>& )>& validator,
                              bool skip_validate_signee = false )const;

   bool                 has_pending_producers()const { return !pending_schedule.data_empty(); }
   uint32_t             calc_dpos_last_irreversible( account_name producer_of_next_block )const;

   producer_authority     get_scheduled_producer( block_timestamp_type t )const;
   optional<producer_authority> get_backup_scheduled_producer( block_timestamp_type t )const;
   const block_id_type&   prev()const { return header.previous; }
   digest_type            sig_digest()const;
   void                   sign( const signer_callback_type& signer );
   void                   verify_signee()const;
   bool                   is_header_backup()const{return header.is_backup();}
   const vector<digest_type>& get_new_protocol_feature_activations()const;
};

using block_header_state_ptr = std::shared_ptr<block_header_state>;

struct snapshot_chain_head_state{
   static constexpr uint32_t minimum_version = 4;
   static constexpr uint32_t maximum_version = 4;
   static_assert(chain_snapshot_header::minimum_compatible_version <= maximum_version, "snapshot_block_header_state is no longer needed");
   
   block_header_state_ptr pre_head_state;
   block_header_state head_state;
};

} } /// namespace eosio::chain

FC_REFLECT( eosio::chain::detail::backup_schedule_index_t,
          (schedule)
)

FC_REFLECT( eosio::chain::detail::block_header_state_common,
            (block_num)
            (dpos_proposed_irreversible_blocknum)
            (dpos_irreversible_blocknum)
            (active_schedule)
            (blockroot_merkle)
            (producer_to_last_produced)
            (producer_to_last_implied_irb)
            (valid_block_signing_authority)
            (confirm_count)
            (active_backup_schedule)
)

FC_REFLECT( eosio::chain::detail::schedule_info,
            (schedule_lib_num)
            (schedule_hash)
            (schedule)
)

FC_REFLECT_DERIVED(  eosio::chain::block_header_state, (eosio::chain::detail::block_header_state_common),
                     (id)
                     (header)
                     (pending_schedule)
                     (activated_protocol_features)
                     (additional_signatures)
)


FC_REFLECT( eosio::chain::legacy::snapshot_block_header_state_v2::schedule_info,
          ( schedule_lib_num )
          ( schedule_hash )
          ( schedule )
)


FC_REFLECT( eosio::chain::legacy::snapshot_block_header_state_v2,
          ( block_num )
          ( dpos_proposed_irreversible_blocknum )
          ( dpos_irreversible_blocknum )
          ( active_schedule )
          ( blockroot_merkle )
          ( producer_to_last_produced )
          ( producer_to_last_implied_irb )
          ( block_signing_key )
          ( confirm_count )
          ( id )
          ( header )
          ( pending_schedule )
          ( activated_protocol_features )
)

FC_REFLECT( eosio::chain::legacy::snapshot_block_header_state_v3::schedule_info,
            (schedule_lib_num)
            (schedule_hash)
            (schedule)
)

FC_REFLECT(  eosio::chain::legacy::snapshot_block_header_state_v3,
            (block_num)
            (dpos_proposed_irreversible_blocknum)
            (dpos_irreversible_blocknum)
            (active_schedule)
            (blockroot_merkle)
            (producer_to_last_produced)
            (producer_to_last_implied_irb)
            (valid_block_signing_authority)
            (confirm_count)
            (id)
            (header)
            (pending_schedule)
            (activated_protocol_features)
            (additional_signatures)
)

FC_REFLECT( eosio::chain::snapshot_chain_head_state,
     (pre_head_state)
     (head_state)
)
