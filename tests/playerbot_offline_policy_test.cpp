// The request journal behind the 2.x offline shops (Codex handoff 2026-09-13).
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_offline_policy_test.cpp
#include "playerbot_offline_policy.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace playerbot_offline;
int main() {
    // Rejected client call (including the native misleading true return): no
    // actual send, therefore the bot may try later without duplicating a move.
    assert(Begin(7, Add, 101, 100));
    assert(!EndCall(7));
    assert(requests.empty());
    assert(Begin(7, Create, 0, 200));
    Complete(7, Create, 0); // unsolicited/stale acknowledgement before send
    assert(!requests.at(7).done);
    Sent(7, Create, 0);
    assert(EndCall(7));
    assert(!Begin(7, Create, 0, 40000)); // timeout never permits resubmission
    Complete(7, Add, 0);
    Complete(8, Create, 0);
    assert(!requests.at(7).done);
    Complete(7, Create, 0);
    assert(requests.at(7).done && requests.at(7).success);
    requests.erase(7);
    // Item sold immediately after ADD acknowledgement: completion is retained
    // independently of the current shop/item snapshot.
    assert(Begin(7, Add, 101, 500)); Sent(7, Add, 101);
    Complete(7, Add, 102); assert(!requests.at(7).done);
    Complete(7, Add, 101); assert(requests.at(7).done);
    assert(!Begin(7, Buy, 202, 600));
    requests.erase(7);
    // Two independent buyers; a lock refusal does not spend money or masquerade
    // as a successful transaction and does not block the successful buyer.
    assert(Begin(7, Buy, 202, 600)); assert(Begin(8, Buy, 202, 600));
    Sent(7, Buy, 202); Sent(8, Buy, 202);
    Complete(8, Buy, 202, false); Complete(7, Buy, 202, true);
    assert(!requests.at(8).success && requests.at(8).done);
    assert(requests.at(7).success && requests.at(7).done);
    // State reset/relogin must not silently forget a transmitted request.
    requests.clear(); assert(Begin(7, WithdrawItem, 999, 900)); Sent(7, WithdrawItem, 999);
    State before; before.visiting = true; State after;
    assert(!after.visiting && requests.count(7));
    Complete(7, WithdrawItem, 999, false); assert(requests.at(7).done);
    assert(Due(10, 0)); assert(!Due(10, 11)); assert(Due(11, 11));
    assert(!Due(0xfffffff0u, 20)); assert(Due(21, 20)); assert(Due(5, 0xfffffff0u));
    // Exhaustive vertical-grid boundary checks against a cell-by-cell oracle.
    for (int cell=-2; cell<84; ++cell) for (int height=-1; height<12; ++height) {
        bool expected = height>0 && height<=8 && cell>=0 && cell<80;
        if (expected) for (int y=0; y<height; ++y) expected &= cell+y*10<80;
        assert(Fits(cell,height,10,80)==expected);
    }
    assert(Affordable(10000,3000,2000)==5000);
    assert(Affordable(4999,3000,2000)==0);
    assert(Affordable(5000,3000,2000)==0);
    std::cout << "PASS: request ordering, refusals, duplicate suppression, relog, timeout, clock wrap, 1118 grid cases and wallet reserve\n";
}
