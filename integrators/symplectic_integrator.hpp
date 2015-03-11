﻿#pragma once

#include <vector>

#include "quantities/quantities.hpp"

namespace principia {

using quantities::Time;

namespace integrators {

// A simple container for a scalar value and the related error.  The
// constructor is not explicit to make it easy to construct an object with no
// error.
template<typename Scalar>
struct DoublePrecision {
  DoublePrecision() = default;
  DoublePrecision(Scalar const& value);  // NOLINT(runtime/explicit)

  void Increment(Scalar const& increment);

  Scalar value;
  Scalar error;
};

class SymplecticIntegrator {
 public:
  SymplecticIntegrator() = default;
  virtual ~SymplecticIntegrator() = default;

  // The entire state of the system at a given time.  The vectors are indexed by
  // dimension.
  template<typename Position, typename Momentum>
  struct SystemState {
    std::vector<DoublePrecision<Position>> positions;
    std::vector<DoublePrecision<Momentum>> momenta;
    DoublePrecision<Time> time;
  };

  template<typename Position, typename Momentum>
  using Solution = std::vector<SystemState<Position, Momentum>>;

  template<typename Position, typename Momentum>
  struct Parameters {
    // The initial state of the system.
    SystemState<Position, Momentum> initial;
    // The ending time of the resolution.
    Time tmax;
    // The time step.
    Time Δt;
    // To save memory, we only return a datapoint every sampling_period steps
    // (for trajectory plotting), as well as the result from the last step. If
    // sampling_period == 0, we only return the result from the last step
    // (that's for when we just want to advance the system, not to plot its
    // evolution).
    // NOTE(eggrobin): The images in the OP of the forum thread show the problem
    // with the current approach: with reasonable sampling_periods, the plotted
    // trajectory sometimes becomes polygonal at high velocities, while points
    // are wasted at low velocities. At some point I think this should be
    // handled with a function that evaluates the velocity in the plot frame to
    // decide when to sample.  Plotting some sort of higher-order spline, rather
    // than a polygon, would help, but isn't enough.
    int sampling_period;
    // If true, the time for the last step of the integration is exactly |tmax|.
    // If false, the time for the last step may be slightly less than |tmax|.
    // It never exceeds |tmax|.
    bool tmax_is_exact = false;
  };
};

}  // namespace integrators
}  // namespace principia

#include "integrators/symplectic_integrator_body.hpp"
