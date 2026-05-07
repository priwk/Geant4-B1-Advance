#include "StageDReentrySampler.hh"

#include "Randomize.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cmath>

namespace
{
  constexpr G4double kBoundaryEpsilon = 1.0e-4 * um;
  constexpr G4int kMaxMatrixTrials = 10000;
}

StageDReentrySampler::StageDReentrySampler(const DetectorConstruction *detector)
    : fDetector(detector)
{
}

const DetectorConstruction::SphereInfo *
StageDReentrySampler::FindContainingOrNearestSphere(
    DetectorConstruction::Phase phase,
    const G4ThreeVector &position) const
{
  if (fDetector == nullptr)
    return nullptr;

  const std::vector<DetectorConstruction::SphereInfo> *spheres = nullptr;
  if (phase == DetectorConstruction::Phase::BN)
    spheres = &fDetector->GetBNSpheres();
  else if (phase == DetectorConstruction::Phase::ZnS)
    spheres = &fDetector->GetZnSSpheres();
  else
    return nullptr;

  const DetectorConstruction::SphereInfo *best = nullptr;
  G4double bestDistance2 = DBL_MAX;

  for (const auto &sphere : *spheres)
  {
    const G4double distance2 = (position - sphere.center).mag2();
    if (distance2 <= sphere.radius * sphere.radius)
      return &sphere;

    if (distance2 < bestDistance2)
    {
      bestDistance2 = distance2;
      best = &sphere;
    }
  }

  return best;
}

G4ThreeVector StageDReentrySampler::RandomUnitVector() const
{
  const G4double cosTheta = 2.0 * G4UniformRand() - 1.0;
  const G4double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
  const G4double phi = CLHEP::twopi * G4UniformRand();
  return G4ThreeVector(sinTheta * std::cos(phi),
                       sinTheta * std::sin(phi),
                       cosTheta);
}

G4ThreeVector StageDReentrySampler::NudgeInsidePhase(
    const G4ThreeVector &candidate,
    DetectorConstruction::Phase phase) const
{
  if (fDetector == nullptr)
    return candidate;

  if (phase == DetectorConstruction::Phase::Matrix)
  {
    const G4double halfX = fDetector->GetPatchHalfXUm() * um - kBoundaryEpsilon;
    const G4double halfY = fDetector->GetPatchHalfYUm() * um - kBoundaryEpsilon;
    const G4double halfZ = fDetector->GetPatchHalfZUm() * um - kBoundaryEpsilon;
    return G4ThreeVector(std::clamp(candidate.x(), -halfX, halfX),
                         std::clamp(candidate.y(), -halfY, halfY),
                         std::clamp(candidate.z(), -halfZ, halfZ));
  }

  const DetectorConstruction::SphereInfo *sphere =
      FindContainingOrNearestSphere(phase, candidate);
  if (sphere == nullptr)
    return candidate;

  G4ThreeVector offset = candidate - sphere->center;
  const G4double norm = offset.mag();
  if (norm <= 0.0)
    offset = RandomUnitVector();
  else
    offset /= norm;

  const G4double radius = std::max(0.0, sphere->radius - kBoundaryEpsilon);
  return sphere->center + radius * offset;
}

G4ThreeVector StageDReentrySampler::RandomPointInMatrixBox() const
{
  const G4double halfX = fDetector->GetPatchHalfXUm() * um;
  const G4double halfY = fDetector->GetPatchHalfYUm() * um;
  const G4double halfZ = fDetector->GetPatchHalfZUm() * um;
  return G4ThreeVector((2.0 * G4UniformRand() - 1.0) * halfX,
                       (2.0 * G4UniformRand() - 1.0) * halfY,
                       (2.0 * G4UniformRand() - 1.0) * halfZ);
}

G4bool StageDReentrySampler::SampleSamePhaseSphereReentry(
    DetectorConstruction::Phase phase,
    const G4ThreeVector &positionBeforeExit,
    const std::string &reentryMode,
    G4ThreeVector &newPosition) const
{
  if (fDetector == nullptr)
    return false;

  const auto *oldSphere = FindContainingOrNearestSphere(phase, positionBeforeExit);
  if (oldSphere == nullptr || oldSphere->radius <= 0.0)
    return false;

  const std::vector<DetectorConstruction::SphereInfo> *candidates = nullptr;
  if (phase == DetectorConstruction::Phase::BN)
    candidates = &fDetector->GetBNSpheres();
  else if (phase == DetectorConstruction::Phase::ZnS)
    candidates = &fDetector->GetZnSSpheres();
  else
    return false;

  if (candidates->empty())
    return false;

  const std::size_t index =
      static_cast<std::size_t>(G4UniformRand() * candidates->size()) % candidates->size();
  const auto &newSphere = (*candidates)[index];

  const G4double rho = (positionBeforeExit - oldSphere->center).mag();
  const G4double q = std::clamp(rho / oldSphere->radius, 0.0, 1.0);

  G4ThreeVector direction = RandomUnitVector();
  if (reentryMode == "same_phase_random")
  {
    const G4double scale = std::cbrt(G4UniformRand());
    direction *= scale;
  }
  newPosition = newSphere.center + q * newSphere.radius * direction;
  newPosition = NudgeInsidePhase(newPosition, phase);
  return true;
}

G4bool StageDReentrySampler::SampleMatrixReentry(
    const std::string &matrixMode,
    G4ThreeVector &newPosition) const
{
  if (fDetector == nullptr)
    return false;

  if (matrixMode != "random_matrix" && matrixMode != "distance_matched_matrix")
    return false;

  for (G4int trial = 0; trial < kMaxMatrixTrials; ++trial)
  {
    const G4ThreeVector candidate = RandomPointInMatrixBox();
    if (fDetector->FindPhaseAtPoint(candidate) == DetectorConstruction::Phase::Matrix)
    {
      newPosition = NudgeInsidePhase(candidate, DetectorConstruction::Phase::Matrix);
      return true;
    }
  }

  return false;
}
