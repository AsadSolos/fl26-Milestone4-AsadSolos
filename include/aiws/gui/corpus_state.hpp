#pragma once

#include "aiws/processing_core.hpp"

namespace aiws::gui {

class MainWindow;

//one corpus lifecycle state where each event is decided by the state object itself
class CorpusState
{
public:
    virtual ~CorpusState() = default;

    //the user asked for a build
    virtual void handle_build_request(MainWindow& window) = 0;

    //the workspace changed through add document or a remote add
    virtual void handle_workspace_change(MainWindow& window) = 0;

    //the worker finished a wanted build with a corpus
    virtual void handle_build_succeeded(MainWindow& window, ProcessingCore&& core) = 0;

    //the worker finished a wanted build that threw
    virtual void handle_build_failed(MainWindow& window) = 0;

    //the user pressed reset
    virtual void handle_reset(MainWindow& window) = 0;

    //true when search term inspector and context may use the corpus
    virtual bool allows_processing() const = 0;
};

//no corpus exists for the present workspace
class NotBuiltState final : public CorpusState
{
public:
    void handle_build_request(MainWindow& window) override;
    void handle_workspace_change(MainWindow& window) override;
    void handle_build_succeeded(MainWindow& window, ProcessingCore&& core) override;
    void handle_build_failed(MainWindow& window) override;
    void handle_reset(MainWindow& window) override;
    bool allows_processing() const override;
};

//the worker is building a corpus for the present workspace
class BuildingState final : public CorpusState
{
public:
    void handle_build_request(MainWindow& window) override;
    void handle_workspace_change(MainWindow& window) override;
    void handle_build_succeeded(MainWindow& window, ProcessingCore&& core) override;
    void handle_build_failed(MainWindow& window) override;
    void handle_reset(MainWindow& window) override;
    bool allows_processing() const override;
};

//a corpus exists and matches the present workspace
class CurrentState final : public CorpusState
{
public:
    void handle_build_request(MainWindow& window) override;
    void handle_workspace_change(MainWindow& window) override;
    void handle_build_succeeded(MainWindow& window, ProcessingCore&& core) override;
    void handle_build_failed(MainWindow& window) override;
    void handle_reset(MainWindow& window) override;
    bool allows_processing() const override;
};

}  // namespace aiws::gui
