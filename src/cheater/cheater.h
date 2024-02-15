/**
 * Copyright 2023 <Hardworking Bee>
 */

#ifndef SRC_CHEATER_CHEATER_H_
#define SRC_CHEATER_CHEATER_H_

#include <Windows.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <vector>

using std::function;
using std::vector;

namespace cheater {

typedef NTSTATUS(NTAPI* NtSuspendProcess)(IN HANDLE Process);
typedef NTSTATUS(NTAPI* NtResumeProcess)(IN HANDLE Process);
typedef function<void(LPVOID)> ReadGameMemoryCb;

void ImportNtDllFunctions();
void ShowErrorMessageBox(LPCTSTR);
bool CheckAllHandle();
void InitHeap();
DWORD GetRealAddress(vector<DWORD>);
void ReadGameMemory(LPCVOID, SIZE_T, ReadGameMemoryCb);
void WriteGameMemory(LPVOID, LPCVOID, SIZE_T);
void OpenGameProcess(HWND);

class WorkerThread : public QThread {
  Q_OBJECT

 public:
  explicit WorkerThread(QObject* parent = nullptr);

 signals:
  void GameTerminated();

 protected:
  void run() override;
};

class Cheater : public QWidget {
  Q_OBJECT

 public:
  explicit Cheater(QWidget* parent = nullptr);

 private:
  // 不需要使用智能指针，详细请看 Qt 内存管理。
  QVBoxLayout* layout_{nullptr};
  QGroupBox* checkbox_group_{nullptr};
  QGridLayout* checkbox_layout_{nullptr};
  // 一键取消
  QPushButton* cancel_button_{nullptr};
  // 无限阳光。
  QCheckBox* sun_checkbox_{nullptr};
  // 植物无冷却。
  QCheckBox* no_plant_cd_checkbox_{nullptr};
  // 植物不死。
  QCheckBox* no_plant_death_checkbox_{nullptr};
  // 僵尸不死。
  QCheckBox* no_zombie_death_checkbox_{nullptr};
  // 一击必杀。
  QCheckBox* kill_checkbox_{nullptr};
  // 后台模式。
  QCheckBox* bg_mode_checkbox_{nullptr};
  // 无限金币。
  QCheckBox* money_checkbox_{nullptr};
  QTimer* timer_{nullptr};
  QTimer* sun_timer_{nullptr};
  WorkerThread* worker_thread_{nullptr};

  void SetupUI();
  void InitTimer();
  void MonitorGameTerminated();

 private slots:
  void OnTimerTimeout();
  void OnGameTerminated();
  void OnSunCheckBoxChange();
  void OnNoPlantCdCheckBoxChange();
  void OnNoPlantDeathCheckBoxChange();
  void OnNoZombieDeathCheckBoxChange();
  void OnKillCheckBoxChange();
  void OnBgModeCheckBoxChange();
  void OnMoneyCheckBoxChange();
  void OnCancelButtonClick();
};

}  // namespace cheater
#endif  // SRC_CHEATER_CHEATER_H_
