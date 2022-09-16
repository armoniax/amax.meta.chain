#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/producer_schedule.hpp>
#include <eosio/testing/tester.hpp>

#include <boost/test/unit_test.hpp>
#include <contracts.hpp>

#include "fork_test_utilities.hpp"

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio::testing;
using namespace eosio::chain;
using mvo = fc::mutable_variant_object;

namespace variant_ext {
   static const vector<string> producer_change_type_names = {
      "producer_authority_add",
      "producer_authority_modify",
      "producer_authority_del"
   };

   fc::variant to_variant(const block_signing_authority& authority) {
      return authority.visit([](const auto& a) {
         fc::variants vars(2);
         vars[0] = std::string(std::decay_t<decltype(a)>::abi_type_name());
         fc::to_variant(a, vars[1]);
         return std::move(vars);
      });
   }

   template<typename T>
   fc::variant to_variant(const optional<T>& vo) {
      return vo ? to_variant(*vo) : fc::variant();
   }

   fc::variant to_variant(const producer_change_record& record) {
      variants vars(2);
      uint32_t op = record.which();
      assert(op <= producer_change_type_names.size());
      vars[0] = producer_change_type_names[op];
      auto auth_var = record.visit( [](const auto &c) {
         return to_variant(c.authority);
      });
      vars[1] = mvo()("authority", auth_var);
      return std::move(vars);
   }

   fc::variant to_variant(const flat_map<name, producer_change_record>& changes) {
      variants vars;
      for (const auto &c : changes) {
         variants pair_vars(2);
         pair_vars[0] = c.first;
         pair_vars[1] = to_variant(c.second);
         vars.push_back(std::move(pair_vars));
      }
      return std::move(vars);
   }

   fc::variant to_variant(const producer_change_map &change_map) {
      return mvo()
         ("producer_count", change_map.producer_count)
         ("changes", to_variant(change_map.changes));
   }

   fc::variant to_variant(const proposed_producer_changes &changes) {
      return mvo()
         ("main_changes", to_variant(changes.main_changes))
         ("backup_changes", to_variant(changes.backup_changes));
   }
}

vector<account_name> gen_producer_names(uint32_t count, uint64_t from) {
   vector<account_name> result;
   for (uint32_t n = from; result.size() < count; n++) {
      result.emplace_back(n << 4);
   }
   return result;
}

class producer_change_tester : public TESTER {
public:
   using TESTER::TESTER;

   static constexpr name contract_name = N(prod.change);

   producer_change_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), contract_name } );
      produce_blocks( 2 );

      set_code( contract_name, contracts::producer_change_test_wasm() );
      set_abi( contract_name, contracts::producer_change_test_abi().data() );

      produce_blocks();

      set_privileged(contract_name);
      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( contract_name );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function( abi_serializer_max_time ));
   }

   transaction_trace_ptr push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      return base_tester::push_action( contract_name, name, signer, data);
   }

   transaction_trace_ptr change( const proposed_producer_changes &changes, const optional<int64_t>& expected) {
      return push_action( contract_name, N(change), mvo()
           ( "changes", variant_ext::to_variant(changes) )
           ( "expected", expected )
      );
   }

   transaction_trace_ptr set_privileged( name account ) {
      auto r = base_tester::push_action(config::system_account_name, N(setpriv),
         config::system_account_name,  mvo()("account", account)("is_priv", 1));
      produce_block();
      return r;
   }


   static auto get_producer_private_key( name producer_name, uint64_t version = 1 ) {
      return get_private_key(producer_name, std::to_string(version));
   }

   static auto get_producer_public_key( name producer_name, uint64_t version = 1 ) {
      return get_producer_private_key(producer_name, version).get_public_key();
   }

   block_signing_authority make_producer_authority(name producer_name, uint64_t version = 1){
      auto privkey = get_producer_private_key(producer_name, version);
      auto pubkey = privkey.get_public_key();
      block_signing_private_keys[pubkey] = privkey;
      return block_signing_authority_v0{
         1, {
            {pubkey, 1}
         }
      };
   }

   abi_serializer abi_ser;
};

   // Calculate expected producer given the schedule and slot number
   account_name get_expected_producer(const vector<producer_authority>& schedule, const uint64_t slot) {
      const auto& index = (slot % (schedule.size() * config::producer_repetitions)) / config::producer_repetitions;
      return schedule.at(index).producer_name;
   };

   // Check if two schedule is equal
   bool is_schedule_equal(const vector<producer_authority>& first, const vector<producer_authority>& second) {
      bool is_equal = first.size() == second.size();
      for (uint32_t i = 0; i < first.size(); i++) {
         is_equal = is_equal && first.at(i) == second.at(i);
      }
      return is_equal;
   };

   // Calculate the block num of the next round first block
   // The new producer schedule will become effective when it's in the block of the next round first block
   // However, it won't be applied until the effective block num is deemed irreversible
   uint64_t calc_block_num_of_next_round_first_block(const controller& control){
      auto res = control.head_block_num() + 1;
      const auto blocks_per_round = control.head_block_state()->active_schedule.producers.size() * config::producer_repetitions;
      while((res % blocks_per_round) != 0) {
         res++;
      }
      return res;
   };

   producer_authority calc_main_scheduled_producer( const vector<producer_authority> &producers, block_timestamp_type t ) {
      auto index = t.slot % (producers.size() * config::producer_repetitions);
      index /= config::producer_repetitions;
      return producers[index];
   }

   producer_authority calc_backup_scheduled_producer( const flat_map<name, block_signing_authority>& producers, block_timestamp_type t ) {
      optional<producer_authority> result;
      auto index = t.slot % (producers.size() * config::backup_producer_repetitions);
      index /= config::producer_repetitions;
      const auto& itr = producers.nth(index);
      return producer_authority{itr->first, itr->second};
   }

BOOST_AUTO_TEST_SUITE(producer_change_tests)


 BOOST_FIXTURE_TEST_CASE( propose_producer_change_api, producer_change_tester ) try {


      producer_change_tester c;
      auto producers = gen_producer_names(121, 1);

      c.create_accounts(producers);
      c.produce_blocks();

      uint32_t main_bp_count = 3;
      uint32_t backup_bp_count = 3;
      uint32_t total_bp_count = main_bp_count + backup_bp_count;
      BOOST_CHECK(total_bp_count <= producers.size());

      proposed_producer_changes changes;
      changes.main_changes.changes[N(amax)] = producer_authority_del{};

      vector<producer_authority> main_producers(main_bp_count);
      for( uint32_t i = 0; i < main_bp_count; i++) {
         auto producer_name = producers[i];
         auto authority = c.make_producer_authority(producer_name, 1);
         changes.main_changes.changes[producer_name] = producer_authority_add{authority};
         main_producers[i] = {producer_name, authority};
      }
      changes.main_changes.producer_count = main_bp_count;

      flat_map<name, block_signing_authority> backup_producers;
      for( uint32_t i = main_bp_count; i < total_bp_count; i++) {
         auto producer_name = producers[i];
         auto authority = c.make_producer_authority(producer_name, 1);
         changes.backup_changes.changes[producer_name] = producer_authority_add{authority};
         backup_producers[producer_name] = authority;
      }
      changes.backup_changes.producer_count = backup_bp_count;

      auto last_block_num = c.control->head_block_num();
      c.change(changes, 1);
      BOOST_REQUIRE_EQUAL( c.control->head_block_num(), last_block_num );
      auto gpo = c.control->get_global_properties();
      BOOST_REQUIRE( gpo.proposed_schedule_block_num.valid() );
      BOOST_REQUIRE_EQUAL( *gpo.proposed_schedule_block_num, c.control->head_block_num() + 1 );
      BOOST_REQUIRE_EQUAL( gpo.proposed_schedule_change.version, 1 );
      BOOST_REQUIRE( producer_change_map::from_shared(gpo.proposed_schedule_change.main_changes) == changes.main_changes );
      BOOST_REQUIRE( producer_change_map::from_shared(gpo.proposed_schedule_change.backup_changes) == changes.backup_changes );
      auto proposed_schedule_block_num = *gpo.proposed_schedule_block_num;

      c.produce_blocks(1);
      BOOST_REQUIRE(c.control->is_building_block());
      gpo = c.control->get_global_properties();
      BOOST_REQUIRE( !gpo.proposed_schedule_block_num.valid() ); // new building block state

      c.produce_blocks(1);
      auto hbs = c.control->head_block_state();
      auto exts = hbs->header.validate_and_extract_header_extensions();
      BOOST_REQUIRE_EQUAL( hbs->dpos_irreversible_blocknum, proposed_schedule_block_num );

      BOOST_REQUIRE_EQUAL( exts.count(producer_schedule_change_extension_v2::extension_id()) , 1 );
      const auto& new_producer_schedule_change = exts.lower_bound(producer_schedule_change_extension_v2::extension_id())->second.get<producer_schedule_change_extension_v2>();
      BOOST_REQUIRE_EQUAL( new_producer_schedule_change.version, 1 );
      BOOST_REQUIRE( new_producer_schedule_change.main_changes == changes.main_changes);
      BOOST_REQUIRE( new_producer_schedule_change.backup_changes == changes.backup_changes);

      // wdump( (hbs->pending_schedule.schedule_lib_num) );
      BOOST_REQUIRE_EQUAL( hbs->pending_schedule.schedule_lib_num, c.control->head_block_num() );
      BOOST_REQUIRE( hbs->pending_schedule.schedule.contains<producer_schedule_change>());
      const auto& change = hbs->pending_schedule.schedule.get<producer_schedule_change>();
      BOOST_REQUIRE_EQUAL( change.version, 1 );
      BOOST_REQUIRE( change.main_changes == changes.main_changes);
      BOOST_REQUIRE( change.backup_changes == changes.backup_changes);

      // wdump( (hbs->block_num) (hbs->active_backup_schedule.schedule) (hbs->active_backup_schedule.pre_schedule) );

      c.produce_blocks(1);
      hbs = c.control->head_block_state();
      exts = hbs->header.validate_and_extract_header_extensions();
      BOOST_REQUIRE_EQUAL( exts.count(producer_schedule_change_extension_v2::extension_id()) , 0 );

      BOOST_REQUIRE( hbs->pending_schedule.schedule.contains<uint32_t>());
      auto active_schedule = hbs->active_schedule;

      BOOST_REQUIRE_EQUAL( active_schedule.version, 1 );
      BOOST_REQUIRE( active_schedule.producers == main_producers);
      BOOST_REQUIRE_EQUAL( hbs->header.producer, N(amax) );

      BOOST_REQUIRE( hbs->active_backup_schedule.schedule && !hbs->active_backup_schedule.pre_schedule );
      BOOST_REQUIRE( hbs->active_backup_schedule.schedule == hbs->active_backup_schedule.get_schedule() );
      auto active_backup_schedule = *hbs->active_backup_schedule.schedule;

      BOOST_REQUIRE_EQUAL( active_backup_schedule.version, 1 );
      BOOST_REQUIRE( active_backup_schedule.producers == backup_producers);

      auto active_backup_schedule_block_num1 = hbs->block_num;

      c.produce_blocks(1);
      hbs = c.control->head_block_state();
      BOOST_REQUIRE( !hbs->active_backup_schedule.schedule && hbs->active_backup_schedule.pre_schedule );
      BOOST_REQUIRE_EQUAL( hbs->header.producer, calc_main_scheduled_producer(main_producers, hbs->header.timestamp).producer_name );
      const auto& new_backup_producer = hbs->get_backup_scheduled_producer(hbs->header.timestamp);
      BOOST_REQUIRE( new_backup_producer && *new_backup_producer == calc_backup_scheduled_producer(backup_producers, hbs->header.timestamp) );

      c.close();
      auto cfg = c.get_config();
      c.init( cfg );

      auto last_hbs = hbs;
      hbs = c.control->head_block_state();

      BOOST_REQUIRE( hbs->header.id() == last_hbs->header.id() );
      BOOST_REQUIRE(!hbs->active_backup_schedule.schedule && hbs->active_backup_schedule.pre_schedule);
      active_backup_schedule = *hbs->active_backup_schedule.pre_schedule;
      BOOST_REQUIRE_EQUAL( active_backup_schedule.version, 1 );
      BOOST_REQUIRE( active_backup_schedule.producers == backup_producers);

      c.produce_blocks( (main_bp_count + 1) * config::producer_repetitions - 2);

      hbs = c.control->head_block_state();
      // wdump( (hbs->block_num) (hbs->dpos_irreversible_blocknum) (hbs->dpos_proposed_irreversible_blocknum) );
      BOOST_REQUIRE_EQUAL( hbs->dpos_irreversible_blocknum, active_backup_schedule_block_num1 );
      c.produce_blocks(1);
      hbs = c.control->head_block_state();
      // wdump( (hbs->block_num) (hbs->dpos_irreversible_blocknum) (hbs->dpos_proposed_irreversible_blocknum) );
      BOOST_REQUIRE( hbs->dpos_irreversible_blocknum > active_backup_schedule_block_num1 );

      auto root_block_state = c.control->fork_db().root();
      BOOST_REQUIRE( root_block_state && root_block_state->active_backup_schedule.schedule && root_block_state->active_backup_schedule.pre_schedule );
      BOOST_REQUIRE( root_block_state->active_backup_schedule.schedule == root_block_state->active_backup_schedule.pre_schedule );

      c.close();
      c.init( cfg );
      hbs = c.control->head_block_state();
      BOOST_REQUIRE( !hbs->active_backup_schedule.schedule && hbs->active_backup_schedule.pre_schedule );
      root_block_state = c.control->fork_db().root();
      BOOST_REQUIRE( root_block_state && root_block_state->active_backup_schedule.schedule && !root_block_state->active_backup_schedule.pre_schedule );

      backup_block_tester bbc;
      bbc.block_signing_private_keys = c.block_signing_private_keys;
      bbc.sync_with(c);

      auto backup_block1 = bbc.produce_block();

      BOOST_REQUIRE_EQUAL( bbc.control->head_block_id(), c.control->head_block_id() );
      BOOST_REQUIRE_EQUAL( backup_block1->is_backup(), true );
      BOOST_REQUIRE_EQUAL( backup_block1->block_num(), c.control->head_block_num() + 1 );
      BOOST_REQUIRE_EQUAL( backup_block1->previous, c.control->head_block_id() );

   } FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
