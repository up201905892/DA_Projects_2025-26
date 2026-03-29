#include "../parser.h"
#include <cassert>
#include <iostream>
#include <fstream>
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

void test_figure1() {
    std::string input =
        "#Submissions\n31, 4\n87, 1\n\n#Reviewers\n1, 1\n2, 4\n\n"
        "#Parameters\nMinReviewsPerSubmission, 1\nMaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\nPrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 1\n\n#Control\nGenerateAssignments, 1\n"
        "RiskAnalysis, 0\nOutputFileName, \"assign.csv\"\n";
    auto path = writeTempFile(input, "t1.csv");
    Parser parser;
    ProblemData data = parser.parse(path);
    assert(data.numSubmissions() == 2);
    assert(data.submissions[0].id == 31);
    assert(data.submissions[0].primaryDomain == 4);
    assert(!data.submissions[0].secondaryDomain.has_value());
    assert(data.submissions[1].id == 87);
    assert(data.numReviewers() == 2);
    assert(data.reviewers[0].id == 1);
    assert(data.reviewers[0].primaryExpertise == 1);
    assert(data.reviewers[1].id == 2);
    assert(data.reviewers[1].primaryExpertise == 4);
    assert(data.config.minReviewsPerSubmission == 1);
    assert(data.config.matchMode == MatchMode::PRIMARY_ONLY);
    assert(data.config.outputFileName == "assign.csv");
    std::cout << "  [PASS] test_figure1\n";
}

int main() {
    std::cout << "Testing figure1 with -O2...\n";
    test_figure1();
    std::cout << "Done.\n";
    return 0;
}
