#include "csv_stats.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/thread.hpp>

namespace algos {

namespace fs = std::filesystem;
namespace mo = model;

CsvStats::CsvStats(const FDAlgorithm::Config& config)
    : Primitive(config.data, config.separator, config.has_header, {"Calculating statistics"}),
      config_(config),
      col_data_(FDAlgorithm::CreateColumnData(config)),
      all_stats_(col_data_.size()),
      threads_num_(config_.parallelism) {}

Statistic CsvStats::GetMin(size_t index, mo::CompareResult order) const {
    const mo::TypedColumnData& col = col_data_[index];
    if (!mo::Type::IsOrdered(col.GetTypeId())) return {};

    const mo::Type& type = col.GetType();
    const std::vector<const std::byte*>& data = col.GetData();
    const std::byte* result = nullptr;
    for (size_t i = 0; i < data.size(); ++i) {
        if (col.IsNullOrEmpty(i)) continue;
        if (result != nullptr) {
            if (type.Compare(data[i], result) == order) 
                result = data[i];
        } else {
            result = data[i];
        }
    }
    return Statistic(result, &type, true);
}

Statistic CsvStats::GetMax(size_t index) const {
    return GetMin(index, mo::CompareResult::kGreater);
}

Statistic CsvStats::GetSum(size_t index) const {
    if (all_stats_[index].sum.HasValue()) return all_stats_[index].sum;
    const mo::TypedColumnData& col = col_data_[index];
    if (!col.IsNumeric()) return {};

    const std::vector<const std::byte*>& data = col.GetData();
    const auto& type = static_cast<const mo::INumericType&>(col.GetType());
    std::byte* sum(type.Allocate());
    for (size_t i = 0; i < data.size(); ++i) {
        if (!col.IsNullOrEmpty(i)) type.Add(sum, data[i], sum);
    }
    return Statistic(sum, &type, false);
};

Statistic CsvStats::GetAvg(size_t index) const {
    if (all_stats_[index].avg.HasValue()) return all_stats_[index].avg;
    const mo::TypedColumnData& col = col_data_[index];
    if (!col.IsNumeric()) return {};
    mo::DoubleType double_type;

    Statistic sum_stat = GetSum(index);
    std::byte* double_sum = mo::DoubleType::MakeFrom(sum_stat.GetData(), col.GetType());
    std::byte* avg = double_type.Allocate();
    const std::byte* count_of_nums = double_type.MakeValue(this->NumberOfValues(index));
    double_type.Div(double_sum, count_of_nums, avg);

    double_type.Free(double_sum);
    double_type.Free(count_of_nums);
    return Statistic(avg, &double_type, false);
}

Statistic CsvStats::CalculateCentralMoment(size_t index, int number, bool bessel_correction) const {
    const mo::TypedColumnData& col = col_data_[index];
    if (!col.IsNumeric()) return {};
    const std::vector<const std::byte*>& data = col.GetData();
    mo::DoubleType double_type;

    Statistic avg = GetAvg(index);
    std::byte* neg_avg = double_type.Allocate();
    double_type.Negate(avg.GetData(), neg_avg);
    std::byte* sum_of_difs = double_type.Allocate();
    std::byte* dif = double_type.Allocate();
    for (size_t i = 0; i < data.size(); ++i) {
        if (col.IsNullOrEmpty(i)) continue;
        const std::byte* double_num = mo::DoubleType::MakeFrom(data[i], col.GetType());
        double_type.Add(double_num, neg_avg, dif);
        double_type.Power(dif, number, dif);
        double_type.Add(sum_of_difs, dif, sum_of_difs);
        double_type.Free(double_num);
    }
    std::byte* count_of_nums =
            double_type.MakeValue(this->NumberOfValues(index) - (bessel_correction ? 1 : 0));
    std::byte* result = double_type.Allocate();
    double_type.Div(sum_of_difs, count_of_nums, result);
    double_type.Free(neg_avg);
    double_type.Free(sum_of_difs);
    double_type.Free(dif);
    double_type.Free(count_of_nums);
    return Statistic(result, &double_type, false);
}

Statistic CsvStats::GetCorrectedSTD(size_t index) const {
    if (!col_data_[index].IsNumeric()) return {};
    mo::DoubleType double_type;
    std::byte* result = double_type.Allocate();
    double_type.Power(CalculateCentralMoment(index, 2, true).GetData(), 0.5, result);
    return Statistic(result, &double_type, false);
}

Statistic CsvStats::GetCentralMomentOfDist(size_t index, int number) const {
    return CalculateCentralMoment(index, number, false);
}

Statistic CsvStats::GetStandardizedCentralMomentOfDist(size_t index, int number) const {
    mo::DoubleType double_type;
    Statistic std = GetCorrectedSTD(index);
    Statistic central_moment = GetCentralMomentOfDist(index, number);
    if (!central_moment.HasValue() || !std.HasValue()) return {};

    std::byte* result(double_type.Allocate());
    double_type.Power(std.GetData(), number, result);
    double_type.Div(central_moment.GetData(), result, result);
    return Statistic(result, &double_type, false);
}

Statistic CsvStats::GetSkewness(size_t index) const {
    if (all_stats_[index].skewness.HasValue()) return all_stats_[index].skewness;
    const mo::TypedColumnData& col = col_data_[index];
    if (!col.IsNumeric()) return {};
    return GetStandardizedCentralMomentOfDist(index, 3);
}

Statistic CsvStats::GetKurtosis(size_t index) const {
    if (all_stats_[index].kurtosis.HasValue()) return all_stats_[index].kurtosis;
    const mo::TypedColumnData& col = col_data_[index];
    if (!col.IsNumeric()) return {};
    return GetStandardizedCentralMomentOfDist(index, 4);
}

size_t CsvStats::NumberOfValues(size_t index) const {
    const mo::TypedColumnData& col = col_data_[index];
    return col.GetNumRows() - col.GetNumNulls() - col.GetNumEmpties();
};

static size_t inline CountDistinctInSortedData(const std::vector<const std::byte*>& data,
                                               const mo::Type& type) {
    size_t distinct = data.size() == 0 ? 0 : 1;
    for (size_t i = 0; i + 1 < data.size(); ++i) {
        if (type.Compare(data[i], data[i + 1]) != model::CompareResult::kEqual) 
            ++distinct;
    }
    return distinct;
}

size_t CsvStats::MixedDistinct(size_t index) const {
    const mo::TypedColumnData& col = col_data_[index];
    const std::vector<const std::byte*>& data = col.GetData();
    mo::MixedType mixed_type(config_.is_null_equal_null);

    std::vector<std::vector<const std::byte*>> values_by_type_id(mo::TypeId::_size());

    for (size_t i = 0; i < data.size(); ++i) {
        if (col.IsNullOrEmpty(i)) continue;
        values_by_type_id[mixed_type.RetrieveTypeId(data[i])._to_index()].push_back(data[i]);
    }

    size_t result = 0;
    for (std::vector<const std::byte*>& type_values : values_by_type_id) {
        std::sort(type_values.begin(), type_values.end(), mixed_type.GetComparator());
        result += CountDistinctInSortedData(type_values, mixed_type);
    }
    return result;
}

size_t CsvStats::Distinct(size_t index) {
    if (all_stats_[index].distinct != 0) return all_stats_[index].distinct;
    const mo::TypedColumnData& col = col_data_[index];
    if (col.GetTypeId() == +mo::TypeId::kMixed) {
        all_stats_[index].distinct = MixedDistinct(index);
        return all_stats_[index].distinct;
    }
    const auto& type = col.GetType();

    std::vector<const std::byte*> data = DeleteNullAndEmpties(index);
    std::sort(data.begin(), data.end(), type.GetComparator());
    return all_stats_[index].distinct = CountDistinctInSortedData(data, type);
}

std::vector<std::vector<std::string>> CsvStats::ShowSample(size_t start_row, size_t end_row,
                                                           size_t start_col, size_t end_col,
                                                           size_t str_len, size_t unsigned_len,
                                                           size_t double_len) const {
    auto cut_str = [](const std::string& str, size_t len) {
        return str.substr(0, std::min(len, str.length()));
    };

    auto get_max_len = [str_len, double_len, unsigned_len](mo::TypeId type_id) {
        switch (type_id) {
            case mo::TypeId::kDouble:
                return double_len;
            case mo::TypeId::kInt:
                return unsigned_len;
            default:
                return str_len;
        }
    };

    std::vector<std::vector<std::string>> res(end_row - start_row + 1,
                                              std::vector<std::string>(end_col - start_col + 1));

    for (size_t j = start_col - 1; j < end_col; ++j) {
        const mo::TypedColumnData& col = col_data_[j];
        const auto& type = col.GetType();
        mo::NullType null_type(config_.is_null_equal_null);
        mo::EmptyType empty_type;
        for (size_t i = start_row - 1; i < end_row; ++i) {
            res[i][j] = cut_str(col.GetDataAsString(i), get_max_len(type.GetTypeId()));
        }
    }
    return res;
}

bool CsvStats::IsCategorical(size_t index, size_t quantity) {
    return this->Distinct(index) <= quantity;
}

std::vector<const std::byte*> CsvStats::DeleteNullAndEmpties(size_t index) {
    const mo::TypedColumnData& col = col_data_[index];
    mo::TypeId type_id = col.GetTypeId();
    if (type_id == +mo::TypeId::kNull || type_id == +mo::TypeId::kEmpty ||
        type_id == +mo::TypeId::kUndefined)
        return {};
    const std::vector<const std::byte*>& data = col.GetData();
    std::vector<const std::byte*> res;
    res.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        if (!col.IsNullOrEmpty(i)) res.push_back(data[i]);
    }
    return res;
}

Statistic CsvStats::GetQuantile(double part, size_t index, bool calc_all) {
    const mo::TypedColumnData& col = col_data_[index];
    if (!mo::Type::IsOrdered(col.GetTypeId())) return {};
    const mo::Type& type = col.GetType();
    std::vector<const std::byte*> data = DeleteNullAndEmpties(index);
    int quantile = data.size() * part;

    if (calc_all && !all_stats_[index].quantile25.HasValue()) {
        std::sort(data.begin(), data.end(), type.GetComparator());
        all_stats_[index].quantile25 =
                Statistic(data[(size_t)(data.size() * 0.25)], &col.GetType(), true);
        all_stats_[index].quantile50 =
                Statistic(data[(size_t)(data.size() * 0.5)], &col.GetType(), true);
        all_stats_[index].quantile75 =
                Statistic(data[(size_t)(data.size() * 0.75)], &col.GetType(), true);
        all_stats_[index].min = Statistic(data[0], &col.GetType(), true);
        all_stats_[index].max = Statistic(data.back(), &col.GetType(), true);
        all_stats_[index].distinct = CountDistinctInSortedData(data, type);
    } else {
        std::nth_element(data.begin(), data.begin() + quantile, data.end(), type.GetComparator());
    }

    return Statistic(data[quantile], &col.GetType(), true);
}

unsigned long long CsvStats::Execute() {
    auto start_time = std::chrono::system_clock::now();
    double percent_per_col = kTotalProgressPercent / all_stats_.size();
    auto task = [percent_per_col, this](size_t index) {
        all_stats_[index].count = NumberOfValues(index);
        if (this->col_data_[index].GetTypeId() != +mo::TypeId::kMixed) {
            all_stats_[index].sum = GetSum(index);
            // will use all_stats_[index].sum
            all_stats_[index].avg = GetAvg(index);

            GetQuantile(0.25, index, true);  // distint is calculated here
            // after distinct, for faster executing
            all_stats_[index].kurtosis = GetKurtosis(index);
            all_stats_[index].skewness = GetSkewness(index);
            all_stats_[index].STD = GetCorrectedSTD(index);
        }
        // distinct for mixed type will be calculated here
        all_stats_[index].is_categorical = IsCategorical(
                index, std::min(all_stats_[index].count - 1, 10 + all_stats_[index].count / 1000));
        all_stats_[index].type = this->col_data_[index].GetType().ToString().substr(1);
        AddProgress(percent_per_col);
    };

    if (threads_num_ > 1) {
        boost::asio::thread_pool pool(threads_num_);
        for (size_t i = 0; i < all_stats_.size(); ++i)
            boost::asio::post(pool, [i, task]() { return task(i); });
        pool.join();
    } else {
        for (size_t i = 0; i < all_stats_.size(); ++i) 
            task(i);
    }

    SetProgress(kTotalProgressPercent);
    auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);
    return elapsed_milliseconds.count();
}

size_t CsvStats::GetNumberOfColumns() const {
    return col_data_.size();
}

const ColumnStats& CsvStats::GetAllStats(size_t index) const {
    return all_stats_[index];
}

const std::vector<ColumnStats>& CsvStats::GetAllStats() const {
    return all_stats_;
}

const std::vector<model::TypedColumnData>& CsvStats::GetData() const noexcept {
    return col_data_;
}

std::string CsvStats::ToString() const {
    std::stringstream res;
    for (size_t i = 0; i < GetNumberOfColumns(); ++i) {
        res << "Column num = " << i << '\n';
        res << all_stats_[i].ToString() << '\n';
    }
    return res.str();
}

}  // namespace algos
