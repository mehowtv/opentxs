// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::api::implementation
{
class Periodic : virtual public api::Periodic
{
public:
    bool Cancel(const int task) const final;
    bool Reschedule(const int task, const std::chrono::seconds& interval)
        const final;
    int Schedule(
        const std::chrono::seconds& interval,
        const PeriodicTask& task,
        const std::chrono::seconds& last) const final;

    ~Periodic() override;

protected:
    Flag& running_;

    void Shutdown();

    Periodic(Flag& running);

private:
    /** Last performed, Interval, Task */
    using TaskItem = std::tuple<Time, std::chrono::seconds, PeriodicTask>;
    using TaskList = std::map<int, TaskItem>;

    mutable std::atomic<int> next_id_;
    mutable std::mutex periodic_lock_;
    mutable TaskList periodic_task_list_;
    std::thread periodic_;

    Periodic() = delete;
    Periodic(const Periodic&) = delete;
    Periodic(Periodic&&) = delete;
    Periodic& operator=(const Periodic&) = delete;
    Periodic& operator=(Periodic&&) = delete;

    void thread();
};
}  // namespace opentxs::api::implementation
