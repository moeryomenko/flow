#pragma once

// Core execution framework - P2300 concepts split by proposal sections
// Each header corresponds to a specific section of the P2300 proposal:
//   - utils.hpp: General utilities and type traits
//   - queries.hpp: Execution resource queries (get_scheduler, etc.)
//   - env.hpp: Execution environments
//   - receiver.hpp: Receiver concepts and customization points
//   - operation_state.hpp: Operation state concepts
//   - completion_signatures.hpp: Completion signatures
//   - sender.hpp: Sender concepts and connection
//   - scheduler.hpp: Scheduler concepts and factories

#include "execution/adaptors.hpp"     // Sender adaptors
#include "execution/algorithms.hpp"   // Sender algorithms
#include "execution/async_scope.hpp"  // Async scope support (P3149)
#include "execution/factories.hpp"    // Sender factories (just, just_error, etc.)
#include "execution/schedulers.hpp"   // Standard scheduler implementations
#include "execution/stop_token.hpp"   // Stop token support
#include "execution/sync_wait.hpp"    // Synchronization utilities
#include "execution/type_list.hpp"    // Type list utilities
