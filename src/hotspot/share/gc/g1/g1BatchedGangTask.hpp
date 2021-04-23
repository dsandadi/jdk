/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_G1_G1BATCHEDGANGTASK_HPP
#define SHARE_GC_G1_G1BATCHEDGANGTASK_HPP

#include "gc/g1/g1GCPhaseTimes.hpp"
#include "gc/shared/workgroup.hpp"
#include "memory/allocation.hpp"

template <typename E, MEMFLAGS F>
class GrowableArrayCHeap;

// G1AbstractSubTask represents a task to be performed either within an
// G1BatchedGangTask running on a single thread ("serially") or multiple threads
// ("in parallel"). A G1AbstractSubTask is always associated with a phase tag
// that is used to automatically store timing information.
//
// See G1BatchedGangTask for information on execution.
class G1AbstractSubTask : public CHeapObj<mtGC> {
  G1GCPhaseTimes::GCParPhases _tag;

  NONCOPYABLE(G1AbstractSubTask);

protected:
  // Record work item for this tag in G1GCPhaseTimes.
  void record_work_item(uint worker_id, uint index, size_t count);

public:
  G1AbstractSubTask(G1GCPhaseTimes::GCParPhases tag) : _tag(tag) { }
  virtual ~G1AbstractSubTask() { }

  // How many worker (threads) would this task be able to keep busy for at least
  // as long as to amortize worker startup costs.
  // Called by G1BatchedGangTask to determine total number of workers.
  virtual double num_busy_workers() const { return 1.0; }

  // Called by G1BatchedGangTask to provide information about the the maximum
  // number of workers for all subtasks after it has been determined.
  virtual void set_max_workers(uint max_workers) { }

  // Perform the actual work.
  virtual void do_work(uint worker_id) = 0;

  // Tag for this G1AbstractSubTask.
  G1GCPhaseTimes::GCParPhases tag() const { return _tag; }
  // Human readable name.
  const char* name() const;
};

// G1BatchedGangTask runs a set of G1AbstractSubTask using a work gang.
//
// Subclasses of this class add their G1AbstractSubTasks into either the list
// of "serial" or the list of "parallel" tasks.
// During execution in the work gang, this class will make sure that the "serial"
// tasks are executed by a single thread only exactly once, while "parallel"
// tasks may be executed by different workers, at most once per given worker id.
//
// The G1AbstractSubTask's do_work() method gets a unique worker_id each time the
// method is called.
// There is no guarantee that G1AbstractSubTasks::do_work() of different tasks
// are actually run in parallel.
//
// The current framework assumes that constructors and destructors of the
// G1AbstractSubTasks are executed in the constructor/destructor of an instance
// of this class.
//
// The constructor, destructor and the do_work() methods from different
// G1AbstractSubTasks may run in any order so they must not have any
// dependencies at all.
//
// For a given G1AbstractSubTask T call order of its methods are as follows:
//
// 1) T()
// 2) T::thread_usage()
// 3) T::set_max_workers()
// 4) T::do_work()  // potentially in parallel with any other registered G1AbstractSubTask
// 5) ~T()
//
class G1BatchedGangTask : public AbstractGangTask {
  volatile int _num_serial_tasks_done;
  G1GCPhaseTimes* _phase_times;

  bool try_claim_serial_task(int& task);

  NONCOPYABLE(G1BatchedGangTask);

protected:
  GrowableArrayCHeap<G1AbstractSubTask*, mtGC> _serial_tasks;
  GrowableArrayCHeap<G1AbstractSubTask*, mtGC> _parallel_tasks;

  G1BatchedGangTask(const char* name, G1GCPhaseTimes* phase_times);

public:
  void work(uint worker_id) override;

  // How many workers can this gang task keep busy and could be started.
  uint num_busy_workers() const;
  // Informs the G1AbstractSubTasks about that we will start execution with the
  // given number of workers.
  void set_max_workers(uint max_workers);

  ~G1BatchedGangTask();
};

#endif // SHARE_GC_G1_G1BATCHEDGANGTASK_HPP