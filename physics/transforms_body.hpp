#pragma once

#include "physics/transforms.hpp"

#include "geometry/affine_map.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/identity.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/permutation.hpp"
#include "geometry/r3x3_matrix.hpp"
#include "geometry/rotation.hpp"
#include "glog/logging.h"
#include "physics/massive_body.hpp"
#include "quantities/named_quantities.hpp"
#include "quantities/si.hpp"

using principia::geometry::AffineMap;
using principia::geometry::Bivector;
using principia::geometry::Displacement;
using principia::geometry::Identity;
using principia::geometry::Permutation;
using principia::geometry::Position;
using principia::geometry::R3x3Matrix;
using principia::geometry::Rotation;
using principia::geometry::Wedge;
using principia::quantities::AngularFrequency;
using principia::quantities::Pow;
using principia::si::Radian;

namespace principia {
namespace physics {

namespace {

// Fills |*rotation| with the rotation that maps the basis of the barycentric
// frame to the standard basis.  Fills |*angular_frequency| with the
// corresponding angular velocity.  These pointers must be nonnull, and there is
// no transfer of ownership.  |barycentre_degrees_of_freedom| must be a convex
// combination of the two other degrees of freedom.
template<typename FromFrame, typename ToFrame>
void FromBasisOfBarycentricFrameToStandardBasis(
    DegreesOfFreedom<FromFrame> const& barycentre_degrees_of_freedom,
    DegreesOfFreedom<FromFrame> const& primary_degrees_of_freedom,
    DegreesOfFreedom<FromFrame> const& secondary_degrees_of_freedom,
    Rotation<FromFrame, ToFrame>* rotation,
    Bivector<AngularFrequency, FromFrame>* angular_frequency) {
  CHECK_NOTNULL(rotation);
  CHECK_NOTNULL(angular_frequency);
  Displacement<FromFrame> const reference_direction =
      primary_degrees_of_freedom.position -
      barycentre_degrees_of_freedom.position;
  Velocity<FromFrame> reference_normal =
      primary_degrees_of_freedom.velocity -
      barycentre_degrees_of_freedom.velocity;
  reference_direction.Orthogonalize(&reference_normal);
  Bivector<Product<Length, Speed>, FromFrame> const reference_binormal =
      Wedge(reference_direction, reference_normal);
  *rotation = Rotation<FromFrame, ToFrame>(
                  R3x3Matrix(Normalize(reference_direction).coordinates(),
                             Normalize(reference_normal).coordinates(),
                             Normalize(reference_binormal).coordinates()));
  *angular_frequency =
      (Radian / Pow<2>(reference_direction.Norm())) * reference_binormal;
}

}  // namespace

template<typename FromFrame, typename ThroughFrame, typename ToFrame>
std::unique_ptr<Transforms<FromFrame, ThroughFrame, ToFrame>>
Transforms<FromFrame, ThroughFrame, ToFrame>::BodyCentredNonRotating(
    Trajectory<FromFrame> const& from_centre_trajectory,
    Trajectory<ToFrame> const& to_centre_trajectory) {
  std::unique_ptr<Transforms> transforms(new Transforms);

  // From the perspective of the lambda the following variable is really |this|,
  // hence the name.
  Transforms* that = transforms.get();
  transforms->first_ =
      [&from_centre_trajectory, that](
          Instant const& t,
          DegreesOfFreedom<FromFrame> const& from_degrees_of_freedom,
          Trajectory<FromFrame> const* trajectory) ->
      DegreesOfFreedom<ThroughFrame> {
    // First check if the result is cached.
    auto cache_it = that->first_cache_.find(std::make_pair(trajectory, t));
    if (cache_it != that->first_cache_.end()) {
      return cache_it->second;
    }

    // on_or_after() is Ln(N), but it doesn't matter unless the map gets very
    // big, in which case we'll have cache misses anyway.
    Trajectory<FromFrame>::NativeIterator const centre_it =
        from_centre_trajectory.on_or_after(t);
    CHECK_EQ(centre_it.time(), t)
        << "Time " << t << " not in centre trajectory";
    DegreesOfFreedom<FromFrame> const& centre_degrees_of_freedom =
        centre_it.degrees_of_freedom();

    AffineMap<FromFrame, ThroughFrame, Length, Identity> const position_map(
        centre_degrees_of_freedom.position,
        ThroughFrame::origin,
        Identity<FromFrame, ThroughFrame>());
    // TODO(phl): Should |velocity_map| be an affine map?
    Identity<FromFrame, ThroughFrame> const velocity_map;
    DegreesOfFreedom<ThroughFrame> through_degrees_of_freedom =
        {position_map(from_degrees_of_freedom.position),
         velocity_map(from_degrees_of_freedom.velocity -
                      centre_degrees_of_freedom.velocity)};

    // Cache the result before returning it.
    that->first_cache_.emplace(std::make_pair(trajectory, t),
                               through_degrees_of_freedom);
    return std::move(through_degrees_of_freedom);
  };

  transforms->second_ =
      [&to_centre_trajectory](
          Instant const& t,
          DegreesOfFreedom<ThroughFrame> const& through_degrees_of_freedom,
          Trajectory<ThroughFrame> const* trajectory) ->
      DegreesOfFreedom<ToFrame> {
    DegreesOfFreedom<ToFrame> const& last_centre_degrees_of_freedom =
        to_centre_trajectory.last().degrees_of_freedom();

    AffineMap<ThroughFrame, ToFrame, Length, Identity> const position_map(
        ThroughFrame::origin,
        last_centre_degrees_of_freedom.position,
        Identity<ThroughFrame, ToFrame>());
    Identity<ThroughFrame, ToFrame> const velocity_map;
    return {position_map(through_degrees_of_freedom.position),
            velocity_map(through_degrees_of_freedom.velocity)};
  };

  return transforms;
}

template<typename FromFrame, typename ThroughFrame, typename ToFrame>
std::unique_ptr<Transforms<FromFrame, ThroughFrame, ToFrame>>
Transforms<FromFrame, ThroughFrame, ToFrame>::BarycentricRotating(
      Trajectory<FromFrame> const& from_primary_trajectory,
      Trajectory<ToFrame> const& to_primary_trajectory,
      Trajectory<FromFrame> const& from_secondary_trajectory,
      Trajectory<ToFrame> const& to_secondary_trajectory) {
  std::unique_ptr<Transforms> transforms(new Transforms);

  // From the perspective of the lambda the following variable is really |this|,
  // hence the name.
  Transforms* that = transforms.get();
  transforms->first_ =
      [&from_primary_trajectory, &from_secondary_trajectory, that](
          Instant const& t,
          DegreesOfFreedom<FromFrame> const& from_degrees_of_freedom,
          Trajectory<FromFrame> const* trajectory) ->
      DegreesOfFreedom<ThroughFrame> {
    // First check if the result is cached.
    auto cache_it = that->first_cache_.find(std::make_pair(trajectory, t));
    if (cache_it != that->first_cache_.end()) {
      return cache_it->second;
    }

    // on_or_after() is Ln(N).
    Trajectory<FromFrame>::NativeIterator const primary_it =
        from_primary_trajectory.on_or_after(t);
    CHECK_EQ(primary_it.time(), t)
        << "Time " << t << " not in primary trajectory";
    Trajectory<FromFrame>::NativeIterator secondary_it =
        from_secondary_trajectory.on_or_after(t);
    CHECK_EQ(secondary_it.time(), t)
        << "Time " << t << " not in secondary trajectory";

    DegreesOfFreedom<FromFrame> const& primary_degrees_of_freedom =
        primary_it.degrees_of_freedom();
    DegreesOfFreedom<FromFrame> const& secondary_degrees_of_freedom =
        secondary_it.degrees_of_freedom();
    DegreesOfFreedom<FromFrame> const barycentre_degrees_of_freedom =
        Barycentre<FromFrame, GravitationalParameter>(
            {primary_degrees_of_freedom,
             secondary_degrees_of_freedom},
            {from_primary_trajectory.body<MassiveBody>().
                 gravitational_parameter(),
             from_secondary_trajectory.body<MassiveBody>().
                 gravitational_parameter()});
    Rotation<FromFrame, ThroughFrame>
        from_basis_of_barycentric_frame_to_standard_basis;
    Bivector<AngularFrequency, FromFrame> angular_frequency;
    FromBasisOfBarycentricFrameToStandardBasis(
        barycentre_degrees_of_freedom,
        primary_degrees_of_freedom,
        secondary_degrees_of_freedom,
        &from_basis_of_barycentric_frame_to_standard_basis,
        &angular_frequency);

    AffineMap<FromFrame, ThroughFrame, Length, Rotation> const position_map(
        barycentre_degrees_of_freedom.position,
        ThroughFrame::origin,
        from_basis_of_barycentric_frame_to_standard_basis);
    // TODO(phl): This is where we wonder if |velocity_map| should be an affine
    // map.  Also, the filioque.
    Rotation<FromFrame, ThroughFrame> const& velocity_map =
        from_basis_of_barycentric_frame_to_standard_basis;
    DegreesOfFreedom<ThroughFrame> through_degrees_of_freedom =
        {position_map(from_degrees_of_freedom.position),
         velocity_map(from_degrees_of_freedom.velocity -
                      barycentre_degrees_of_freedom.velocity -
                        angular_frequency *
                          (from_degrees_of_freedom.position -
                           barycentre_degrees_of_freedom.position) / Radian)};

    // Cache the result before returning it.
    that->first_cache_.emplace(std::make_pair(trajectory, t),
                               through_degrees_of_freedom);
    return std::move(through_degrees_of_freedom);
  };

  transforms->second_ =
      [&to_primary_trajectory, &to_secondary_trajectory](
          Instant const& t,
          DegreesOfFreedom<ThroughFrame> const& through_degrees_of_freedom,
          Trajectory<ThroughFrame> const* trajectory) ->
      DegreesOfFreedom<ToFrame> {
    DegreesOfFreedom<ToFrame> const& last_primary_degrees_of_freedom =
        to_primary_trajectory.last().degrees_of_freedom();
    DegreesOfFreedom<ToFrame> const& last_secondary_degrees_of_freedom =
        to_secondary_trajectory.last().degrees_of_freedom();
    DegreesOfFreedom<ToFrame> const last_barycentre_degrees_of_freedom =
        Barycentre<ToFrame, GravitationalParameter>(
            {last_primary_degrees_of_freedom,
             last_secondary_degrees_of_freedom},
            {to_primary_trajectory.body<MassiveBody>().
                 gravitational_parameter(),
             to_secondary_trajectory.body<MassiveBody>().
                 gravitational_parameter()});
    Rotation<ToFrame, ThroughFrame>
        from_basis_of_last_barycentric_frame_to_standard_basis;
    Bivector<AngularFrequency, ToFrame> angular_frequency;
    FromBasisOfBarycentricFrameToStandardBasis(
        last_barycentre_degrees_of_freedom,
        last_primary_degrees_of_freedom,
        last_secondary_degrees_of_freedom,
        &from_basis_of_last_barycentric_frame_to_standard_basis,
        &angular_frequency);
    Rotation<ThroughFrame, ToFrame> const
        from_standard_basis_to_basis_of_last_barycentric_frame =
            from_basis_of_last_barycentric_frame_to_standard_basis.Inverse();

    AffineMap<ThroughFrame, ToFrame, Length, Rotation> const position_map(
        ThroughFrame::origin,
        last_barycentre_degrees_of_freedom.position,
        from_standard_basis_to_basis_of_last_barycentric_frame);
    Rotation<ThroughFrame, ToFrame> const& velocity_map =
        from_standard_basis_to_basis_of_last_barycentric_frame;
    return {position_map(through_degrees_of_freedom.position),
            velocity_map(through_degrees_of_freedom.velocity)};
  };

  return transforms;
}

template<typename FromFrame, typename ThroughFrame, typename ToFrame>
typename Trajectory<FromFrame>::template TransformingIterator<ThroughFrame>
Transforms<FromFrame, ThroughFrame, ToFrame>::first(
    Trajectory<FromFrame> const* from_trajectory) {
  return CHECK_NOTNULL(from_trajectory)->first_with_transform(first_);
}

template<typename FromFrame, typename ThroughFrame, typename ToFrame>
typename Trajectory<ThroughFrame>::template TransformingIterator<ToFrame>
Transforms<FromFrame, ThroughFrame, ToFrame>::second(
    Trajectory<ThroughFrame> const* through_trajectory) {
  return CHECK_NOTNULL(through_trajectory)->
             first_with_transform(second_);
}

}  // namespace physics
}  // namespace principia