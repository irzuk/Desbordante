#pragma once

#include <boost/unordered_map.hpp>
#include <filesystem>
#include <set>

#include "column_combination.h"
#include "column_layout_relation_data.h"
#include "pli_based_fd_algorithm.h"
#include "position_list_index.h"
#include "vertical.h"

class Fd_mine : public PliBasedFDAlgorithm {
private:
    const RelationalSchema* schema_;

    std::set<dynamic_bitset<>> candidate_set_;
    boost::unordered_map<dynamic_bitset<>, std::unordered_set<dynamic_bitset<>>> eq_set_;
    boost::unordered_map<dynamic_bitset<>, dynamic_bitset<>> fd_set_;
    boost::unordered_map<dynamic_bitset<>, dynamic_bitset<>> final_fd_set_;
    std::set<dynamic_bitset<>> key_set_;
    boost::unordered_map<dynamic_bitset<>, dynamic_bitset<>> closure_;
    boost::unordered_map<dynamic_bitset<>, std::shared_ptr<util::PositionListIndex const>> plis_;
    dynamic_bitset<> relation_indices_;

    void ComputeNonTrivialClosure(dynamic_bitset<> const& xi);
    void ObtainFDandKey(dynamic_bitset<> const& xi);
    void ObtainEqSet();
    void PruneCandidates();
    void GenerateNextLevelCandidates();
    void Reconstruct();
    void Display();

    unsigned long long ExecuteInternal() override;
public:
    Fd_mine(Config const& config) : PliBasedFDAlgorithm(config, {kDefaultPhaseName}) {}
    explicit Fd_mine(std::shared_ptr<ColumnLayoutRelationData> relation, Config const& config)
        : PliBasedFDAlgorithm(std::move(relation), config, {kDefaultPhaseName}) {}
};
