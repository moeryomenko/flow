#pragma once

// Core execution concepts organized by P2300 proposal sections
#include "completion_signatures.hpp"
#include "env.hpp"
#include "operation_state.hpp"
#include "queries.hpp"
#include "receiver.hpp"
#include "scheduler.hpp"
#include "sender.hpp"
#include "utils.hpp"

// Note: The execution.hpp file now acts as a unified header that includes
// all the core P2300 concepts in their proper dependency order:
// 1. utils - fundamental type utilities
// 2. queries - execution resource queries
// 3. env - execution environments
// 4. receiver - receiver concepts and customization points
// 5. operation_state - operation state concepts
// 6. completion_signatures - completion signature utilities
// 7. sender - sender concepts and connection
// 8. scheduler - scheduler concepts and factories

namespace flow::execution {
// All types and concepts are now defined in their respective headers
}  // namespace flow::execution
