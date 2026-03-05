/**
 * @file test_parser.cpp
 * @brief Comprehensive unit tests for the Parser state machine.
 */

#include "parser.h"
#include <cassert>
#include <iostream>
#include <fstream>

// Helper: write a string to a temp file and return its path
static std::string writeTempFile(const std::string& content, const std::string& name) {
    std::string path = "/tmp/" + name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

// ============================================================================
// TEST 1: Happy path — exact Figure 1 from the PDF
// ============================================================================
void test_figure1() {
    std::string input =
        "#Submissions\n"
        "31, 4\n"
        "87, 1\n"
        "\n"
        "#Reviewers\n"
        "1, 1\n"
        "2, 4\n"
        "\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 1\n"
        "\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"assign.csv\"\n";

    auto path = writeTempFile(input, "test_fig1.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    // Submissions
    assert(data.numSubmissions() == 2);
    assert(data.submissions[0].id == 31);
    assert(data.submissions[0].primaryDomain == 4);
    assert(!data.submissions[0].secondaryDomain.has_value());
    assert(data.submissions[1].id == 87);
    assert(data.submissions[1].primaryDomain == 1);

    // Reviewers
    assert(data.numReviewers() == 2);
    assert(data.reviewers[0].id == 1);
    assert(data.reviewers[0].primaryExpertise == 1);
    assert(data.reviewers[1].id == 2);
    assert(data.reviewers[1].primaryExpertise == 4);

    // Parameters
    assert(data.config.minReviewsPerSubmission == 1);
    assert(data.config.maxReviewsPerReviewer == 1);
    assert(data.config.secondaryReviewerExpertise == false);
    assert(data.config.primarySubmissionDomain == true);
    assert(data.config.secondarySubmissionDomain == true);

    // Control
    assert(data.config.matchMode == MatchMode::PRIMARY_ONLY);
    assert(data.config.riskAnalysis == 0);
    assert(data.config.outputFileName == "assign.csv");

    // Index maps
    assert(data.submissionIndex.at(31) == 0);
    assert(data.submissionIndex.at(87) == 1);
    assert(data.reviewerIndex.at(1) == 0);
    assert(data.reviewerIndex.at(2) == 1);

    std::cout << "  [PASS] test_figure1" << std::endl;
}

// ============================================================================
// TEST 2: Secondary domains present
// ============================================================================
void test_secondary_domains() {
    std::string input =
        "#Submissions\n"
        "10, 1, 3\n"
        "20, 2\n"
        "30, 4, 1\n"
        "#Reviewers\n"
        "100, 1, 2\n"
        "200, 3\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 2\n"
        "MaxReviewsPerReviewer, 3\n"
        "SecondaryReviewerExpertise, 1\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 1\n"
        "#Control\n"
        "GenerateAssignments, 3\n"
        "RiskAnalysis, 2\n"
        "OutputFileName, \"out.csv\"\n";

    auto path = writeTempFile(input, "test_secondary.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    // Submission 10: primary=1, secondary=3
    assert(data.submissions[0].id == 10);
    assert(data.submissions[0].primaryDomain == 1);
    assert(data.submissions[0].secondaryDomain.has_value());
    assert(data.submissions[0].secondaryDomain.value() == 3);

    // Submission 20: primary=2, no secondary
    assert(!data.submissions[1].secondaryDomain.has_value());

    // Reviewer 100: primary=1, secondary=2
    assert(data.reviewers[0].secondaryExpertise.has_value());
    assert(data.reviewers[0].secondaryExpertise.value() == 2);

    // Config
    assert(data.config.minReviewsPerSubmission == 2);
    assert(data.config.maxReviewsPerReviewer == 3);
    assert(data.config.matchMode == MatchMode::ALL);
    assert(data.config.riskAnalysis == 2);
    assert(data.config.secondaryReviewerExpertise == true);

    // Derived queries
    assert(data.config.useSubmissionSecondary() == true);
    assert(data.config.useReviewerSecondary() == true);

    std::cout << "  [PASS] test_secondary_domains" << std::endl;
}

// ============================================================================
// TEST 3: Sections in non-standard order
// ============================================================================
void test_section_order() {
    std::string input =
        "#Control\n"
        "GenerateAssignments, 0\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Reviewers\n"
        "5, 2\n"
        "#Submissions\n"
        "99, 2\n";

    auto path = writeTempFile(input, "test_order.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    assert(data.numSubmissions() == 1);
    assert(data.numReviewers() == 1);
    assert(data.submissions[0].id == 99);
    assert(data.reviewers[0].id == 5);
    assert(data.config.matchMode == MatchMode::SILENT);

    std::cout << "  [PASS] test_section_order" << std::endl;
}

// ============================================================================
// TEST 4: Extra whitespace and empty lines
// ============================================================================
void test_whitespace_tolerance() {
    std::string input =
        "   \n"
        "\n"
        "  #Submissions  \n"
        "  31 ,  4  \n"
        "\n"
        "  #Reviewers  \n"
        "  1 ,  1  \n"
        "\n"
        "  #Parameters  \n"
        "  MinReviewsPerSubmission ,  1  \n"
        "  MaxReviewsPerReviewer ,  1  \n"
        "  SecondaryReviewerExpertise ,  0  \n"
        "  PrimarySubmissionDomain ,  1  \n"
        "  SecondarySubmissionDomain ,  0  \n"
        "\n"
        "  #Control  \n"
        "  GenerateAssignments ,  1  \n"
        "  RiskAnalysis ,  0  \n"
        "  OutputFileName ,  \"output.csv\"  \n";

    auto path = writeTempFile(input, "test_ws.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    assert(data.numSubmissions() == 1);
    assert(data.submissions[0].id == 31);
    assert(data.submissions[0].primaryDomain == 4);
    assert(data.config.minReviewsPerSubmission == 1);

    std::cout << "  [PASS] test_whitespace_tolerance" << std::endl;
}

// ============================================================================
// TEST 5: Error — duplicate submission ID
// ============================================================================
void test_error_duplicate_submission() {
    std::string input =
        "#Submissions\n"
        "31, 4\n"
        "31, 2\n"
        "#Reviewers\n"
        "1, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_dup_sub.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on duplicate submission");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("Duplicate submission ID") != std::string::npos ||
               msg.find("duplicate") != std::string::npos ||
               msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_duplicate_submission" << std::endl;
}

// ============================================================================
// TEST 6: Error — duplicate reviewer ID
// ============================================================================
void test_error_duplicate_reviewer() {
    std::string input =
        "#Submissions\n"
        "10, 1\n"
        "#Reviewers\n"
        "5, 2\n"
        "5, 3\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_dup_rev.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on duplicate reviewer");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("Duplicate reviewer ID") != std::string::npos ||
               msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_duplicate_reviewer" << std::endl;
}

// ============================================================================
// TEST 7: Error — malformed submission line (too few fields)
// ============================================================================
void test_error_malformed_submission() {
    std::string input =
        "#Submissions\n"
        "31\n"  // missing domain
        "#Reviewers\n"
        "1, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_malformed.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on malformed line");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("expects 2") != std::string::npos ||
               msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_malformed_submission" << std::endl;
}

// ============================================================================
// TEST 8: Error — invalid integer value
// ============================================================================
void test_error_invalid_integer() {
    std::string input =
        "#Submissions\n"
        "abc, 4\n"
        "#Reviewers\n"
        "1, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_badint.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on invalid integer");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("not a valid integer") != std::string::npos ||
               msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_invalid_integer" << std::endl;
}

// ============================================================================
// TEST 9: Error — missing mandatory section
// ============================================================================
void test_error_missing_section() {
    std::string input =
        "#Submissions\n"
        "10, 1\n"
        "#Reviewers\n"
        "1, 1\n"
        // Missing #Parameters and #Control
        ;

    auto path = writeTempFile(input, "test_missing.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on missing sections");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("#Parameters") != std::string::npos);
        assert(msg.find("#Control") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_missing_section" << std::endl;
}

// ============================================================================
// TEST 10: Error — GenerateAssignments out of range
// ============================================================================
void test_error_bad_match_mode() {
    std::string input =
        "#Submissions\n"
        "10, 1\n"
        "#Reviewers\n"
        "1, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 7\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_badmode.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on bad match mode");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("0") != std::string::npos || msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_bad_match_mode" << std::endl;
}

// ============================================================================
// TEST 11: Error — file not found
// ============================================================================
void test_error_file_not_found() {
    Parser parser;
    try {
        parser.parse("/nonexistent/file.csv");
        assert(false && "Should have thrown on missing file");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Cannot open") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_file_not_found" << std::endl;
}

// ============================================================================
// TEST 12: Larger dataset — stress test with many entries
// ============================================================================
void test_large_dataset() {
    std::ostringstream oss;
    oss << "#Submissions\n";
    int numSub = 500;
    int numRev = 200;
    for (int i = 1; i <= numSub; i++) {
        oss << i << ", " << ((i % 5) + 1);
        if (i % 3 == 0) oss << ", " << ((i % 4) + 1);  // some have secondary
        oss << "\n";
    }
    oss << "#Reviewers\n";
    for (int j = 1; j <= numRev; j++) {
        oss << (1000 + j) << ", " << ((j % 5) + 1);
        if (j % 2 == 0) oss << ", " << ((j % 3) + 1);
        oss << "\n";
    }
    oss << "#Parameters\n"
        << "MinReviewsPerSubmission, 2\n"
        << "MaxReviewsPerReviewer, 5\n"
        << "SecondaryReviewerExpertise, 1\n"
        << "PrimarySubmissionDomain, 1\n"
        << "SecondarySubmissionDomain, 1\n"
        << "#Control\n"
        << "GenerateAssignments, 3\n"
        << "RiskAnalysis, 1\n"
        << "OutputFileName, \"large_output.csv\"\n";

    auto path = writeTempFile(oss.str(), "test_large.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    assert(data.numSubmissions() == static_cast<size_t>(numSub));
    assert(data.numReviewers() == static_cast<size_t>(numRev));
    assert(data.config.minReviewsPerSubmission == 2);
    assert(data.config.maxReviewsPerReviewer == 5);
    assert(data.config.matchMode == MatchMode::ALL);

    // Verify index maps are consistent
    for (size_t i = 0; i < data.submissions.size(); i++) {
        assert(data.submissionIndex.at(data.submissions[i].id) == i);
    }
    for (size_t j = 0; j < data.reviewers.size(); j++) {
        assert(data.reviewerIndex.at(data.reviewers[j].id) == j);
    }

    // Capacity check
    assert(data.hasEnoughCapacity() == true); // 200*5=1000 >= 500*2=1000

    std::cout << "  [PASS] test_large_dataset (500 subs, 200 revs)" << std::endl;
}

// ============================================================================
// TEST 13: Case-insensitive section headers
// ============================================================================
void test_case_insensitive_headers() {
    std::string input =
        "#SUBMISSIONS\n"
        "1, 1\n"
        "#REVIEWERS\n"
        "10, 1\n"
        "#PARAMETERS\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#CONTROL\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_case.csv");
    Parser parser;
    ProblemData data = parser.parse(path);

    assert(data.numSubmissions() == 1);
    assert(data.numReviewers() == 1);

    std::cout << "  [PASS] test_case_insensitive_headers" << std::endl;
}

// ============================================================================
// TEST 14: Error — negative submission ID
// ============================================================================
void test_error_negative_id() {
    std::string input =
        "#Submissions\n"
        "-5, 1\n"
        "#Reviewers\n"
        "1, 1\n"
        "#Parameters\n"
        "MinReviewsPerSubmission, 1\n"
        "MaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\n"
        "PrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n"
        "#Control\n"
        "GenerateAssignments, 1\n"
        "RiskAnalysis, 0\n"
        "OutputFileName, \"output.csv\"\n";

    auto path = writeTempFile(input, "test_negid.csv");
    Parser parser;
    try {
        parser.parse(path);
        assert(false && "Should have thrown on negative ID");
    } catch (const ParseError& e) {
        std::string msg = e.what();
        assert(msg.find("positive") != std::string::npos ||
               msg.find("error") != std::string::npos);
    }

    std::cout << "  [PASS] test_error_negative_id" << std::endl;
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "=== Parser Unit Tests ===" << std::endl;

    test_figure1();
    test_secondary_domains();
    test_section_order();
    test_whitespace_tolerance();
    test_error_duplicate_submission();
    test_error_duplicate_reviewer();
    test_error_malformed_submission();
    test_error_invalid_integer();
    test_error_missing_section();
    test_error_bad_match_mode();
    test_error_file_not_found();
    test_large_dataset();
    test_case_insensitive_headers();
    test_error_negative_id();

    std::cout << "=== All 14 parser tests passed. ===" << std::endl;
    return 0;
}
