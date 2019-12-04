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

#include "ortools/sat/linear_constraint_manager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"

namespace operations_research {
namespace sat {

namespace {

const LinearConstraintManager::ConstraintIndex kInvalidConstraintIndex(-1);

size_t ComputeHashOfTerms(const LinearConstraint& ct) {
  DCHECK(std::is_sorted(ct.vars.begin(), ct.vars.end()));
  size_t hash = 0;
  const int num_terms = ct.vars.size();
  for (int i = 0; i < num_terms; ++i) {
    hash = util_hash::Hash(ct.vars[i].value(), hash);
    hash = util_hash::Hash(ct.coeffs[i].value(), hash);
  }
  return hash;
}

// TODO(user): it would be better if LinearConstraint natively supported
// term and not two separated vectors. Fix?
void CanonicalizeConstraint(LinearConstraint* ct) {
  std::vector<std::pair<IntegerVariable, IntegerValue>> terms;

  const int size = ct->vars.size();
  for (int i = 0; i < size; ++i) {
    if (VariableIsPositive(ct->vars[i])) {
      terms.push_back({ct->vars[i], ct->coeffs[i]});
    } else {
      terms.push_back({NegationOf(ct->vars[i]), -ct->coeffs[i]});
    }
  }
  std::sort(terms.begin(), terms.end());

  ct->vars.clear();
  ct->coeffs.clear();
  for (const auto& term : terms) {
    ct->vars.push_back(term.first);
    ct->coeffs.push_back(term.second);
  }
}

}  // namespace

LinearConstraintManager::~LinearConstraintManager() {
  if (num_merged_constraints_ > 0) {
    VLOG(2) << "num_merged_constraints: " << num_merged_constraints_;
  }
  if (num_shortened_constraints_ > 0) {
    VLOG(2) << "num_shortened_constraints: " << num_shortened_constraints_;
  }
  if (num_splitted_constraints_ > 0) {
    VLOG(2) << "num_splitted_constraints: " << num_splitted_constraints_;
  }
  if (num_coeff_strenghtening_ > 0) {
    VLOG(2) << "num_coeff_strenghtening: " << num_coeff_strenghtening_;
  }
  for (const auto entry : type_to_num_cuts_) {
    VLOG(1) << "Added " << entry.second << " cuts of type '" << entry.first
            << "'.";
  }
}

bool LinearConstraintManager::MaybeRemoveSomeInactiveConstraints(
    glop::BasisState* solution_state) {
  if (solution_state->IsEmpty()) return false;  // Mainly to simplify tests.
  const glop::RowIndex num_rows(lp_constraints_.size());
  const glop::ColIndex num_cols =
      solution_state->statuses.size() - RowToColIndex(num_rows);

  int new_size = 0;
  for (int i = 0; i < num_rows; ++i) {
    const ConstraintIndex constraint_index = lp_constraints_[i];

    // Constraints that are not tight in the current solution have a basic
    // status. We remove the ones that have been inactive in the last recent
    // solves.
    //
    // TODO(user): More advanced heuristics might perform better, I didn't do
    // a lot of tuning experiments yet.
    const glop::VariableStatus row_status =
        solution_state->statuses[num_cols + glop::ColIndex(i)];
    if (row_status == glop::VariableStatus::BASIC) {
      constraint_infos_[constraint_index].inactive_count++;
      if (constraint_infos_[constraint_index].inactive_count >
          sat_parameters_.max_consecutive_inactive_count()) {
        constraint_infos_[constraint_index].is_in_lp = false;
        continue;  // Remove it.
      }
    } else {
      // Only count consecutive inactivities.
      constraint_infos_[constraint_index].inactive_count = 0;
    }

    lp_constraints_[new_size] = constraint_index;
    solution_state->statuses[num_cols + glop::ColIndex(new_size)] = row_status;
    new_size++;
  }
  const int num_removed_constraints = lp_constraints_.size() - new_size;
  lp_constraints_.resize(new_size);
  solution_state->statuses.resize(num_cols + glop::ColIndex(new_size));
  if (num_removed_constraints > 0) {
    VLOG(2) << "Removed " << num_removed_constraints << " constraints";
  }
  return num_removed_constraints > 0;
}

// Because sometimes we split a == constraint in two (>= and <=), it makes sense
// to detect duplicate constraints and merge bounds. This is also relevant if
// we regenerate identical cuts for some reason.
LinearConstraintManager::ConstraintIndex LinearConstraintManager::Add(
    LinearConstraint ct) {
  CHECK(!ct.vars.empty());
  SimplifyConstraint(&ct);
  DivideByGCD(&ct);
  CanonicalizeConstraint(&ct);
  DCHECK(DebugCheckConstraint(ct));

  // If an identical constraint exists, only updates its bound.
  const size_t key = ComputeHashOfTerms(ct);
  if (gtl::ContainsKey(equiv_constraints_, key)) {
    const ConstraintIndex ct_index = equiv_constraints_[key];
    if (constraint_infos_[ct_index].constraint.vars == ct.vars &&
        constraint_infos_[ct_index].constraint.coeffs == ct.coeffs) {
      if (ct.lb > constraint_infos_[ct_index].constraint.lb) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.lb = ct.lb;
      }
      if (ct.ub < constraint_infos_[ct_index].constraint.ub) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.ub = ct.ub;
      }
      ++num_merged_constraints_;
      return ct_index;
    }
  }

  const ConstraintIndex ct_index(constraint_infos_.size());
  ConstraintInfo ct_info;
  ct_info.constraint = std::move(ct);
  ct_info.l2_norm = ComputeL2Norm(ct_info.constraint);
  ct_info.is_in_lp = false;
  ct_info.is_cut = false;
  ct_info.objective_parallelism_computed = false;
  ct_info.objective_parallelism = 0.0;
  ct_info.inactive_count = 0;
  ct_info.permanently_removed = false;
  ct_info.hash = key;
  equiv_constraints_[key] = ct_index;

  constraint_infos_.push_back(std::move(ct_info));
  return ct_index;
}

void LinearConstraintManager::ComputeObjectiveParallelism(
    const ConstraintIndex ct_index) {
  CHECK(objective_is_defined_);
  // lazy computation of objective norm.
  if (!objective_norm_computed_) {
    double sum = 0.0;
    for (const double coeff : dense_objective_coeffs_) {
      sum += coeff * coeff;
    }
    objective_l2_norm_ = std::sqrt(sum);
    objective_norm_computed_ = true;
  }
  CHECK_GT(objective_l2_norm_, 0.0);

  constraint_infos_[ct_index].objective_parallelism_computed = true;
  if (constraint_infos_[ct_index].l2_norm == 0.0) {
    constraint_infos_[ct_index].objective_parallelism = 0.0;
    return;
  }

  const LinearConstraint& lc = constraint_infos_[ct_index].constraint;
  double unscaled_objective_parallelism = 0.0;
  for (int i = 0; i < lc.vars.size(); ++i) {
    const IntegerVariable var = lc.vars[i];
    DCHECK(VariableIsPositive(var));
    if (var < dense_objective_coeffs_.size()) {
      unscaled_objective_parallelism +=
          ToDouble(lc.coeffs[i]) * dense_objective_coeffs_[var];
    }
  }
  const double objective_parallelism =
      unscaled_objective_parallelism /
      (constraint_infos_[ct_index].l2_norm * objective_l2_norm_);
  constraint_infos_[ct_index].objective_parallelism =
      std::abs(objective_parallelism);
}

// Same as Add(), but logs some information about the newly added constraint.
// Cuts are also handled slightly differently than normal constraints.
void LinearConstraintManager::AddCut(
    LinearConstraint ct, std::string type_name,
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  if (ct.vars.empty()) return;

  const double activity = ComputeActivity(ct, lp_solution);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);

  // Only add cut with sufficient efficacy.
  if (violation / l2_norm < 1e-5) return;

  VLOG(1) << "Cut '" << type_name << "'"
          << " size=" << ct.vars.size()
          << " max_magnitude=" << ComputeInfinityNorm(ct) << " norm=" << l2_norm
          << " violation=" << violation << " eff=" << violation / l2_norm;

  // Add the constraint. We only mark the constraint as a cut if it is not an
  // update of an already existing one.
  const int64 prev_size = constraint_infos_.size();
  const ConstraintIndex ct_index = Add(std::move(ct));
  if (prev_size + 1 == constraint_infos_.size()) {
    num_cuts_++;
    type_to_num_cuts_[type_name]++;
    constraint_infos_[ct_index].is_cut = true;
  }
}

void LinearConstraintManager::SetObjectiveCoefficient(IntegerVariable var,
                                                      IntegerValue coeff) {
  if (coeff == IntegerValue(0)) return;
  objective_is_defined_ = true;
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    coeff = -coeff;
  }
  if (var.value() >= dense_objective_coeffs_.size()) {
    dense_objective_coeffs_.resize(var.value() + 1, 0.0);
  }
  dense_objective_coeffs_[var] = ToDouble(coeff);
}

bool LinearConstraintManager::SimplifyConstraint(LinearConstraint* ct) {
  bool term_changed = false;

  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  IntegerValue max_magnitude(0);
  int new_size = 0;
  const int num_terms = ct->vars.size();
  for (int i = 0; i < num_terms; ++i) {
    const IntegerVariable var = ct->vars[i];
    const IntegerValue coeff = ct->coeffs[i];
    const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);

    // For now we do not change ct, but just compute its new_size if we where
    // to remove a fixed term.
    if (lb == ub) continue;
    ++new_size;

    max_magnitude = std::max(max_magnitude, IntTypeAbs(coeff));
    if (coeff > 0.0) {
      min_sum += coeff * lb;
      max_sum += coeff * ub;
    } else {
      min_sum += coeff * ub;
      max_sum += coeff * lb;
    }
  }

  // Shorten the constraint if needed.
  if (new_size < num_terms) {
    term_changed = true;
    ++num_shortened_constraints_;
    new_size = 0;
    for (int i = 0; i < num_terms; ++i) {
      const IntegerVariable var = ct->vars[i];
      const IntegerValue coeff = ct->coeffs[i];
      const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
      const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
      if (lb == ub) {
        const IntegerValue rhs_adjust = lb * coeff;
        if (ct->lb > kMinIntegerValue) ct->lb -= rhs_adjust;
        if (ct->ub < kMaxIntegerValue) ct->ub -= rhs_adjust;
        continue;
      }
      ct->vars[new_size] = var;
      ct->coeffs[new_size] = coeff;
      ++new_size;
    }
    ct->vars.resize(new_size);
    ct->coeffs.resize(new_size);
  }

  // Relax the bound if needed, note that this doesn't require a change to
  // the equiv map.
  if (min_sum >= ct->lb) ct->lb = kMinIntegerValue;
  if (max_sum <= ct->ub) ct->ub = kMaxIntegerValue;

  // Clear constraints that are always true.
  // We rely on the deletion code to remove them eventually.
  if (ct->lb == kMinIntegerValue && ct->ub == kMaxIntegerValue) {
    ct->vars.clear();
    ct->coeffs.clear();
    return true;
  }

  // TODO(user): Split constraint in two if it is boxed and there is possible
  // reduction?
  //
  // TODO(user): Make sure there cannot be any overflow. They shouldn't, but
  // I am not sure all the generated cuts are safe regarding min/max sum
  // computation. We should check this.
  if (ct->ub != kMaxIntegerValue && max_magnitude > max_sum - ct->ub) {
    if (ct->lb != kMinIntegerValue) {
      ++num_splitted_constraints_;
    } else {
      term_changed = true;
      ++num_coeff_strenghtening_;
      const int num_terms = ct->vars.size();
      const IntegerValue target = max_sum - ct->ub;
      for (int i = 0; i < num_terms; ++i) {
        const IntegerValue coeff = ct->coeffs[i];
        if (coeff > target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
          ct->coeffs[i] = target;
          ct->ub -= (coeff - target) * ub;
        } else if (coeff < -target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
          ct->coeffs[i] = -target;
          ct->ub += (-target - coeff) * lb;
        }
      }
    }
  }

  if (ct->lb != kMinIntegerValue && max_magnitude > ct->lb - min_sum) {
    if (ct->ub != kMaxIntegerValue) {
      ++num_splitted_constraints_;
    } else {
      term_changed = true;
      ++num_coeff_strenghtening_;
      const int num_terms = ct->vars.size();
      const IntegerValue target = ct->lb - min_sum;
      for (int i = 0; i < num_terms; ++i) {
        const IntegerValue coeff = ct->coeffs[i];
        if (coeff > target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
          ct->coeffs[i] = target;
          ct->lb -= (coeff - target) * lb;
        } else if (coeff < -target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
          ct->coeffs[i] = -target;
          ct->lb += (-target - coeff) * ub;
        }
      }
    }
  }

  return term_changed;
}

bool LinearConstraintManager::ChangeLp(
    const gtl::ITIVector<IntegerVariable, double>& lp_solution,
    glop::BasisState* solution_state) {
  VLOG(3) << "Enter ChangeLP, scan " << constraint_infos_.size()
          << " constraints";
  std::vector<ConstraintIndex> new_constraints;
  std::vector<double> new_constraints_efficacies;
  std::vector<double> new_constraints_orthogonalities;

  const bool simplify_constraints =
      integer_trail_.num_level_zero_enqueues() > last_simplification_timestamp_;
  last_simplification_timestamp_ = integer_trail_.num_level_zero_enqueues();

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance".
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].permanently_removed) continue;

    // Inprocessing of the constraint.
    if (simplify_constraints &&
        SimplifyConstraint(&constraint_infos_[i].constraint)) {
      DivideByGCD(&constraint_infos_[i].constraint);
      DCHECK(DebugCheckConstraint(constraint_infos_[i].constraint));

      constraint_infos_[i].objective_parallelism_computed = false;
      constraint_infos_[i].l2_norm =
          ComputeL2Norm(constraint_infos_[i].constraint);

      if (constraint_infos_[i].is_in_lp) current_lp_is_changed_ = true;
      equiv_constraints_.erase(constraint_infos_[i].hash);
      constraint_infos_[i].hash =
          ComputeHashOfTerms(constraint_infos_[i].constraint);
      equiv_constraints_[constraint_infos_[i].hash] = i;
    }

    if (constraint_infos_[i].is_in_lp) continue;

    const double activity =
        ComputeActivity(constraint_infos_[i].constraint, lp_solution);
    const double lb_violation =
        ToDouble(constraint_infos_[i].constraint.lb) - activity;
    const double ub_violation =
        activity - ToDouble(constraint_infos_[i].constraint.ub);
    const double violation = std::max(lb_violation, ub_violation);
    if (violation >= tolerance) {
      constraint_infos_[i].inactive_count = 0;
      new_constraints.push_back(i);
      new_constraints_efficacies.push_back(violation /
                                           constraint_infos_[i].l2_norm);
      new_constraints_orthogonalities.push_back(1.0);

      if (objective_is_defined_ &&
          !constraint_infos_[i].objective_parallelism_computed) {
        ComputeObjectiveParallelism(i);
      } else if (!objective_is_defined_) {
        constraint_infos_[i].objective_parallelism = 0.0;
      }

      constraint_infos_[i].current_score =
          new_constraints_efficacies.back() +
          constraint_infos_[i].objective_parallelism;
    }
  }

  // Remove constraints from the current LP that have been inactive for a while.
  // We do that after we computed new_constraints so we do not need to iterate
  // over the just deleted constraints.
  if (MaybeRemoveSomeInactiveConstraints(solution_state)) {
    current_lp_is_changed_ = true;
  }

  // Note that the algo below is in O(limit * new_constraint). In order to
  // limit spending too much time on this, we first sort all the constraints
  // with an imprecise score (no orthogonality), then limit the size of the
  // vector of constraints to precisely score, then we do the actual scoring.
  //
  // On problem crossword_opt_grid-19.05_dict-80_sat with linearization_level=2,
  // new_constraint.size() > 1.5M.
  //
  // TODO(user): This blowup factor could be adaptative w.r.t. the constraint
  // limit.
  const int kBlowupFactor = 4;
  int constraint_limit = std::min(sat_parameters_.new_constraints_batch_size(),
                                  static_cast<int>(new_constraints.size()));
  if (lp_constraints_.empty()) {
    constraint_limit = std::min(1000, static_cast<int>(new_constraints.size()));
  }
  VLOG(3) << "   - size = " << new_constraints.size()
          << ", limit = " << constraint_limit;

  std::stable_sort(new_constraints.begin(), new_constraints.end(),
                   [&](ConstraintIndex a, ConstraintIndex b) {
                     return constraint_infos_[a].current_score >
                            constraint_infos_[b].current_score;
                   });
  if (new_constraints.size() > kBlowupFactor * constraint_limit) {
    VLOG(3) << "Resize candidate constraints from " << new_constraints.size()
            << " down to " << kBlowupFactor * constraint_limit;
    new_constraints.resize(kBlowupFactor * constraint_limit);
  }

  int num_added = 0;
  int num_skipped_checks = 0;
  const int kCheckFrequency = 100;
  ConstraintIndex last_added_candidate = kInvalidConstraintIndex;
  for (int i = 0; i < constraint_limit; ++i) {
    // Iterate through all new constraints and select the one with the best
    // score.
    double best_score = 0.0;
    ConstraintIndex best_candidate = kInvalidConstraintIndex;
    for (int j = 0; j < new_constraints.size(); ++j) {
      // Checks the time limit, and returns if the lp has changed.
      if (++num_skipped_checks >= kCheckFrequency) {
        if (time_limit_->LimitReached()) return current_lp_is_changed_;
        num_skipped_checks = 0;
      }

      const ConstraintIndex new_constraint = new_constraints[j];
      if (constraint_infos_[new_constraint].permanently_removed) continue;
      if (constraint_infos_[new_constraint].is_in_lp) continue;

      if (last_added_candidate != kInvalidConstraintIndex) {
        const double current_orthogonality =
            1.0 - (std::abs(ScalarProduct(
                       constraint_infos_[last_added_candidate].constraint,
                       constraint_infos_[new_constraint].constraint)) /
                   (constraint_infos_[last_added_candidate].l2_norm *
                    constraint_infos_[new_constraint].l2_norm));
        new_constraints_orthogonalities[j] =
            std::min(new_constraints_orthogonalities[j], current_orthogonality);
      }

      // NOTE(user): It is safe to permanently remove this constraint as the
      // constraint that is almost parallel to this constraint is present in the
      // LP or is inactive for a long time and is removed from the LP. In either
      // case, this constraint is not adding significant value and is only
      // making the LP larger.
      if (new_constraints_orthogonalities[j] <
          sat_parameters_.min_orthogonality_for_lp_constraints()) {
        constraint_infos_[new_constraint].permanently_removed = true;
        VLOG(2) << "Constraint permanently removed: " << new_constraint;
        continue;
      }

      // TODO(user): Experiment with different weights or different
      // functions for computing score.
      const double score = new_constraints_orthogonalities[j] +
                           constraint_infos_[new_constraint].current_score;
      CHECK_GE(score, 0.0);
      if (score > best_score || best_candidate == kInvalidConstraintIndex) {
        best_score = score;
        best_candidate = new_constraint;
      }
    }

    if (best_candidate != kInvalidConstraintIndex) {
      // Add the best constraint in the LP.
      constraint_infos_[best_candidate].is_in_lp = true;
      // Note that it is important for LP incremental solving that the old
      // constraints stays at the same position in this list (and thus in the
      // returned GetLp()).
      ++num_added;
      current_lp_is_changed_ = true;
      lp_constraints_.push_back(best_candidate);
      last_added_candidate = best_candidate;
    }
  }

  if (num_added > 0) {
    // We update the solution sate to match the new LP size.
    VLOG(2) << "Added " << num_added << " constraints.";
    solution_state->statuses.resize(solution_state->statuses.size() + num_added,
                                    glop::VariableStatus::BASIC);
  }

  // The LP changed only if we added new constraints or if some constraints
  // already inside changed (simplification or tighter bounds).
  if (current_lp_is_changed_) {
    current_lp_is_changed_ = false;
    return true;
  }
  return false;
}

void LinearConstraintManager::AddAllConstraintsToLp() {
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].is_in_lp) continue;
    constraint_infos_[i].is_in_lp = true;
    lp_constraints_.push_back(i);
  }
}

bool LinearConstraintManager::DebugCheckConstraint(
    const LinearConstraint& cut) {
  if (model_->Get<DebugSolution>() == nullptr) return true;
  const auto& debug_solution = *(model_->Get<DebugSolution>());
  if (debug_solution.empty()) return true;

  IntegerValue activity(0);
  for (int i = 0; i < cut.vars.size(); ++i) {
    const IntegerVariable var = cut.vars[i];
    const IntegerValue coeff = cut.coeffs[i];
    activity += coeff * debug_solution[var];
  }
  if (activity > cut.ub || activity < cut.lb) {
    LOG(INFO) << "activity " << activity << " not in [" << cut.lb << ","
              << cut.ub << "]";
    return false;
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
