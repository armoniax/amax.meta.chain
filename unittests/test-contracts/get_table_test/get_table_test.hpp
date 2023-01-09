/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/asset.hpp>

using namespace eosio;

static const  eosio::symbol    CORE_SYMBOL     = symbol("AMAX", 8);
#define CORE_ASSET(amount) eosio::asset(amount, CORE_SYMBOL)

class [[eosio::contract]] get_table_test : public eosio::contract {
    public:
    using eosio::contract::contract;

    // Number object
    struct [[eosio::table]] numobj {
        uint64_t        key;
        uint64_t        sec64;
        uint128_t       sec128;
        double          secdouble;
        long double     secldouble;

        uint64_t primary_key() const { return key; }
        uint64_t sec64_key() const { return sec64; }
        uint128_t sec128_key() const { return sec128; }
        double secdouble_key() const { return secdouble; }
        long double secldouble_key() const { return secldouble; }
    };

    // Hash object
    struct [[eosio::table]] hashobj {
        uint64_t        key;
        std::string     hash_input;
        checksum256     sec256;
        checksum160     sec160;

        uint64_t primary_key() const { return key; }
        checksum256 sec256_key() const { return sec256; }
        checksum256 sec160_key() const { return checksum256(sec160.get_array()); }
    };

    typedef eosio::multi_index< "numobjs"_n, numobj,
                                indexed_by<"bysec1"_n, const_mem_fun<numobj, uint64_t, &numobj::sec64_key>>,
                                indexed_by<"bysec2"_n, const_mem_fun<numobj, uint128_t, &numobj::sec128_key>>,
                                indexed_by<"bysec3"_n, const_mem_fun<numobj, double, &numobj::secdouble_key>>,
                                indexed_by<"bysec4"_n, const_mem_fun<numobj, long double, &numobj::secldouble_key>>
                                > numobjs;

    typedef eosio::multi_index< "hashobjs"_n, hashobj,
                            indexed_by<"bysec1"_n, const_mem_fun<hashobj, checksum256, &hashobj::sec256_key>>,
                            indexed_by<"bysec2"_n, const_mem_fun<hashobj, checksum256, &hashobj::sec160_key>>
                            > hashobjs;


    struct [[eosio::table]] producer {
        name            owner;
        eosio::asset    total_votes                 = CORE_ASSET(0);
        eosio::asset    unallocated_rewards         = CORE_ASSET(0);
        eosio::asset    allocating_rewards          = CORE_ASSET(0);
        double          rewards_per_vote           = 0;

        uint64_t primary_key() const { return owner.value; }
        typedef eosio::multi_index< "producers"_n, producer> table;
    };

    struct [[eosio::table]] voter {
        name            owner;
        name            producer_name;
        eosio::asset    votes                       = CORE_ASSET(0);
        eosio::asset    rewards                     = CORE_ASSET(0);
        double          rewards_per_vote_paid      = 0;

        uint64_t primary_key() const { return owner.value; }
        typedef eosio::multi_index< "voters"_n, voter> table;
    };

   [[eosio::action]]
   void addnumobj(uint64_t input);


   [[eosio::action]]
   void addhashobj(std::string hashinput);

   [[eosio::action]]
   void addreward(const name& owner, const eosio::asset& rewards);

   [[eosio::action]]
   void vote(const name& owner, const name& producer_name, const eosio::asset& votes);

};