#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

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

  // 探测器构造
  runManager->SetUserInitialization(new DetectorConstruction());

  // 物理过程初始化
  auto *physicsList = new PhysicsList();
  physicsList->SetVerboseLevel(1);
  runManager->SetUserInitialization(physicsList);

  // 动作初始化
  runManager->SetUserInitialization(new ActionInitialization());

  // 可视化
  G4VisManager *visManager = new G4VisExecutive;
  visManager->Initialize();

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
    UImanager->ApplyCommand("/control/execute init_vis.mac");
    ui->SessionStart();
    delete ui;
  }

  delete visManager;
  delete runManager;

  return 0;
}
