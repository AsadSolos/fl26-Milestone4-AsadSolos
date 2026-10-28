#include "aiws/gui/workspace_widget.hpp"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

namespace aiws::gui {

//builds the workspace and selected document areas and wires local signals
WorkspaceWidget::WorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* workspace_group = new QGroupBox(QStringLiteral("Workspace"), this);
    auto* workspace_buttons = new QHBoxLayout();
    add_document_button_ = new QPushButton(QStringLiteral("Add Document"), workspace_group);
    build_corpus_button_ = new QPushButton(QStringLiteral("Build Corpus"), workspace_group);
    build_corpus_button_->setEnabled(false);
    workspace_buttons->addWidget(add_document_button_);
    workspace_buttons->addWidget(build_corpus_button_);
    workspace_buttons->addStretch();

    document_list_ = new QListWidget(workspace_group);
    corpus_status_label_ = new QLabel(workspace_group);
    corpus_status_label_->setText(QStringLiteral("Corpus: Not built"));

    auto* workspace_layout = new QVBoxLayout(workspace_group);
    workspace_layout->addLayout(workspace_buttons);
    workspace_layout->addWidget(document_list_, 1);
    workspace_layout->addWidget(corpus_status_label_);

    auto* selected_group = new QGroupBox(QStringLiteral("Selected Document"), this);
    document_title_label_ = new QLabel(selected_group);
    document_title_label_->setText(QStringLiteral("-"));
    QFont title_font = document_title_label_->font();
    title_font.setBold(true);
    document_title_label_->setFont(title_font);

    document_text_ = new QPlainTextEdit(selected_group);
    document_text_->setReadOnly(true);
    document_text_->setPlaceholderText(QStringLiteral("Select a document to view its contents."));

    auto* selected_layout = new QVBoxLayout(selected_group);
    selected_layout->addWidget(document_title_label_);
    selected_layout->addWidget(document_text_, 1);

    auto* top_row = new QHBoxLayout();
    top_row->addWidget(workspace_group, 1);
    top_row->addWidget(selected_group, 1);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->addLayout(top_row, 1);

    connect(add_document_button_, &QPushButton::clicked,
            this, &WorkspaceWidget::add_document_requested);
    connect(build_corpus_button_, &QPushButton::clicked,
            this, &WorkspaceWidget::build_corpus_requested);
    connect(document_list_, &QListWidget::currentRowChanged,
            this, &WorkspaceWidget::document_selected);

    clear_selected_document();
}

//appends a document title to the workspace list
void WorkspaceWidget::add_document(const QString& title)
{
    document_list_->addItem(title);
}

//selects the requested document row without backend work
void WorkspaceWidget::select_document(int row)
{
    document_list_->setCurrentRow(row);
}

//shows the selected document title and text
void WorkspaceWidget::show_selected_document(const QString& title,
                                             const QString& text)
{
    document_title_label_->setText(title);
    document_text_->setPlainText(text);
}

//clears the selected document display
void WorkspaceWidget::clear_selected_document()
{
    document_title_label_->setText(QStringLiteral("-"));
    document_text_->clear();
}

//updates the corpus status text
void WorkspaceWidget::set_corpus_status(const QString& text)
{
    corpus_status_label_->setText(text);
}

//enables or disables the build corpus action
void WorkspaceWidget::set_build_corpus_enabled(bool enabled)
{
    build_corpus_button_->setEnabled(enabled);
}

//restores the workspace component to its initial state
void WorkspaceWidget::reset()
{
    document_list_->clear();
    clear_selected_document();
    corpus_status_label_->setText(QStringLiteral("Corpus: Not built"));
    build_corpus_button_->setEnabled(false);
    add_document_button_->setEnabled(true);
}

}  // namespace aiws::gui
