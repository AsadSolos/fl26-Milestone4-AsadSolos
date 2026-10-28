#include "aiws/gui/context_widget.hpp"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <cstddef>

namespace aiws::gui {

//builds the context area and wires local signals
ContextWidget::ContextWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* context_group = new QGroupBox(QStringLiteral("Context"), this);
    auto* budget_label = new QLabel(QStringLiteral("Token budget:"), context_group);
    context_budget_slider_ = new QSlider(Qt::Horizontal, context_group);
    context_budget_slider_->setRange(50, 1000);
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);
    context_budget_slider_->setValue(300);
    budget_value_label_ = new QLabel(QStringLiteral("300"), context_group);
    build_context_button_ = new QPushButton(QStringLiteral("Build Context"), context_group);
    build_context_button_->setEnabled(false);

    auto* budget_row = new QHBoxLayout();
    budget_row->addWidget(budget_label);
    budget_row->addWidget(context_budget_slider_, 1);
    budget_row->addWidget(budget_value_label_);
    budget_row->addWidget(build_context_button_);

    context_text_ = new QPlainTextEdit(context_group);
    context_text_->setReadOnly(true);
    context_text_->setPlaceholderText(QStringLiteral("Built context will appear here."));

    auto* context_layout = new QVBoxLayout(context_group);
    context_layout->addLayout(budget_row);
    context_layout->addWidget(context_text_, 1);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->addWidget(context_group, 1);

    connect(context_budget_slider_, &QSlider::valueChanged,
            this, &ContextWidget::budget_changed);
    connect(build_context_button_, &QPushButton::clicked,
            this, &ContextWidget::request_context);
}

//returns the currently selected token budget
int ContextWidget::token_budget() const
{
    return context_budget_slider_->value();
}

//enables or disables the build context action
void ContextWidget::set_build_context_enabled(bool enabled)
{
    build_context_button_->setEnabled(enabled);
}

//replaces the context display with the supplied items
void ContextWidget::set_context(const std::vector<ContextItem>& items)
{
    QString display;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const ContextItem& item = items[i];
        if (i > 0)
        {
            display += QStringLiteral("\n\n");
        }
        display += QStringLiteral("Document: %1\n").arg(QString::fromStdString(item.document_id));
        display += QStringLiteral("Chunk: %1\n").arg(static_cast<qulonglong>(item.chunk_sequence));
        display += QStringLiteral("Tokens: %1\n").arg(static_cast<qulonglong>(item.token_count));
        display += QStringLiteral("Score: %1\n").arg(item.score);
        display += QStringLiteral("Truncated: %1\n").arg(item.truncated ? QStringLiteral("Yes") : QStringLiteral("No"));
        display += QStringLiteral("Text: %1").arg(QString::fromStdString(item.text));
    }
    context_text_->setPlainText(display);
}

//clears the displayed context
void ContextWidget::clear_context()
{
    context_text_->clear();
}

//restores the component to its initial state
void ContextWidget::reset()
{
    context_budget_slider_->setValue(300);
    budget_value_label_->setText(QStringLiteral("300"));
    clear_context();
    build_context_button_->setEnabled(false);
}

//emits a build context request with the current budget
void ContextWidget::request_context()
{
    emit build_context_requested(token_budget());
}

//updates the visible budget value without triggering processing
void ContextWidget::budget_changed(int value)
{
    budget_value_label_->setText(QString::number(value));
}

}  // namespace aiws::gui
