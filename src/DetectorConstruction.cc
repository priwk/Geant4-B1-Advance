#include "DetectorConstruction.hh"

#include "G4RunManager.hh"
#include "G4NistManager.hh"
#include "G4GeometryManager.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SolidStore.hh"

#include "G4Box.hh"
#include "G4Orb.hh"
#include "G4IntersectionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"

#include "G4MaterialPropertiesTable.hh"

#include "G4Material.hh"
#include "G4Element.hh"
#include "G4Isotope.hh"

#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
  struct PlacementData
  {
    std::vector<G4ThreeVector> bnCenters;
    std::vector<G4ThreeVector> znsCenters;
  };

  std::string Trim(const std::string &s)
  {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
      ++b;

    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
      --e;

    return s.substr(b, e - b);
  }

  std::filesystem::path MakePlacementRootDirectoryPath()
  {
    // 约定：程序从 build/ 目录运行
    // build/ -> ../Input/placements
    return std::filesystem::current_path().parent_path() / "Input" / "placements";
  }

  std::string MakeRatioFolderName(G4double bnWt, G4double znsWt)
  {
    // 支持 1:1, 2:1, 1:2, 1:3
    // 也兼容 1:1.5 -> 2-3, 1:2.5 -> 2-5 这类情况
    const long long scale = 1000;

    long long a = static_cast<long long>(std::llround(bnWt * scale));
    long long b = static_cast<long long>(std::llround(znsWt * scale));

    if (a <= 0 || b <= 0)
    {
      G4Exception("DetectorConstruction::Construct",
                  "BNZS201", FatalException,
                  "Invalid BN/ZnS weight ratio.");
      return "";
    }

    const long long g = std::gcd(std::llabs(a), std::llabs(b));
    a /= g;
    b /= g;

    return std::to_string(a) + "-" + std::to_string(b);
  }

  std::string PickRandomPlacementFilePath(G4double bnWt, G4double znsWt)
  {
    namespace fs = std::filesystem;

    const fs::path rootDir = MakePlacementRootDirectoryPath();

    if (!fs::exists(rootDir) || !fs::is_directory(rootDir))
    {
      G4Exception("DetectorConstruction::Construct",
                  "BNZS202", FatalException,
                  ("Placement root directory not found: " + rootDir.string()).c_str());
      return "";
    }

    const std::string ratioFolderName = MakeRatioFolderName(bnWt, znsWt);
    const fs::path ratioDir = rootDir / ratioFolderName;

    if (!fs::exists(ratioDir) || !fs::is_directory(ratioDir))
    {
      std::ostringstream oss;
      oss << "No placement folder for BN:ZnS wt = "
          << bnWt << ":" << znsWt
          << " , expected folder: " << ratioDir.string();
      G4Exception("DetectorConstruction::Construct",
                  "BNZS203", FatalException,
                  oss.str().c_str());
      return "";
    }

    std::vector<fs::path> files;
    for (const auto &entry : fs::directory_iterator(ratioDir))
    {
      if (!entry.is_regular_file())
        continue;

      const std::string ext = entry.path().extension().string();
      if (ext == ".csv" || ext == ".txt")
      {
        files.push_back(entry.path());
      }
    }

    if (files.empty())
    {
      G4Exception("DetectorConstruction::Construct",
                  "BNZS204", FatalException,
                  ("No placement CSV/TXT files found in: " + ratioDir.string()).c_str());
      return "";
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, files.size() - 1);

    return files[dist(rng)].string();
  }

  PlacementData ReadPlacementCSV(const std::string &filePath)
  {
    PlacementData data;

    std::ifstream fin(filePath.c_str(), std::ios::in);
    if (!fin.is_open())
    {
      G4Exception("DetectorConstruction::Construct",
                  "BNZS205", FatalException,
                  ("Failed to open placement CSV: " + filePath).c_str());
      return data;
    }

    std::string line;
    bool headerHandled = false;

    while (std::getline(fin, line))
    {
      line = Trim(line);
      if (line.empty())
        continue;

      if (line[0] == '#')
        continue;

      if (!headerHandled)
      {
        headerHandled = true;

        std::string lower = line;
        for (char &ch : lower)
          ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        if (lower.find("type") != std::string::npos &&
            lower.find("x") != std::string::npos &&
            lower.find("y") != std::string::npos &&
            lower.find("z") != std::string::npos)
        {
          continue;
        }
      }

      std::stringstream ss(line);
      std::string type, sx, sy, sz;

      if (!std::getline(ss, type, ','))
        continue;
      if (!std::getline(ss, sx, ','))
        continue;
      if (!std::getline(ss, sy, ','))
        continue;
      if (!std::getline(ss, sz, ','))
        continue;

      type = Trim(type);
      sx = Trim(sx);
      sy = Trim(sy);
      sz = Trim(sz);

      const G4double x = std::stod(sx) * um;
      const G4double y = std::stod(sy) * um;
      const G4double z = std::stod(sz) * um;

      G4ThreeVector pos(x, y, z);

      std::string typeLower = type;
      for (char &ch : typeLower)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

      if (typeLower == "bn")
      {
        data.bnCenters.push_back(pos);
      }
      else if (typeLower == "zns")
      {
        data.znsCenters.push_back(pos);
      }
      else
      {
        G4Exception("DetectorConstruction::Construct",
                    "BNZS206", FatalException,
                    ("Unknown particle type in placement CSV: " + type).c_str());
        return data;
      }
    }

    if (data.bnCenters.empty() && data.znsCenters.empty())
    {
      G4Exception("DetectorConstruction::Construct",
                  "BNZS207", FatalException,
                  ("Placement CSV is empty: " + filePath).c_str());
      return data;
    }

    return data;
  }

  G4bool IsFullyInsideBox(const G4ThreeVector &pos,
                          G4double radius,
                          G4double patchXY,
                          G4double localThickness)
  {
    return (std::abs(pos.x()) + radius <= 0.5 * patchXY &&
            std::abs(pos.y()) + radius <= 0.5 * patchXY &&
            std::abs(pos.z()) + radius <= 0.5 * localThickness);
  }

  void ValidatePlacementData(const PlacementData &data,
                             G4double patchXY,
                             G4double localThickness,
                             G4double bnRadius,
                             G4double znsRadius,
                             G4double overlapGap)
  {
    auto checkInside = [&](const G4ThreeVector &p, const char *name)
    {
      const G4double xMin = -0.5 * patchXY;
      const G4double xMax = 0.5 * patchXY;
      const G4double yMin = -0.5 * patchXY;
      const G4double yMax = 0.5 * patchXY;
      const G4double zMin = -0.5 * localThickness;
      const G4double zMax = 0.5 * localThickness;

      if (p.x() < xMin || p.x() > xMax ||
          p.y() < yMin || p.y() > yMax ||
          p.z() < zMin || p.z() > zMax)
      {
        std::ostringstream oss;
        oss << name << " center outside allowed region: ("
            << p.x() / um << ", " << p.y() / um << ", " << p.z() / um << ") um";
        G4Exception("DetectorConstruction::Construct",
                    "BNZS206", FatalException,
                    oss.str().c_str());
      }
    };

    for (const auto &p : data.bnCenters)
      checkInside(p, "BN");

    for (const auto &p : data.znsCenters)
      checkInside(p, "ZnS");

    // --- O(N) 网格空间索引代码 ---
    struct Part
    {
      G4ThreeVector pos;
      G4double radius;
    };
    std::vector<Part> allParts;
    allParts.reserve(data.bnCenters.size() + data.znsCenters.size());
    for (const auto &p : data.bnCenters)
      allParts.push_back({p, bnRadius});
    for (const auto &p : data.znsCenters)
      allParts.push_back({p, znsRadius});

    if (allParts.empty())
      return;

    // 1. 构建网格
    const G4double maxRadius = std::max(bnRadius, znsRadius);
    const G4double cellSize = 2.0 * maxRadius + overlapGap;

    const int nx = std::max(1, static_cast<int>(std::ceil(patchXY / cellSize)));
    const int ny = std::max(1, static_cast<int>(std::ceil(patchXY / cellSize)));
    const int nz = std::max(1, static_cast<int>(std::ceil(localThickness / cellSize)));

    std::vector<std::vector<int>> grid(nx * ny * nz);

    auto getCellIndex = [&](const G4ThreeVector &p)
    {
      int cx = std::clamp(static_cast<int>((p.x() + 0.5 * patchXY) / cellSize), 0, nx - 1);
      int cy = std::clamp(static_cast<int>((p.y() + 0.5 * patchXY) / cellSize), 0, ny - 1);
      int cz = std::clamp(static_cast<int>((p.z() + 0.5 * localThickness) / cellSize), 0, nz - 1);
      return cx + cy * nx + cz * nx * ny;
    };

    // 2. 将球体分配到网格中
    for (std::size_t i = 0; i < allParts.size(); ++i)
    {
      grid[getCellIndex(allParts[i].pos)].push_back(static_cast<int>(i));
    }

    // 3. 仅检查相邻网格
    for (int cz = 0; cz < nz; ++cz)
    {
      for (int cy = 0; cy < ny; ++cy)
      {
        for (int cx = 0; cx < nx; ++cx)
        {
          const auto &bucket = grid[cx + cy * nx + cz * nx * ny];
          for (int i : bucket)
          {
            for (int dz = -1; dz <= 1; ++dz)
            {
              for (int dy = -1; dy <= 1; ++dy)
              {
                for (int dx = -1; dx <= 1; ++dx)
                {
                  int ncx = cx + dx, ncy = cy + dy, ncz = cz + dz;
                  if (ncx < 0 || ncx >= nx || ncy < 0 || ncy >= ny || ncz < 0 || ncz >= nz)
                    continue;

                  for (int j : grid[ncx + ncy * nx + ncz * nx * ny])
                  {
                    if (i >= j)
                      continue; // 避免重复比较和自己比自己

                    G4double reqDist = allParts[i].radius + allParts[j].radius + overlapGap;
                    if ((allParts[i].pos - allParts[j].pos).mag2() < reqDist * reqDist)
                    {
                      G4Exception("DetectorConstruction::Construct",
                                  "BNZS207", FatalException,
                                  "Overlap detected in placement CSV! Fast check failed.");
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // --- 数学解析球冠体积函数 ---

  G4double CalculateClippedVolume(const G4ThreeVector &c,
                                  G4double r,
                                  G4double patchXY,
                                  G4double localThickness,
                                  G4int nz = 40,
                                  G4int nx = 40)
  {
    const G4double pi = CLHEP::pi;

    // 捷径 1：如果球体完全在箱子内部，直接返回完整体积
    if (std::abs(c.x()) + r <= 0.5 * patchXY &&
        std::abs(c.y()) + r <= 0.5 * patchXY &&
        std::abs(c.z()) + r <= 0.5 * localThickness)
    {
      return (4.0 / 3.0) * pi * r * r * r;
    }

    // 捷径 2：确定 Z 轴有效积分范围
    const G4double zMinBox = -0.5 * localThickness;
    const G4double zMaxBox = 0.5 * localThickness;
    const G4double z0 = std::max(zMinBox, c.z() - r);
    const G4double z1 = std::min(zMaxBox, c.z() + r);

    // 如果完全在箱子上方或下方
    if (z1 <= z0)
      return 0.0;

    G4double volume = 0.0;
    const G4double dz = (z1 - z0) / static_cast<G4double>(nz);

    // 第一层：沿 Z 轴切成圆盘
    for (G4int iz = 0; iz < nz; ++iz)
    {
      G4double z = z0 + (iz + 0.5) * dz;
      G4double rz2 = r * r - (z - c.z()) * (z - c.z());
      if (rz2 <= 0.0)
        continue; // 理论上不会发生，防御性判断

      G4double rho = std::sqrt(rz2); // 当前高度的二维圆半径

      // 确定 X 轴有效积分范围
      G4double x0 = std::max(-0.5 * patchXY, c.x() - rho);
      G4double x1 = std::min(0.5 * patchXY, c.x() + rho);

      if (x1 <= x0)
        continue; // 当前切片在这个高度上完全超出了 X 边界

      G4double dx = (x1 - x0) / static_cast<G4double>(nx);
      G4double area = 0.0;

      // 第二层：沿 X 轴切成条带
      for (G4int ix = 0; ix < nx; ++ix)
      {
        G4double x = x0 + (ix + 0.5) * dx;
        G4double ry2 = rho * rho - (x - c.x()) * (x - c.x());
        if (ry2 <= 0.0)
          continue;

        G4double ySpan = std::sqrt(ry2); // 当前条带在 Y 轴的半长

        // 解析求解 Y 轴落在箱内的实际长度 (极其优雅的处理了角落问题)
        G4double y0 = std::max(-0.5 * patchXY, c.y() - ySpan);
        G4double y1 = std::min(0.5 * patchXY, c.y() + ySpan);

        if (y1 > y0)
          area += (y1 - y0) * dx;
      }
      volume += area * dz;
    }

    return volume;
  }
}

// --------------------------------------------------------------------

DetectorConstruction::DetectorConstruction()
    : G4VUserDetectorConstruction(),
      fScreenThickness(125.0 * um),
      fMicroThickness(30.0 * um),
      fPatchXY(50.0 * um),
      fBNRadius(2.5 * um),  // BN diameter = 5 um
      fZnSRadius(1.0 * um), // ZnS diameter = 2 um
      fBNPitch(5.0 * um),
      fZnSPitch(2.0 * um),
      fOverlapGap(0.01 * um),
      fSafeMarginXY(8.0 * um),
      fVoidVolumeFraction(0.425),
      fBnWt(1.0),
      fZnsWt(2.0),
      fVacuumMaterial(nullptr),
      fMatrixMaterial(nullptr),
      fBNMaterial(nullptr),
      fZnSMaterial(nullptr),
      fScoringVolume(nullptr),
      fWorldLogical(nullptr),
      fMatrixLogical(nullptr),
      fBNLogical(nullptr),
      fZnSLogical(nullptr)
{
}

// --------------------------------------------------------------------

DetectorConstruction::~DetectorConstruction() = default;

// --------------------------------------------------------------------

void DetectorConstruction::NotifyGeometryChanged()
{
  auto *rm = G4RunManager::GetRunManager();
  if (rm)
  {
    rm->GeometryHasBeenModified();
  }
}

// --------------------------------------------------------------------

void DetectorConstruction::SetScreenThicknessUm(G4double thicknessUm)
{
  if (thicknessUm <= 0.0)
    return;
  fScreenThickness = thicknessUm * um;
  NotifyGeometryChanged();
}

// --------------------------------------------------------------------

void DetectorConstruction::SetMicroThicknessUm(G4double thicknessUm)
{
  if (thicknessUm <= 0.0)
    return;
  fMicroThickness = thicknessUm * um;
  NotifyGeometryChanged();
}

// --------------------------------------------------------------------

void DetectorConstruction::SetPatchXYUm(G4double patchXYUm)
{
  if (patchXYUm <= 0.0)
    return;
  fPatchXY = patchXYUm * um;
  NotifyGeometryChanged();
}

// --------------------------------------------------------------------

void DetectorConstruction::SetWeightRatio(G4double bnWt, G4double znsWt)
{
  if (bnWt <= 0.0 || znsWt <= 0.0)
    return;
  fBnWt = bnWt;
  fZnsWt = znsWt;
  NotifyGeometryChanged();
}

// --------------------------------------------------------------------

void DetectorConstruction::SetPresetRatio(G4int presetIndex)
{
  switch (presetIndex)
  {
  case 0:
    SetWeightRatio(2.0, 1.0);
    break;
  case 1:
    SetWeightRatio(1.0, 1.0);
    break;
  case 2:
    SetWeightRatio(1.0, 1.5);
    break;
  case 3:
    SetWeightRatio(1.0, 2.0);
    break;
  case 4:
    SetWeightRatio(1.0, 2.5);
    break;
  case 5:
    SetWeightRatio(1.0, 3.0);
    break;
  default:
    SetWeightRatio(1.0, 1.0);
    break;
  }
}

// --------------------------------------------------------------------

G4double DetectorConstruction::GetEffectiveLocalThickness() const
{
  return std::min(fScreenThickness, fMicroThickness);
}

// --------------------------------------------------------------------

G4double DetectorConstruction::HashToUnit(G4int ix, G4int iy, G4int iz, G4int salt)
{
  std::uint32_t h = 2166136261u;
  h = (h ^ static_cast<std::uint32_t>(ix + 10007 + 13 * salt)) * 16777619u;
  h = (h ^ static_cast<std::uint32_t>(iy + 10009 + 17 * salt)) * 16777619u;
  h = (h ^ static_cast<std::uint32_t>(iz + 10037 + 19 * salt)) * 16777619u;

  return static_cast<G4double>(h & 0x00FFFFFFu) /
         static_cast<G4double>(0x01000000u);
}

// --------------------------------------------------------------------

G4bool DetectorConstruction::IsInsideSafeXY(const G4ThreeVector &pos) const
{
  const G4double halfXY = 0.5 * fPatchXY;
  return (std::abs(pos.x()) < (halfXY - fSafeMarginXY) &&
          std::abs(pos.y()) < (halfXY - fSafeMarginXY));
}

// --------------------------------------------------------------------

void DetectorConstruction::DefineMaterials()
{
  auto *nist = G4NistManager::Instance();

  // ---------- vacuum ----------
  fVacuumMaterial = nist->FindOrBuildMaterial("G4_Galactic");

  // ---------- common materials ----------
  auto *air = nist->FindOrBuildMaterial("G4_AIR");
  auto *binder = nist->FindOrBuildMaterial("G4_PLEXIGLASS");

  // ---------- enriched boron ----------
  auto *enrichedB = G4Element::GetElement("EnrichedBoron_10B_99p15", false);
  if (!enrichedB)
  {
    auto *B10 = new G4Isotope("B10_iso", 5, 10, 10.0129370 * g / mole);
    auto *B11 = new G4Isotope("B11_iso", 5, 11, 11.0093050 * g / mole);

    enrichedB = new G4Element("EnrichedBoron_10B_99p15", "B_enr", 2);
    enrichedB->AddIsotope(B10, 19.78 * perCent);
    enrichedB->AddIsotope(B11, 80.22 * perCent);
  }

  auto *N = nist->FindOrBuildElement("N");
  auto *Zn = nist->FindOrBuildElement("Zn");
  auto *S = nist->FindOrBuildElement("S");
  auto *Ag = nist->FindOrBuildElement("Ag");

  // ---------- BN ----------
  fBNMaterial = G4Material::GetMaterial("BN_10B99p15", false);
  if (!fBNMaterial)
  {
    fBNMaterial = new G4Material("BN_10B99p15", 2.10 * g / cm3, 2);
    fBNMaterial->AddElement(enrichedB, 1);
    fBNMaterial->AddElement(N, 1);
  }

  // ---------- ZnS base ----------
  auto *znsBase = G4Material::GetMaterial("ZnS_Base", false);
  if (!znsBase)
  {
    znsBase = new G4Material("ZnS_Base", 4.09 * g / cm3, 2);
    znsBase->AddElement(Zn, 1);
    znsBase->AddElement(S, 1);
  }

  // ---------- ZnS:Ag 0.1 wt% ----------
  fZnSMaterial = G4Material::GetMaterial("ZnS_Ag_0p1wt", false);
  if (!fZnSMaterial)
  {
    fZnSMaterial = new G4Material("ZnS_Ag_0p1wt", 4.09 * g / cm3, 2);
    fZnSMaterial->AddMaterial(znsBase, 99.9 * perCent);
    fZnSMaterial->AddElement(Ag, 0.1 * perCent);
  }

  // ---------- Binder + air voids background ----------
  fMatrixMaterial = G4Material::GetMaterial("BinderVoidMix", false);
  if (!fMatrixMaterial)
  {
    const G4double rhoBinder = binder->GetDensity();
    const G4double rhoAir = air->GetDensity();
    const G4double vfVoid = fVoidVolumeFraction;

    const G4double rhoMix =
        (1.0 - vfVoid) * rhoBinder + vfVoid * rhoAir;

    const G4double binderMass = (1.0 - vfVoid) * rhoBinder;
    const G4double airMass = vfVoid * rhoAir;

    const G4double binderMassFrac = binderMass / (binderMass + airMass);
    const G4double airMassFrac = airMass / (binderMass + airMass);

    fMatrixMaterial = new G4Material("BinderVoidMix", rhoMix, 2);
    fMatrixMaterial->AddMaterial(binder, binderMassFrac);
    fMatrixMaterial->AddMaterial(air, airMassFrac);
  }

  // =========================================================
  // 光学属性定义 (Optical Properties)
  // =========================================================

  // 定义光子能量范围：2.0 eV (~620 nm) 到 3.0 eV (~413 nm)
  // ZnS:Ag 的发光峰值通常在 450 nm 左右，约等于 2.75 eV
  G4double photonEnergy[] = {2.0 * eV, 2.75 * eV, 3.0 * eV};
  const G4int nEntries = sizeof(photonEnergy) / sizeof(G4double);

  // ---------------------------------------------------------
  // 1. 真空 (Vacuum)
  // ---------------------------------------------------------
  G4double rindexVacuum[] = {1.0, 1.0, 1.0};
  auto *mptVacuum = new G4MaterialPropertiesTable();
  mptVacuum->AddProperty("RINDEX", photonEnergy, rindexVacuum, nEntries);
  fVacuumMaterial->SetMaterialPropertiesTable(mptVacuum);

  // ---------------------------------------------------------
  // 2. 基质/空气孔隙混合物 (Binder + Voids Matrix)
  // ---------------------------------------------------------
  // 假设树脂基质的折射率约为 1.5，吸收长度设置为 1 米（假设其相对透明）
  G4double rindexMatrix[] = {1.5, 1.5, 1.5};
  G4double absMatrix[] = {1.0 * m, 1.0 * m, 1.0 * m};
  auto *mptMatrix = new G4MaterialPropertiesTable();
  mptMatrix->AddProperty("RINDEX", photonEnergy, rindexMatrix, nEntries);
  mptMatrix->AddProperty("ABSLENGTH", photonEnergy, absMatrix, nEntries);
  fMatrixMaterial->SetMaterialPropertiesTable(mptMatrix);

  // ---------------------------------------------------------
  // 3. 氮化硼 (BN)
  // ---------------------------------------------------------
  // 六方 BN 折射率较高，且对可见光吸收较强（不透明），吸收长度设短一点
  G4double rindexBN[] = {2.1, 2.1, 2.1};
  G4double absBN[] = {10.0 * um, 10.0 * um, 10.0 * um};
  auto *mptBN = new G4MaterialPropertiesTable();
  mptBN->AddProperty("RINDEX", photonEnergy, rindexBN, nEntries);
  mptBN->AddProperty("ABSLENGTH", photonEnergy, absBN, nEntries);
  fBNMaterial->SetMaterialPropertiesTable(mptBN);

  // ---------------------------------------------------------
  // 4. 闪烁体 (ZnS:Ag 0.1 wt%)
  // ---------------------------------------------------------
  // ZnS 折射率极高 (~2.36)，微粉内部存在严重的自吸收和强散射
  G4double rindexZnS[] = {2.36, 2.36, 2.36};
  G4double absZnS[] = {50.0 * um, 50.0 * um, 50.0 * um};

  // 发光光谱相对强度 (在 2.75 eV 时达到峰值 1.0)
  G4double emissionZnS[] = {0.1, 1.0, 0.2};

  auto *mptZnS = new G4MaterialPropertiesTable();
  mptZnS->AddProperty("RINDEX", photonEnergy, rindexZnS, nEntries);
  mptZnS->AddProperty("ABSLENGTH", photonEnergy, absZnS, nEntries);

  // ----- 核心发光属性 -----
  // 发射谱分布 (Geant4 10.7 及以上版本使用 SCINTILLATIONCOMPONENT1)
  // 注意：如果你的 G4 版本低于 10.7，请将 SCINTILLATIONCOMPONENT1 替换为 FASTCOMPONENT
  mptZnS->AddProperty("SCINTILLATIONCOMPONENT1", photonEnergy, emissionZnS, nEntries);

  // ZnS:Ag 理论发光产额极高（约 75000 光子/MeV，但实际在粉末屏中测出效率会降低，此处给理论值）
  mptZnS->AddConstProperty("SCINTILLATIONYIELD", 75000. / MeV);

  // 展宽系数，设为 1.0 表现出符合泊松分布的光子涨落
  mptZnS->AddConstProperty("RESOLUTIONSCALE", 1.0);

  // ZnS:Ag 衰减时间较长，主峰大约 200 ns
  // 注意：低版本 G4 请将 SCINTILLATIONTIMECONSTANT1 替换为 FASTTIMECONSTANT
  mptZnS->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 200. * ns);
  mptZnS->AddConstProperty("SCINTILLATIONYIELD1", 1.0);

  fZnSMaterial->SetMaterialPropertiesTable(mptZnS);
}

// --------------------------------------------------------------------

G4int DetectorConstruction::ComputeTargetZnSCount(G4int placedBNCount, G4int usableZnSCount) const
{
  if (placedBNCount <= 0 || usableZnSCount <= 0)
    return 0;

  const G4double vBN = (4.0 / 3.0) * pi * std::pow(fBNRadius, 3);
  const G4double vZnS = (4.0 / 3.0) * pi * std::pow(fZnSRadius, 3);

  const G4double rhoBN = 2.10 * g / cm3;
  const G4double rhoZnS = 4.09 * g / cm3;

  const G4double mBN = rhoBN * vBN;
  const G4double mZnS = rhoZnS * vZnS;

  const G4double targetZnSCountReal =
      static_cast<G4double>(placedBNCount) *
      (mBN / mZnS) *
      (fZnsWt / fBnWt);

  G4int targetZnSCount = static_cast<G4int>(std::llround(targetZnSCountReal));
  targetZnSCount = std::max(0, std::min(targetZnSCount, usableZnSCount));

  return targetZnSCount;
}

// --------------------------------------------------------------------

G4VPhysicalVolume *DetectorConstruction::Construct()
{
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  fPlacedBNCenters.clear();
  fSafeBNCenters.clear();
  fUsableZnSCandidateCenters.clear();
  fPlacedZnSCenters.clear();

  DefineMaterials();

  const G4double localThickness = GetEffectiveLocalThickness();

  // ---------- world ----------
  const G4double worldXY = 2 * fPatchXY;
  const G4double worldZ = localThickness + 100.0 * um;

  auto *solidWorld = new G4Box("WorldSolid",
                               0.5 * worldXY,
                               0.5 * worldXY,
                               0.5 * worldZ);

  fWorldLogical = new G4LogicalVolume(
      solidWorld, fVacuumMaterial, "WorldLV");

  auto *physWorld = new G4PVPlacement(
      nullptr, G4ThreeVector(), fWorldLogical, "WorldPV",
      nullptr, false, 0, false);

  // ---------- local patch ----------
  auto *solidMatrix = new G4Box("MicroPatchSolid",
                                0.5 * fPatchXY,
                                0.5 * fPatchXY,
                                0.5 * localThickness);

  fMatrixLogical = new G4LogicalVolume(
      solidMatrix, fMatrixMaterial, "MatrixLV");

  new G4PVPlacement(
      nullptr, G4ThreeVector(), fMatrixLogical, "MatrixPV",
      fWorldLogical, false, 0, false);

  fScoringVolume = fMatrixLogical;

  // ---------- particle solids ----------
  auto *solidBN = new G4Orb("BNSolid", fBNRadius);
  auto *solidZnS = new G4Orb("ZnSSolid", fZnSRadius);

  fBNLogical = new G4LogicalVolume(solidBN, fBNMaterial, "BN_LV");
  fZnSLogical = new G4LogicalVolume(solidZnS, fZnSMaterial, "ZnS_LV");

  // =========================================================
  // Load external placement file instead of generating packing internally
  // =========================================================
  const std::string loadedPlacementFile = PickRandomPlacementFilePath(fBnWt, fZnsWt);

  G4cout << "[DetectorConstruction] Loading placement file: "
         << loadedPlacementFile << G4endl;

  PlacementData loaded = ReadPlacementCSV(loadedPlacementFile);

  ValidatePlacementData(loaded,
                        fPatchXY,
                        localThickness,
                        fBNRadius,
                        fZnSRadius,
                        fOverlapGap);

  fPlacedBNCenters = loaded.bnCenters;
  fPlacedZnSCenters = loaded.znsCenters;

  fSafeBNCenters.clear();
  for (const auto &pos : fPlacedBNCenters)
  {
    if (IsInsideSafeXY(pos))
      fSafeBNCenters.push_back(pos);
  }

  // ---------- quantities for report ----------
  const G4double vBN = (4.0 / 3.0) * pi * std::pow(fBNRadius, 3);
  const G4double vZnS = (4.0 / 3.0) * pi * std::pow(fZnSRadius, 3);

  const G4double rhoBN = 2.10 * g / cm3;
  const G4double rhoZnS = 4.09 * g / cm3;

  const G4double volumeUnit_um3 = um * um * um;

  // whole matrix-box volume
  const G4double Vpatch = fPatchXY * fPatchXY * localThickness;

  // actual particle volume kept inside the box
  G4double insideBNVolume = 0.0;
  G4double insideZnSVolume = 0.0;

  // debug counters for clipped particles
  G4int clippedBNCount = 0;
  G4int clippedZnSCount = 0;

  G4double minClippedBNVolume = -1.0;
  G4double minClippedZnSVolume = -1.0;

  // ---------- visualization ----------
  auto *worldVis = new G4VisAttributes();
  worldVis->SetVisibility(false);
  fWorldLogical->SetVisAttributes(worldVis);

  // 调试阶段：先把 matrix 隐掉，避免挡住裁剪体
  auto *matrixVis = new G4VisAttributes();
  matrixVis->SetVisibility(false);
  fMatrixLogical->SetVisAttributes(matrixVis);

  // 正常完整球
  auto *bnVis = new G4VisAttributes(G4Colour(0.20, 0.60, 1.00)); // 蓝
  bnVis->SetForceSolid(true);
  fBNLogical->SetVisAttributes(bnVis);

  auto *znsVis = new G4VisAttributes(G4Colour(1.00, 0.85, 0.20)); // 黄
  znsVis->SetForceSolid(true);
  fZnSLogical->SetVisAttributes(znsVis);

  // 裁剪球专用显示
  auto *bnClipVis = new G4VisAttributes(G4Colour(0.20, 0.60, 1.00, 0.20));
  bnClipVis->SetForceSolid(true);

  auto *znsClipVis = new G4VisAttributes(G4Colour(1.00, 0.85, 0.20, 0.20));
  znsClipVis->SetForceSolid(true);

  // =========================================================
  // 4) Build physical placements only after final centers are ready
  // =========================================================
  auto *solidClipBox = new G4Box("ClipBoxSolid",
                                 0.5 * fPatchXY,
                                 0.5 * fPatchXY,
                                 0.5 * localThickness);

  G4int copyBN = 0;
  for (const auto &pos : fPlacedBNCenters)
  {
    if (IsFullyInsideBox(pos, fBNRadius, fPatchXY, localThickness))
    {
      new G4PVPlacement(
          nullptr, pos, fBNLogical, "BN_PV",
          fMatrixLogical, false, copyBN++, false);

      insideBNVolume += vBN;
    }
    else
    {
      const std::string solidName = "BN_ClipSolid_" + std::to_string(copyBN);
      const std::string lvName = "BN_ClipLV_" + std::to_string(copyBN);
      const std::string pvName = "BN_ClipPV_" + std::to_string(copyBN);

      auto *clippedSolid = new G4IntersectionSolid(
          solidName,
          solidBN,
          solidClipBox,
          nullptr,
          G4ThreeVector(-pos.x(), -pos.y(), -pos.z()));

      const G4double clipVol = CalculateClippedVolume(pos, fBNRadius, fPatchXY, localThickness);

      auto *clippedLV = new G4LogicalVolume(
          clippedSolid, fBNMaterial, lvName);
      clippedLV->SetVisAttributes(bnClipVis);

      new G4PVPlacement(
          nullptr, pos, clippedLV, pvName,
          fMatrixLogical, false, copyBN++, true);

      insideBNVolume += clipVol;
      ++clippedBNCount;

      if (minClippedBNVolume < 0.0 || clipVol < minClippedBNVolume)
        minClippedBNVolume = clipVol;

      G4cout
          << "[Clip BN] center = ("
          << pos.x() / um << ", "
          << pos.y() / um << ", "
          << pos.z() / um << ") um"
          << "  volume = " << clipVol / volumeUnit_um3 << " um^3"
          << G4endl;
    }
  }

  G4int copyZnS = 0;
  for (const auto &pos : fPlacedZnSCenters)
  {
    if (IsFullyInsideBox(pos, fZnSRadius, fPatchXY, localThickness))
    {
      new G4PVPlacement(
          nullptr, pos, fZnSLogical, "ZnS_PV",
          fMatrixLogical, false, copyZnS++, false);

      insideZnSVolume += vZnS;
    }
    else
    {
      const std::string solidName = "ZnS_ClipSolid_" + std::to_string(copyZnS);
      const std::string lvName = "ZnS_ClipLV_" + std::to_string(copyZnS);
      const std::string pvName = "ZnS_ClipPV_" + std::to_string(copyZnS);

      auto *clippedSolid = new G4IntersectionSolid(
          solidName,
          solidZnS,
          solidClipBox,
          nullptr,
          G4ThreeVector(-pos.x(), -pos.y(), -pos.z()));

      const G4double clipVol = CalculateClippedVolume(pos, fBNRadius, fPatchXY, localThickness);

      auto *clippedLV = new G4LogicalVolume(
          clippedSolid, fZnSMaterial, lvName);
      clippedLV->SetVisAttributes(znsClipVis);

      new G4PVPlacement(
          nullptr, pos, clippedLV, pvName,
          fMatrixLogical, false, copyZnS++, true);

      insideZnSVolume += clipVol;
      ++clippedZnSCount;

      if (minClippedZnSVolume < 0.0 || clipVol < minClippedZnSVolume)
        minClippedZnSVolume = clipVol;

      G4cout
          << "[Clip ZnS] center = ("
          << pos.x() / um << ", "
          << pos.y() / um << ", "
          << pos.z() / um << ") um"
          << "  volume = " << clipVol / volumeUnit_um3 << " um^3"
          << G4endl;
    }
  }

  // ---------- achieved ratio report ----------
  const G4double totalBNMass = rhoBN * insideBNVolume;
  const G4double totalZnSMass = rhoZnS * insideZnSVolume;

  const G4double particleVolume = insideBNVolume + insideZnSVolume;

  const G4double phiAchieved =
      (Vpatch > 0.0) ? (particleVolume / Vpatch) : 0.0;

  const G4double achievedBNToZnSMass =
      (totalBNMass > 0.0) ? (totalBNMass / totalZnSMass) : 0.0;

  G4cout
      << "\n[DetectorConstruction] External placement loaded"
      << "\n  placement file            = " << loadedPlacementFile
      << "\n  screen thickness          = " << fScreenThickness / um << " um"
      << "\n  local thickness           = " << localThickness / um << " um"
      << "\n  patch XY                  = " << fPatchXY / um << " um"
      << "\n  target BN:ZnS (wt)        = " << fBnWt << " : " << fZnsWt
      << "\n  BN radius                 = " << fBNRadius / um << " um"
      << "\n  ZnS radius                = " << fZnSRadius / um << " um"
      << "\n  BN pitch                  = " << fBNPitch / um << " um"
      << "\n  ZnS pitch                 = " << fZnSPitch / um << " um"
      << "\n  safe margin XY            = " << fSafeMarginXY / um << " um"
      << "\n  void fraction             = " << 100.0 * fVoidVolumeFraction << " %"
      << "\n  placed BN spheres         = " << fPlacedBNCenters.size()
      << "\n  safe BN spheres           = " << fSafeBNCenters.size()
      << "\n  placed ZnS spheres        = " << fPlacedZnSCenters.size()
      << "\n  clipped BN spheres        = " << clippedBNCount
      << "\n  clipped ZnS spheres       = " << clippedZnSCount
      << "\n  achieved BN mass          = " << totalBNMass / g << " g"
      << "\n  achieved ZnS mass         = " << totalZnSMass / g << " g"
      << "\n  achieved BN/ZnS mass      = " << achievedBNToZnSMass
      << "\n  phi achieved              = " << phiAchieved;

  if (clippedBNCount > 0)
  {
    G4cout << "\n  min clipped BN volume     = "
           << minClippedBNVolume / volumeUnit_um3 << " um^3";
  }

  if (clippedZnSCount > 0)
  {
    G4cout << "\n  min clipped ZnS volume    = "
           << minClippedZnSVolume / volumeUnit_um3 << " um^3";
  }

  G4cout << G4endl;

  return physWorld;
}