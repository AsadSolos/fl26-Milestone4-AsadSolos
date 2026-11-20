#include "aiws/gui/corpus_state.hpp"
#include "aiws/gui/main_window.hpp"

#include <memory>
#include <utility>

namespace aiws::gui {

//a transition destroys the running state object so set_state is always the last statement of a handler

//not built starts the first build and moves to building
void NotBuiltState::handle_build_request(MainWindow& window)
{
    window.start_build();
    window.set_state(std::make_unique<BuildingState>());
}

//not built stays not built and only refreshes the display
void NotBuiltState::handle_workspace_change(MainWindow& window)
{
    window.show_not_built();
    window.update_action_states();
}

//no build is running so a late success is ignored
void NotBuiltState::handle_build_succeeded(MainWindow& window, ProcessingCore&& core)
{
    (void)window;
    (void)core;
}

//no build is running so a late failure is ignored
void NotBuiltState::handle_build_failed(MainWindow& window)
{
    (void)window;
}

//not built stays not built after a reset
void NotBuiltState::handle_reset(MainWindow& window)
{
    window.restore_startup();
    window.update_action_states();
}

//nothing may use a corpus that does not exist
bool NotBuiltState::allows_processing() const
{
    return false;
}

//a new request while building cancels the running build and starts a fresh one
void BuildingState::handle_build_request(MainWindow& window)
{
    window.cancel_build();
    window.start_build();
}

//the running build is useless so it is cancelled
void BuildingState::handle_workspace_change(MainWindow& window)
{
    window.cancel_build();
    window.show_not_built();
    window.set_state(std::make_unique<NotBuiltState>());
}

//the build finished so its corpus becomes current
void BuildingState::handle_build_succeeded(MainWindow& window, ProcessingCore&& core)
{
    window.install_core(std::move(core));
    window.set_state(std::make_unique<CurrentState>());
}

//the build threw so there is no corpus
void BuildingState::handle_build_failed(MainWindow& window)
{
    window.show_build_failed();
    window.set_state(std::make_unique<NotBuiltState>());
}

//the running build is useless so it is cancelled before the restore
void BuildingState::handle_reset(MainWindow& window)
{
    window.cancel_build();
    window.restore_startup();
    window.set_state(std::make_unique<NotBuiltState>());
}

//a corpus that is still being built may not be used
bool BuildingState::allows_processing() const
{
    return false;
}

//a rebuild of the present workspace moves to building
void CurrentState::handle_build_request(MainWindow& window)
{
    window.start_build();
    window.set_state(std::make_unique<BuildingState>());
}

//the corpus no longer matches the workspace
void CurrentState::handle_workspace_change(MainWindow& window)
{
    window.show_not_built();
    window.set_state(std::make_unique<NotBuiltState>());
}

//no build is running so a late success is ignored
void CurrentState::handle_build_succeeded(MainWindow& window, ProcessingCore&& core)
{
    (void)window;
    (void)core;
}

//no build is running so a late failure is ignored
void CurrentState::handle_build_failed(MainWindow& window)
{
    (void)window;
}

//the whole application returns to its startup state
void CurrentState::handle_reset(MainWindow& window)
{
    window.restore_startup();
    window.set_state(std::make_unique<NotBuiltState>());
}

//the corpus matches the workspace so it may be used
bool CurrentState::allows_processing() const
{
    return true;
}

}  // namespace aiws::gui
