//
// Created by Irina Zelentsova on 28.11.2022.
//

#pragma once

#include <primitive.h>
#include <vertical.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <string>

#include "relational_schema.h"
//#include "../tests/testing_utils.h"

namespace fs = std::filesystem;

class ZigzagAlgorithm : public algos::Primitive {
public:
    struct Config {
        std::vector<std::filesystem::path> data{}; /* Path to input files */
        char separator = ',';                      /* Separator for csv */
        bool has_header = true;                    /* Indicates if input file has header */
        bool is_null_equal_null = true;            /* Is NULL value equals another NULL value */

        unsigned int start_level = 1; /* Starting level of the algorithm */
        double error = 0;             /* Value for validation g function */

        /* Satisfied INDs of certain level */
        /* Un-Satisfied INDs of certain level */
    };

public:
    class UnaryInclusionDependence {
    public:
        int refInd;
        int depInd;

        std::unique_ptr<RelationalSchema>& refSchema;
        std::unique_ptr<RelationalSchema>& depSchema;

        UnaryInclusionDependence(int ref, int dep, std::unique_ptr<RelationalSchema>& refSch,
                                 std::unique_ptr<RelationalSchema>& depSch)
            : refSchema(refSch), depSchema(depSch) {
            refInd = ref;
            depInd = dep;
        }

        std::string toString() {
            return "{" + std::to_string(refInd) + " " + std::to_string(depInd) + "}";
        }
    };

    class InclusionDependence {
    private:
        Vertical ref;
        Vertical dep;
        std::vector<int> permentation;

        size_t sizeOfIND;

    public:
        bool operator<(InclusionDependence const ind) const {
            if (this->ref == ind.ref) {
                return this->permentation < ind.permentation;
            } else {
                return this->ref < ind.ref;
            }
        }

        bool operator==(InclusionDependence const ind) const {
            return this->ref == ind.ref && this->permentation == ind.permentation;
        }

        bool canAddUnaryIND(UnaryInclusionDependence i) {
            InclusionDependence ind = InclusionDependence(i);
            // TODO add check for last attribute in ref
            return !this->dep.Contains(ind.dep);
        }

        InclusionDependence addUnaryIND(UnaryInclusionDependence i) {
            auto r_bitset = this->ref.GetColumnIndices();
            r_bitset[i.refInd] = true;
            Vertical r = this->ref.GetSchema()->GetVertical(r_bitset);
            auto d_bitset = this->dep.GetColumnIndices();
            d_bitset[i.depInd] = true;
            Vertical d = this->dep.GetSchema()->GetVertical(d_bitset);
            std::vector<int> perm = this->permentation;
            perm.push_back(i.depInd);
            InclusionDependence ind = InclusionDependence(r,d,perm);
            return ind;
        }

        // input sample: {1, 3}, {4, 2}
        InclusionDependence(std::unique_ptr<RelationalSchema>& refSchema,
                            std::unique_ptr<RelationalSchema>& depSchema,
                            const std::vector<int>& refAttributes, std::vector<int> depAttributes) {
            // TODO: check that ref sorted
            if (refAttributes.size() != depAttributes.size()) {
                throw std::runtime_error("Invalid IND: different sizes of attributes");
            }
            sizeOfIND = depAttributes.size();

            ref = refSchema->GetVertical(refSchema->IndicesToBitset(refAttributes));
            dep = depSchema->GetVertical(depSchema->IndicesToBitset(depAttributes));
            permentation = depAttributes;

            // std::cout << "IND created:" << this->toString() << "\n";
        }

        InclusionDependence(Vertical r, Vertical d, std::vector<int> depAttributes) {
            // TODO: check that ref sorted
            sizeOfIND = depAttributes.size();
            ref = std::move(r);
            dep = std::move(d);
            permentation = depAttributes;
        }

        InclusionDependence(UnaryInclusionDependence i)
            : InclusionDependence(i.refSchema, i.depSchema, {i.refInd}, {i.depInd}) {}

        static InclusionDependence genLessIND(InclusionDependence ind, int index) {
            std::vector<unsigned int> r_indices = ind.ref.GetColumnIndicesAsVector();
            r_indices.erase(r_indices.begin() + index);
            Vertical r = ind.ref.GetSchema()->GetVertical(
                    ind.ref.GetSchema()->IndicesToBitset(r_indices));
            std::vector<unsigned int> d_indices = ind.dep.GetColumnIndicesAsVector();
            d_indices.erase(d_indices.begin() + index);
            Vertical d = ind.dep.GetSchema()->GetVertical(
                    ind.dep.GetSchema()->IndicesToBitset(d_indices));
            ind.permentation.erase(ind.permentation.begin() + index);
            InclusionDependence indOut = InclusionDependence(r, d, ind.permentation);
            return indOut;
        }

        unsigned int size() {
            return sizeOfIND;
        }

        bool isGeneralised(const InclusionDependence& other) const {
            if (!this->ref.Contains(other.ref)) {
                return false;
            }
            if (!this->dep.Contains(other.dep)) {
                return false;
            }
            //std::cout << "Almost there\n";
            std::vector<unsigned int> refIndOther = other.ref.GetColumnIndicesAsVector();
            std::vector<unsigned int> refIndThis = this->ref.GetColumnIndicesAsVector();

            std::set<std::pair<unsigned int,int>> oth;
            std::set<std::pair<unsigned int,int>> ths;
            for(unsigned int i = 0; i < refIndOther.size(); i++){
                oth.insert(std::pair(refIndOther[i],other.permentation[i]));
            }
            for(unsigned int i = 0; i < refIndThis.size(); i++){
                ths.insert(std::pair(refIndThis[i],this->permentation[i]));
            }
            for(auto o : oth) {
                if(ths.find(o) == ths.end()) {
                    return false;
                }
            }
            return true;
        }
        bool isSatisfied(const InclusionDependence& other) const {
            if (!other.ref.Contains(this->ref)) {
                return false;
            }
            if (!other.dep.Contains(this->dep)) {
                return false;
            }
           // std::cout << "Almost there\n";
            std::vector<unsigned int> refIndOther = other.ref.GetColumnIndicesAsVector();
            std::vector<unsigned int> refIndThis = this->ref.GetColumnIndicesAsVector();

            std::set<std::pair<unsigned int,int>> oth;
            std::set<std::pair<unsigned int,int>> ths;
            for(unsigned int i = 0; i < refIndOther.size(); i++){
                oth.insert(std::pair(refIndOther[i],other.permentation[i]));
            }
            for(unsigned int i = 0; i < refIndThis.size(); i++){
                ths.insert(std::pair(refIndThis[i],this->permentation[i]));
            }
            for(auto o : ths) {
                if(oth.find(o) == oth.end()) {
                    return false;
                }
            }
            return true;
        }

        RelationalSchema const* getRefSchema() {
            return ref.GetSchema();
        }

        RelationalSchema const* getDepSchema() {
            return dep.GetSchema();
        }

        std::string getRefAttributes(std::vector<std::string> line) {
            std::string ans;
            for (unsigned i = ref.GetColumnIndices().find_first();
                 i < ref.GetColumnIndices().size(); i = ref.GetColumnIndices().find_next(i)) {
                ans += line[i];
            }
            return ans;
        }

        std::string getDepAttributes(std::vector<std::string>& line) {
            std::string ans;
            for (int i : permentation) {
                ans += line[i];
            }
            return ans;
        }

        std::set<InclusionDependence> getSmallerINDs(unsigned int curLevel, unsigned long deleteAfter = 0) {
            std::set<InclusionDependence> ans;
            if(sizeOfIND == curLevel) {
                ans.insert(*this);
                return ans;
            }
            //std::cout << "Try to make smaller" << this->toString() << "\n";
            for (unsigned long i = deleteAfter; i <= curLevel; i++) { // size of last el
                InclusionDependence ind = InclusionDependence(*this);
                ans.merge(InclusionDependence::genLessIND(ind, i).getSmallerINDs(curLevel, i));
            }
            return ans;
        }

        std::string toString() const {
            // std::cout << permentation[0] << "\n";
            std::string perm = "";
            for (auto x : permentation) {
                perm += std::to_string(x);
                perm += ", ";
            }
            return "IND: " + ref.GetSchema()->GetName() + ref.ToIndicesString() + " : " +
                   dep.GetSchema()->GetName() + " [" + perm + "]";
        }
    };

private:
public:
    std::vector<std::unique_ptr<RelationalSchema>> schemas;

    std::set<InclusionDependence> positiveBorder;  // Bd+
    std::set<InclusionDependence> negativeBorder;  // Bd-

    std::vector<std::vector<UnaryInclusionDependence>> nodes;
    unsigned int currentLevel = 1;
    int currentRefSchema = 0;
    int currentDepSchema = 1;
    std::vector<int> num_columns;

    bool ValidationOfInput();  // check that all INDs are satisfied and size is correct

    bool ValidationOfINDinTable(InclusionDependence ind);  //
    bool ValidationOfINDinBorder();                        //

    void addToPositiveBorder(std::set<InclusionDependence>& s, InclusionDependence ind);
    void addToNegativeBorder(std::set<InclusionDependence>& s, InclusionDependence ind);
    bool inNegativeBorder(InclusionDependence ind);

    void calc(unsigned int currLevel, ZigzagAlgorithm::InclusionDependence ind,
              std::set<ZigzagAlgorithm::InclusionDependence>& ans);
    std::set<InclusionDependence> calculateOpt(std::set<InclusionDependence>& negBd);
    std::set<InclusionDependence> genNextLevel(std::set<ZigzagAlgorithm::InclusionDependence>& pess);
    std::set<InclusionDependence> genCandidates(
            std::set<ZigzagAlgorithm::InclusionDependence>& opt);
    Config config_;

    // public:
    std::pair<int, int> g(InclusionDependence& ind);
    bool g(UnaryInclusionDependence& ind);

    unsigned long long Execute() override;
    ZigzagAlgorithm(Config c, std::unique_ptr<CSVParser> input_gen);
    // ZigzagAlgorithm(std::vector<fs::path> const& paths, char separator, bool hasHeader);

    // for tests
    std::vector<std::unique_ptr<RelationalSchema>>& getAllSchemas();
    std::set<InclusionDependence> result;
    void setRef(int schema);
    void setDep(int schema);

    int amountOfValidation = 0;
};
