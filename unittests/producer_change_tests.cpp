#include <eosio/chain/global_property_object.hpp>
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


   static auto get_producer_public_key( name producer_name, uint64_t version = 1 ) {
      return get_public_key(producer_name, std::to_string(version));
   }

   static block_signing_authority make_producer_authority(name producer_name, uint64_t version = 1) {
      return block_signing_authority_v0{
         1, {
            {get_producer_public_key(producer_name, version), 1}
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


BOOST_AUTO_TEST_SUITE(producer_change_tests)


 BOOST_FIXTURE_TEST_CASE( propose_producer_change_api, producer_change_tester ) try {

      auto producers = gen_producer_names(121, 1);

      create_accounts(producers);
      produce_blocks();

      uint32_t main_bp_count = 1;
      uint32_t backup_bp_count = 0;
      uint32_t total_bp_count = main_bp_count + backup_bp_count;
      BOOST_CHECK(total_bp_count <= producers.size());

      proposed_producer_changes changes;
      changes.main_changes.changes[N(amax)] = producer_authority_del{};

      for( uint32_t i = 0; i < main_bp_count; i++) {
         auto producer_name = producers[i];
         changes.main_changes.changes[producer_name] = producer_authority_add{make_producer_authority(producer_name, 1)};
      }
      changes.main_changes.producer_count = main_bp_count;
      for( uint32_t i = main_bp_count; i < total_bp_count; i++) {
         auto producer_name = producers[i];
         changes.backup_changes.changes[producer_name] = producer_authority_add{make_producer_authority(producer_name, 1)};
      }
      changes.backup_changes.producer_count = backup_bp_count;

      auto last_block_num = control->head_block_num();
      change(changes, 1);
      BOOST_REQUIRE_EQUAL( control->head_block_num(), last_block_num );
      const auto& gpo = control->get_global_properties();
      BOOST_REQUIRE( gpo.proposed_schedule_block_num.valid() );
      BOOST_REQUIRE_EQUAL( *gpo.proposed_schedule_block_num, control->head_block_num() + 1 );
      BOOST_REQUIRE_EQUAL( gpo.proposed_schedule_change.version, 1 );
      BOOST_REQUIRE( producer_change_map::from_shared(gpo.proposed_schedule_change.main_changes) == changes.main_changes );
      BOOST_REQUIRE( producer_change_map::from_shared(gpo.proposed_schedule_change.backup_changes) == changes.backup_changes );
      auto proposed_schedule_block_num = *gpo.proposed_schedule_block_num;

      produce_blocks(2);
      BOOST_REQUIRE_EQUAL( control->head_block_state()->dpos_irreversible_blocknum, proposed_schedule_block_num );
      auto exts = control->head_block_header().validate_and_extract_header_extensions();
      BOOST_REQUIRE_EQUAL( exts.count(producer_schedule_change_extension_v2::extension_id()) , 1 );
      const auto& new_producer_schedule_change = exts.lower_bound(producer_schedule_change_extension_v2::extension_id())->second.get<producer_schedule_change_extension_v2>();
      BOOST_REQUIRE_EQUAL( new_producer_schedule_change.version, 1 );
      BOOST_REQUIRE( new_producer_schedule_change.main_changes == changes.main_changes);
      BOOST_REQUIRE( new_producer_schedule_change.backup_changes == changes.backup_changes);

      // vector<account_name> valid_producers = {
      //    "inita", "initb", "initc", "initd", "inite", "initf", "initg",
      //    "inith", "initi", "initj", "initk", "initl", "initm", "initn",
      //    "inito", "initp", "initq", "initr", "inits", "initt", "initu"
      // };
      // create_accounts(valid_producers);
      // set_producers(valid_producers);

      // // account initz does not exist
      // vector<account_name> nonexisting_producer = { "initz" };
      // BOOST_CHECK_THROW(set_producers(nonexisting_producer), wasm_execution_error);

      // // replace initg with inita, inita is now duplicate
      // vector<account_name> invalid_producers = {
      //    "inita", "initb", "initc", "initd", "inite", "initf", "inita",
      //    "inith", "initi", "initj", "initk", "initl", "initm", "initn",
      //    "inito", "initp", "initq", "initr", "inits", "initt", "initu"
      // };

      // BOOST_CHECK_THROW(set_producers(invalid_producers), wasm_execution_error);

   } FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
