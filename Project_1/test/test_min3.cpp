#include "../parser.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
int main() {
    std::cout << "step 1\n";
    const char* t = std::getenv("TEMP");
    std::string dir = t ? std::string(t) + "/" : "./";
    std::string path = dir + "t2.csv";
    std::ofstream f(path);
    f << "#Submissions\n31, 4\n#Reviewers\n1, 1\n#Parameters\nMinReviewsPerSubmission, 1\nMaxReviewsPerReviewer, 1\nSecondaryReviewerExpertise, 0\nPrimarySubmissionDomain, 1\nSecondarySubmissionDomain, 0\n#Control\nGenerateAssignments, 1\nRiskAnalysis, 0\nOutputFileName, \"x.csv\"\n";
    f.close();
    std::cout << "step 2\n";
    try {
        Parser parser;
        ProblemData data = parser.parse("/nonexistent/path.csv");
        assert(false && "should throw");
    } catch (const std::runtime_error& e) {
        std::cout << "step 3 caught: " << e.what() << "\n";
    }
    std::cout << "step 4\n";
    return 0;
}
