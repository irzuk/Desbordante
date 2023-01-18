#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>

#include "datasets.h"
#include "depminer.h"
#include "dfd.h"
#include "fastfds.h"
#include "fdep/fdep.h"
#include "fun.h"
#include "inclusion_dependences/zigzag/zigzag_algorithm.h"
#include "pyro.h"
#include "relational_schema.h"
#include "tane.h"
//#include "testing_utils.h"

using std::string, std::vector;
using ::testing::ContainerEq, ::testing::Eq;

namespace fs = std::filesystem;

class AlgorithmTest : public LightDatasets, public HeavyDatasets, public ::testing::Test {};

TEST_F(AlgorithmTest, Help) {
    // test

#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));
    ZigzagAlgorithm::InclusionDependence ind = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 2}, {1, 4});
    for (auto const &r : algorithm.getAllSchemas()) {
        std::cout << r->GetName() << "\n";
        for (auto &cc : r->GetColumns()) {
            std::cout << cc->GetName() << "\n";
        }
        std::cout << "-----------"
                  << "\n";
    }

    std::cout << algorithm.g(ind).second << "\n";  // count Fails
    std::cout << "-----------"
              << "\n";
    std::cout << ind.toString() << "\n";

    SUCCEED();
    // ASSERT_THROW(algorithm.Execute(), std::runtime_error);
}

TEST_F(AlgorithmTest, GenFirstLevelCandidates) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    algorithm.Execute();

    SUCCEED();
}

TEST_F(AlgorithmTest, AddToPositiveBorder) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    ZigzagAlgorithm::InclusionDependence indinPositiveBd1 = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {2}, {4});
    ZigzagAlgorithm::InclusionDependence indinPositiveBd2 = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0}, {1});

    ZigzagAlgorithm::InclusionDependence indToAdd1 = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 2}, {1, 4});
    ZigzagAlgorithm::InclusionDependence indToAdd2 = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 1, 2}, {1, 3, 4});
    ZigzagAlgorithm::InclusionDependence indToAdd3 = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 1, 2}, {1, 4, 3});


    std::set<ZigzagAlgorithm::InclusionDependence> border;
    border.insert(indinPositiveBd1);
    border.insert(indinPositiveBd2);

    algorithm.addToPositiveBorder(border, indToAdd1);

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    ans.insert(indToAdd1);
    EXPECT_EQ(border, ans);

    algorithm.addToPositiveBorder(border, indinPositiveBd1);
    EXPECT_EQ(border, ans);

    algorithm.addToPositiveBorder(border, indToAdd2);
    ans.clear();
    ans.insert(indToAdd2);
    EXPECT_EQ(border, ans);

    algorithm.addToPositiveBorder(border, indToAdd3);
    ans.insert(indToAdd3);
    EXPECT_EQ(border, ans);

    // Enter calc IND: TestLong.csv[0,1,2] : TestWide.csv [0, 2, 1, ]
    // IND in borderIND: TestLong.csv[0,1] : TestWide.csv [0, 2, ]
    for(auto b : border) {
        std::cout << b.toString() << "\n";
    }

    SUCCEED();
    // ASSERT_THROW(algorithm.Execute(), std::runtime_error);
}

TEST_F(AlgorithmTest, AddToNegativeBorder) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    ZigzagAlgorithm::InclusionDependence indinNegativeBd = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 2}, {1, 4});

    ZigzagAlgorithm::InclusionDependence indToAdd = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0}, {1});

    std::set<ZigzagAlgorithm::InclusionDependence> border;
    border.insert(indinNegativeBd);
    algorithm.addToNegativeBorder(border, indToAdd);

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    ans.insert(indToAdd);

    EXPECT_EQ(border, ans);

    SUCCEED();
}

TEST_F(AlgorithmTest, GenerateOptBorder) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));
    //
    ZigzagAlgorithm::InclusionDependence ind = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 1}, {1, 3});
    ZigzagAlgorithm::UnaryInclusionDependence uind = ZigzagAlgorithm::UnaryInclusionDependence(
            2, 4, algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1]);
    std::cout << ind.addUnaryIND(uind).toString() << "\n";
    //
    algorithm.Execute();
    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> output = algorithm.calculateOpt(algorithm.negativeBorder);


    std::cout << "Opt border\n";
    for (auto &x : output) {
        std::cout << x.toString() << "\n";
    }

    SUCCEED();
}

TEST_F(AlgorithmTest, GenerateCandidates) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> input;

    ZigzagAlgorithm::InclusionDependence ind = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 1, 2}, {1, 3, 4});

    input.insert(ind);

    std::cout << "Candidates\n";
    for (auto candidate : algorithm.genCandidates(input)) {
        std::cout << candidate.toString() << "\n";
    }

    SUCCEED();
}

TEST_F(AlgorithmTest, GenerateNextLevel) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
                              .separator = ',',
                              .has_header = true,
                              .start_level = 1,
                              .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> input;

    ZigzagAlgorithm::InclusionDependence ind = ZigzagAlgorithm::InclusionDependence(
            algorithm.getAllSchemas()[0], algorithm.getAllSchemas()[1], {0, 1, 2}, {1, 3, 4});

    input.insert(ind);

    std::cout << "Next Level\n";
    for (auto next : algorithm.genNextLevel(input)) {
        std::cout << next.toString() << "\n";
    }

    SUCCEED();
}


TEST_F(AlgorithmTest, Algo) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "TestLong.csv";
    auto const path2 = fs::current_path() / "input_data" / "TestWide.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
            .separator = ',',
            .has_header = true,
            .start_level = 1,
            .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> input;

    algorithm.Execute();

    SUCCEED();
}

TEST_F(AlgorithmTest, Algo2) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "People.csv";
    auto const path2 = fs::current_path() / "input_data" / "Teachers.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
            .separator = ',',
            .has_header = true,
            .start_level = 1,
            .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> input;

    algorithm.Execute();

    SUCCEED();
}

TEST_F(AlgorithmTest, AlgoMeasureTime) {
#define test_zigzag
    auto const path1 = fs::current_path() / "input_data" / "MyTest12.csv";
    auto const path2 = fs::current_path() / "input_data" / "MyTest32.csv";

    ZigzagAlgorithm::Config c{.data = {path1, path2},
            .separator = ',',
            .has_header = true,
            .start_level = 1,
            .error = 0};
    std::unique_ptr<CSVParser> input_generator =
            std::make_unique<CSVParser>(path1, c.separator, c.has_header);
    ZigzagAlgorithm algorithm = ZigzagAlgorithm(c, std::move(input_generator));

    std::set<ZigzagAlgorithm::InclusionDependence> ans;
    std::set<ZigzagAlgorithm::InclusionDependence> input;

    auto start_time = std::chrono::system_clock::now();
    algorithm.Execute();
    auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);

    std::cout<<"Time" << elapsed_milliseconds.count() << "\n";
    std::cout<<"Num of validations" << algorithm.amountOfValidation << "\n";

    SUCCEED();
}

/*  To measure time
 *  auto start_time = std::chrono::system_clock::now();
 *   auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    return elapsed_milliseconds.count();
 *  */
