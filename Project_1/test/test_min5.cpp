#include "../parser.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdlib>

static std::string writeTempFile(const std::string& content, const std::string& name) {
    const char* t = std::getenv("TEMP");
    std::string dir = t ? std::string(t) + "/" : "./";
    std::ofstream f(dir + name);
    f << content; f.close();
    return dir + name;
}

void test1() {
    std::string input = "#Submissions\n31, 4\n87, 1\n\n#Reviewers\n1, 1\n2, 4\n\n"
        "#Parameters\nMinReviewsPerSubmission, 1\nMaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\nPrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 1\n\n#Control\nGenerateAssignments, 1\n"
        "RiskAnalysis, 0\nOutputFileName, \"assign.csv\"\n";
    auto path = writeTempFile(input, "t5.csv");
    Parser parser;
    ProblemData data = parser.parse(path);
    assert(data.numSubmissions() == 2);
    std::cout << "t1 passed\n";
}

int main() {
    std::cout << "start\n";
    test1();
    std::cout << "done\n";
    return 0;
}
