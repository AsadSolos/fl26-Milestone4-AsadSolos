#include "aiws/gui/search_widget.hpp"

#include "aiws/gui/search_result_model.hpp"

#include <QAbstractItemView>
#include <QDebug>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <utility>

namespace aiws::gui {

//builds the query results and selected result areas and wires local signals
SearchWidget::SearchWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* query_group = new QGroupBox(QStringLiteral("Query"), this);
    query_edit_ = new QLineEdit(query_group);
    query_edit_->setPlaceholderText(QStringLiteral("Search the current corpus"));
    auto* results_label = new QLabel(QStringLiteral("Results:"), query_group);
    result_count_spin_ = new QSpinBox(query_group);
    result_count_spin_->setRange(1, 100);
    result_count_spin_->setValue(3);
    search_button_ = new QPushButton(QStringLiteral("Search"), query_group);
    search_button_->setEnabled(false);

    auto* query_layout = new QHBoxLayout(query_group);
    query_layout->addWidget(query_edit_, 1);
    query_layout->addWidget(results_label);
    query_layout->addWidget(result_count_spin_);
    query_layout->addWidget(search_button_);

    result_model_ = new SearchResultModel(this);
    results_view_ = new QTableView(this);
    results_view_->setModel(result_model_);
    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_view_->horizontalHeader()->setStretchLastSection(true);

    auto* selected_group = new QGroupBox(QStringLiteral("Selected Result"), this);
    view_source_button_ = new QPushButton(QStringLiteral("View Source"), selected_group);
    view_source_button_->setEnabled(false);
    selected_result_text_ = new QPlainTextEdit(selected_group);
    selected_result_text_->setReadOnly(true);
    selected_result_text_->setPlaceholderText(QStringLiteral("Select a result to inspect its details."));

    auto* selected_header = new QHBoxLayout();
    auto* selected_title = new QLabel(QStringLiteral("Selected Result:"), selected_group);
    QFont title_font = selected_title->font();
    title_font.setBold(true);
    selected_title->setFont(title_font);
    selected_header->addWidget(selected_title);
    selected_header->addStretch();
    selected_header->addWidget(view_source_button_);

    auto* selected_layout = new QVBoxLayout(selected_group);
    selected_layout->addLayout(selected_header);
    selected_layout->addWidget(selected_result_text_, 1);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->addWidget(query_group);
    root_layout->addWidget(results_view_, 1);
    root_layout->addWidget(selected_group);

    connect(search_button_, &QPushButton::clicked,
            this, &SearchWidget::request_search);
    connect(query_edit_, &QLineEdit::textChanged,
            this, &SearchWidget::query_changed);
    connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SearchWidget::show_selected_result);
    connect(view_source_button_, &QPushButton::clicked,
            this, &SearchWidget::request_source_document);

    clear_results();
}

//returns the query text exactly as entered
QString SearchWidget::query() const
{
    return query_edit_->text();
}

//returns the requested maximum number of results
int SearchWidget::result_count() const
{
    return result_count_spin_->value();
}

//returns true when the query has non whitespace text
bool SearchWidget::has_query() const
{
    return !query().trimmed().isEmpty();
}

//enables or disables the search action
void SearchWidget::set_search_enabled(bool enabled)
{
    search_button_->setEnabled(enabled);
}

//replaces the model contents with new ranked results
void SearchWidget::set_results(std::vector<SearchResult> results)
{
    result_model_->set_results(std::move(results));
    results_view_->clearSelection();
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);
    emit selected_chunk_changed(QString());
}

//clears results and all result dependent presentation state
void SearchWidget::clear_results()
{
    result_model_->clear();
    results_view_->clearSelection();
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);
    emit selected_chunk_changed(QString());
}

//restores the component to its initial state
void SearchWidget::reset()
{
    query_edit_->clear();
    result_count_spin_->setValue(3);
    clear_results();
    search_button_->setEnabled(false);
}

//emits a search request using the current query and k
void SearchWidget::request_search()
{
    emit search_requested(query(), result_count());
}

//updates the selected result details from the current table row
void SearchWidget::show_selected_result()
{
    const QModelIndexList rows = results_view_->selectionModel()->selectedRows();
    if (rows.isEmpty())
    {
        qDebug() << "[show_selected_result] no rows selected";
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    const int row = rows.first().row();
    const SearchResult* result = result_model_->result_at(row);
    if (result == nullptr)
    {
        qDebug() << "[show_selected_result] result_at" << row << "is null";
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    QString details;
    details += QStringLiteral("Document: %1\n").arg(QString::fromStdString(result->document_id));
    details += QStringLiteral("Chunk: %1\n").arg(static_cast<qulonglong>(result->chunk_sequence));
    details += QStringLiteral("Score: %1\n").arg(result->score);
    details += QStringLiteral("Matched Terms: %1\n\n").arg(static_cast<qulonglong>(result->matched_terms));
    details += QString::fromStdString(result->text);

    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true);

    qDebug() << "[show_selected_result] emitting chunk id" << QString::fromStdString(result->chunk_id);
    emit selected_chunk_changed(QString::fromStdString(result->chunk_id));
}

//emits a source document request for the selected result
void SearchWidget::request_source_document()
{
    const QModelIndexList rows = results_view_->selectionModel()->selectedRows();
    if (rows.isEmpty())
    {
        return;
    }
    const SearchResult* result = result_model_->result_at(rows.first().row());
    if (result == nullptr)
    {
        return;
    }
    emit source_document_requested(QString::fromStdString(result->document_id));
}

}  // namespace aiws::gui
