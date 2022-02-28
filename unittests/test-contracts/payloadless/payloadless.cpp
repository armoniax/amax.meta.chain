#include "payloadless.hpp"

using namespace eosio;

void payloadless::doit() {
   print("Im a payloadless action");
}

// use the EOSIO_DISPATCH() to avoid error code "8000000000000000000" 
EOSIO_DISPATCH(payloadless, (doit) )
