#include <eosio/chain/block_header_state.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/producer_change.hpp>
#include <limits>

const fc::string backup_block_trace_logger_name("backup_block_tracing");
fc::logger _backup_block_trace_log;
namespace eosio { namespace chain {

   namespace detail {
      bool is_builtin_activated( const protocol_feature_activation_set_ptr& pfa,
                                 const protocol_feature_set& pfs,
                                 builtin_protocol_feature_t feature_codename )
      {
         auto digest = pfs.get_builtin_digest(feature_codename);
         const auto& protocol_features = pfa->protocol_features;
         return digest && protocol_features.find(*digest) != protocol_features.end();
      }
   }

   struct emplace_produce_change_ext_visitor {

      emplace_produce_change_ext_visitor(const protocol_feature_set& pfs,
         const protocol_feature_activation_set_ptr& prev_activated_protocol_features, signed_block_header& header):
            _pfs(pfs), _prev_activated_protocol_features(prev_activated_protocol_features), _header(header) {}

      void operator()( const producer_authority_schedule& new_producers ) {
         if ( detail::is_builtin_activated(_prev_activated_protocol_features, _pfs, builtin_protocol_feature_t::wtmsig_block_signatures) ) {
            // add the header extension to update the block schedule
            emplace_extension(
                  _header.header_extensions,
                  producer_schedule_change_extension::extension_id(),
                  fc::raw::pack( producer_schedule_change_extension( new_producers ) )
            );
         } else {
            legacy::producer_schedule_type downgraded_producers;
            downgraded_producers.version = new_producers.version;
            for (const auto &p : new_producers.producers) {
               p.authority.visit([&downgraded_producers, &p](const auto& auth){
                  EOS_ASSERT(auth.keys.size() == 1 && auth.keys.front().weight == auth.threshold, producer_schedule_exception, "multisig block signing present before enabled!");
                  downgraded_producers.producers.emplace_back(legacy::producer_key{p.producer_name, auth.keys.front().key});
               });
            }
            _header.new_producers = std::move(downgraded_producers);
         }
      }

      void operator()( const producer_schedule_change& changes ) {
         // TODO: check apos feature activated
         // if ( detail::is_builtin_activated(_prev_activated_protocol_features, _pfs, builtin_protocol_feature_t::wtmsig_block_signatures) ) {

         // add the header extension to update the block schedule
         emplace_extension(
               _header.header_extensions,
               producer_schedule_change_extension_v2::extension_id(),
               fc::raw::pack( producer_schedule_change_extension_v2( changes ) )
         );
      }

      void operator()( const uint32_t& ) {} // do nothing

      private:
         const protocol_feature_set& _pfs;
         const protocol_feature_activation_set_ptr& _prev_activated_protocol_features;
         signed_block_header& _header;
   };

   producer_authority block_header_state::get_scheduled_producer( block_timestamp_type t )const {
      auto index = t.slot % (active_schedule.producers.size() * config::producer_repetitions);
      index /= config::producer_repetitions;
      return active_schedule.producers[index];
   }

   optional<producer_authority> block_header_state::get_backup_scheduled_producer( block_timestamp_type t ) const {
      optional<producer_authority> result;
      auto schedule = active_backup_schedule.get_schedule();
      if (schedule && schedule->producers.size() > 0) {
         auto index = t.slot % (schedule->producers.size() * config::backup_producer_repetitions);
         index /= config::backup_producer_repetitions;
         const auto& itr = schedule->producers.nth(index);
         return producer_authority{itr->first, itr->second};
      }
      return optional<producer_authority>{};
   }

   uint32_t block_header_state::calc_dpos_last_irreversible( account_name producer_of_next_block )const {
      vector<uint32_t> blocknums; blocknums.reserve( producer_to_last_implied_irb.size() );
      for( auto& i : producer_to_last_implied_irb ) {
         blocknums.push_back( (i.first == producer_of_next_block) ? dpos_proposed_irreversible_blocknum : i.second);
      }
      /// 2/3 must be greater, so if I go 1/3 into the list sorted from low to high, then 2/3 are greater

      if( blocknums.size() == 0 ) return 0;

      std::size_t index = (blocknums.size()-1) / 3;
      std::nth_element( blocknums.begin(),  blocknums.begin() + index, blocknums.end() );
      return blocknums[ index ];
   }

   pending_block_header_state  block_header_state::next( block_timestamp_type when,
                                                         uint16_t num_prev_blocks_to_confirm,
                                                         const backup_block_extension& backup_ext)const
   {
      pending_block_header_state result;

      if( when != block_timestamp_type() ) {
        EOS_ASSERT( when > header.timestamp, block_validate_exception, "next block must be in the future" );
      } else {
        (when = header.timestamp).slot++;
      }
      producer_authority prod_auth;
      if(!backup_ext.is_backup){
         // fc_dlog(_backup_block_trace_log,"[BACKUP_TRACE] next is main block......");
         prod_auth = get_scheduled_producer(when);
         fc_dlog(_backup_block_trace_log,"[BACKUP_TRACE] get_schedule_producer: ${bp}", ("bp", prod_auth.producer_name));
      }else {
         // fc_dlog(_backup_block_trace_log,"[BACKUP_TRACE] next is backup block......");
         auto temp = get_backup_scheduled_producer(when);
         EOS_ASSERT(temp.valid(),block_validate_exception, "get next backup block scheduled producer failed");
         prod_auth = *temp;
         fc_dlog(_backup_block_trace_log,"[BACKUP_TRACE] get_backup_schedule_producer: ${bp}", ("bp", prod_auth.producer_name));
      }

      auto itr = producer_to_last_produced.find( prod_auth.producer_name );
      if( itr != producer_to_last_produced.end() ) {
        EOS_ASSERT( itr->second < (block_num+1) - num_prev_blocks_to_confirm, producer_double_confirm,
                    "producer ${prod} double-confirming known range",
                    ("prod", prod_auth.producer_name)("num", block_num+1)
                    ("confirmed", num_prev_blocks_to_confirm)("last_produced", itr->second) );
      }

      result.block_num                                       = block_num + 1;
      result.previous                                        = id;
      result.timestamp                                       = when;
      result.confirmed                                       = num_prev_blocks_to_confirm;
      result.active_schedule_version                         = active_schedule.version;
      result.prev_activated_protocol_features                = activated_protocol_features;

      result.valid_block_signing_authority                   = prod_auth.authority;
      result.producer                                        = prod_auth.producer_name;
      result.backup_ext                                       = backup_ext;
      // fc_dlog(_backup_block_trace_log,"[BACKUP_TRACE] next block producer is: ${bp} .....",("bp",proauth.producer_name)); // TODO: remove?

      result.blockroot_merkle = blockroot_merkle;
      result.blockroot_merkle.append( id );

      /// grow the confirmed count
      static_assert(std::numeric_limits<uint8_t>::max() >= (config::max_producers * 2 / 3) + 1, "8bit confirmations may not be able to hold all of the needed confirmations");

      // This uses the previous block active_schedule because thats the "schedule" that signs and therefore confirms _this_ block
      auto num_active_producers = active_schedule.producers.size();
      uint32_t required_confs = (uint32_t)(num_active_producers * 2 / 3) + 1;

      if( confirm_count.size() < config::maximum_tracked_dpos_confirmations ) {
         result.confirm_count.reserve( confirm_count.size() + 1 );
         result.confirm_count  = confirm_count;
         result.confirm_count.resize( confirm_count.size() + 1 );
         result.confirm_count.back() = (uint8_t)required_confs;
      } else {
         result.confirm_count.resize( confirm_count.size() );
         memcpy( &result.confirm_count[0], &confirm_count[1], confirm_count.size() - 1 );
         result.confirm_count.back() = (uint8_t)required_confs;
      }

      auto new_dpos_proposed_irreversible_blocknum = dpos_proposed_irreversible_blocknum;

      int32_t i = (int32_t)(result.confirm_count.size() - 1);
      uint32_t blocks_to_confirm = num_prev_blocks_to_confirm + 1; /// confirm the head block too
      while( i >= 0 && blocks_to_confirm ) {
        --result.confirm_count[i];
        //idump((confirm_count[i]));
        if( result.confirm_count[i] == 0 )
        {
            uint32_t block_num_for_i = result.block_num - (uint32_t)(result.confirm_count.size() - 1 - i);
            new_dpos_proposed_irreversible_blocknum = block_num_for_i;
            //idump((dpos2_lib)(block_num)(dpos_irreversible_blocknum));

            if (i == static_cast<int32_t>(result.confirm_count.size() - 1)) {
               result.confirm_count.resize(0);
            } else {
               memmove( &result.confirm_count[0], &result.confirm_count[i + 1], result.confirm_count.size() - i  - 1);
               result.confirm_count.resize( result.confirm_count.size() - i - 1 );
            }

            break;
        }
        --i;
        --blocks_to_confirm;
      }

      result.dpos_proposed_irreversible_blocknum   = new_dpos_proposed_irreversible_blocknum;
      result.dpos_irreversible_blocknum            = calc_dpos_last_irreversible( prod_auth.producer_name );

      result.prev_pending_schedule                 = pending_schedule;
      const auto& pre_backup_schedule = active_backup_schedule.get_schedule();

      if( !pending_schedule.data_empty() &&
          result.dpos_irreversible_blocknum >= pending_schedule.schedule_lib_num )
      {
         if (pending_schedule.schedule.contains<producer_authority_schedule>()) {
            result.active_schedule = pending_schedule.schedule.get<producer_authority_schedule>();
         } else {
            // merge producer changes to main active producers
            const auto& schedule_change = pending_schedule.schedule.get<producer_schedule_change>();

            EOS_ASSERT( schedule_change.version == active_schedule.version + 1, producer_schedule_exception, "wrong producer schedule version specified" );
            result.active_schedule.version = schedule_change.version;

            const auto& main_changes = schedule_change.main_changes;
            flat_map<name, block_signing_authority> new_producers;
            if (!main_changes.clear_existed) {
               for (const auto& p : active_schedule.producers) {
                  new_producers[p.producer_name] = p.authority;
               }
            } // else clear existed producers

            producer_change_merger::merge(main_changes, new_producers);
            auto& new_active_producers = result.active_schedule.producers;
            new_active_producers.resize(new_producers.size());
            for (size_t i = 0; i < new_producers.size(); i++) {
               new_active_producers[i] = producer_authority{std::move(new_producers.nth(i)->first), std::move(new_producers.nth(i)->second)};
            }

            const auto& backup_changes = schedule_change.backup_changes;
            auto new_backup_schedule = std::make_shared<backup_producer_schedule>();
            if (pre_backup_schedule && !backup_changes.clear_existed) {
               *new_backup_schedule = *pre_backup_schedule; // deep copy
            }
            producer_change_merger::merge(backup_changes, new_backup_schedule->producers);
            new_backup_schedule->version = schedule_change.version;
            result.active_backup_schedule.schedule = new_backup_schedule;
         }

         flat_map<account_name,uint32_t> new_producer_to_last_produced;

         for( const auto& pro : result.active_schedule.producers ) {
            if( pro.producer_name == result.producer ) {
               new_producer_to_last_produced[pro.producer_name] = result.block_num;
            } else {
               auto existing = producer_to_last_produced.find( pro.producer_name );
               if( existing != producer_to_last_produced.end() ) {
                  new_producer_to_last_produced[pro.producer_name] = existing->second;
               } else {
                  new_producer_to_last_produced[pro.producer_name] = result.dpos_irreversible_blocknum;
               }
            }
         }
         new_producer_to_last_produced[result.producer] = result.block_num;

         result.producer_to_last_produced = std::move( new_producer_to_last_produced );

         flat_map<account_name,uint32_t> new_producer_to_last_implied_irb;

         for( const auto& pro : result.active_schedule.producers ) {
            if( pro.producer_name == result.producer ) {
               new_producer_to_last_implied_irb[pro.producer_name] = dpos_proposed_irreversible_blocknum;
            } else {
               auto existing = producer_to_last_implied_irb.find( pro.producer_name );
               if( existing != producer_to_last_implied_irb.end() ) {
                  new_producer_to_last_implied_irb[pro.producer_name] = existing->second;
               } else {
                  new_producer_to_last_implied_irb[pro.producer_name] = result.dpos_irreversible_blocknum;
               }
            }
         }

         result.producer_to_last_implied_irb = std::move( new_producer_to_last_implied_irb );

         result.was_pending_promoted = true;
      } else {
         result.active_schedule                  = active_schedule;
         result.producer_to_last_produced        = producer_to_last_produced;
         result.producer_to_last_produced[result.producer] = result.block_num;
         result.producer_to_last_implied_irb     = producer_to_last_implied_irb;
         result.producer_to_last_implied_irb[result.producer] = dpos_proposed_irreversible_blocknum;

      }
      result.active_backup_schedule.pre_schedule = pre_backup_schedule;

      return result;
   }

   signed_block_header pending_block_header_state::make_block_header(
                                                      const checksum256_type& transaction_mroot,
                                                      const checksum256_type& action_mroot,
                                                      const block_producer_schedule_change& producer_schedule_change,
                                                      vector<digest_type>&& new_protocol_feature_activations,
                                                      const protocol_feature_set& pfs ) const
   {
      signed_block_header h;

      h.timestamp         = timestamp;
      h.producer          = producer;
      h.confirmed         = confirmed;
      h.previous          = previous;
      h.transaction_mroot = transaction_mroot;
      h.action_mroot      = action_mroot;
      h.schedule_version  = active_schedule_version;
      h.set_backup_ext(backup_ext);

      if( new_protocol_feature_activations.size() > 0 ) {
         emplace_extension(
               h.header_extensions,
               protocol_feature_activation::extension_id(),
               fc::raw::pack( protocol_feature_activation{ std::move(new_protocol_feature_activations) } )
         );
      }

      emplace_produce_change_ext_visitor produce_change_visitor(pfs, prev_activated_protocol_features, h);
      producer_schedule_change.visit(produce_change_visitor);

      if( detail::is_builtin_activated(prev_activated_protocol_features, pfs, builtin_protocol_feature_t::apos ) ){
         emplace_extension(
            h.header_extensions,
            backup_block_extension::extension_id(),
            fc::raw::pack( backup_ext )
         );
      }

      // if (new_producers) {
      //    if ( detail::is_builtin_activated(prev_activated_protocol_features, pfs, builtin_protocol_feature_t::wtmsig_block_signatures) ) {
      //       // add the header extension to update the block schedule
      //       emplace_extension(
      //             h.header_extensions,
      //             producer_schedule_change_extension::extension_id(),
      //             fc::raw::pack( producer_schedule_change_extension( *new_producers ) )
      //       );
      //    } else {
      //       legacy::producer_schedule_type downgraded_producers;
      //       downgraded_producers.version = new_producers->version;
      //       for (const auto &p : new_producers->producers) {
      //          p.authority.visit([&downgraded_producers, &p](const auto& auth){
      //             EOS_ASSERT(auth.keys.size() == 1 && auth.keys.front().weight == auth.threshold, producer_schedule_exception, "multisig block signing present before enabled!");
      //             downgraded_producers.producers.emplace_back(legacy::producer_key{p.producer_name, auth.keys.front().key});
      //          });
      //       }
      //       h.new_producers = std::move(downgraded_producers);
      //    }
      // }

      return h;
   }

   block_header_state pending_block_header_state::_finish_next(
                                 const signed_block_header& h,
                                 const protocol_feature_set& pfs,
                                 const std::function<void( block_timestamp_type,
                                                           const flat_set<digest_type>&,
                                                           const vector<digest_type>& )>& validator

   )&&
   {
      EOS_ASSERT( h.timestamp == timestamp, block_validate_exception, "timestamp mismatch" );
      EOS_ASSERT( h.previous == previous, unlinkable_block_exception, "previous mismatch" );
      EOS_ASSERT( h.confirmed == confirmed, block_validate_exception, "confirmed mismatch" );
      EOS_ASSERT( h.producer == producer, wrong_producer, "wrong producer specified" );
      EOS_ASSERT( h.schedule_version == active_schedule_version, producer_schedule_exception, "schedule_version in signed block is corrupted" );

      auto exts = h.validate_and_extract_header_extensions();

      block_producer_schedule_change maybe_new_producer_schedule;
      std::optional<digest_type> maybe_new_producer_schedule_hash;
      bool wtmsig_enabled = false;

      if (h.new_producers || exts.count(producer_schedule_change_extension::extension_id()) > 0 ) {
         wtmsig_enabled = detail::is_builtin_activated(prev_activated_protocol_features, pfs, builtin_protocol_feature_t::wtmsig_block_signatures);
      }

      if( h.new_producers ) {
         EOS_ASSERT(!wtmsig_enabled, producer_schedule_exception, "Block header contains legacy producer schedule outdated by activation of WTMsig Block Signatures" );

         EOS_ASSERT( !was_pending_promoted, producer_schedule_exception, "cannot set pending producer schedule in the same block in which pending was promoted to active" );

         const auto& new_producers = *h.new_producers;
         EOS_ASSERT( new_producers.version == active_schedule.version + 1, producer_schedule_exception, "wrong producer schedule version specified" );
         EOS_ASSERT( prev_pending_schedule.data_empty(), producer_schedule_exception,
                    "cannot set new pending producers until last pending is confirmed" );

         maybe_new_producer_schedule_hash.emplace(digest_type::hash(new_producers));
         maybe_new_producer_schedule = producer_authority_schedule(new_producers);
      }

      if ( exts.count(producer_schedule_change_extension::extension_id()) > 0 ) {
         EOS_ASSERT(wtmsig_enabled, producer_schedule_exception, "Block header producer_schedule_change_extension before activation of WTMsig Block Signatures" );
         EOS_ASSERT( !was_pending_promoted, producer_schedule_exception, "cannot set pending producer schedule in the same block in which pending was promoted to active" );

         const auto& new_producer_schedule = exts.lower_bound(producer_schedule_change_extension::extension_id())->second.get<producer_schedule_change_extension>();

         EOS_ASSERT( new_producer_schedule.version == active_schedule.version + 1, producer_schedule_exception, "wrong producer schedule version specified" );
         EOS_ASSERT( prev_pending_schedule.data_empty(), producer_schedule_exception,
                     "cannot set new pending producers until last pending is confirmed" );

         maybe_new_producer_schedule_hash.emplace(digest_type::hash(new_producer_schedule));
         maybe_new_producer_schedule = producer_authority_schedule(new_producer_schedule);
      }

      if ( exts.count(producer_schedule_change_extension_v2::extension_id()) > 0 ) {
         // TODO: check apos feature activated
         // EOS_ASSERT(wtmsig_enabled, producer_schedule_exception, "Block header producer_schedule_change_extension before activation of WTMsig Block Signatures" );

         EOS_ASSERT( maybe_new_producer_schedule.which() == 0, producer_schedule_exception, "cannot set pending producer schedule in the same block in other ways" );
         EOS_ASSERT( !was_pending_promoted, producer_schedule_exception, "cannot set pending producer schedule change in the same block in which pending was promoted to active" );

         const auto& new_producer_schedule = exts.lower_bound(producer_schedule_change_extension_v2::extension_id())->second.get<producer_schedule_change_extension_v2>();

         EOS_ASSERT( new_producer_schedule.version == active_schedule.version + 1, producer_schedule_exception, "wrong producer schedule version specified" );
         EOS_ASSERT( prev_pending_schedule.data_empty(), producer_schedule_exception,
                     "cannot set new pending producers until last pending is confirmed" );
         EOS_ASSERT( !new_producer_schedule.empty(), producer_schedule_exception,
                     "new pending producers cannot be empty" );

         maybe_new_producer_schedule_hash.emplace(digest_type::hash(new_producer_schedule));
         maybe_new_producer_schedule = producer_schedule_change(new_producer_schedule);
      }

      protocol_feature_activation_set_ptr new_activated_protocol_features;
      { // handle protocol_feature_activation
         if( exts.count(protocol_feature_activation::extension_id() > 0) ) {
            const auto& new_protocol_features = exts.lower_bound(protocol_feature_activation::extension_id())->second.get<protocol_feature_activation>().protocol_features;
            validator( timestamp, prev_activated_protocol_features->protocol_features, new_protocol_features );

            new_activated_protocol_features =   std::make_shared<protocol_feature_activation_set>(
                                                   *prev_activated_protocol_features,
                                                   new_protocol_features
                                                );
         } else {
            new_activated_protocol_features = std::move( prev_activated_protocol_features );
         }
      }

      auto block_number = block_num;

      block_header_state result( std::move( *static_cast<detail::block_header_state_common*>(this) ) );

      result.id      = h.id();
      result.header  = h;

      result.header_exts = std::move(exts);
      if( maybe_new_producer_schedule.which() > 0 ) {
         result.pending_schedule.schedule = std::move(maybe_new_producer_schedule);
         result.pending_schedule.schedule_hash = std::move(*maybe_new_producer_schedule_hash);
         result.pending_schedule.schedule_lib_num    = block_number;
      } else {
         if( was_pending_promoted ) {
            result.pending_schedule.data_clear(prev_pending_schedule.get_version());
         } else {
            result.pending_schedule.schedule         = std::move( prev_pending_schedule.schedule );
         }
         result.pending_schedule.schedule_hash       = std::move( prev_pending_schedule.schedule_hash );
         result.pending_schedule.schedule_lib_num    = prev_pending_schedule.schedule_lib_num;
      }

      result.activated_protocol_features = std::move( new_activated_protocol_features );

      return result;
   }

   block_header_state pending_block_header_state::finish_next(
                                 const signed_block_header& h,
                                 vector<signature_type>&& additional_signatures,
                                 const protocol_feature_set& pfs,
                                 const std::function<void( block_timestamp_type,
                                                           const flat_set<digest_type>&,
                                                           const vector<digest_type>& )>& validator,
                                 bool skip_validate_signee
   )&&
   {
      if( !additional_signatures.empty() ) {
         bool wtmsig_enabled = detail::is_builtin_activated(prev_activated_protocol_features, pfs, builtin_protocol_feature_t::wtmsig_block_signatures);

         EOS_ASSERT(wtmsig_enabled, producer_schedule_exception, "Block contains multiple signatures before WTMsig block signatures are enabled" );
      }

      auto result = std::move(*this)._finish_next( h, pfs, validator );

      if( !additional_signatures.empty() ) {
         result.additional_signatures = std::move(additional_signatures);
      }

      // ASSUMPTION FROM controller_impl::apply_block = all untrusted blocks will have their signatures pre-validated here
      if( !skip_validate_signee ) {
        result.verify_signee( );
      }

      return result;
   }

   block_header_state pending_block_header_state::finish_next(
                                 signed_block_header& h,
                                 const protocol_feature_set& pfs,
                                 const std::function<void( block_timestamp_type,
                                                           const flat_set<digest_type>&,
                                                           const vector<digest_type>& )>& validator,
                                 const signer_callback_type& signer
   )&&
   {
      auto pfa = prev_activated_protocol_features;

      auto result = std::move(*this)._finish_next( h, pfs, validator );
      result.sign( signer );
      h.producer_signature = result.header.producer_signature;

      if( !result.additional_signatures.empty() ) {
         bool wtmsig_enabled = detail::is_builtin_activated(pfa, pfs, builtin_protocol_feature_t::wtmsig_block_signatures);
         EOS_ASSERT(wtmsig_enabled, producer_schedule_exception, "Block was signed with multiple signatures before WTMsig block signatures are enabled" );
      }

      return result;
   }

   /**
    *  Transitions the current header state into the next header state given the supplied signed block header.
    *
    *  Given a signed block header, generate the expected template based upon the header time,
    *  then validate that the provided header matches the template.
    *
    *  If the header specifies new_producers then apply them accordingly.
    */
   block_header_state block_header_state::next(
                        const signed_block_header& h,
                        vector<signature_type>&& _additional_signatures,
                        const protocol_feature_set& pfs,
                        const std::function<void( block_timestamp_type,
                                                  const flat_set<digest_type>&,
                                                  const vector<digest_type>& )>& validator,
                        bool skip_validate_signee )const
   {
      return next( h.timestamp, h.confirmed, h.backup_ext() ).finish_next(
                h, std::move(_additional_signatures), pfs, validator, skip_validate_signee );
   }

   digest_type   block_header_state::sig_digest()const {
      auto header_bmroot = digest_type::hash( std::make_pair( header.digest(), blockroot_merkle.get_root() ) );
      return digest_type::hash( std::make_pair(header_bmroot, pending_schedule.schedule_hash) );
   }

   void block_header_state::sign( const signer_callback_type& signer ) {
      auto d = sig_digest();
      auto sigs = signer( d );

      EOS_ASSERT(!sigs.empty(), no_block_signatures, "Signer returned no signatures");
      header.producer_signature = sigs.back();
      sigs.pop_back();

      additional_signatures = std::move(sigs);

      verify_signee();
   }

   void block_header_state::verify_signee( )const {

      size_t num_keys_in_authority = valid_block_signing_authority.visit([](const auto &a){ return a.keys.size(); });
      EOS_ASSERT(1 + additional_signatures.size() <= num_keys_in_authority, wrong_signing_key,
                 "number of block signatures (${num_block_signatures}) exceeds number of keys in block signing authority (${num_keys})",
                 ("num_block_signatures", 1 + additional_signatures.size())
                 ("num_keys", num_keys_in_authority)
                 ("authority", valid_block_signing_authority)
      );

      std::set<public_key_type> keys;
      auto digest = sig_digest();
      keys.emplace(fc::crypto::public_key( header.producer_signature, digest, true ));

      for (const auto& s: additional_signatures) {
         auto res = keys.emplace(s, digest, true);
         EOS_ASSERT(res.second, wrong_signing_key, "block signed by same key twice", ("key", *res.first));
      }

      bool is_satisfied = false;
      size_t relevant_sig_count = 0;

      std::tie(is_satisfied, relevant_sig_count) = producer_authority::keys_satisfy_and_relevant(keys, valid_block_signing_authority);

      EOS_ASSERT(relevant_sig_count == keys.size(), wrong_signing_key,
                 "block signed by unexpected key",
                 ("signing_keys", keys)("authority", valid_block_signing_authority));

      EOS_ASSERT(is_satisfied, wrong_signing_key,
                 "block signatures do not satisfy the block signing authority",
                 ("signing_keys", keys)("authority", valid_block_signing_authority));
   }

   /**
    *  Reference cannot outlive *this. Assumes header_exts is not mutated after instatiation.
    */
   const vector<digest_type>& block_header_state::get_new_protocol_feature_activations()const {
      static const vector<digest_type> no_activations{};

      if( header_exts.count(protocol_feature_activation::extension_id()) == 0 )
         return no_activations;

      return header_exts.lower_bound(protocol_feature_activation::extension_id())->second.get<protocol_feature_activation>().protocol_features;
   }

   block_header_state::block_header_state( legacy::snapshot_block_header_state_v2&& snapshot )
   {
      block_num                             = snapshot.block_num;
      dpos_proposed_irreversible_blocknum   = snapshot.dpos_proposed_irreversible_blocknum;
      dpos_irreversible_blocknum            = snapshot.dpos_irreversible_blocknum;
      active_schedule                       = producer_authority_schedule( snapshot.active_schedule );
      blockroot_merkle                      = std::move(snapshot.blockroot_merkle);
      producer_to_last_produced             = std::move(snapshot.producer_to_last_produced);
      producer_to_last_implied_irb          = std::move(snapshot.producer_to_last_implied_irb);
      valid_block_signing_authority         = block_signing_authority_v0{ 1, {{std::move(snapshot.block_signing_key), 1}} };
      confirm_count                         = std::move(snapshot.confirm_count);
      id                                    = std::move(snapshot.id);
      header                                = std::move(snapshot.header);
      pending_schedule.schedule_lib_num     = snapshot.pending_schedule.schedule_lib_num;
      pending_schedule.schedule_hash        = std::move(snapshot.pending_schedule.schedule_hash);
      pending_schedule.schedule             = producer_authority_schedule( snapshot.pending_schedule.schedule );
      activated_protocol_features           = std::move(snapshot.activated_protocol_features);
   }


   block_header_state::block_header_state( legacy::snapshot_block_header_state_v3&& snapshot )
   {
      // commons
      block_num                             = snapshot.block_num;
      dpos_proposed_irreversible_blocknum   = snapshot.dpos_proposed_irreversible_blocknum;
      dpos_irreversible_blocknum            = snapshot.dpos_irreversible_blocknum;
      active_schedule                       = producer_authority_schedule( snapshot.active_schedule );
      blockroot_merkle                      = std::move(snapshot.blockroot_merkle);
      producer_to_last_produced             = std::move(snapshot.producer_to_last_produced);
      producer_to_last_implied_irb          = std::move(snapshot.producer_to_last_implied_irb);
      valid_block_signing_authority         = std::move(snapshot.valid_block_signing_authority);
      confirm_count                         = std::move(snapshot.confirm_count);
      active_backup_schedule.schedule       = std::make_shared<backup_producer_schedule>();

      // block header state
      id                                    = std::move(snapshot.id);
      header                                = std::move(snapshot.header);
      pending_schedule.schedule_lib_num     = snapshot.pending_schedule.schedule_lib_num;
      pending_schedule.schedule_hash        = std::move(snapshot.pending_schedule.schedule_hash);
      if (snapshot.pending_schedule.schedule.producers.size() > 0) {
         pending_schedule.schedule             = std::move(snapshot.pending_schedule.schedule);
      } else {
         pending_schedule.data_clear(snapshot.pending_schedule.schedule.version);
      }

      activated_protocol_features           = std::move(snapshot.activated_protocol_features);
      additional_signatures                  = std::move(snapshot.additional_signatures);
   }

} } /// namespace eosio::chain
