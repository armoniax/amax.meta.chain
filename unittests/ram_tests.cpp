#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <boost/test/unit_test.hpp>
#pragma GCC diagnostic pop

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/exception/exception.hpp>
#include <fc/variant_object.hpp>

#include <contracts.hpp>

#include "amax_system_tester.hpp"

/*
 * register test suite `ram_tests`
 */
BOOST_AUTO_TEST_SUITE(ram_tests)

constexpr uint64_t billable_size_key_value_object = config::billable_size_v<key_value_object>;
constexpr uint64_t billable_size_table_id_object = config::billable_size_v<table_id_object>;

/*************************************************************************************
 * ram_tests test case
 *************************************************************************************/
BOOST_FIXTURE_TEST_CASE(ram_tests, amax_system::amax_system_tester) { try {

   BOOST_REQUIRE_EQUAL(billable_size_key_value_object, 112);
   // ram usage for creating table
   BOOST_REQUIRE_EQUAL(billable_size_table_id_object, 112);


   auto rlm = control->get_resource_limits_manager();

   auto get_account_limits = [&]( const account_name &acct ) {
      int64_t ram_bytes = 0, net = 0, cpu = 0;
      rlm.get_account_limits( acct, ram_bytes, net, cpu );
      return mvo()
            ("ram_bytes", ram_bytes)
            ("net", net)
            ("cpu", cpu);
   };

   auto get_account_ram_amount = [&]( const account_name &acct ) {
      int64_t ram_bytes = 0, net = 0, cpu = 0;
      rlm.get_account_limits( acct, ram_bytes, net, cpu );
      return ram_bytes;
   };

   auto get_account_ram_available = [&]( const account_name &acct ) {
      int64_t ram_bytes = get_account_ram_amount(acct);
      int64_t ram_usage = rlm.get_account_ram_usage(acct);
      return ram_bytes - ram_usage;
   };

   // auto init_request_bytes = 80000 + 7110; // `7110' is for table token row
   auto init_request_bytes = 10000;
   const auto table_allocation_bytes = 12000;

   buyrambytes(config::system_account_name, config::system_account_name, 70000);
   produce_blocks(10);
   create_account_with_resources(N(testram11111),config::system_account_name, init_request_bytes);
   create_account_with_resources(N(testram22222),config::system_account_name, init_request_bytes);
   produce_blocks();

   BOOST_REQUIRE_EQUAL( success(), stake( name("amax.stake"), name("testram11111"), core_from_string("10.00000000"), core_from_string("5.00000000") ) );
   produce_blocks(10);

   wdump( ("stake")( rlm.get_account_ram_usage(N(testram11111)) )
          ( get_account_ram_amount(N(testram11111)) )
          ( get_account_ram_available(N(testram11111)) ));

   auto testram11111_ram_usage = rlm.get_account_ram_usage(N(testram11111));

   auto wasm_data_size = contracts::test_ram_limit_wasm().size();
   auto add_ram_usage =  wasm_data_size * config::setcode_ram_bytes_multiplier;
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram11111),
         add_ram_usage - get_account_ram_available(N(testram11111)) + 1 ));
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram22222),
         add_ram_usage - get_account_ram_available(N(testram22222)) + 1 ));
   set_code( N(testram11111), contracts::test_ram_limit_wasm() );
   set_code( N(testram22222), contracts::test_ram_limit_wasm() );

   wdump( ("set_code")( rlm.get_account_ram_usage(N(testram11111)) )
          ( get_account_ram_amount(N(testram11111)) )
          ( get_account_ram_available(N(testram11111)) )
          (wasm_data_size)
          (add_ram_usage) );
   BOOST_REQUIRE_EQUAL(rlm.get_account_ram_usage(N(testram11111)), testram11111_ram_usage + add_ram_usage);
   testram11111_ram_usage = rlm.get_account_ram_usage(N(testram11111));
   produce_blocks(10);

   add_ram_usage = fc::raw::pack(
         fc::json::from_string(contracts::test_ram_limit_abi().data()).template as<abi_def>()
   ).size();

   // amax.system contract setabi() save abi_hash data to "abihash"_n table
   // struct [[eosio::table]] abi_hash {
   //    name              owner;
   //    checksum256       hash;
   //    EOSLIB_SERIALIZE( abi_hash, (owner)(hash) )
   // };
   auto abi_hash_billable_size = billable_size_key_value_object + 40;
   add_ram_usage += abi_hash_billable_size;

   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram11111),
         add_ram_usage - get_account_ram_available(N(testram11111)) + 1 ));
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram22222),
         add_ram_usage - get_account_ram_available(N(testram22222)) + 1 ));
   set_abi( N(testram11111), contracts::test_ram_limit_abi().data() );
   set_abi( N(testram22222), contracts::test_ram_limit_abi().data() );
   wdump( ("set_abi")( rlm.get_account_ram_usage(N(testram11111)) )
          ( get_account_ram_amount(N(testram11111)) )
          ( get_account_ram_available(N(testram11111)) )
          (add_ram_usage) );

   BOOST_REQUIRE_EQUAL(rlm.get_account_ram_usage(N(testram11111)), testram11111_ram_usage + add_ram_usage);
   testram11111_ram_usage = rlm.get_account_ram_usage(N(testram11111));

   produce_blocks(10);

   // setentry save data to test table as struct test, and create new table
   // serial size of test(data 1780) = key(8) + data.size(2) + data(1780)
   // the vector serialization: size + data = pack(unsigned_int(size)) + pack(data)
   // TABLE test {
   //    uint64_t            key;
   //    std::vector<int8_t> data;
   // };
   add_ram_usage = (1780 + (8 + 2) + billable_size_key_value_object) * 10 + billable_size_table_id_object; //19132

   auto testram11111_available = get_account_ram_available(N(testram11111));
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram11111),
         add_ram_usage - get_account_ram_available(N(testram11111)) + 1 ));

   wdump( ("before setentry 1")( rlm.get_account_ram_usage(N(testram11111)) )
          ( get_account_ram_amount(N(testram11111)) )
          ( get_account_ram_available(N(testram11111)) )
          (add_ram_usage)
          (testram11111_available ) );

   TESTER* tester = this;
   // allocate just under the allocated bytes
   tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 10)
                        ("size", 1780));
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL(rlm.get_account_ram_usage(N(testram11111)), testram11111_ram_usage + add_ram_usage);
   testram11111_ram_usage = rlm.get_account_ram_usage(N(testram11111));

   auto ram_usage = rlm.get_account_ram_usage(N(testram11111));

   wdump( ("setentry 1")( rlm.get_account_ram_usage(N(testram11111)) )
          ( get_account_ram_amount(N(testram11111)) )
          ( get_account_ram_available(N(testram11111)) )
          (add_ram_usage) );

   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), 0);
   auto exceeded_ram_usage = 10 * 10; // 100
   wdump( (get_account_ram_available(N(testram11111))) (exceeded_ram_usage) );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 10)
                           ("size", 1790)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // update the entries with smaller allocations so that we can verify space is freed and new allocations can be made
   auto refund_ram = 100 * 10; // 1000
   tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 10)
                        ("size", 1680));
   produce_blocks(1);
   // == 1000
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), refund_ram);

   // verify the added entry is beyond the allocation bytes limit
   exceeded_ram_usage = 1680 + (8 + 2) + billable_size_key_value_object;
   BOOST_REQUIRE_LT( get_account_ram_available(N(testram11111)), exceeded_ram_usage );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 11)
                           ("size", 1680/*1810*/)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify the new entry's bytes minus the freed up bytes for existing entries still exceeds the allocation bytes limit
   BOOST_REQUIRE_GT( 1760 + (8 + 2) + billable_size_key_value_object, 1760 - 1680 );
   exceeded_ram_usage = ( 1760 + (8 + 2) + billable_size_key_value_object ) - (1760 - 1680);
   BOOST_REQUIRE_LT( get_account_ram_available(N(testram11111)), exceeded_ram_usage );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 11)
                           ("size", 1760)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify the new entry's bytes minus the freed up bytes for existing entries are under the allocation bytes limit
   BOOST_REQUIRE_GE( (1600 + (8 + 2) + billable_size_key_value_object ), (1680 - 1600) * 10 );
   add_ram_usage = ( 1600 + (8 + 2) + billable_size_key_value_object ) - (1680 - 1600) * 10;
   auto testram11111_ram_available = get_account_ram_available(N(testram11111));
   wdump( (testram11111_ram_available) (refund_ram) );
   tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 11)
                        ("size", 1600/*1720*/));
   produce_blocks(1);

   // == 78
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available - add_ram_usage );

   refund_ram = ( 1600 + (8 + 2) + billable_size_key_value_object );
   testram11111_ram_available = get_account_ram_available(N(testram11111));
   wdump( (testram11111_ram_available) (refund_ram) );
   tester->push_action( N(testram11111), N(rmentry), N(testram11111), mvo()
                        ("from", 3)
                        ("to", 3));
   produce_blocks(1);
   // == 1800
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available + refund_ram );

   // verify that the new entry will exceed the allocation bytes limit
   exceeded_ram_usage = ( 1780 + (8 + 2) + billable_size_key_value_object );
   BOOST_REQUIRE_LT( get_account_ram_available(N(testram11111)), exceeded_ram_usage );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 12)
                           ("to", 12)
                           ("size", 1780)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify that the new entry is under the allocation bytes limit
   add_ram_usage = 1620 + (8 + 2) + billable_size_key_value_object;
   testram11111_ram_available = get_account_ram_available(N(testram11111));
   wdump( (testram11111_ram_available) (add_ram_usage) );
   tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                        ("payer", "testram11111")
                        ("from", 12)
                        ("to", 12)
                        ("size", 1620/*1720*/));
   produce_blocks(1);
   // == 58
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available - add_ram_usage );

   // verify that another new entry will exceed the allocation bytes limit, to setup testing of new payer
   exceeded_ram_usage = ( 1660 + (8 + 2) + billable_size_key_value_object );
   BOOST_REQUIRE_LT( get_account_ram_available(N(testram11111)), exceeded_ram_usage );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 13)
                           ("to", 13)
                           ("size", 1660)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify that the new entry is under the allocation bytes limit
   refund_ram = 1620 + (8 + 2) + billable_size_key_value_object;
   add_ram_usage = 1720 + (8 + 2) + billable_size_key_value_object;
   BOOST_REQUIRE_LT(get_account_ram_available(N(testram22222)), add_ram_usage );
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram22222),
         add_ram_usage - get_account_ram_available(N(testram22222)) + 1 ));
   testram11111_ram_available = get_account_ram_available(N(testram11111));
   auto testram22222_ram_available = get_account_ram_available(N(testram22222));
   wdump( (testram11111_ram_available) (testram22222_ram_available) (refund_ram) (add_ram_usage) );
   tester->push_action( N(testram11111), N(setentry), {N(testram11111),N(testram22222)}, mvo()
                        ("payer", "testram22222")
                        ("from", 12)
                        ("to", 12)
                        ("size", 1720));
   produce_blocks(1);
   // == 1800
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available + refund_ram );
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram22222)), 0 );

   // verify that another new entry that is too big will exceed the allocation bytes limit, to setup testing of new payer
   exceeded_ram_usage = 1900 + (8 + 2) + billable_size_key_value_object;
   BOOST_REQUIRE_LT(get_account_ram_available(N(testram11111)), exceeded_ram_usage );
   wdump( (get_account_ram_available(N(testram11111))) (exceeded_ram_usage) );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                           ("payer", "testram11111")
                           ("from", 13)
                           ("to", 13)
                           ("size", 1900)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify that the new entry is under the allocation bytes limit, because entry 12 is now charged to testram22222
   add_ram_usage = 1678 + (8 + 2) + billable_size_key_value_object;
   testram11111_ram_available = get_account_ram_available(N(testram11111));
   wdump( (testram11111_ram_available) (add_ram_usage) );
   tester->push_action( N(testram11111), N(setentry), N(testram11111), mvo()
                        ("payer", "testram11111")
                        ("from", 13)
                        ("to", 13)
                        ("size", 1678) );
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available - add_ram_usage );

   // verify that new entries for testram22222 exceed the allocation bytes limit
   refund_ram = 1678 + (8 + 2) + billable_size_key_value_object;
   add_ram_usage = (1910 - 1720) + (1910 + (8 + 2) + billable_size_key_value_object) * 9;
   BOOST_REQUIRE_LT(get_account_ram_available(N(testram22222)), add_ram_usage );
   BOOST_REQUIRE_EQUAL( success(),
      buyrambytes(config::system_account_name, N(testram22222),
         add_ram_usage - get_account_ram_available(N(testram22222)) + 1 ));
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram22222)), add_ram_usage );

   exceeded_ram_usage = (1930 - 1720) + (1930 + (8 + 2) + billable_size_key_value_object) * 9;
   BOOST_REQUIRE_LT(get_account_ram_available(N(testram22222)), exceeded_ram_usage );
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), {N(testram11111),N(testram22222)}, mvo()
                           ("payer", "testram22222")
                           ("from", 12)
                           ("to", 21)
                           ("size", 1930)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram22222 has insufficient ram"));
   produce_blocks(1);

   // verify that new entries for testram22222 are under the allocation bytes limit
   testram11111_ram_available = get_account_ram_available(N(testram11111));
   testram22222_ram_available = get_account_ram_available(N(testram22222));
   wdump( (testram11111_ram_available) (testram22222_ram_available) (refund_ram) (add_ram_usage) );
   tester->push_action( N(testram11111), N(setentry), {N(testram11111),N(testram22222)}, mvo()
                        ("payer", "testram22222")
                        ("from", 12)
                        ("to", 21)
                        ("size", 1910));
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram11111)), testram11111_ram_available + refund_ram );
   BOOST_REQUIRE_EQUAL(get_account_ram_available(N(testram22222)), 0 );

   // verify that new entry for testram22222 exceed the allocation bytes limit
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( N(testram11111), N(setentry), {N(testram11111),N(testram22222)}, mvo()
                           ("payer", "testram22222")
                           ("from", 22)
                           ("to", 22)
                           ("size", 1910)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram22222 has insufficient ram"));
   produce_blocks(1);

   tester->push_action( N(testram11111), N(rmentry), N(testram11111), mvo()
                        ("from", 20)
                        ("to", 20));
   produce_blocks(1);

   // verify that new entry for testram22222 are under the allocation bytes limit
   tester->push_action( N(testram11111), N(setentry), {N(testram11111),N(testram22222)}, mvo()
                        ("payer", "testram22222")
                        ("from", 22)
                        ("to", 22)
                        ("size", 1910));
   produce_blocks(1);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
