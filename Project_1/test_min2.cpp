#include "parser.h"
#include <iostream>
#include <cstdlib>
int main() {
    const char* t = std::getenv("TEMP");
    std::string dir = t ? std::string(t) + "/" : "./";
    std::string path = dir + "t1.csv";
    std::ofstream f(path);
    f << "#Submissions\n31, 4\n#Reviewers\n1, 1\n#Parameters\nMinReviewsPerSubmission, 1\nMaxReviewsPerReviewer, 1\nSecondaryReviewerExpertise, 0\nPrimarySubmissionDomain, 1\nSecondarySubmissionDomain, 0\n#Control\nGenerateAssignments, 1\nRiskAnalysis, 0\nOutputFileName, \"x.csv\"\n";
    f.close();
    std::cout << "wrote file: " << path << "\n";
    Parser parser;
    ProblemData data = parser.parse(path);
    std::cout << "parsed: " << data.numSubmissions() << " subs\n";
    return 0;
}
