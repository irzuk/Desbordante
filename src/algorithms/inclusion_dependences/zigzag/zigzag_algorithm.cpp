//
// Created by Irina Zelentsova on 28.11.2022.
//

#include "zigzag_algorithm.h"

#include <column.h>

#include <algorithm>
#include <iostream>

ZigzagAlgorithm::ZigzagAlgorithm(Config c, std::unique_ptr<CSVParser> input_gen)
    : Primitive(std::move(input_gen), {}) {
    config_ = std::move(c);
    // validate amount of tables
    if (config_.data.size() == 1) {
        throw std::runtime_error("Only one table!");
    }

    for (auto path : config_.data) {  // define all schemas
        // TODO: do reset for input generator?
        CSVParser input_generator(path, config_.separator, config_.has_header);
        std::unique_ptr<RelationalSchema> schema = std::make_unique<RelationalSchema>(
                input_generator.GetRelationName(), config_.is_null_equal_null);
        num_columns.push_back(input_generator.GetNumberOfColumns());

        for (int i = 0; i < num_columns.back(); ++i) {
            Column column(schema.get(), input_generator.GetColumnName(i), i);
            schema->AppendColumn(std::move(column));
        }
        schema->Init();
        schemas.emplace_back(std::move(schema));
    }
    currentRefSchema = 0;
    currentDepSchema = 1;
}

unsigned long long ZigzagAlgorithm::Execute() {
    // make restriction for only two tables
    if (config_.start_level == 1) {
#ifdef test_zigzag
        std::cout << "Generate 1 level"
                  << "\n";
#endif
        // TODO: get num of attributes
        for (int i = 0; i < num_columns[currentRefSchema]; i++) {
            nodes.push_back(std::vector<UnaryInclusionDependence>());
            for (int j = 0; j < num_columns[currentDepSchema]; j++) {
                UnaryInclusionDependence uind = UnaryInclusionDependence(
                        i, j, schemas[currentRefSchema], schemas[currentDepSchema]);
                if (g(uind)) {
                    positiveBorder.insert(InclusionDependence(uind));
                    nodes[i].push_back(uind);
                } else {
                    negativeBorder.insert(InclusionDependence(uind));
                }
            }
        }
    } else {
        // TODO: add to input data level k from different algo + build graph
    }

    std::cout << "Positive Border: "
              << "\n";
    for (auto i : positiveBorder) {
        std::cout << i.toString() << "\n";
    }
    std::cout << "Negative Border: "
              << "\n";
    for (auto i : negativeBorder) {
        std::cout << i.toString() << "\n";
    }
    std::cout << "Graph: "
              << "\n";
    int i = 0;
    for (auto& x : nodes) {
        std::cout << i << ": ";
        for (auto y : x) {
            std::cout << y.toString() << ", ";
        }
        i++;
        std::cout << "\n";
    }
    // TODO calculate optBd

    std::set<InclusionDependence> optBorder = calculateOpt(negativeBorder);
    // calculate diff Bd+ and Bd+(Iopt)
    std::set<InclusionDependence> diff = optBorder;

    std::cout << "Diff "
              << "\n";
    for (auto x : positiveBorder) {
        diff.erase(x);
    }
    for (auto x : diff) {
        std::cout << x.toString() << "\n";
    }

    while (!diff.empty()) {
        std::set<InclusionDependence> opt;
        std::set<InclusionDependence> pess;

        for (InclusionDependence ind : diff) {
            auto error = g(ind);
            if (error.first - error.second == 0) {
                addToPositiveBorder(positiveBorder, ind);
            } else {
                if ((double)error.second / (double)error.first < config_.error && // check nums
                    ind.size() > currentLevel + 1) {
                    opt.insert(ind);
                } else {
                    pess.insert(ind);
                }
            }
        }

        while (!opt.empty()) {
            std::set<InclusionDependence> candidates = genCandidates(opt);
            std::set<InclusionDependence> newCandidates;
            for (auto ind : candidates) {
                auto error = g(ind);
                if (error.first - error.second == 0) {
                    addToPositiveBorder(positiveBorder, ind);
                    newCandidates.insert(ind);
                } else {
                    addToNegativeBorder(negativeBorder, ind);
                }
            }
            opt = newCandidates;
        }

        std::set<InclusionDependence> c_next_level = genNextLevel(pess);  // generate
        for (InclusionDependence p : c_next_level) {
            if (ValidationOfINDinTable(p)) {  // is satisfied by border ???
                addToPositiveBorder(positiveBorder, p);
            } else {
                addToNegativeBorder(negativeBorder, p);
            }
        }
        // compute optimistic border
        optBorder = calculateOpt(negativeBorder);
        // calculate diff Bd+ and Bd+(Iopt)
        diff = optBorder;

        std::cout << "Diff "
                  << "\n";
        for (auto x : positiveBorder) {
            diff.erase(x);
        }
        currentLevel++;
        ///
        std::cout << "PositiveBorder"
                  << "\n";
        for (auto p : positiveBorder) {
            std::cout << p.toString() << "\n";
        }
        std::cout << "NegativeBorder"
                  << "\n";
        for (auto p : negativeBorder) {
            std::cout << p.toString() << "\n";
        }
        ///
    }
    result = positiveBorder;
    return 0;
}

bool ZigzagAlgorithm::ValidationOfINDinTable(InclusionDependence ind) {
    std::pair<int, int> p = g(ind);
    return p.first == p.second;  // Satisfied IND
}
bool ZigzagAlgorithm::ValidationOfINDinBorder() {
    // TODO ???
    return false;
}

// to calculate error margin
std::pair<int, int> ZigzagAlgorithm::g(InclusionDependence& ind) {
    amountOfValidation++;
    // load one table to set
    int countFails = 0, countAll = 0;
    std::set<std::string> ref;
    // input_generator_.reset();
    // TODO: put path to config
    auto input_generator_ = std::make_unique<CSVParser>(
            fs::current_path() / "input_data" / ind.getRefSchema()->GetName(), config_.separator,
            config_.has_header);

#ifdef test_zigzag
    std::cout << "Ref: " << ind.getRefSchema()->GetName() << "\n";
#endif
    while (input_generator_->HasNextRow()) {
        auto input = input_generator_->GetNextRow();
        ref.insert(ind.getRefAttributes(input));
#ifdef test_zigzag
        std::cout << ind.getRefAttributes(input) << "\n";
#endif
    }
#ifdef test_zigzag
    std::cout << "Dep: " << ind.getDepSchema()->GetName() << "\n";
#endif
    // check one by one in set
    input_generator_ = std::make_unique<CSVParser>(
            fs::current_path() / "input_data" / ind.getDepSchema()->GetName(), config_.separator,
            config_.has_header);
    while (input_generator_->HasNextRow()) {
        auto input = input_generator_->GetNextRow();
#ifdef test_zigzag
        std::cout << ind.getDepAttributes(input) << "\n";
#endif
        countFails += ref.find(ind.getDepAttributes(input)) == ref.end() ? 0 : 1;
        countAll++;
    }
    return std::pair<int, int>(countAll, countFails);
}

bool ZigzagAlgorithm::g(ZigzagAlgorithm::UnaryInclusionDependence& i) {
    InclusionDependence ind = InclusionDependence(i);
    auto error = g(ind);
    return error.first == error.second;
}

bool ZigzagAlgorithm::ValidationOfInput() {
    // TODO: add validation
    return false;
}
std::vector<std::unique_ptr<RelationalSchema>>& ZigzagAlgorithm::getAllSchemas() {
    return schemas;
}

void ZigzagAlgorithm::calc(unsigned int currLevel, ZigzagAlgorithm::InclusionDependence ind,
                           std::set<ZigzagAlgorithm::InclusionDependence>& ans) {
    // std::cout << "Enter calc " << ind.toString() << "\n";
    currLevel++;
    if (currLevel == nodes.size()) {  // no way to go
                                      // if(!inNegativeBorder(ind)) {
        addToPositiveBorder(ans, ind);
        // std::cout << "Out1\n";
        //}
        return;
    }
    while (currLevel < nodes.size() && nodes[currLevel].empty()) {
        currLevel++;
    }

    for (auto i : nodes[currLevel]) {
        if (ind.canAddUnaryIND(i)) {
            calc(currLevel, ind.addUnaryIND(i), ans);
        } else {
            // calc(currLevel + 1, ind, ans);
            // check with neg border
            // if(!inNegativeBorder(ind)) {
            addToPositiveBorder(ans, ind);
            // std::cout << "Out2\n";
            // }
        }
    }
}

std::set<ZigzagAlgorithm::InclusionDependence> ZigzagAlgorithm::calculateOpt(
        std::set<InclusionDependence>& negBd) {
    std::set<InclusionDependence> ans;
    int first = 0;
    while (nodes[first].empty()) {
        first++;
    }
    for (auto x : nodes[first]) {
        std::cout << x.toString() << "\n";
        calc(0, InclusionDependence(x), ans);
    }
    return ans;
}
std::set<ZigzagAlgorithm::InclusionDependence> ZigzagAlgorithm::genNextLevel(
        std::set<ZigzagAlgorithm::InclusionDependence>& pess) {
    std::set<InclusionDependence> ans;
    for (InclusionDependence ind : pess) {
        ans.merge(ind.getSmallerINDs(currentLevel + 1));
    }
    return ans;
}
std::set<ZigzagAlgorithm::InclusionDependence> ZigzagAlgorithm::genCandidates(
        std::set<ZigzagAlgorithm::InclusionDependence>& opt) {
    // get all INDs that are one size smaller
    std::set<InclusionDependence> ans;
    for (InclusionDependence ind : opt) {
        // std::cout << "Try to gen candidate" << ind.toString() << "\n";
        ans.merge(ind.getSmallerINDs(ind.size() - 1));
    }
    return ans;
}
void ZigzagAlgorithm::addToPositiveBorder(std::set<InclusionDependence>& s,
                                          ZigzagAlgorithm::InclusionDependence ind) {
    std::set<InclusionDependence> toRemove;
    for (auto i : s) {
        // std::cout << "IND in border" << i.toString() << "\n";
        if (i.isSatisfied(ind)) {
            toRemove.insert(i);
        } else {
            // std::cout << "Out3\n";
            if (i.isGeneralised(ind)) return;
            // std::cout << "Out4\n";
        }
    }
    // std::cout << "Out5\n";
    for (auto ss : s) {
        // std::cout << ss.toString() << "\n";
    }
    // std::cout << "Out55\n";
    for (auto toR : toRemove) {
        // std::cout << toR.toString() << "\n";
        s.erase(toR);
    }
    // s.erase(toRemove.begin(), toRemove.end());
    // std::cout << "Out5\n";
    s.insert(ind);
    // std::cout << "Out5\n";
}
void ZigzagAlgorithm::addToNegativeBorder(std::set<InclusionDependence>& s,
                                          ZigzagAlgorithm::InclusionDependence ind) {
    std::set<InclusionDependence> toRemove;
    for (auto& i : s) {
        if (i.isGeneralised(ind)) {
            // s.erase(i);
            toRemove.insert(i);
            // if (s.empty()) {
            //    break;
            //}
        } else {
            if (i.isSatisfied(ind)) return;
        }
    }
    for (auto toR : toRemove) {
        s.erase(toR);
    }
    s.insert(ind);
}

bool ZigzagAlgorithm::inNegativeBorder(ZigzagAlgorithm::InclusionDependence ind) {
    for (auto& i : negativeBorder) {
        if (i.isSatisfied(ind)) {
            return true;
        }
    }
    return false;
}

void ZigzagAlgorithm::setRef(int schema) {
    currentRefSchema = schema;
}
void ZigzagAlgorithm::setDep(int schema) {
    currentDepSchema = schema;
}
