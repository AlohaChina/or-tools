// Copyright 2010-2018 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.ortools.sat;

import com.google.ortools.sat.ConstraintProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.IntervalConstraintProto;

/** An interval variable. This class must be constructed from the CpModel class. */
public class IntervalVar {
  IntervalVar(
      CpModelProto.Builder builder, int startIndex, int sizeIndex, int endIndex, String name) {
    this.modelBuilder = builder;
    this.constraintIndex = modelBuilder.getConstraintsCount();
    ConstraintProto.Builder ct = modelBuilder.addConstraintsBuilder();
    ct.setName(name);
    this.intervalBuilder = ct.getIntervalBuilder();
    this.intervalBuilder.setStart(startIndex);
    this.intervalBuilder.setSize(sizeIndex);
    this.intervalBuilder.setEnd(endIndex);
  }

  IntervalVar(CpModelProto.Builder builder, int startIndex, int sizeIndex, int endIndex,
      int isPresentIndex, String name) {
    this.modelBuilder = builder;
    this.constraintIndex = modelBuilder.getConstraintsCount();
    ConstraintProto.Builder ct = modelBuilder.addConstraintsBuilder();
    ct.setName(name);
    ct.addEnforcementLiteral(isPresentIndex);
    this.intervalBuilder = ct.getIntervalBuilder();
    this.intervalBuilder.setStart(startIndex);
    this.intervalBuilder.setSize(sizeIndex);
    this.intervalBuilder.setEnd(endIndex);
  }

  @Override
  public String toString() {
    return modelBuilder.getConstraints(constraintIndex).toString();
  }

  int getIndex() {
    return constraintIndex;
  }

  /** Returns the name passed in the constructor. */
  public String getName() {
    return modelBuilder.getConstraints(constraintIndex).getName();
  }

  private final CpModelProto.Builder modelBuilder;
  private final int constraintIndex;
  private final IntervalConstraintProto.Builder intervalBuilder;
}
