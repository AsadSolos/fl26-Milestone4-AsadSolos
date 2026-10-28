#include "aiws/gui/search_result_model.hpp"

#include <QString>
#include <QVariant>

#include <utility>

namespace aiws::gui {

namespace {

//column identifiers in the m3 order rank document chunk score text
enum column
{
    column_rank = 0,
    column_document,
    column_chunk,
    column_score,
    column_text,
    column_count
};

}  // namespace

//constructs an empty model
SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

//returns the number of result rows or 0 for a valid parent index
int SearchResultModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(results_.size());
}

//returns the fixed number of result columns or 0 for a valid parent index
int SearchResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return column_count;
}

//returns display text for a result cell or an empty qvariant otherwise
QVariant SearchResultModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
    {
        return {};
    }
    if (index.row() < 0 || index.row() >= static_cast<int>(results_.size()))
    {
        return {};
    }

    const SearchResult& result = results_[static_cast<std::size_t>(index.row())];

    switch (index.column())
    {
        case column_rank:
            return index.row() + 1;
        case column_document:
            return QString::fromStdString(result.document_id);
        case column_chunk:
            return static_cast<qulonglong>(result.chunk_sequence);
        case column_score:
            return result.score;
        case column_text:
            return QString::fromStdString(result.text);
        default:
            return {};
    }
}

//returns the column header text or an empty qvariant otherwise
QVariant SearchResultModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }
    switch (section)
    {
        case column_rank:
            return QStringLiteral("Rank");
        case column_document:
            return QStringLiteral("Document");
        case column_chunk:
            return QStringLiteral("Chunk");
        case column_score:
            return QStringLiteral("Score");
        case column_text:
            return QStringLiteral("Text");
        default:
            return {};
    }
}

//replaces all results and notifies views of the reset
void SearchResultModel::set_results(std::vector<SearchResult> results)
{
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

//removes all results and notifies views of the reset
void SearchResultModel::clear()
{
    beginResetModel();
    results_.clear();
    endResetModel();
}

//returns the result at the given row or nullptr when out of range
const SearchResult* SearchResultModel::result_at(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(results_.size()))
    {
        return nullptr;
    }
    return &results_[static_cast<std::size_t>(row)];
}

}  // namespace aiws::gui
