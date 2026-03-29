#include "parser.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdlib>

static std::string writeTempFile(const std::string& content, const std::string& name) {
    const char* t = std::getenv("TEMP");
    std::string dir = t ? std::string(t) + "/" : "./";
    std::string path = dir + name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

int main() {
    std::cout << "start\n";
    std::string input = "#Submissions\n31, 4\n#Reviewers\n1, 1\n"
        "#Parameters\nMinReviewsPerSubmission, 1\nMaxReviewsPerReviewer, 1\n"
        "SecondaryReviewerExpertise, 0\nPrimarySubmissionDomain, 1\n"
        "SecondarySubmissionDomain, 0\n#Control\nGenerateAssignments, 1\n"
        "RiskAnalysis, 0\nOutputFileName, \"assign.csv\"\n";
    auto path = writeTempFile(input, "t3.csv");
    Parser parser;
    ProblemData data = parser.parse(path);
    assert(data.numSubmissions() == 1);
    std::cout << "done\n";
    return 0;
}
