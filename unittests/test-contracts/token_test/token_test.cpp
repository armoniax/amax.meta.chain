#include "token_test.hpp"
#include <eosio/transaction.hpp>
#include "amax.token.hpp"

using namespace eosio;

static constexpr eosio::name active_permission{"active"_n};
static constexpr eosio::name token_account{"amax.token"_n};

void token_test::testtransfer(const name &from,
                              const name &to,
                              const asset &quantity,
                              const string &memo)
{
   token::transfer_action transfer_act{ token_account, { {from, active_permission} } };
   transfer_act.send( from, to, quantity, memo );
}
