/**
 * @file Allocator.h
 * @brief Register allocation algorithms operating on the interference graph.
 *
 * Supports four allocation strategies:
 *  - BASIC:    Greedy graph coloring; fails hard on spill.
 *  - SPILLING: Tolerates up to K spills to memory.
 *  - SPLITTING:Splits up to K webs before attempting coloring.
 *  - FREE:     Optimized DSATUR-based coloring with integrated spilling.
 */

#pragma once
#include "Graph.h"
#include "Parser.h"
#include <vector>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Result types
// ---------------------------------------------------------------------------

/** @brief Sentinel value meaning "spill to memory". */
static constexpr int SPILL_COLOR = -1;

/**
 * @brief Result of a register allocation attempt.
 *
 * colors[i] holds the register index (0..N-1) for web i,
 * or SPILL_COLOR (-1) if web i was spilled to memory.
 */
struct AllocationResult {
    bool             success;       ///< true if all webs got a register
    int              registersUsed; ///< highest register index used + 1
    std::vector<int> colors;        ///< size = number of webs
};

// ---------------------------------------------------------------------------
// Allocator class
// ---------------------------------------------------------------------------

/**
 * @brief Orchestrates register allocation.
 *
 * Typical usage:
 * @code
 *   Allocator alloc;
 *   AllocationResult r = alloc.allocate(webs, graph, config);
 * @endcode
 */
class Allocator {
public:
    /**
     * @brief Top-level entry point: dispatches to the right algorithm.
     *
     * @param webs   Webs to allocate (may be mutated for SPLITTING).
     * @param graph  Interference graph (may be mutated for SPLITTING).
     * @param config Algorithm configuration.
     * @return Allocation result.
     * @complexity Depends on algorithm — see individual methods.
     */
    AllocationResult allocate(std::vector<Web>& webs, Graph& graph,
                              const AllocConfig& config);

private:
    // ------------------------------------------------------------------
    // Core simplification / coloring
    // ------------------------------------------------------------------

    /**
     * @brief Chaitin-Briggs simplification + coloring.
     *
     * Phase 1 (simplification): while any node in `active` has degree < N,
     * push it onto stack S and remove it from `active`.
     * If no such node exists (all degrees ≥ N):
     *   - If spillsLeft > 0: pick the best spill candidate, add it to
     *     `spilled`, decrement spillsLeft.
     *   - Otherwise: return failure (spilled = all remaining nodes).
     *
     * Phase 2 (coloring): pop from S; assign the lowest color not used by
     * any neighbor in the ORIGINAL graph.
     *
     * @param graph       Original interference graph (read-only for coloring).
     * @param N           Number of available registers.
     * @param webs        Web list (used for spill heuristic).
     * @param spillsLeft  Max additional spills allowed (0 for BASIC).
     * @return AllocationResult with colors vector filled.
     * @complexity O(W^2) where W = number of webs.
     */
    AllocationResult simplifyAndColor(const Graph& graph, int N,
                                      const std::vector<Web>& webs,
                                      int spillsLeft);

    // ------------------------------------------------------------------
    // Spill heuristic
    // ------------------------------------------------------------------

    /**
     * @brief Choose the best web to spill from the active set.
     *
     * Heuristic: maximize (degree × range_length). This prefers webs that
     * are heavily connected AND long-lived — spilling them relieves the
     * most pressure.
     *
     * @complexity O(W) where W = |active|.
     */
    int pickSpill(const std::set<int>& active, const Graph& graph,
                  const std::vector<Web>& webs) const;

    // ------------------------------------------------------------------
    // Splitting
    // ------------------------------------------------------------------

    /**
     * @brief Split a web at its median live line, producing two new webs.
     *
     * The original web is removed; two new webs (possibly with fewer
     * interferences) are appended to `webs` and `graph` is rebuilt.
     *
     * Heuristic: choose the web with the highest degree × range_length.
     *
     * @param webs  Web list (mutated: one web replaced by two).
     * @param graph Interference graph (rebuilt after split).
     * @complexity O(W^2 * L).
     */
    void splitWebInPlace(std::vector<Web>& webs, Graph& graph,
                         const std::vector<Web>::iterator& target);

    /**
     * @brief Choose the web most worth splitting (highest degree × length).
     * @complexity O(W).
     */
    int pickSplitTarget(const std::vector<Web>& webs, const Graph& graph) const;

    // ------------------------------------------------------------------
    // Algorithm variants
    // ------------------------------------------------------------------

    /** @brief BASIC: fail on first required spill. @complexity O(W^2). */
    AllocationResult runBasic(std::vector<Web>& webs, const Graph& graph,
                              int N);

    /** @brief SPILLING: allow up to K spills. @complexity O(K * W^2). */
    AllocationResult runSpilling(std::vector<Web>& webs, const Graph& graph,
                                 int N, int K);

    /**
     * @brief SPLITTING: split up to K webs before coloring.
     * @complexity O(K * W^2 * L).
     */
    AllocationResult runSplitting(std::vector<Web>& webs, Graph& graph,
                                  int N, int K);

    /**
     * @brief FREE: DSATUR algorithm with integrated spilling fallback.
     *
     * DSATUR selects nodes in order of decreasing saturation (number of
     * distinct neighbor colors) breaking ties by degree. This typically
     * uses fewer colors than simple degree ordering.
     *
     * @complexity O(W^2).
     */
    AllocationResult runFree(std::vector<Web>& webs, const Graph& graph,
                             int N);
};

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

/**
 * @brief Write allocation result to a file.
 *
 * Format:
 * @code
 * webs: W
 * web0: <ranges>
 * ...
 * registers: N
 * r0: webX
 * ...
 * M: webY
 * @endcode
 *
 * If success == false, registers: 0 and all webs go to M.
 *
 * @complexity O(W * L).
 */
void writeOutput(const std::string& filename,
                 const std::vector<Web>& webs,
                 const AllocationResult& result,
                 int numRegisters);
