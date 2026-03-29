/**
 * @file test_graph.cpp
 * @brief Comprehensive unit tests for Graph, NetworkBuilder, and Solver.
 *
 * Test categories:
 *   1-4:   Pure Edmonds-Karp algorithm on hand-built graphs
 *   5-8:   NetworkBuilder + Solver integration (from ProblemData)
 *   9-11:  Edge cases and corner cases
 *   12-13: Risk Analysis (T2.2)
 *   14:    Full integration test (Parser → Solver)
 */

#include "parser.h"
#include "graph.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdlib>

static std::string writeTempFile(const std::string& content, const std::string& name) {
    const char* tmpEnv = std::getenv("TEMP");
    if (!tmpEnv) tmpEnv = std::getenv("TMP");
    std::string dir = tmpEnv ? std::string(tmpEnv) + "/" : "./";
    std::string path = dir + name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

// ============================================================================
// TEST 1: Simple EK — classic textbook example
// ============================================================================
void test_ek_basic() {
    // Graph: s=0, a=1, b=2, t=3
    //   s→a: 3, s→b: 2, a→b: 1, a→t: 2, b→t: 3
    // Max flow = 4
    Graph g(4);
    g.addEdge(0, 1, 3); // s→a
    g.addEdge(0, 2, 2); // s→b
    g.addEdge(1, 2, 1); // a→b
    g.addEdge(1, 3, 2); // a→t
    g.addEdge(2, 3, 3); // b→t

    int flow = g.edmondsKarp(0, 3);
    // Min-cut {s} vs {a,b,t}: C(s,a)+C(s,b) = 3+2 = 5
    assert(flow == 5);

    std::cout << "  [PASS] test_ek_basic (flow=5)" << std::endl;
}

// ============================================================================
// TEST 2: EK — bipartite matching (unit capacities)
// ============================================================================
void test_ek_bipartite() {
    // Bipartite: L={1,2,3}, R={4,5,6}, s=0, t=7
    // Edges: s→L (cap 1), L→R (cap 1), R→t (cap 1)
    // L1→R4, L1→R5, L2→R5, L2→R6, L3→R6
    // Max matching = 3 (L1→R4 or R5, L2→other, L3→R6)
    Graph g(8);
    g.addEdge(0, 1, 1); g.addEdge(0, 2, 1); g.addEdge(0, 3, 1); // s→L
    g.addEdge(1, 4, 1); g.addEdge(1, 5, 1); // L1→R4, L1→R5
    g.addEdge(2, 5, 1); g.addEdge(2, 6, 1); // L2→R5, L2→R6
    g.addEdge(3, 6, 1);                      // L3→R6
    g.addEdge(4, 7, 1); g.addEdge(5, 7, 1); g.addEdge(6, 7, 1); // R→t

    int flow = g.edmondsKarp(0, 7);
    assert(flow == 3);

    std::cout << "  [PASS] test_ek_bipartite (matching=3)" << std::endl;
}

// ============================================================================
// TEST 3: EK — no path to sink
// ============================================================================
void test_ek_disconnected() {
    Graph g(4);
    g.addEdge(0, 1, 5);
    g.addEdge(2, 3, 5);
    // No path from 0 to 3

    int flow = g.edmondsKarp(0, 3);
    assert(flow == 0);

    std::cout << "  [PASS] test_ek_disconnected (flow=0)" << std::endl;
}

// ============================================================================
// TEST 4: EK — flow reset and re-solve
// ============================================================================
void test_ek_reset() {
    Graph g(4);
    g.addEdge(0, 1, 3);
    g.addEdge(1, 2, 2);
    g.addEdge(2, 3, 4);
    g.addEdge(0, 2, 1);

    int flow1 = g.edmondsKarp(0, 3);
    assert(flow1 == 3); // min(3+1, 2+1, 4) considering paths

    g.resetFlows();
    int flow2 = g.edmondsKarp(0, 3);
    assert(flow2 == flow1); // Same result after reset

    std::cout << "  [PASS] test_ek_reset (consistent after reset)" << std::endl;
}

// ============================================================================
// TEST 5: NetworkBuilder — Figure 1 from PDF (2 subs, 2 revs, MinRev=1)
// ============================================================================
void test_solver_figure1() {
    ProblemData data;
    data.addSubmission({31, "", "", "", 4, std::nullopt});
    data.addSubmission({87, "", "", "", 1, std::nullopt});
    data.addReviewer({1, "", "", 1, std::nullopt});
    data.addReviewer({2, "", "", 4, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 2);  // 2 submissions × 1 review each
    assert(result.assignments.size() == 2);
    assert(result.unfulfilled.empty());

    // Check specific assignments
    // Rev 1 (expertise=1) → Sub 87 (domain=1)
    // Rev 2 (expertise=4) → Sub 31 (domain=4)
    bool found_r1_s87 = false, found_r2_s31 = false;
    for (const auto& a : result.assignments) {
        if (a.reviewerId == 1 && a.submissionId == 87 && a.matchedDomain == 1)
            found_r1_s87 = true;
        if (a.reviewerId == 2 && a.submissionId == 31 && a.matchedDomain == 4)
            found_r2_s31 = true;
    }
    assert(found_r1_s87);
    assert(found_r2_s31);

    std::cout << "  [PASS] test_solver_figure1 (2 assignments)" << std::endl;
}

// ============================================================================
// TEST 6: Solver — MinRev > 1 (each submission needs 2 reviews)
// ============================================================================
void test_solver_minrev2() {
    // 2 submissions (domain 1), 3 reviewers (all expertise 1)
    // MinRev=2, MaxRev=2
    // Total demand: 2×2=4, Total capacity: 3×2=6 → feasible
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addSubmission({20, "", "", "", 1, std::nullopt});
    data.addReviewer({100, "", "", 1, std::nullopt});
    data.addReviewer({200, "", "", 1, std::nullopt});
    data.addReviewer({300, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 2;
    data.config.maxReviewsPerReviewer = 2;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 4);  // 2 subs × 2 reviews = 4
    assert(result.assignments.size() == 4);
    assert(result.unfulfilled.empty());

    // Each submission should have exactly 2 reviews
    int count_s10 = 0, count_s20 = 0;
    for (const auto& a : result.assignments) {
        if (a.submissionId == 10) count_s10++;
        if (a.submissionId == 20) count_s20++;
    }
    assert(count_s10 == 2);
    assert(count_s20 == 2);

    std::cout << "  [PASS] test_solver_minrev2 (4 assignments, 2 per sub)" << std::endl;
}

// ============================================================================
// TEST 7: Solver — partial assignment (not enough reviewers)
// ============================================================================
void test_solver_partial() {
    // 3 submissions: domains 1, 2, 3
    // 1 reviewer: expertise 1
    // MinRev=1, MaxRev=3
    // Only sub with domain 1 can be reviewed
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addSubmission({20, "", "", "", 2, std::nullopt});
    data.addSubmission({30, "", "", "", 3, std::nullopt});
    data.addReviewer({100, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 3;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 1);  // Only 1 assignment possible
    assert(result.assignments.size() == 1);
    assert(result.assignments[0].submissionId == 10);
    assert(result.assignments[0].reviewerId == 100);

    // 2 unfulfilled submissions
    assert(result.unfulfilled.size() == 2);

    int totalDemand = 3 * 1;
    assert(!result.isComplete(totalDemand));

    std::cout << "  [PASS] test_solver_partial (1 of 3 assigned)" << std::endl;
}

// ============================================================================
// TEST 8: Solver — multiple domains, larger test
// ============================================================================
void test_solver_multi_domain() {
    // 4 submissions: domains 1, 1, 2, 2
    // 4 reviewers: expertise 1, 1, 2, 2
    // MinRev=1, MaxRev=1
    // Perfect matching: each reviewer reviews exactly 1 submission
    ProblemData data;
    data.addSubmission({1, "", "", "", 1, std::nullopt});
    data.addSubmission({2, "", "", "", 1, std::nullopt});
    data.addSubmission({3, "", "", "", 2, std::nullopt});
    data.addSubmission({4, "", "", "", 2, std::nullopt});
    data.addReviewer({10, "", "", 1, std::nullopt});
    data.addReviewer({20, "", "", 1, std::nullopt});
    data.addReviewer({30, "", "", 2, std::nullopt});
    data.addReviewer({40, "", "", 2, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 4);
    assert(result.assignments.size() == 4);
    assert(result.unfulfilled.empty());

    std::cout << "  [PASS] test_solver_multi_domain (4/4 perfect)" << std::endl;
}

// ============================================================================
// TEST 9: Corner case — no submissions
// ============================================================================
void test_corner_no_submissions() {
    ProblemData data;
    data.addReviewer({1, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 0);
    assert(result.assignments.empty());
    assert(result.unfulfilled.empty());

    std::cout << "  [PASS] test_corner_no_submissions" << std::endl;
}

// ============================================================================
// TEST 10: Corner case — no reviewers
// ============================================================================
void test_corner_no_reviewers() {
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 0);
    assert(result.assignments.empty());
    assert(result.unfulfilled.size() == 1);
    assert(result.unfulfilled[0].submissionId == 10);
    assert(result.unfulfilled[0].missingReviews == 1);

    std::cout << "  [PASS] test_corner_no_reviewers" << std::endl;
}

// ============================================================================
// TEST 11: Corner case — MaxRev limits flow
// ============================================================================
void test_corner_maxrev_bottleneck() {
    // 3 submissions all domain 1, 1 reviewer expertise 1
    // MinRev=1, MaxRev=2
    // Reviewer can only do 2 of the 3
    ProblemData data;
    data.addSubmission({1, "", "", "", 1, std::nullopt});
    data.addSubmission({2, "", "", "", 1, std::nullopt});
    data.addSubmission({3, "", "", "", 1, std::nullopt});
    data.addReviewer({100, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 2;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 2);  // Bottlenecked by MaxRev
    assert(result.assignments.size() == 2);
    assert(result.unfulfilled.size() == 1);

    std::cout << "  [PASS] test_corner_maxrev_bottleneck (2 of 3)" << std::endl;
}

// ============================================================================
// TEST 12: Risk Analysis K=1 — removing one reviewer breaks assignment
// ============================================================================
void test_risk_analysis_k1() {
    // 2 submissions: domain 1 and domain 2
    // 2 reviewers: expertise 1 and expertise 2
    // MinRev=1, MaxRev=1
    // Removing EITHER reviewer makes assignment infeasible
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addSubmission({20, "", "", "", 2, std::nullopt});
    data.addReviewer({100, "", "", 1, std::nullopt});
    data.addReviewer({200, "", "", 2, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;
    data.config.riskAnalysis = 1;

    SolverResult result = Solver::solveWithRiskAnalysis(data);

    // Base assignment should be complete
    assert(result.maxFlow == 2);
    assert(result.assignments.size() == 2);

    // Both reviewers are risky (each covers a unique domain)
    assert(result.riskyReviewers.size() == 2);
    assert(result.riskyReviewers[0] == 100);
    assert(result.riskyReviewers[1] == 200);

    std::cout << "  [PASS] test_risk_analysis_k1 (both risky)" << std::endl;
}

// ============================================================================
// TEST 13: Risk Analysis K=1 — redundant reviewers (no risk)
// ============================================================================
void test_risk_analysis_redundant() {
    // 2 submissions: both domain 1
    // 4 reviewers: all expertise 1
    // MinRev=1, MaxRev=1
    // Removing any single reviewer still leaves enough
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addSubmission({20, "", "", "", 1, std::nullopt});
    data.addReviewer({100, "", "", 1, std::nullopt});
    data.addReviewer({200, "", "", 1, std::nullopt});
    data.addReviewer({300, "", "", 1, std::nullopt});
    data.addReviewer({400, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;
    data.config.riskAnalysis = 1;

    SolverResult result = Solver::solveWithRiskAnalysis(data);

    assert(result.maxFlow == 2);
    assert(result.riskyReviewers.empty()); // No single-point-of-failure

    std::cout << "  [PASS] test_risk_analysis_redundant (no risky)" << std::endl;
}

// ============================================================================
// TEST 14: Full integration — Parser → Solver pipeline
// ============================================================================
void test_full_integration() {
    std::string input =
        "#Submissions\n"
        "31, 4\n"
        "87, 1\n"
        "#Reviewers\n"
        "1, 1\n"
        "2, 4\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"assign.csv\"\n";

    auto path = writeTempFile(input, "test_integration.csv");
    Parser parser;
    ProblemData data = parser.parse(path);
    SolverResult result = Solver::solve(data);

    assert(result.maxFlow == 2);
    assert(result.assignments.size() == 2);
    assert(result.unfulfilled.empty());

    // Verify exact assignments match Figure 2 of the PDF
    assert(result.assignments[0].submissionId == 31);
    assert(result.assignments[0].reviewerId == 2);
    assert(result.assignments[0].matchedDomain == 4);

    assert(result.assignments[1].submissionId == 87);
    assert(result.assignments[1].reviewerId == 1);
    assert(result.assignments[1].matchedDomain == 1);

    std::cout << "  [PASS] test_full_integration (Parser→Solver, matches PDF)" << std::endl;
}

// ============================================================================
// TEST 15: Larger stress — 100 subs, 50 revs, MinRev=2
// ============================================================================
void test_stress() {
    ProblemData data;
    int numDomains = 5;

    // 100 submissions: domains cycle through 1..5
    for (int i = 1; i <= 100; i++) {
        data.addSubmission({i, "", "", "", (i % numDomains) + 1, std::nullopt});
    }
    // 50 reviewers: each domain has 10 reviewers, MaxRev=4
    for (int j = 1; j <= 50; j++) {
        data.addReviewer({1000 + j, "", "", (j % numDomains) + 1, std::nullopt});
    }
    data.config.minReviewsPerSubmission = 2;
    data.config.maxReviewsPerReviewer = 4;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    // Capacity: 50×4=200 >= 100×2=200 (tight!)
    assert(data.hasEnoughCapacity());

    SolverResult result = Solver::solve(data);

    // Each domain has 20 subs needing 2 reviews = 40 demand
    // Each domain has 10 revs × 4 cap = 40 supply → exact fit
    assert(result.maxFlow == 200);
    assert(result.assignments.size() == 200);
    assert(result.unfulfilled.empty());

    std::cout << "  [PASS] test_stress (100 subs × 50 revs, flow=200)" << std::endl;
}

// ============================================================================
// TEST 16: Secondary domains (MatchMode::ALL)
// ============================================================================
void test_secondary_match() {
    // Submission: primary=1, secondary=2
    // Reviewer:   primary=3, secondary=2
    // With PRIMARY_ONLY: no match (1 ≠ 3)
    // With ALL: match via secondary reviewer expertise (2) == secondary sub domain (2)
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, 2});    // secondary domain = 2
    data.addReviewer({100, "", "", 3, 2});          // secondary expertise = 2
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.secondaryReviewerExpertise = true;
    data.config.secondarySubmissionDomain = true;

    // Test PRIMARY_ONLY — should fail
    data.config.matchMode = MatchMode::PRIMARY_ONLY;
    SolverResult r1 = Solver::solve(data);
    assert(r1.maxFlow == 0);
    assert(r1.unfulfilled.size() == 1);

    // Test ALL — should succeed
    data.config.matchMode = MatchMode::ALL;
    SolverResult r2 = Solver::solve(data);
    assert(r2.maxFlow == 1);
    assert(r2.assignments.size() == 1);
    assert(r2.assignments[0].matchedDomain == 2);

    std::cout << "  [PASS] test_secondary_match (ALL vs PRIMARY_ONLY)" << std::endl;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "=== Graph / Solver Unit Tests ===" << std::endl;

    // Pure Edmonds-Karp
    test_ek_basic();
    test_ek_bipartite();
    test_ek_disconnected();
    test_ek_reset();

    // Solver integration
    test_solver_figure1();
    test_solver_minrev2();
    test_solver_partial();
    test_solver_multi_domain();

    // Corner cases
    test_corner_no_submissions();
    test_corner_no_reviewers();
    test_corner_maxrev_bottleneck();

    // Risk analysis
    test_risk_analysis_k1();
    test_risk_analysis_redundant();

    // Full pipeline
    test_full_integration();
    test_stress();
    test_secondary_match();

    std::cout << "=== All 16 graph/solver tests passed. ===" << std::endl;
    return 0;
}
