#include "domain.h"
#include <iostream>
#include <cassert>

int main() {
    // === Test 1: Submission creation and domain matching ===
    Submission s1{87, 1, std::nullopt};
    assert(s1.hasDomain(1, true, false)  == true);
    assert(s1.hasDomain(2, true, false)  == false);
    assert(s1.hasDomain(1, false, true)  == false);  // no secondary

    Submission s2{31, 4, 2};  // has secondary domain = 2
    assert(s2.hasDomain(4, true, false) == true);
    assert(s2.hasDomain(2, true, false) == false);
    assert(s2.hasDomain(2, false, true) == true);   // secondary match
    assert(s2.hasDomain(2, true, true)  == true);    // either matches

    // === Test 2: Reviewer creation and expertise matching ===
    Reviewer r1{1, 1, std::nullopt};
    assert(r1.hasExpertise(1) == true);
    assert(r1.hasExpertise(2) == false);

    Reviewer r2{2, 4, 3};  // secondary expertise = 3
    assert(r2.hasExpertise(4, true, false) == true);
    assert(r2.hasExpertise(3, false, true) == true);
    assert(r2.hasExpertise(3, true, true)  == true);

    // === Test 3: MatchMode conversion ===
    assert(toMatchMode(0) == MatchMode::SILENT);
    assert(toMatchMode(1) == MatchMode::PRIMARY_ONLY);
    assert(toMatchMode(3) == MatchMode::ALL);
    try {
        toMatchMode(5);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) {
        // expected
    }

    // === Test 4: Config query methods ===
    Config cfg;
    cfg.matchMode = MatchMode::PRIMARY_ONLY;
    cfg.secondarySubmissionDomain = true;
    cfg.secondaryReviewerExpertise = true;
    assert(cfg.useSubmissionSecondary() == false);  // mode=1, so no
    assert(cfg.useReviewerSecondary()   == false);  // mode=1, so no

    cfg.matchMode = MatchMode::ALL;
    assert(cfg.useSubmissionSecondary() == true);
    assert(cfg.useReviewerSecondary()   == true);

    cfg.matchMode = MatchMode::SUB_BOTH_REV_PRI;
    assert(cfg.useSubmissionSecondary() == true);   // mode=2 + data has it
    assert(cfg.useReviewerSecondary()   == false);  // mode=2, not ALL

    // === Test 5: ProblemData with duplicate detection ===
    ProblemData data;
    data.addSubmission({87, 1, std::nullopt});
    data.addSubmission({31, 4, 2});
    data.addReviewer({1, 1, std::nullopt});
    data.addReviewer({2, 4, std::nullopt});

    assert(data.numSubmissions() == 2);
    assert(data.numReviewers()   == 2);
    assert(data.submissionIndex.at(87) == 0);
    assert(data.submissionIndex.at(31) == 1);
    assert(data.reviewerIndex.at(1)    == 0);
    assert(data.reviewerIndex.at(2)    == 1);

    // Duplicate detection
    try {
        data.addSubmission({87, 3, std::nullopt});
        assert(false && "Should have thrown on duplicate");
    } catch (const std::runtime_error&) {
        // expected
    }

    // === Test 6: Capacity sanity check ===
    data.config.maxReviewsPerReviewer   = 3;
    data.config.minReviewsPerSubmission = 2;
    // 2 reviewers * 3 max = 6 >= 2 submissions * 2 min = 4
    assert(data.hasEnoughCapacity() == true);

    data.config.maxReviewsPerReviewer = 1;
    // 2 * 1 = 2 < 2 * 2 = 4
    assert(data.hasEnoughCapacity() == false);

    // === Test 7: Domain collection ===
    auto subDomains = data.allSubmissionDomains();
    assert(subDomains.count(1) == 1);  // from s1
    assert(subDomains.count(4) == 1);  // from s2 primary
    assert(subDomains.count(2) == 1);  // from s2 secondary

    // === Test 8: SolverResult ===
    SolverResult result;
    result.maxFlow = 4;
    assert(result.isComplete(4) == true);
    assert(result.isComplete(6) == false);

    // === Test 9: Assignment ordering ===
    Assignment a1{31, 2, 4};
    Assignment a2{87, 1, 1};
    assert(a1 < a2);  // 31 < 87

    // === Test 10: ParseError ===
    try {
        throw ParseError("bad value", 42, "#Submissions");
    } catch (const ParseError& e) {
        assert(e.lineNumber() == 42);
        assert(e.section() == "#Submissions");
        std::string msg = e.what();
        assert(msg.find("line 42") != std::string::npos);
        assert(msg.find("#Submissions") != std::string::npos);
    }

    std::cout << "=== All 10 tests passed. domain.h is correct. ===" << std::endl;
    return 0;
}
