#pragma once

#include "tasks/TaskProvider.h"

namespace tasks {

// Todoist concrete implementation of TaskProvider. Talks to
// https://api.todoist.com/api/v1/tasks/filter with a Bearer token.
//
// Provider is a singleton accessed via instance(). Zero heap for the
// object itself; the only transient allocation is the 16KB response
// buffer inside fetch(), claimed lazily post-TLS handshake and freed
// after JSON parse.
class TodoistProvider final : public TaskProvider {
 public:
  static TodoistProvider& instance();

  const char* displayName() const override { return "Todoist"; }
  FetchResult fetch(const TasksFilter& filter,
                    std::vector<Task>& out) override;

 private:
  TodoistProvider() = default;
  TodoistProvider(const TodoistProvider&) = delete;
  TodoistProvider& operator=(const TodoistProvider&) = delete;
};

}  // namespace tasks
