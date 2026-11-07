#include "aiws/gui/main_window.hpp"

#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QFont>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QMetaObject>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <algorithm>

namespace aiws::gui {

//builds the m3 window composes the four components and wires every signal
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AI Workspace"));
    resize(900, 900);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* header = new QFrame(central);
    header->setObjectName(QStringLiteral("header"));
    header->setStyleSheet(QStringLiteral("QFrame#header { background-color: #7a1f2b; }"));
    header->setAutoFillBackground(true);

    auto* header_title = new QLabel(QStringLiteral("AI Workspace"), header);
    QFont header_font = header_title->font();
    header_font.setPointSize(header_font.pointSize() + 6);
    header_font.setBold(true);
    header_title->setFont(header_font);
    header_title->setStyleSheet(QStringLiteral("color: white;"));

    auto* header_subtitle = new QLabel(QStringLiteral("Document Search and Context Explorer"), header);
    header_subtitle->setStyleSheet(QStringLiteral("color: white;"));

    auto* header_layout = new QVBoxLayout(header);
    header_layout->setContentsMargins(12, 8, 12, 8);
    header_layout->addWidget(header_title);
    header_layout->addWidget(header_subtitle);

    workspace_widget_ = new WorkspaceWidget(central);
    search_widget_ = new SearchWidget(central);
    term_inspector_widget_ = new TermInspectorWidget(central);
    context_widget_ = new ContextWidget(central);

    auto* lower_row = new QHBoxLayout();
    lower_row->addWidget(term_inspector_widget_, 1);
    lower_row->addWidget(context_widget_, 1);

    status_label_ = new QLabel(QStringLiteral("Current status: Ready"), central);

    reset_button_ = new QPushButton(QStringLiteral("Reset"), central);
    reset_button_->setStyleSheet(QStringLiteral("background-color: #e6b800; color: black;"));
    exit_button_ = new QPushButton(QStringLiteral("Exit"), central);
    exit_button_->setStyleSheet(QStringLiteral("background-color: #2b5d9b; color: white;"));

    auto* footer_row = new QHBoxLayout();
    footer_row->addWidget(status_label_, 1);
    footer_row->addWidget(reset_button_);
    footer_row->addWidget(exit_button_);

    auto* root_layout = new QVBoxLayout(central);
    root_layout->addWidget(header);
    root_layout->addWidget(workspace_widget_, 1);
    root_layout->addWidget(search_widget_, 1);
    root_layout->addLayout(lower_row);
    root_layout->addLayout(footer_row);

    connect(workspace_widget_, &WorkspaceWidget::add_document_requested,
            this, &MainWindow::add_document);
    connect(workspace_widget_, &WorkspaceWidget::build_corpus_requested,
            this, &MainWindow::build_corpus);
    connect(workspace_widget_, &WorkspaceWidget::document_selected,
            this, &MainWindow::document_selected);

    connect(search_widget_, &SearchWidget::search_requested,
            this, &MainWindow::search_requested);
    connect(search_widget_, &SearchWidget::query_changed,
            this, &MainWindow::query_changed);
    connect(search_widget_, &SearchWidget::source_document_requested,
            this, &MainWindow::source_document_requested);
    connect(search_widget_, &SearchWidget::selected_chunk_changed,
            term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);

    connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested,
            this, &MainWindow::term_inspection_requested);
    connect(context_widget_, &ContextWidget::build_context_requested,
            this, &MainWindow::build_context_requested);

    connect(reset_button_, &QPushButton::clicked,
            this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked,
            this, &MainWindow::exit_application);

    update_action_states();
}

//destructor ensures background thread is joined before destruction
MainWindow::~MainWindow()
{
    cancel_requested_ = true;
    if (build_thread_.joinable())
    {
        build_thread_.join();
    }
}

//stores the exit callback function pointer
void MainWindow::set_exit_callback(ExitCallback callback) noexcept
{
    exit_callback_ = callback;
}

//adds the file at path exactly as the add document action does
bool MainWindow::add_document_file(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }
    QTextStream stream(&file);
    const QString contents = stream.readAll();
    file.close();

    const QFileInfo info(path);
    const QString title = info.fileName();
    const QString absolute = info.absoluteFilePath();

    workspace_.add_document(Document(absolute.toStdString(), title.toStdString(), contents.toStdString()));
    workspace_widget_->add_document(title);
    workspace_widget_->select_document(static_cast<int>(workspace_.documents().size()) - 1);

    invalidate_corpus();
    status_label_->setText(QStringLiteral("Current status: Document added"));
    update_action_states();
    return true;
}

//sets the extra build time per document for builds started after this call
void MainWindow::set_build_delay_ms(int ms_per_document)
{
    qDebug() << "[set_build_delay_ms] received:" << ms_per_document;
    build_delay_ms_ = ms_per_document;
}

//opens a file dialog loads the chosen document and invalidates the corpus
void MainWindow::add_document()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Add Document"), QString(),
        QStringLiteral("Text Files (*.txt);;All Files (*)"));
    if (path.isEmpty())
    {
        return;
    }

    if (!add_document_file(path))
    {
        status_label_->setText(QStringLiteral("Current status: Failed to open file"));
    }
}

//rebuilds the corpus clears old output and shows the chunk count
void MainWindow::build_corpus()
{
    if (build_thread_.joinable())
    {
        cancel_requested_ = true;
        build_thread_.join();
        cancel_requested_ = false;
    }

    const int current_gen = ++build_generation_;
    corpus_current_ = false;
    clear_processing_outputs();
    workspace_widget_->set_corpus_status(QStringLiteral("Corpus: Building..."));
    update_action_states();

    Workspace snapshot = workspace_;
    build_thread_ = std::thread(&MainWindow::run_background_build, this, std::move(snapshot), current_gen);
}

//shows the selected workspace document title and text
void MainWindow::document_selected(int row)
{
    if (row < 0 || row >= static_cast<int>(workspace_.documents().size()))
    {
        workspace_widget_->clear_selected_document();
        return;
    }
    const Document& doc = workspace_.documents()[static_cast<std::size_t>(row)];
    workspace_widget_->show_selected_document(QString::fromStdString(doc.title()),
                                              QString::fromStdString(doc.text()));
}

//runs a search on the current corpus and shows the results
void MainWindow::search_requested(const QString& query, int k)
{
    if (!corpus_current_)
    {
        return;
    }
    std::vector<SearchResult> results =
        processing_core_.search(query.toStdString(), k);
    search_widget_->set_results(std::move(results));
    context_widget_->clear_context();
    status_label_->setText(QStringLiteral("Current status: Search complete"));
    update_action_states();
}

//updates action availability after a query edit without searching
void MainWindow::query_changed(const QString& query)
{
    (void)query;
    update_action_states();
}

//builds context for the current query k and budget
void MainWindow::build_context_requested(int token_budget)
{
    if (!corpus_current_)
    {
        return;
    }
    std::vector<ContextItem> items = processing_core_.build_context(
        search_widget_->query().toStdString(),
        search_widget_->result_count(),
        static_cast<std::size_t>(token_budget));
    context_widget_->set_context(items);
    status_label_->setText(QStringLiteral("Current status: Context built"));
    update_action_states();
}

//selects and shows the workspace document for a search result
void MainWindow::source_document_requested(const QString& document_id)
{
    const std::string id = document_id.toStdString();
    const std::vector<Document>& docs = workspace_.documents();
    for (std::size_t i = 0; i < docs.size(); ++i)
    {
        if (docs[i].id() == id)
        {
            workspace_widget_->select_document(static_cast<int>(i));
            document_selected(static_cast<int>(i));
            return;
        }
    }
}

//queries df and optional chunk tf and shows the results
void MainWindow::term_inspection_requested(const QString& term,
                                           const QString& chunk_id)
{
    qDebug() << "[term_inspection_requested] term" << term
             << "chunk_id" << chunk_id
             << "corpus_current_" << corpus_current_;
    if (!corpus_current_)
    {
        return;
    }
    const std::string normalized = ProcessingCore::normalize(term.toStdString());
    if (normalized.empty())
    {
        return;
    }
    if (normalized.find(' ') != std::string::npos)
    {
        status_label_->setText(QStringLiteral("Current status: Term must be a single token"));
        return;
    }

    const std::size_t df = processing_core_.document_frequency(normalized);
    bool has_tf = false;
    std::size_t tf = 0;
    if (!chunk_id.isEmpty())
    {
        tf = processing_core_.term_frequency(normalized, chunk_id.toStdString());
        has_tf = true;
    }
    qDebug() << "[term_inspection_requested] df" << df
             << "has_tf" << has_tf << "tf" << tf;
    term_inspector_widget_->set_statistics(df, has_tf, tf);
    status_label_->setText(QStringLiteral("Current status: Term inspected"));
}

//restores the whole application to the startup state
void MainWindow::reset_application()
{
    if (build_thread_.joinable())
    {
        cancel_requested_ = true;
        build_thread_.join();
        cancel_requested_ = false;
    }

    workspace_.clear();
    corpus_current_ = false;
    ++build_generation_;

    Resettable* parts[] =
    {
        workspace_widget_,
        search_widget_,
        term_inspector_widget_,
        context_widget_
    };
    for (Resettable* part : parts)
    {
        part->reset();
    }

    workspace_widget_->set_corpus_status(QStringLiteral("Corpus: Not built"));
    status_label_->setText(QStringLiteral("Current status: Application reset"));
    update_action_states();
}

//invokes the registered exit callback if present
void MainWindow::exit_application()
{
    if (exit_callback_ != nullptr)
    {
        exit_callback_(this);
    }
}

//recomputes enabled states based on corpus and input
void MainWindow::update_action_states()
{
    const bool has_documents = !workspace_.documents().empty();
    workspace_widget_->set_build_corpus_enabled(has_documents);

    const bool can_process = corpus_current_ && search_widget_->has_query();
    search_widget_->set_search_enabled(can_process);
    context_widget_->set_build_context_enabled(can_process);
    term_inspector_widget_->set_inspection_enabled(corpus_current_);
}

//marks the corpus stale and clears processing dependent output
void MainWindow::invalidate_corpus()
{
    corpus_current_ = false;
    ++build_generation_;
    clear_processing_outputs();
    workspace_widget_->set_corpus_status(QStringLiteral("Corpus: Not built"));
}

//clears search selection statistics and context displays
void MainWindow::clear_processing_outputs()
{
    search_widget_->clear_results();
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context();
}

//worker function that runs on the background thread to build the corpus
void MainWindow::run_background_build(Workspace snapshot, int generation)
{
    qDebug() << "[worker] starting build";
    bool success = true;
    std::size_t chunk_count = 0;

    try
    {
        if (build_delay_ms_ > 0)
        {
            const long long total_delay = static_cast<long long>(build_delay_ms_) * static_cast<long long>(snapshot.documents().size());
            
            //sleep in 50ms increments so we can check cancel_requested_ regularly
            const long long sleep_chunk = 50;
            long long slept = 0;
            while (slept < total_delay)
            {
                if (cancel_requested_)
                {
                    break;
                }
                const long long this_sleep = std::min(sleep_chunk, total_delay - slept);
                std::this_thread::sleep_for(std::chrono::milliseconds(this_sleep));
                slept += this_sleep;
            }
        }
        
        if (!cancel_requested_)
        {
            processing_core_.rebuild(snapshot);
        }
        chunk_count = processing_core_.chunk_count();
    }
    catch (const std::exception&)
    {
        success = false;
    }

    QMetaObject::invokeMethod(this, [this, success, chunk_count, generation]()
    {
        on_build_finished(success, chunk_count, generation);
    }, Qt::QueuedConnection);
}

//called on the gui thread when the background build finishes
void MainWindow::on_build_finished(bool success, std::size_t chunk_count, int generation)
{
#ifdef AIWS_FAULT_LOSE_FAILURE
    success = true;
#endif

#ifndef AIWS_FAULT_ACCEPT_STALE
    if (generation != build_generation_ || cancel_requested_)
    {
        return;
    }
#endif

    if (success)
    {
        corpus_current_ = true;
        workspace_widget_->set_corpus_status(QStringLiteral("Corpus: %1 chunks").arg(static_cast<qulonglong>(chunk_count)));
        status_label_->setText(QStringLiteral("Current status: Corpus built"));
    }
    else
    {
        corpus_current_ = false;
        workspace_widget_->set_corpus_status(QStringLiteral("Corpus: Not built"));
        status_label_->setText(QStringLiteral("Current status: Corpus rebuild failed"));
    }
    update_action_states();
}

}  // namespace aiws::gui
