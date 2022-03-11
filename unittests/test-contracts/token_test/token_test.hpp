#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using namespace std;

class [[eosio::contract]] token_test : public eosio::contract {
public:
   using eosio::contract::contract;

   [[eosio::action]] void testtransfer(const name &from,
                                       const name &to,
                                       const asset &quantity,
                                       const string &memo);
};
