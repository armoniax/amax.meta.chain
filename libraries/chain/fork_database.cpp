#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/exceptions.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/io/fstream.hpp>
#include <fstream>

namespace eosio
{
   namespace chain
   {
      using boost::multi_index_container;
      using namespace boost::multi_index;

      const uint32_t fork_database::magic_number = 0x30510FDB;

      const uint32_t fork_database::min_supported_version = 2;
      const uint32_t fork_database::max_supported_version = 2;

      // work around block_state::is_valid being private
      inline bool block_state_is_valid(const block_state &bs)
      {
         return bs.is_valid();
      }

      // TODO: remove??
      // inline bool block_state_is_main(const block_state &bs)
      // {
      //    return !bs.is_backup();
      // }
      /**
       * History:
       * Version 1: initial version of the new refactored fork database portable format
       */

      enum class block_valid_status: uint8_t {
         valid             = 0,  // valid block
         invalid_main      = 1,  // invalid main block
         invalid_backup    = 2,  // invalid backup block
      };

      inline block_valid_status get_block_valid_status(const block_state &bs) {
         if (block_state_is_valid(bs)) {
            return block_valid_status::valid;
         } else if (!bs.is_backup()){
            return block_valid_status::invalid_main;
         } else { // unvalidated backup block
            return block_valid_status::invalid_backup;
         }
      }


      struct by_block_id;
      struct by_lib_block_num;
      struct by_prev;
      struct best_backup_by_prev;
      typedef multi_index_container<
          block_state_ptr,
          indexed_by<
              hashed_unique<tag<by_block_id>, member<block_header_state, block_id_type, &block_header_state::id>, std::hash<block_id_type>>,
              ordered_non_unique<tag<by_prev>, const_mem_fun<block_header_state, const block_id_type &, &block_header_state::prev>>,
              ordered_unique<tag<best_backup_by_prev>,
                           composite_key<block_header_state,
                                    const_mem_fun<block_header_state, const block_id_type &, &block_header_state::prev>,
                                    const_mem_fun<block_header_state, bool, &block_header_state::is_backup>,
                                    member<block_header_state, block_id_type, &block_header_state::id>>,
                           composite_key_compare<sha256_less, std::greater<bool> , sha256_less>>,
              ordered_unique<tag<by_lib_block_num>,
                             composite_key<block_state,
                                           global_fun<const block_state &, block_valid_status, &get_block_valid_status>,
                                          //  global_fun<const block_state &, bool, &block_state_is_main>,
                                           member<detail::block_header_state_common, uint32_t, &detail::block_header_state_common::dpos_irreversible_blocknum>,
                                           member<detail::block_header_state_common, uint32_t, &detail::block_header_state_common::block_num>,
                                           member<block_header_state, block_id_type, &block_header_state::id>>,
                             composite_key_compare<
                                 std::less<block_valid_status>,
                                 std::greater<uint32_t>,
                                 std::greater<uint32_t>,
                                 // std::greater<bool>,
                                 sha256_less>>>>
          fork_multi_index_type;

      bool first_preferred(const block_header_state &lhs, const block_header_state &rhs)
      {
         return std::tie(lhs.dpos_irreversible_blocknum, lhs.block_num) > std::tie(rhs.dpos_irreversible_blocknum, rhs.block_num);
      }

      template<typename Iterator>
      struct iterator_range {
            Iterator itr;
            Iterator end;

            iterator_range(Iterator itr, Iterator end): itr(itr), end(end) {}

            inline bool has_value() const { return itr != end; }

            void next() {
               itr++;
            }

            static inline iterator_range* select_less(iterator_range* a, iterator_range* b) {
               if (a && b && a->has_value() && b->has_value() ) {
                  return first_preferred(**(a->itr), **(b->itr)) ? b : a;
               } else if (a && a->has_value()) {
                  return a;
               } else if (b && b->has_value()) {
                  return b;
               }
               return nullptr;
            }
      };

      struct fork_database_impl
      {
         fork_database_impl(fork_database &self, const fc::path &data_dir)
             : self(self), datadir(data_dir)
         {
         }

         fork_database &self;
         fork_multi_index_type index;
         block_state_ptr root; // Only uses the block_header_state portion
         block_state_ptr root_previous; // this piont to root's previous block state
         std::map<block_id_type, block_state_ptr> backup_siblings_to_root; //point to backup blocks to root(only exist in memory not in index, same block num with root)
         block_state_ptr head;
         fc::path datadir;

         void add(const block_state_ptr &n,
                  bool ignore_duplicate, bool validate,
                  const std::function<void(block_timestamp_type,
                                           const flat_set<digest_type> &,
                                           const vector<digest_type> &)> &validator);
      };

      fork_database::fork_database(const fc::path &data_dir)
          : my(new fork_database_impl(*this, data_dir))
      {
      }

      void fork_database::open(const std::function<void(block_timestamp_type,
                                                        const flat_set<digest_type> &,
                                                        const vector<digest_type> &)> &validator)
      {
         if (!fc::is_directory(my->datadir))
            fc::create_directories(my->datadir);

         auto fork_db_dat = my->datadir / config::forkdb_filename;
         if (fc::exists(fork_db_dat))
         {
            try
            {
               string content;
               fc::read_file_contents(fork_db_dat, content);

               fc::datastream<const char *> ds(content.data(), content.size());

               // validate totem
               uint32_t totem = 0;
               fc::raw::unpack(ds, totem);
               EOS_ASSERT(totem == magic_number, fork_database_exception,
                          "Fork database file '${filename}' has unexpected magic number: ${actual_totem}. Expected ${expected_totem}",
                          ("filename", fork_db_dat.generic_string())("actual_totem", totem)("expected_totem", magic_number));

               // validate version
               uint32_t version = 0;
               fc::raw::unpack(ds, version);
               EOS_ASSERT(version >= min_supported_version && version <= max_supported_version,
                          fork_database_exception,
                          "Unsupported version of fork database file '${filename}'. "
                          "Fork database version is ${version} while code supports version(s) [${min},${max}]",
                          ("filename", fork_db_dat.generic_string())("version", version)("min", min_supported_version)("max", max_supported_version));

               shared_ptr<block_header_state> pbhs;
               fc::raw::unpack(ds, pbhs);
               block_header_state bhs;
               fc::raw::unpack(ds, bhs);
               reset(bhs, pbhs.get());

               unsigned_int num_blocks_to_backup_siblings;
               fc::raw::unpack(ds, num_blocks_to_backup_siblings);
               my->backup_siblings_to_root.clear();
               for(uint32_t i = 0, n = num_blocks_to_backup_siblings.value; i < n; ++i){
                  block_state_ptr s = std::make_shared<block_state>();
                  fc::raw::unpack(ds, *s);
                  my->backup_siblings_to_root.insert(std::pair( s->id, s));
               }

               unsigned_int size;
               fc::raw::unpack(ds, size);
               for (uint32_t i = 0, n = size.value; i < n; ++i)
               {
                  block_state_ptr s = std::make_shared<block_state>();
                  fc::raw::unpack(ds, *s);
                  // do not populate transaction_metadatas, they will be created as needed in apply_block with appropriate key recovery
                  s->header_exts = s->block->validate_and_extract_header_extensions();
                  my->add(s, false, true, validator);
               }
               block_id_type head_id;
               fc::raw::unpack(ds, head_id);

               if (my->root->id == head_id)
               {
                  my->head = my->root;
               }
               else
               {
                  my->head = get_block(head_id);
                  EOS_ASSERT(my->head, fork_database_exception,
                             "could not find head while reconstructing fork database from file; '${filename}' is likely corrupted",
                             ("filename", fork_db_dat.generic_string()));
               }

               auto candidate = my->index.get<by_lib_block_num>().begin();
               if (candidate == my->index.get<by_lib_block_num>().end() || !(*candidate)->is_valid())
               {
                  EOS_ASSERT(my->head->id == my->root->id, fork_database_exception,
                             "head not set to root despite no better option available; '${filename}' is likely corrupted",
                             ("filename", fork_db_dat.generic_string()));
               }
               else
               {
                  if(!(*candidate)->is_backup()){
                      // header is longest chain, so it should be only and only if loaded from fork_db.dat file
                      EOS_ASSERT(!first_preferred(**candidate, *my->head), fork_database_exception,
                             "head not set to best available option available; '${filename}' is likely corrupted",
                             ("filename", fork_db_dat.generic_string()));
                  }else{
                      EOS_ASSERT(first_preferred(**candidate, *my->head), fork_database_exception,
                             "head not set to best available option available; '${filename}' is likely corrupted",
                             ("filename", fork_db_dat.generic_string()));
                  }

               }
            }
            FC_CAPTURE_AND_RETHROW((fork_db_dat))

            fc::remove(fork_db_dat);
         }
      }

      void fork_database::close()
      {
         auto fork_db_dat = my->datadir / config::forkdb_filename;

         if (!my->root)
         {
            if (my->index.size() > 0)
            {
               elog("fork_database is in a bad state when closing; not writing out '${filename}'",
                    ("filename", fork_db_dat.generic_string()));
            }
            return;
         }

         std::ofstream out(fork_db_dat.generic_string().c_str(), std::ios::out | std::ios::binary | std::ofstream::trunc);
         fc::raw::pack(out, magic_number);
         fc::raw::pack(out, max_supported_version); // write out current version which is always max_supported_version
         fc::raw::pack(out, static_cast<block_header_state_ptr>(my->root_previous));
         fc::raw::pack(out, *static_cast<block_header_state *>(&*my->root));
         uint32_t num_blocks_to_backup_siblings = my->backup_siblings_to_root.size();
         fc::raw::pack(out, unsigned_int{num_blocks_to_backup_siblings});
         for( const auto& bs : my->backup_siblings_to_root){
            fc::raw::pack(out, *(bs.second));
         }
         uint32_t num_blocks_in_fork_db = my->index.size();
         fc::raw::pack(out, unsigned_int{num_blocks_in_fork_db});

         const auto &indx = my->index.get<by_lib_block_num>();

         auto unvalidated_backup_ir = iterator_range(
            /* itr = */ indx.rbegin(),
            /* end = */ boost::make_reverse_iterator(indx.lower_bound(block_valid_status::invalid_backup))
         );

         auto unvalidated_main_ir = iterator_range(
            /* itr = */ unvalidated_backup_ir.end,
            /* end = */ boost::make_reverse_iterator(indx.lower_bound(block_valid_status::invalid_main))
         );

         auto validated_ir = iterator_range(
            /* itr = */ unvalidated_main_ir.end,
            /* end = */ indx.rend()
         );

         typedef decltype(validated_ir) iterator_range_type;
         iterator_range_type* ir_ptr = nullptr;
         size_t block_count = 0;
         while (unvalidated_backup_ir.has_value() || unvalidated_main_ir.has_value() || validated_ir.has_value()) {
            ir_ptr = iterator_range_type::select_less(&validated_ir, &unvalidated_main_ir);
            ir_ptr = iterator_range_type::select_less(ir_ptr, &unvalidated_backup_ir);
            EOS_ASSERT(ir_ptr != nullptr, fork_database_exception, "invalid index iterator range");
            fc::raw::pack(out, *(*ir_ptr->itr));
            ir_ptr->next();
            block_count++;
         }

         EOS_ASSERT(block_count == num_blocks_in_fork_db, fork_database_exception, "serialized block count(${bc}) mismatch with ${db_bc}", ("bc", block_count)("db_bc", num_blocks_in_fork_db));

         if (my->head)
         {
            fc::raw::pack(out, my->head->id);
         }
         else
         {
            elog("head not set in fork database; '${filename}' will be corrupted",
                 ("filename", fork_db_dat.generic_string()));
         }

         my->index.clear();
      }

      fork_database::~fork_database()
      {
         close();
      }

      void fork_database::reset(const block_header_state &root_bhs, const block_header_state* root_previous_bhs)
      {
         my->index.clear();
         my->root = std::make_shared<block_state>();
         static_cast<block_header_state &>(*my->root) = root_bhs;
         my->root->validated = true;
         my->head = my->root;

         my->root_previous.reset();

         // EOS_ASSERT(root_previous_bhs, fork_database_exception,
         //             "Root previous can not be empty. root_id:${id}", ("id", root_bhs.id));
         // EOS_ASSERT(root_bhs.prev() == root_previous_bhs->id, fork_database_exception,
         //             "Root previous id mismatch. root_id:${id}, expected:${expected}, got:${got}",
         //             ("id", root_bhs.id)("expected", root_bhs.prev())("got", root_previous_bhs->id));
         if(root_previous_bhs){
            my->root_previous = std::make_shared<block_state>();
            static_cast<block_header_state &>(*my->root_previous) = *root_previous_bhs;
         }
      }

      void fork_database::rollback_head_to_root()
      {
         auto &by_id_idx = my->index.get<by_block_id>();
         auto itr = by_id_idx.begin();
         while (itr != by_id_idx.end())
         {
            by_id_idx.modify(itr, [&](block_state_ptr &bsp)
                             { bsp->validated = false; });
            ++itr;
         }
         my->head = my->root;
      }

      void fork_database::advance_root(const block_id_type &id)
      {
         EOS_ASSERT(my->root, fork_database_exception, "root not yet set");

         auto new_root = get_block(id);
         if(new_root->header.previous == my->root->id){
            my->root_previous = my->root;
         }else{
            my->root_previous = get_block(new_root->header.previous);
         }

         EOS_ASSERT(new_root, fork_database_exception,
                    "cannot advance root to a block that does not exist in the fork database");
         EOS_ASSERT(new_root->is_valid(), fork_database_exception,
                    "cannot advance root to a block that has not yet been validated");

         vector<block_id_type> blocks_to_remove;
         for (auto b = new_root; b;)
         {
            blocks_to_remove.push_back(b->header.previous);
            b = get_block(blocks_to_remove.back());
            EOS_ASSERT(b || blocks_to_remove.back() == my->root->id, fork_database_exception, "invariant violation: orphaned branch was present in forked database");
         }
         const auto &previdx = my->index.get<by_prev>();
         auto previtr = previdx.lower_bound(new_root->header.previous);
         my->backup_siblings_to_root.clear();
         while (previtr != previdx.end()){
             if( (*previtr)->block->is_backup() && (*previtr)->header.previous == new_root->header.previous ) {
                 my->backup_siblings_to_root.insert(std::pair((*previtr)->block->id(),(*previtr)));
             }
             ++previtr;
         }
         // The new root block should be erased from the fork database index individually rather than with the remove method,
         // because we do not want the blocks branching off of it to be removed from the fork database.

         /**
          *@Description: this is necessary ,if not do this operation,using remove method or not may lead to all blocks would be removed from fork database
          */
         my->index.erase(my->index.find(id));

         // The other blocks to be removed are removed using the remove method so that orphaned branches do not remain in the fork database.
         for (const auto &block_id : blocks_to_remove)
         {
            remove(block_id);
         }

         // Even though fork database no longer needs block or trxs when a block state becomes a root of the tree,
         // avoid mutating the block state at all, for example clearing the block shared pointer, because other
         // parts of the code which run asynchronously (e.g. mongo_db_plugin) may later expect it remain unmodified.

         my->root = new_root;
         my->root->active_backup_schedule.ensure_persisted();
         if (my->root_previous) {
            my->root_previous->active_backup_schedule.ensure_persisted();
         }
      }

      block_header_state_ptr fork_database::get_block_header(const block_id_type &id, bool finding_root_previous) const
      {
         const auto &by_id_idx = my->index.get<by_block_id>();

         if (my->root->id == id)
         {
            return my->root;
         }

         auto itr = my->index.find(id);
         if (itr != my->index.end())
            return *itr;

         if(finding_root_previous && my->root_previous && id == my->root_previous->id)
            return my->root_previous;

         std::map<block_id_type,block_state_ptr>::iterator it = my->backup_siblings_to_root.find(id);
         return it==my->backup_siblings_to_root.end() ? block_state_ptr() : it->second;

         return block_header_state_ptr();
      }

      void fork_database_impl::add(const block_state_ptr &n,
                                   bool ignore_duplicate, bool validate,
                                   const std::function<void(block_timestamp_type,
                                                            const flat_set<digest_type> &,
                                                            const vector<digest_type> &)> &validator)
      {
         EOS_ASSERT(root, fork_database_exception, "root not yet set");
         EOS_ASSERT(n, fork_database_exception, "attempt to add null block state");

         auto prev_bh = self.get_block_header(n->header.previous, n->header.is_backup());

         EOS_ASSERT(prev_bh, unlinkable_block_exception,
                    "unlinkable block", ("id", n->id)("previous", n->header.previous));

         // ensure backup active schedule is valid
         n->active_backup_schedule.ensure_pre_schedule(prev_bh->active_backup_schedule);

         if (validate)
         {
            try
            {
               const auto &exts = n->header_exts;

               if (exts.count(protocol_feature_activation::extension_id()) > 0)
               {
                  const auto &new_protocol_features = exts.lower_bound(protocol_feature_activation::extension_id())->second.get<protocol_feature_activation>().protocol_features;
                  validator(n->header.timestamp, prev_bh->activated_protocol_features->protocol_features, new_protocol_features);
               }
            }
            EOS_RETHROW_EXCEPTIONS(fork_database_exception, "serialized fork database is incompatible with configured protocol features")
         }

         //add backup block same num as root in its siblings
         if (n->header.previous == root->header.previous && n->header.is_backup())
         {
             backup_siblings_to_root.insert(std::pair(n->id,n));
             return;
         }

         auto inserted = index.insert(n);
         if (!inserted.second)
         {
            if (ignore_duplicate || n->is_backup())
               return;
            EOS_THROW(fork_database_exception, "duplicate block added", ("id", n->id));
         }

         /**
          *@Description: this index sorted by first priority validated, sencond priority greater last irreversiable block num
          *3rd priority greater block num, 4th priority less hash value. so begin block is validated biggest irreversible block
          *num biggest block num ,least hash vale block, in other words, it is header.
          *for backup block here needn't to checkout head
          */
         auto candidate = index.get<by_lib_block_num>().begin();
         if ((*candidate)->is_valid() && !(*candidate)->is_backup())
         {
            head = *candidate;
         }

      }

      void fork_database::add(const block_state_ptr &n, bool ignore_duplicate)
      {
         my->add(n, ignore_duplicate, false,
                 [](block_timestamp_type timestamp,
                    const flat_set<digest_type> &cur_features,
                    const vector<digest_type> &new_features) {});
      }

      const block_state_ptr &fork_database::root() const { return my->root; }
      const block_state_ptr &fork_database::root_previous()const {return my->root_previous; };
      const block_state_ptr &fork_database::head() const { return my->head; }

      block_state_ptr fork_database::pending_head() const
      {
         const auto &indx = my->index.get<by_lib_block_num>();

         auto itr = indx.lower_bound(block_valid_status::invalid_main);
         if (itr != indx.end() && !(*itr)->is_valid() && !(*itr)->is_backup())
         {
            if (first_preferred(**itr, *my->head))
               return *itr;
         }

         return my->head;
      }

      branch_type fork_database::fetch_branch(const block_id_type &h, uint32_t trim_after_block_num) const
      {
         branch_type result;
         for (auto s = get_block(h); s; s = get_block(s->header.previous))
         {
            if (s->block_num <= trim_after_block_num)
               result.push_back(s);
         }

         return result;
      }

      block_state_ptr fork_database::search_on_branch(const block_id_type &h, uint32_t block_num) const
      {
         for (auto s = get_block(h); s; s = get_block(s->header.previous))
         {
            if (s->block_num == block_num)
               return s;
         }

         return {};
      }

      /**
       *  Given two head blocks, return two branches of the fork graph that
       *  end with a common ancestor (same prior block)
       */
      pair<branch_type, branch_type> fork_database::fetch_branch_from(const block_id_type &first,
                                                                      const block_id_type &second) const
      {
         pair<branch_type, branch_type> result;
         auto first_branch = (first == my->root->id) ? my->root : get_block(first);
         auto second_branch = (second == my->root->id) ? my->root : get_block(second);

         EOS_ASSERT(first_branch, fork_db_block_not_found, "block ${id} does not exist", ("id", first));
         EOS_ASSERT(second_branch, fork_db_block_not_found, "block ${id} does not exist", ("id", second));

         while (first_branch->block_num > second_branch->block_num)
         {
            result.first.push_back(first_branch);
            const auto &prev = first_branch->header.previous;
            first_branch = (prev == my->root->id) ? my->root : get_block(prev);
            EOS_ASSERT(first_branch, fork_db_block_not_found,
                       "block ${id} does not exist",
                       ("id", prev));
         }

         while (second_branch->block_num > first_branch->block_num)
         {
            result.second.push_back(second_branch);
            const auto &prev = second_branch->header.previous;
            second_branch = (prev == my->root->id) ? my->root : get_block(prev);
            EOS_ASSERT(second_branch, fork_db_block_not_found,
                       "block ${id} does not exist",
                       ("id", prev));
         }

         if (first_branch->id == second_branch->id)
            return result;

         while (first_branch->header.previous != second_branch->header.previous)
         {
            result.first.push_back(first_branch);
            result.second.push_back(second_branch);
            const auto &first_prev = first_branch->header.previous;
            first_branch = get_block(first_prev);
            const auto &second_prev = second_branch->header.previous;
            second_branch = get_block(second_prev);
            EOS_ASSERT(first_branch, fork_db_block_not_found,
                       "block ${id} does not exist",
                       ("id", first_prev));
            EOS_ASSERT(second_branch, fork_db_block_not_found,
                       "block ${id} does not exist",
                       ("id", second_prev));
         }

         if (first_branch && second_branch)
         {
            result.first.push_back(first_branch);
            result.second.push_back(second_branch);
         }
         return result;
      } /// fetch_branch_from

      /// remove all of the invalid forks built off of this id including this id
      void fork_database::remove(const block_id_type &id)
      {
         vector<block_id_type> remove_queue{id};
         const auto &previdx = my->index.get<by_prev>();
         const auto head_id = my->head->id;

         for (uint32_t i = 0; i < remove_queue.size(); ++i)
         {
            EOS_ASSERT(remove_queue[i] != head_id, fork_database_exception,
                       "removing the block and its descendants would remove the current head block");
            /**
             *@Description: would return lower bound iterater of a element in index set ,for example
             * block num set {10,11,12,15,16,......}==>lower_bound(15)
             *                         ^
             *                         |   {16,......}
             * this will add all forks extended from this index i (including i) into remove queue.
             */
            auto previtr = previdx.lower_bound(remove_queue[i]);
            while (previtr != previdx.end() && (*previtr)->header.previous == remove_queue[i])
            {

               remove_queue.push_back((*previtr)->id);
               ++previtr;
            }
         }

         for (const auto &block_id : remove_queue)
         {
            auto itr = my->index.find(block_id);
            if (itr != my->index.end())
               my->index.erase(itr);
         }
      }

      void fork_database::mark_valid(const block_state_ptr &h)
      {
         if (h->validated)
            return;

         auto &by_id_idx = my->index.get<by_block_id>();

         auto itr = by_id_idx.find(h->id);
         EOS_ASSERT(itr != by_id_idx.end(), fork_database_exception,
                    "block state not in fork database; cannot mark as valid",
                    ("id", h->id));

         by_id_idx.modify(itr, [](block_state_ptr &bsp)
                          { bsp->validated = true; });

         auto candidate = my->index.get<by_lib_block_num>().begin();
         if (first_preferred(**candidate, *my->head) && !(*candidate)->is_backup())
         {
            my->head = *candidate;
         }

         if (first_preferred(**candidate, *my->head) && (*candidate)->is_backup() && (*candidate)->block_num == my->head->block_num)
         {
            dlog("same height backup and main, backup irreversible num: ${bi},main irreversible num: ${mi}, new block irreversible: ${ni}",("bi",(*candidate)->dpos_irreversible_blocknum)("mi",my->head->dpos_irreversible_blocknum)("ni",(*itr)->dpos_irreversible_blocknum));
         }
      }

      block_state_ptr fork_database::get_block(const block_id_type &id) const
      {
         auto itr = my->index.find(id);
         if (itr != my->index.end())
            return *itr;
         std::map<block_id_type,block_state_ptr>::iterator it = my->backup_siblings_to_root.find(id);
         return it==my->backup_siblings_to_root.end() ? block_state_ptr() : it->second;
      }

      block_state_ptr fork_database::get_backup_head_block( const block_id_type head_prev) const
      {
         const auto &previdx = my->index.get<best_backup_by_prev>();
         auto best_backup_state = previdx.lower_bound( head_prev );
         if( best_backup_state != previdx.end() && (*best_backup_state)->header.is_backup() && (*best_backup_state)->header.previous == head_prev ){
            return *best_backup_state;
         }
         return block_state_ptr();
      }

      block_state_ptr  fork_database::get_producer_backup_block( name prod, const block_id_type prev ) const
      {
         const auto &previdx = my->index.get<best_backup_by_prev>();
         auto best_backup_state = previdx.lower_bound( prev );
         while( best_backup_state != previdx.end() && (*best_backup_state)->header.previous == prev ){
            if( (*best_backup_state)->header.producer == prod ){
               return *best_backup_state;
            }
            best_backup_state++;
         }
         return block_state_ptr();
      }
   }
} /// eosio::chain
