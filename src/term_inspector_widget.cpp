#include "aiws/gui/term_inspector_widget.hpp"

#include <QDebug>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstddef>

namespace aiws::gui {

//builds the term inspector area and wires local signals
TermInspectorWidget::TermInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* inspector_group = new QGroupBox(QStringLiteral("Term Inspector"), this);
    auto* term_label = new QLabel(QStringLiteral("Term:"), inspector_group);
    term_edit_ = new QLineEdit(inspector_group);
    term_edit_->setPlaceholderText(QStringLiteral("Enter a term"));
    inspect_button_ = new QPushButton(QStringLiteral("Inspect"), inspector_group);
    inspect_button_->setEnabled(false);

    auto* term_row = new QHBoxLayout();
    term_row->addWidget(term_label);
    term_row->addWidget(term_edit_, 1);
    term_row->addWidget(inspect_button_);

    auto* df_label = new QLabel(QStringLiteral("Document Frequency:"), inspector_group);
    QFont df_font = df_label->font();
    df_font.setBold(true);
    df_label->setFont(df_font);
    document_frequency_value_ = new QLabel(QStringLiteral("-"), inspector_group);

    auto* tf_label = new QLabel(QStringLiteral("Selected Chunk TF:"), inspector_group);
    QFont tf_font = tf_label->font();
    tf_font.setBold(true);
    tf_label->setFont(tf_font);
    chunk_frequency_value_ = new QLabel(QStringLiteral("-"), inspector_group);

    auto* stats_row = new QHBoxLayout();
    stats_row->addWidget(df_label);
    stats_row->addWidget(document_frequency_value_);
    stats_row->addSpacing(24);
    stats_row->addWidget(tf_label);
    stats_row->addWidget(chunk_frequency_value_);
    stats_row->addStretch();

    auto* inspector_layout = new QVBoxLayout(inspector_group);
    inspector_layout->addLayout(term_row);
    inspector_layout->addLayout(stats_row);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->addWidget(inspector_group, 1);

    connect(inspect_button_, &QPushButton::clicked,
            this, &TermInspectorWidget::request_inspection);
}

//enables or disables the inspect action
void TermInspectorWidget::set_inspection_enabled(bool enabled)
{
    inspect_button_->setEnabled(enabled);
}

//records the chunk associated with the current result selection
void TermInspectorWidget::set_selected_chunk(const QString& chunk_id)
{
    qDebug() << "[set_selected_chunk] received" << chunk_id;
    selected_chunk_id_ = chunk_id;
}

//displays document and chunk frequency statistics
void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency)
{
    document_frequency_value_->setText(QString::number(static_cast<qulonglong>(document_frequency)));
    if (has_chunk_frequency)
    {
        chunk_frequency_value_->setText(QString::number(static_cast<qulonglong>(chunk_frequency)));
    }
    else
    {
        chunk_frequency_value_->setText(QStringLiteral("-"));
    }
}

//clears the displayed statistics
void TermInspectorWidget::clear_statistics()
{
    document_frequency_value_->setText(QStringLiteral("-"));
    chunk_frequency_value_->setText(QStringLiteral("-"));
}

//restores the component to its initial state
void TermInspectorWidget::reset()
{
    term_edit_->clear();
    selected_chunk_id_.clear();
    clear_statistics();
    inspect_button_->setEnabled(false);
}

//emits an inspection request for the current term and selected chunk
void TermInspectorWidget::request_inspection()
{
    qDebug() << "[request_inspection] term" << term_edit_->text()
             << "selected_chunk_id_" << selected_chunk_id_;
    emit term_inspection_requested(term_edit_->text(), selected_chunk_id_);
}

}  // namespace aiws::gui
