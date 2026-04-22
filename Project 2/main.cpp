/**
 * @file main.cpp
 * @brief Entry point for the Compiler Register Allocation Tool.
 *
 * Supports two execution modes:
 *  - Batch mode:       myProg -b ranges.txt registers.txt allocation.txt
 *  - Interactive mode: myProg  (launches menu-driven UI)
 *
 * Error messages are written to stderr; all other output to stdout or file.
 */

#include "Parser.h"
#include "Graph.h"
#include "Allocator.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void runBatch(const std::string& rangesFile,
                     const std::string& configFile,
                     const std::string& outputFile);

static void runInteractive();

// ---------------------------------------------------------------------------
// Batch mode helpers
// ---------------------------------------------------------------------------

/**
 * @brief Execute the full pipeline once given file paths.
 *
 * 1. Parse ranges → build webs → build interference graph.
 * 2. Parse config.
 * 3. Run allocator.
 * 4. Write output file.
 *
 * @complexity O(E^2 * L + W^2) dominated by web-build and coloring.
 */
static void runBatch(const std::string& rangesFile,
                     const std::string& configFile,
                     const std::string& outputFile) {
    try {
        // 1. Parse
        std::vector<RangeEntry> entries = Parser::parseRanges(rangesFile);
        AllocConfig              config  = Parser::parseConfig(configFile);

        // 2. Build webs + interference graph
        std::vector<Web> webs  = buildWebs(entries);
        Graph            graph = buildInterferenceGraph(webs);

        // 3. Allocate
        Allocator        alloc;
        AllocationResult result = alloc.allocate(webs, graph, config);

        // 4. Output
        writeOutput(outputFile, webs, result, config.numRegisters);

        if (result.success) {
            std::cout << "Allocation succeeded. Output written to: "
                      << outputFile << '\n';
        } else {
            std::cerr << "Allocation failed (some webs spilled to memory). "
                         "Output written to: " << outputFile << '\n';
        }

    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// ---------------------------------------------------------------------------
// Interactive mode helpers
// ---------------------------------------------------------------------------

/** @brief Print the interactive menu. */
static void printMenu() {
    std::cout << "\n=== Register Allocator ===\n"
              << "  1) Load ranges file\n"
              << "  2) Load config file\n"
              << "  3) Show webs\n"
              << "  4) Show interference graph\n"
              << "  5) Run allocation\n"
              << "  6) Save output to file\n"
              << "  0) Exit\n"
              << "Choice: ";
}

/** @brief Pretty-print the interference graph as adjacency list. */
static void printGraph(const std::vector<Web>& webs, const Graph& graph) {
    if (webs.empty()) { std::cout << "(no webs loaded)\n"; return; }
    std::cout << "Interference Graph (" << webs.size() << " webs):\n";
    for (int i = 0; i < (int)webs.size(); ++i) {
        std::cout << "  web" << i << " [" << webs[i].varName << "]: ";
        bool first = true;
        for (int nb : graph.neighbors(i)) {
            if (!first) std::cout << ", ";
            std::cout << "web" << nb;
            first = false;
        }
        if (first) std::cout << "(no interferences)";
        std::cout << '\n';
    }
}

/** @brief Print allocation result to console. */
static void printResult(const std::vector<Web>& webs,
                        const AllocationResult& result,
                        int numRegisters) {
    std::cout << "\n--- Allocation Result ---\n";
    std::cout << "webs: " << webs.size() << '\n';
    for (int i = 0; i < (int)webs.size(); ++i)
        std::cout << "web" << i << ": " << webs[i].toString() << '\n';

    if (!result.success &&
        std::all_of(result.colors.begin(), result.colors.end(),
                    [](int c){ return c == SPILL_COLOR; })) {
        std::cout << "registers: 0\n";
        for (int i = 0; i < (int)webs.size(); ++i)
            std::cout << "M: web" << i << '\n';
        return;
    }

    std::cout << "registers: " << numRegisters << '\n';
    for (int c = 0; c < numRegisters; ++c) {
        for (int i = 0; i < (int)webs.size(); ++i)
            if (result.colors[i] == c)
                std::cout << 'r' << c << ": web" << i << '\n';
    }
    for (int i = 0; i < (int)webs.size(); ++i)
        if (result.colors[i] == SPILL_COLOR)
            std::cout << "M: web" << i << '\n';
}

// ---------------------------------------------------------------------------
// runInteractive
// ---------------------------------------------------------------------------

static void runInteractive() {
    std::vector<RangeEntry>  entries;
    AllocConfig              config;
    std::vector<Web>         webs;
    Graph                    graph(0);
    AllocationResult         result;
    bool                     hasWebs   = false;
    bool                     hasConfig = false;
    bool                     hasResult = false;

    while (true) {
        printMenu();
        int choice = -1;
        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }
        std::cin.ignore();

        switch (choice) {
        case 0:
            std::cout << "Goodbye.\n";
            return;

        case 1: {
            std::cout << "Ranges file path: ";
            std::string path;
            std::getline(std::cin, path);
            try {
                entries  = Parser::parseRanges(path);
                webs     = buildWebs(entries);
                graph    = buildInterferenceGraph(webs);
                hasWebs  = true;
                hasResult = false;
                std::cout << "Loaded " << webs.size() << " web(s).\n";
            } catch (const std::exception& ex) {
                std::cerr << "[ERROR] " << ex.what() << '\n';
            }
            break;
        }

        case 2: {
            std::cout << "Config file path: ";
            std::string path;
            std::getline(std::cin, path);
            try {
                config    = Parser::parseConfig(path);
                hasConfig = true;
                std::cout << "Config loaded: "
                          << config.numRegisters << " register(s), algorithm=";
                switch (config.type) {
                    case AlgorithmType::BASIC:    std::cout << "basic";    break;
                    case AlgorithmType::SPILLING: std::cout << "spilling(K=" << config.K << ')'; break;
                    case AlgorithmType::SPLITTING:std::cout << "splitting(K=" << config.K << ')'; break;
                    case AlgorithmType::FREE:     std::cout << "free";     break;
                }
                std::cout << '\n';
            } catch (const std::exception& ex) {
                std::cerr << "[ERROR] " << ex.what() << '\n';
            }
            break;
        }

        case 3: {
            if (!hasWebs) { std::cout << "No ranges loaded. Use option 1 first.\n"; break; }
            std::cout << "Webs (" << webs.size() << "):\n";
            for (const auto& w : webs)
                std::cout << "  web" << w.id << " [" << w.varName
                          << "]: " << w.toString() << '\n';
            break;
        }

        case 4: {
            if (!hasWebs) { std::cout << "No ranges loaded. Use option 1 first.\n"; break; }
            printGraph(webs, graph);
            break;
        }

        case 5: {
            if (!hasWebs)   { std::cout << "No ranges loaded.\n";  break; }
            if (!hasConfig) { std::cout << "No config loaded.\n";  break; }
            try {
                Allocator alloc;
                result    = alloc.allocate(webs, graph, config);
                hasResult = true;
                printResult(webs, result, config.numRegisters);
            } catch (const std::exception& ex) {
                std::cerr << "[ERROR] " << ex.what() << '\n';
            }
            break;
        }

        case 6: {
            if (!hasResult) { std::cout << "No result yet. Run allocation first (option 5).\n"; break; }
            std::cout << "Output file path: ";
            std::string path;
            std::getline(std::cin, path);
            try {
                writeOutput(path, webs, result, config.numRegisters);
                std::cout << "Written to: " << path << '\n';
            } catch (const std::exception& ex) {
                std::cerr << "[ERROR] " << ex.what() << '\n';
            }
            break;
        }

        default:
            std::cout << "Unknown option.\n";
        }
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

/**
 * @brief Program entry point.
 *
 * Batch usage: myProg -b ranges.txt registers.txt allocation.txt
 * Interactive: myProg
 *
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on fatal error.
 */
int main(int argc, char* argv[]) {
    if (argc == 5 && std::string(argv[1]) == "-b") {
        // Batch mode
        runBatch(argv[2], argv[3], argv[4]);
    } else if (argc == 1) {
        // Interactive mode
        runInteractive();
    } else {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " -b ranges.txt registers.txt allocation.txt\n"
                  << "  " << argv[0] << "  (interactive mode)\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
