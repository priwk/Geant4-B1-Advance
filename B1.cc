#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include "AnalysisConfig.hh"
#include "AnalysisMessenger.hh"

#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "QGSP_BIC_HP.hh"

#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "PhysicsList.hh"

#include "Randomize.hh"
#include "CLHEP/Random/RanecuEngine.h"

int main(int argc, char **argv)
{
  // 没有提供宏时的交互模式
  G4UIExecutive *ui = nullptr;
  if (argc == 1)
  {
    ui = new G4UIExecutive(argc, argv);
  }

  // 随机引擎
  G4Random::setTheEngine(new CLHEP::RanecuEngine);

  // 强制单线程运行管理器
  auto *runManager = new G4RunManager();

  // 统一分析配置（当前默认仍然走 Stage B）
  auto *analysisConfig = new AnalysisConfig();

  // 分析配置的 UI 命令入口：支持在 .mac 中用 /cfg/... 修改 runMode、captureCsvPath 等
  auto *analysisMessenger = new AnalysisMessenger(analysisConfig);

  G4cout << "[B1] AnalysisConfig initialized:"
         << " runMode=" << AnalysisConfig::RunModeName(analysisConfig->runMode)
         << " patchXY=" << analysisConfig->patchXY_um << " um"
         << " microThickness=" << analysisConfig->microThickness_um << " um"
         << " bnWt=" << analysisConfig->bnWt
         << " znsWt=" << analysisConfig->znsWt
         << G4endl;

  // 探测器构造
  runManager->SetUserInitialization(new DetectorConstruction(analysisConfig));

  // 物理过程初始化
  auto *physicsList = new PhysicsList();
  physicsList->SetVerboseLevel(1);
  runManager->SetUserInitialization(physicsList);

  // 动作初始化
  runManager->SetUserInitialization(new ActionInitialization(analysisConfig));

  // 可视化：批处理模式下不要提前初始化，避免在执行 .mac 前触发内核初始化
  G4VisManager *visManager = nullptr;

  // 用户界面管理器
  G4UImanager *UImanager = G4UImanager::GetUIpointer();

  // 批处理或交互式
  if (!ui)
  {
    G4String command = "/control/execute ";
    G4String fileName = argv[1];
    UImanager->ApplyCommand(command + fileName);
  }
  else
  {
    visManager = new G4VisExecutive;
    visManager->Initialize();

    UImanager->ApplyCommand("/control/execute init_vis.mac");
    ui->SessionStart();
    delete ui;
  }

  delete visManager;
  delete analysisMessenger;
  delete runManager;
  delete analysisConfig;
  return 0;
}