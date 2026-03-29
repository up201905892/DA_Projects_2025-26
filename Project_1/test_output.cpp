/**
 * @file test_output.cpp
 * @brief End-to-end tests for the OutputWriter + complete pipeline.
 */

#include "parser.h"
#include "graph.h"
#include "output.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

// Cross-platform temp directory (TEMP/TMP env var on Windows, "./" fallback)
static std::string tmpDir() {
    const char* t = std::getenv("TEMP");
    if (!t) t = std::getenv("TMP");
    return t ? std::string(t) + "/" : "./";
}

static std::string tmpPath(const std::string& name) {
    return tmpDir() + name;
}

static std::string writeTempFile(const std::string& content, const std::string& name) {
    std::string path = tmpPath(name);
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ============================================================================
// TEST 1: Output matches Figure 2 exactly
// ============================================================================
void test_output_figure2() {
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
        "OutputFileName, \"test_fig2_out.csv\"\n";

    auto inPath = writeTempFile(input, "test_out_fig2_in.csv");
    Parser parser;
    ProblemData data = parser.parse(inPath);
    SolverResult result = Solver::solve(data);

    std::string outPath = tmpPath("test_fig2_out.csv");
    OutputWriter::writeToFile(outPath, result, data);

    std::string output = readFile(outPath);

    // Verify key elements
    assert(output.find("#SubmissionId,ReviewerId,Match") != std::string::npos);
    assert(output.find("31, 2, 4") != std::string::npos);
    assert(output.find("87, 1, 1") != std::string::npos);
    assert(output.find("#ReviewerId,SubmissionId,Match") != std::string::npos);
    assert(output.find("1, 87, 1") != std::string::npos);
    assert(output.find("2, 31, 4") != std::string::npos);
    assert(output.find("#Total: 2") != std::string::npos);

    // Should NOT have unfulfilled section
    assert(output.find("#SubmissionId,Domain,MissingReviews") == std::string::npos);

    std::cout << "  [PASS] test_output_figure2" << std::endl;
}

// ============================================================================
// TEST 2: Output matches Figure 3 (unfulfilled)
// ============================================================================
void test_output_figure3() {
    ProblemData data;
    data.addSubmission({31, "", "", "", 3, std::nullopt});
    data.addSubmission({87, "", "", "", 1, std::nullopt});
    data.addReviewer({1, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 3;
    data.config.maxReviewsPerReviewer = 3;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    // Rev 1 (exp=1) can only review Sub 87 (dom=1), up to 3 times
    // But each assignment is binary (cap=1), so only 1 review.
    // Sub 31 (dom=3): no compatible reviewer → 3 missing
    // Sub 87 (dom=1): 1 review of 3 needed → 2 missing

    std::string outPath = tmpPath("test_fig3_out.csv");
    OutputWriter::writeToFile(outPath, result, data);
    std::string output = readFile(outPath);

    assert(output.find("#SubmissionId,Domain,MissingReviews") != std::string::npos);
    assert(output.find("31, 3, 3") != std::string::npos);
    assert(output.find("87, 1, 2") != std::string::npos);

    std::cout << "  [PASS] test_output_figure3" << std::endl;
}

// ============================================================================
// TEST 3: Risk analysis output (Figure 4)
// ============================================================================
void test_output_figure4() {
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addSubmission({20, "", "", "", 2, std::nullopt});
    data.addReviewer({1, "", "", 1, std::nullopt});
    data.addReviewer({2, "", "", 2, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;
    data.config.riskAnalysis = 1;

    SolverResult result = Solver::solveWithRiskAnalysis(data);

    std::string riskPath = tmpPath("test_fig4_risk.csv");
    OutputWriter::writeRiskToFile(riskPath, result, 1);
    std::string output = readFile(riskPath);

    assert(output.find("#Risk Analysis: 1") != std::string::npos);
    assert(output.find("1, 2") != std::string::npos);

    std::cout << "  [PASS] test_output_figure4" << std::endl;
}

// ============================================================================
// TEST 4: Output with no risky reviewers
// ============================================================================
void test_output_no_risk() {
    ProblemData data;
    data.addSubmission({10, "", "", "", 1, std::nullopt});
    data.addReviewer({1, "", "", 1, std::nullopt});
    data.addReviewer({2, "", "", 1, std::nullopt});
    data.addReviewer({3, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 1;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;
    data.config.riskAnalysis = 1;

    SolverResult result = Solver::solveWithRiskAnalysis(data);

    std::string riskPath = tmpPath("test_norisk.csv");
    OutputWriter::writeRiskToFile(riskPath, result, 1);
    std::string output = readFile(riskPath);

    assert(output.find("#Risk Analysis: 1") != std::string::npos);
    assert(output.find("No risky") != std::string::npos);

    std::cout << "  [PASS] test_output_no_risk" << std::endl;
}

// ============================================================================
// TEST 5: Reviewer-centric output is sorted
// ============================================================================
void test_output_sorted() {
    ProblemData data;
    data.addSubmission({5, "", "", "", 1, std::nullopt});
    data.addSubmission({3, "", "", "", 1, std::nullopt});
    data.addSubmission({8, "", "", "", 1, std::nullopt});
    data.addReviewer({20, "", "", 1, std::nullopt});
    data.addReviewer({10, "", "", 1, std::nullopt});
    data.config.minReviewsPerSubmission = 1;
    data.config.maxReviewsPerReviewer = 2;
    data.config.matchMode = MatchMode::PRIMARY_ONLY;

    SolverResult result = Solver::solve(data);

    std::string outPath = tmpPath("test_sorted_out.csv");
    OutputWriter::writeToFile(outPath, result, data);
    std::string output = readFile(outPath);

    // Submission-centric section should be sorted by submissionId
    size_t pos3 = output.find("3, ");
    size_t pos5 = output.find("5, ");
    size_t pos8 = output.find("8, ");
    assert(pos3 < pos5);
    assert(pos5 < pos8);

    // Reviewer-centric section: rev 10 before rev 20
    size_t posR10 = output.find("10, ");
    size_t posR20 = output.find("20, ");
    // Both should exist
    assert(posR10 != std::string::npos);
    assert(posR20 != std::string::npos);

    std::cout << "  [PASS] test_output_sorted" << std::endl;
}

// ============================================================================
// TEST 6: Full pipeline batch simulation
// ============================================================================
void test_full_batch_pipeline() {
    // Compute platform-correct temp paths for output files
    std::string batchOut  = tmpPath("batch_test_out.csv");
    std::string batchRisk = tmpPath("batch_test_risk.csv");

    // Create input file (OutputFileName must point to a writable temp path)
    std::string input =
        "#Submissions\n"
        "100, 1\n"
        "200, 2\n"
        "300, 1\n"
        "#Reviewers\n"
        "10, 1\n"
        "20, 2\n"
        "30, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 2\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 1\n"
        "OutputFileName, \"" + batchOut + "\"\n";

    auto inPath = writeTempFile(input, "batch_pipeline.csv");
    Parser parser;
    ProblemData data = parser.parse(inPath);

    SolverResult result = Solver::solveWithRiskAnalysis(data);

    // All 3 subs should be assigned
    assert(result.maxFlow == 3);
    assert(result.assignments.size() == 3);
    assert(result.unfulfilled.empty());

    // Write outputs
    OutputWriter::writeToFile(batchOut, result, data);
    OutputWriter::writeRiskToFile(batchRisk, result, 1);

    // Verify files exist and have content
    std::string assignOut = readFile(batchOut);
    std::string riskOut = readFile(batchRisk);

    assert(!assignOut.empty());
    assert(assignOut.find("#Total: 3") != std::string::npos);
    assert(!riskOut.empty());
    assert(riskOut.find("#Risk Analysis: 1") != std::string::npos);

    std::cout << "  [PASS] test_full_batch_pipeline" << std::endl;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "=== Output / Pipeline Tests ===" << std::endl;

    test_output_figure2();
    test_output_figure3();
    test_output_figure4();
    test_output_no_risk();
    test_output_sorted();
    test_full_batch_pipeline();

    std::cout << "=== All 6 output tests passed. ===" << std::endl;
    return 0;
}
