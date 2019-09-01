//
// Created by kek on 17.07.19.
//

#pragma once

#include <boost/dynamic_bitset.hpp>
#include <string>
#include <vector>
#include "Column.h"
#include <memory>

using boost::dynamic_bitset, std::string, std::weak_ptr, std::shared_ptr, std::vector;

class RelationalSchema;

class Vertical {
protected:
    //Vertical(shared_ptr<RelationalSchema>& relSchema, int indices);

    dynamic_bitset<> columnIndices;
    weak_ptr<RelationalSchema> schema;

public:
    Vertical(shared_ptr<RelationalSchema>& relSchema, int indices);
    Vertical() = default;
    Vertical(Vertical& other) = default;
    Vertical& operator=(const Vertical& rhs) = default;
    Vertical(Vertical&& other) noexcept;
    Vertical& operator=(Vertical&& rhs) noexcept ;

    dynamic_bitset<> getColumnIndices();
    shared_ptr<RelationalSchema> getSchema();
    bool contains(Vertical& that);
    bool intersects(Vertical& that);
    Vertical Union(Vertical& that);
    Vertical project(Vertical& that);
    Vertical without (Vertical& that);
    Vertical invert();
    Vertical invert(Vertical& scope);
    static Vertical emptyVertical(shared_ptr<RelationalSchema> relSchema);
    int getArity();
    vector<shared_ptr<Column>> getColumns();
    //possible to use list or set?

    virtual string toString();
};
