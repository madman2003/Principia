﻿
#pragma once

#include <vector>

#include "base/not_null.hpp"
#include "geometry/named_quantities.hpp"
#include "integrators/ordinary_differential_equations.hpp"
#include "ksp_plugin/burn.hpp"
#include "ksp_plugin/frames.hpp"
#include "ksp_plugin/manœuvre.hpp"
#include "physics/degrees_of_freedom.hpp"
#include "physics/discrete_trajectory.hpp"
#include "physics/ephemeris.hpp"
#include "quantities/named_quantities.hpp"
#include "quantities/quantities.hpp"
#include "serialization/ksp_plugin.pb.h"

namespace principia {
namespace ksp_plugin {
namespace internal_flight_plan {

using base::not_null;
using geometry::Instant;
using integrators::AdaptiveStepSizeIntegrator;
using physics::DegreesOfFreedom;
using physics::DiscreteTrajectory;
using physics::Ephemeris;
using quantities::Length;
using quantities::Mass;
using quantities::Speed;

// A stack of |Burn|s that manages a chain of trajectories obtained by executing
// the corresponding |NavigationManœuvre|s.
class FlightPlan {
 public:
  // Creates a |FlightPlan| with no burns starting at |initial_time| with
  // |initial_degrees_of_freedom| and with the given |initial_mass|.  The
  // trajectories are computed using the given |integrator| in the given
  // |ephemeris|.
  FlightPlan(Mass const& initial_mass,
             Instant const& initial_time,
             DegreesOfFreedom<Barycentric> const& initial_degrees_of_freedom,
             Instant const& desired_final_time,
             not_null<Ephemeris<Barycentric>*> ephemeris,
             Ephemeris<Barycentric>::AdaptiveStepParameters const&
                 adaptive_step_parameters,
             Ephemeris<Barycentric>::GeneralizedAdaptiveStepParameters const&
                 generalized_adaptive_step_parameters);
  virtual ~FlightPlan() = default;

  virtual Instant initial_time() const;
  virtual Instant actual_final_time() const;
  virtual Instant desired_final_time() const;

  virtual int number_of_manœuvres() const;
  // |index| must be in [0, number_of_manœuvres()[.
  virtual NavigationManœuvre const& GetManœuvre(int index) const;

  // The following two functions return false and have no effect if the given
  // |burn| would start before |initial_time_| or before the end of the previous
  // burn, or end after |desired_final_time_|, or if the integration of the
  // coasting phase times out or is singular before the burn.
  virtual bool Append(Burn burn);

  // Forgets the flight plan at least before |time|.  The actual cutoff time
  // will be in a coast trajectory and may be after |time|.  |on_empty| is run
  // if the flight plan would become empty (it is not modified before running
  // |on_empty|).
  virtual void ForgetBefore(Instant const& time,
                            std::function<void()> const& on_empty);


  // |size()| must be greater than 0.
  virtual void RemoveLast();
  // |size()| must be greater than 0.
  virtual bool ReplaceLast(Burn burn);

  // Returns false and has no effect if |desired_final_time| is before the end
  // of the last manœuvre or before |initial_time_|.
  virtual bool SetDesiredFinalTime(Instant const& desired_final_time);

  virtual Ephemeris<Barycentric>::AdaptiveStepParameters const&
  adaptive_step_parameters() const;
  virtual Ephemeris<Barycentric>::GeneralizedAdaptiveStepParameters const&
  generalized_adaptive_step_parameters() const;

  // Sets the parameters used to compute the trajectories.  The trajectories are
  // recomputed.  Returns false (and doesn't change this object) if the
  // parameters would make it impossible to recompute the trajectories.
  virtual bool SetAdaptiveStepParameters(
      Ephemeris<Barycentric>::AdaptiveStepParameters const&
          adaptive_step_parameters,
      Ephemeris<Barycentric>::GeneralizedAdaptiveStepParameters const&
          generalized_adaptive_step_parameters);

  // Returns the number of trajectory segments in this object.
  virtual int number_of_segments() const;

  // |index| must be in [0, number_of_segments()[.  Sets the iterators to denote
  // the given trajectory segment.
  virtual void GetSegment(int index,
                          DiscreteTrajectory<Barycentric>::Iterator& begin,
                          DiscreteTrajectory<Barycentric>::Iterator& end) const;
  virtual void GetAllSegments(
      DiscreteTrajectory<Barycentric>::Iterator& begin,
      DiscreteTrajectory<Barycentric>::Iterator& end) const;

  void WriteToMessage(not_null<serialization::FlightPlan*> message) const;

  // This may return a null pointer if the flight plan contained in the
  // |message| is anomalous.
  static std::unique_ptr<FlightPlan> ReadFromMessage(
      serialization::FlightPlan const& message,
      not_null<Ephemeris<Barycentric>*> ephemeris);

  static std::int64_t constexpr max_ephemeris_steps_per_frame = 1000;

 protected:
  // For mocking.
  FlightPlan();

 private:
  // Appends |manœuvre| to |manœuvres_|, adds a burn and a coast segment.
  // |manœuvre| must fit between |start_of_last_coast()| and
  // |desired_final_time_|, the last coast segment must end at
  // |manœuvre.initial_time()|.
  void Append(NavigationManœuvre manœuvre);

  // Recomputes all trajectories in |segments_|.  Returns false if the
  // recomputation resulted in more than 2 anomalous segments.
  bool RecomputeSegments();

  // Flows the last segment for the duration of |manœuvre| using its intrinsic
  // acceleration.
  void BurnLastSegment(NavigationManœuvre const& manœuvre);
  // Flows the last segment until |desired_final_time| with no intrinsic
  // acceleration.
  void CoastLastSegment(Instant const& desired_final_time);

  // Replaces the last segment with |segment|.  |segment| must be forked from
  // the same trajectory as the last segment, and at the same time.  |segment|
  // must not be anomalous.
  void ReplaceLastSegment(not_null<DiscreteTrajectory<Barycentric>*> segment);

  // Adds a trajectory to |segments_|, forked at the end of the last one.
  void AddSegment();
  // Forgets the last segment after its fork.
  void ResetLastSegment();

  // Deletes the last segment and removes it from |segments_|.
  void PopLastSegment();

  // If the integration of a coast from the fork of |coast| until
  // |manœuvre.initial_time()| reaches the end, returns the integrated
  // trajectory.  Otherwise, returns null.
  DiscreteTrajectory<Barycentric>* CoastIfReachesManœuvreInitialTime(
      DiscreteTrajectory<Barycentric>& coast,
      NavigationManœuvre const& manœuvre);

  Instant start_of_last_coast() const;
  Instant start_of_penultimate_coast() const;

  DiscreteTrajectory<Barycentric>& last_coast();
  DiscreteTrajectory<Barycentric>& penultimate_coast();

  Mass const initial_mass_;
  Instant initial_time_;
  DegreesOfFreedom<Barycentric> initial_degrees_of_freedom_;
  Instant desired_final_time_;
  // The root of the flight plan.  Contains a single point, not part of
  // |segments_|.  Owns all the |segments_|.
  not_null<std::unique_ptr<DiscreteTrajectory<Barycentric>>> root_;
  // Never empty; Starts and ends with a coasting segment; coasting and burning
  // alternate.  This simulates a stack.  Each segment is a fork of the previous
  // one.
  std::vector<not_null<DiscreteTrajectory<Barycentric>*>> segments_;
  std::vector<NavigationManœuvre> manœuvres_;
  not_null<Ephemeris<Barycentric>*> ephemeris_;
  Ephemeris<Barycentric>::AdaptiveStepParameters adaptive_step_parameters_;
  Ephemeris<Barycentric>::GeneralizedAdaptiveStepParameters
      generalized_adaptive_step_parameters_;
  // The last |anomalous_segments_| of |segments_| are anomalous, i.e. they
  // either end prematurely or follow an anomalous segment; in the latter case
  // they are empty.
  // The contract of |Append| and |ReplaceLast| implies that
  // |anomalous_segments_| is at most 2: the penultimate coast is never
  // anomalous.
  int anomalous_segments_ = 0;
};

}  // namespace internal_flight_plan

using internal_flight_plan::FlightPlan;

}  // namespace ksp_plugin
}  // namespace principia
