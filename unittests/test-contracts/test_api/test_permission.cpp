#include <limits>

#include <eosio/action.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/print.hpp>
#include <eosio/serialize.hpp>

#include "test_api.hpp"

struct check_auth_msg {
   eosio::name                    account;
   eosio::name                    permission;
   std::vector<eosio::public_key> pubkeys;

   EOSLIB_SERIALIZE( check_auth_msg, (account)(permission)(pubkeys)  )
};

void test_permission::check_authorization( uint64_t receiver, uint64_t code, uint64_t action ) {
   (void)code;
   (void)action;
   using namespace eosio;

   auto self = receiver;
   auto params = unpack_action_data<check_auth_msg>();
   auto packed_pubkeys = pack(params.pubkeys);
   int64_t res64 = ::check_permission_authorization( params.account,
                                                     params.permission,
                                                     packed_pubkeys.data(), packed_pubkeys.size(),
                                                     (const char*)0,        0,
                                                     microseconds( std::numeric_limits<int64_t>::max() )
                                                   );

   auto itr = db_lowerbound_i64( self, self, self, 1 );
   if(itr == -1) {
      db_store_i64( self, self, self, 1, &res64, sizeof(int64_t) );
   } else {
      db_update_i64( itr, self, &res64, sizeof(int64_t) );
   }
}

struct test_permission_last_used_msg {
   eosio::name account;
   eosio::name permission;
   int64_t     last_used_time;

   EOSLIB_SERIALIZE( test_permission_last_used_msg, (account)(permission)(last_used_time) )
};

struct test_creator {
   eosio::name account;
   eosio::name creator;

   EOSLIB_SERIALIZE( test_creator, (account)(creator) )
};

void test_permission::test_permission_last_used( uint64_t /* receiver */, uint64_t code, uint64_t action ) {
   (void)code;
   (void)action;
   using namespace eosio;

   auto params = unpack_action_data<test_permission_last_used_msg>();

   check( get_permission_last_used(params.account, params.permission).sec_since_epoch() == params.last_used_time / 1000000, 
      "unexpected last used permission time" );
}

void test_permission::test_account_creation_time( uint64_t /* receiver */, uint64_t code, uint64_t action ) {
   (void)code;
   (void)action;
   using namespace eosio;

   auto params = unpack_action_data<test_permission_last_used_msg>();

   check( get_account_creation_time(params.account).sec_since_epoch() == params.last_used_time / 1000000, 
      "unexpected account creation time" );
}

void test_permission::test_account_creator( uint64_t /* receiver */, uint64_t code, uint64_t action ) {
   (void)code;
   (void)action;
   using namespace eosio;

   auto params = unpack_action_data<test_creator>();
   auto got_creator = get_account_creator(params.account);
   check( got_creator == params.creator, 
      "unexpected account creator expected:" + params.creator.to_string() 
      + " but got:" +  got_creator.to_string());
}