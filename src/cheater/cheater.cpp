/**
 * Copyright 2023 <Hardworking Bee>
 */

#include "cheater.h"

#include <strsafe.h>

#include <QString>

using std::make_unique;

namespace cheater {

static HANDLE handle = nullptr;
static HANDLE heap_handle = nullptr;
static NtSuspendProcess SuspendProcess = nullptr;
static NtResumeProcess ResumeProcess = nullptr;
static const DWORD kSun = 9990;
static const DWORD kMoney = 99999;

void ImportNtDllFunctions() {
  auto ntdll = GetModuleHandle(TEXT("ntdll.dll"));
  if (ntdll == nullptr) {
    ShowErrorMessageBox(TEXT("GetModuleHandle"));
    return;
  }

  SuspendProcess = reinterpret_cast<NtSuspendProcess>(
      GetProcAddress(ntdll, "NtSuspendProcess"));
  if (SuspendProcess == nullptr) {
    ShowErrorMessageBox(TEXT("GetProcAddress NtSuspendProcess"));
    return;
  }

  ResumeProcess = reinterpret_cast<NtResumeProcess>(
      GetProcAddress(ntdll, "NtResumeProcess"));
  if (ResumeProcess == nullptr) {
    ShowErrorMessageBox(TEXT("GetProcAddress NtResumeProcess"));
    return;
  }
}

void ShowErrorMessageBox(LPCTSTR function_name) {
  LPVOID error_message = nullptr;
  LPVOID display_buffer = nullptr;
  DWORD error_code = GetLastError();

  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&error_message, 0, nullptr);

  display_buffer = (LPVOID)LocalAlloc(
      LMEM_ZEROINIT,
      (lstrlen((LPCTSTR)error_message) + lstrlen(function_name) + 40) *
          sizeof(TCHAR));

  StringCchPrintf((LPTSTR)display_buffer,
                  LocalSize(display_buffer) / sizeof(TCHAR),
                  TEXT("%s failed with error %d: %s"), function_name,
                  error_code, error_message);

  MessageBox(nullptr, (LPCTSTR)display_buffer, TEXT("Error"), MB_OK);

  LocalFree(error_message);
  LocalFree(display_buffer);
}

bool CheckAllHandle() { return handle != nullptr && heap_handle != nullptr; }

void InitHeap() {
  heap_handle = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
  if (heap_handle == nullptr) {
    ShowErrorMessageBox(TEXT("HeapCreate"));
  }
}

/**
 * 获取真实地址。
 * @param offsets 偏移量，第一个元素是基址，后面的元素是偏移量。
 * @return 如果该函数成功返回真实地址，否则返回 0。
 */
DWORD GetRealAddress(vector<DWORD> offsets) {
  if (!CheckAllHandle()) return 0;
  // LPVOID a = 0x0;
  DWORD real_address = 0;
  DWORD tmp_address = 0;

  for (auto offset : offsets) {
    real_address = tmp_address + offset;
    auto result =
        ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(real_address),
                          &tmp_address, sizeof(tmp_address), nullptr);
    if (result == FALSE) {
      ShowErrorMessageBox(TEXT("GetRealAddress"));
      return 0;
    }
  }
  return real_address;
}

/**
 * 读取游戏内存。
 * @param base_address 基址。
 * @param size 大小。
 */
void ReadGameMemory(LPCVOID base_address, SIZE_T size, ReadGameMemoryCb CB) {
  if (!CheckAllHandle()) return;

  auto buffer = HeapAlloc(heap_handle, HEAP_ZERO_MEMORY, size);
  if (buffer == nullptr) {
    ShowErrorMessageBox(TEXT("ReadGameMemory HeapAlloc"));
    return;
  }

  auto result = ReadProcessMemory(handle, base_address, buffer, size, nullptr);
  if (result == FALSE) {
    ShowErrorMessageBox(TEXT("ReadGameMemory ReadProcessMemory"));
  } else {
    CB(buffer);
  }

  auto heap_free_result = HeapFree(heap_handle, 0, buffer);
  if (heap_free_result == FALSE) {
    ShowErrorMessageBox(TEXT("ReadGameMemory HeapFree"));
  }
}

void WriteGameMemory(LPVOID base_address, LPCVOID buffer, SIZE_T size) {
  if (handle == nullptr || SuspendProcess == nullptr ||
      ResumeProcess == nullptr)
    return;

  /**
   * 修改进程内存之前，我们需要先挂起目标进程。
   * 参考：https://stackoverflow.com/questions/11010165/how-to-suspend-resume-a-process-in-windows、https://blog.csdn.net/weixin_42112038/article/details/126243863
   */
  SuspendProcess(handle);
  auto result = WriteProcessMemory(handle, base_address, buffer, size, nullptr);
  ResumeProcess(handle);
  if (result == FALSE) {
    ShowErrorMessageBox(TEXT("WriteGameMemory"));
    return;
  }
}

void OpenGameProcess(HWND hwnd) {
  DWORD pid;
  auto tid = GetWindowThreadProcessId(hwnd, &pid);
  if (tid == 0) {
    ShowErrorMessageBox(TEXT("OpenGameProcess GetWindowThreadProcessId"));
    return;
  }

  handle =
      OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE | PROCESS_VM_WRITE |
                      PROCESS_VM_READ | PROCESS_VM_OPERATION,
                  FALSE, pid);
  if (handle == nullptr) {
    ShowErrorMessageBox(TEXT("OpenGameProcess OpenProcess"));
    return;
  }
}

WorkerThread::WorkerThread(QObject* parent) : QThread(parent) {}

void WorkerThread::run() {
  if (handle == nullptr) return;

  auto waiting_code = WaitForSingleObject(handle, INFINITE);
  if (waiting_code == WAIT_FAILED) {
    ShowErrorMessageBox(TEXT("WaitForSingleObject"));
    return;
  }

  emit GameTerminated();
}

Cheater::Cheater(QWidget* parent) : QWidget(parent) {
  ImportNtDllFunctions();
  InitHeap();
  SetupUI();
  InitTimer();
}

void Cheater::closeEvent(QCloseEvent* event) {
  OnCancelButtonClick();
  QTimer::singleShot(200, this, [&]() { event->accept(); });
}

void Cheater::SetupUI() {
  setWindowTitle(QString("PVZ - Cheater"));
  setMinimumSize(300, 200);

  layout_ = new QVBoxLayout(this);

  checkbox_group_ = new QGroupBox(QString("作弊选项"));
  layout_->addWidget(checkbox_group_);
  checkbox_layout_ = new QGridLayout(checkbox_group_);

  cancel_button_ = new QPushButton(QString("一键取消"));
  connect(cancel_button_, &QPushButton::clicked, this,
          &Cheater::OnCancelButtonClick);
  layout_->addWidget(cancel_button_);

  sun_checkbox_ = new QCheckBox(QString("无限阳光"));
  connect(sun_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnSunCheckBoxChange);
  checkbox_layout_->addWidget(sun_checkbox_, 0, 0);

  no_plant_cd_checkbox_ = new QCheckBox(QString("植物无冷却"));
  connect(no_plant_cd_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnNoPlantCdCheckBoxChange);
  checkbox_layout_->addWidget(no_plant_cd_checkbox_, 0, 1);

  no_plant_death_checkbox_ = new QCheckBox(QString("植物不死"));
  connect(no_plant_death_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnNoPlantDeathCheckBoxChange);
  checkbox_layout_->addWidget(no_plant_death_checkbox_, 1, 0);

  no_zombie_death_checkbox_ = new QCheckBox(QString("僵尸不死"));
  connect(no_zombie_death_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnNoZombieDeathCheckBoxChange);
  checkbox_layout_->addWidget(no_zombie_death_checkbox_, 1, 1);

  kill_checkbox_ = new QCheckBox(QString("一击必杀"));
  connect(kill_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnKillCheckBoxChange);
  checkbox_layout_->addWidget(kill_checkbox_, 2, 0);

  bg_mode_checkbox_ = new QCheckBox(QString("后台模式"));
  connect(bg_mode_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnBgModeCheckBoxChange);
  checkbox_layout_->addWidget(bg_mode_checkbox_, 2, 1);

  money_checkbox_ = new QCheckBox(QString("无限金币"));
  connect(money_checkbox_, &QCheckBox::toggled, this,
          &Cheater::OnMoneyCheckBoxChange);
  checkbox_layout_->addWidget(money_checkbox_, 3, 0);
}

void Cheater::InitTimer() {
  // 监视游戏窗口定时器。
  timer_ = new QTimer(this);
  connect(timer_, &QTimer::timeout, this, &Cheater::OnTimerTimeout);
  timer_->start(1000);

  // 监视阳光定时器。
  sun_timer_ = new QTimer(this);
  connect(sun_timer_, &QTimer::timeout, this, &Cheater::OnSunCheckBoxChange);
}

void Cheater::MonitorGameTerminated() {
  if (worker_thread_ == nullptr) {
    worker_thread_ = new WorkerThread(this);
    connect(worker_thread_, &WorkerThread::GameTerminated, this,
            &Cheater::OnGameTerminated);
    connect(worker_thread_, &WorkerThread::finished, worker_thread_,
            &QObject::deleteLater);
  }

  worker_thread_->start();
}

void Cheater::OnTimerTimeout() {
  auto hwnd = FindWindowW(nullptr, TEXT("Plants vs. Zombies"));
  if (hwnd == nullptr && isEnabled()) {
    setEnabled(false);
  } else if (hwnd != nullptr) {
    timer_->stop();
    OpenGameProcess(hwnd);
    MonitorGameTerminated();
    setEnabled(true);
  }
}

void Cheater::OnGameTerminated() {
  auto result = CloseHandle(handle);
  if (result == FALSE) {
    ShowErrorMessageBox(TEXT("OnGameTerminated CloseHandle"));
    return;
  }
  handle = nullptr;
  timer_->start(1000);
}

/**
 * 无限阳光。
 *
 * 增加阳光数量的汇编代码：
 * 0041F4D0 - 01 88 78550000  - add [eax+00005578],ecx
 * 0041F4D6 - 8B 88 78550000  - mov ecx,[eax+00005578]
 *
 * 将第一行改为：
 * 0041F4D0 - 90 90 90909090  - nop
 *
 * 减少阳光数量的汇编代码：
 * 0041F634 - 2B F3           - sub esi,ebx
 * 0041F636 - 89 B7 78550000  - mov [edi+00005578],esi
 *
 * 将第一行改为：
 * 0041F634 - 90 90           - nop
 *
 * 阳光值地址：17E183C0
 * 第一次找到的指针地址：17E12E48，偏移量：00005578
 * 第二次找到的指针地址：039DB2E8，偏移量：00000868
 * 最终找到阳光基址：00731C50 00731CDC，这两个都可以用。
 * 参考：https://zhuanlan.zhihu.com/p/159033119
 */
void Cheater::OnSunCheckBoxChange() {
  auto sun_address = GetRealAddress({0x00731C50, 0x00000868, 0x00005578});
  if (sun_address == 0) return;

  DWORD tmp_value = 0;

  ReadGameMemory(
      reinterpret_cast<LPCVOID>(sun_address), sizeof(kSun),
      [&](LPVOID buffer) { tmp_value = *reinterpret_cast<DWORD*>(buffer); });

  if (sun_checkbox_->isChecked() && tmp_value != kSun) {
    WriteGameMemory(reinterpret_cast<LPVOID>(sun_address), &kSun, sizeof(kSun));
    // buffer 还可以使用 BYTE 数组的形式。
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0041F4D0),
                    "\x90\x90\x90\x90\x90\x90", 6);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0041F634), "\x90\x90", 2);
    // 因为切换场景时，阳光值会被重置，所以我们需要定时修改阳光值。
    sun_timer_->start(1000);
  } else if (!sun_checkbox_->isChecked()) {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0041F4D0),
                    "\x01\x88\x78\x55\x00\x00", 6);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0041F634), "\x2B\xF3", 2);
    sun_timer_->stop();
  }
}

/**
 * 植物无冷却。
 *
 * 植物大战僵尸的 CD 在可选时为 0，不可选时会一直自增到一定数值后，重新赋值为
 * 0。参考：https://blog.chenmo1212.cn/?p=58
 *
 * 修改植物冷却时间的汇编代码：
 * 004958BC - FF 47 24           - inc [edi+24]
 * 004958BF - 8B 47 24           - mov eax,[edi+24]
 * 004958C2 - 3B 47 28           - cmp eax,[edi+28]
 * 004958C5 - 7E 14              - jle 004958DB
 * 004958C7 - C7 47 24 00000000  - mov [edi+24],00000000
 * 004958CE - C6 47 49 00        - mov byte ptr [edi+49],00
 * 004958D2 - C6 47 48 01        - mov byte ptr [edi+48],01
 * 004958D6 - E8 E5FEFFFF        - call 004957C0
 * 004958DB - 8B 47 3C           - mov eax,[edi+3C]
 *
 * 将第三行改为：
 * 004958C2 - 39 47 28           - cmp [edi+28],eax
 *
 * 以下两行也与植物冷却时间有关（但可以不改，有可能是 CD 动画）：
 * 0056B11F - 03 48 0C           - add ecx,[eax+0C]
 * 0056B122 - 89 8D 58F3FFFF     - mov [ebp-00000CA8],ecx
 */
void Cheater::OnNoPlantCdCheckBoxChange() {
  if (no_plant_cd_checkbox_->isChecked()) {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004958C2), "\x39\x47\x28", 3);
  } else {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004958C2), "\x3B\x47\x28", 3);
  }
}

/**
 * 植物不死。
 *
 * 修改植物生命值的汇编代码：
 * 005447A0 - 83 46 40 FC - add dword ptr [esi+40],-04
 *
 * 改为：
 * 005447A0 - 90 90 90 90 - nop
 *
 */
void Cheater::OnNoPlantDeathCheckBoxChange() {
  if (no_plant_death_checkbox_->isChecked()) {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x005447A0), "\x90\x90\x90\x90",
                    4);
  } else {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x005447A0), "\x83\x46\x40\xFC",
                    4);
  }
}

/**
 * 僵尸不死。
 */
void Cheater::OnNoZombieDeathCheckBoxChange() {
  if (no_zombie_death_checkbox_->isChecked()) {
    kill_checkbox_->setEnabled(false);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545DFA), "\x90\x90\x90\x90",
                    4);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545B14), "\x90\x90", 2);
  } else {
    kill_checkbox_->setEnabled(true);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545DFA), "\x2B\x6C\x24\x20",
                    4);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545B14), "\x2B\xC8", 2);
  }
}

/**
 * 一击必杀。
 *
 * 修改僵尸生命值的汇编代码：
 * 00545DFA - 2B 6C 24 20     - sub ebp,[esp+20]
 * 00545E04 - 89 AF C8000000  - mov [edi+000000C8],ebp
 *
 * 将第一行改为：
 * 00545DFA - 2B ED 90 90     - sub ebp,ebp
 *
 * 修改带有护甲的僵尸生命值的汇编代码：
 * 00545B14 - 2B C8           - sub ecx,eax
 * 00545B1A - 89 8D D0000000  - mov [ebp+000000D0],ecx
 *
 * 将第一行改为：
 * 00545B14 - 29 C9           - sub ecx,ecx
 */
void Cheater::OnKillCheckBoxChange() {
  if (kill_checkbox_->isChecked()) {
    no_zombie_death_checkbox_->setEnabled(false);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545DFA), "\x2B\xED\x90\x90",
                    4);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545B14), "\x29\xC9", 2);
  } else {
    no_zombie_death_checkbox_->setEnabled(true);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545DFA), "\x2B\x6C\x24\x20",
                    4);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x00545B14), "\x2B\xC8", 2);
  }
}

/**
 * 后台模式。
 *
 * 使用 x64dbg 搜索弹窗字符串，可以找到弹窗 call
 * 所在函数，需要注意弹窗显示的字符串是经过格式化的。
 *
 * 弹窗 call：
 * 00455439 - FFD0  - call eax
 *
 * 弹窗 call 所在函数地址：
 * 00455330
 *
 * 调用 00455330 函数的汇编代码：
 * 004540D0 - 56                       - push esi
 * 004540D1 - 8BF1                     - mov esi,ecx
 * 004540D3 - 80BE 15090000 00         - cmp byte ptr ds:[esi+915],0
 * 004540DA - 75 0F                    - jne 004540EB
 * 004540DC - E8 9FFFFFFF              - call 00454080
 * 004540E1 - 84C0                     - test al,al
 * 004540E3 - 74 06                    - je 004540EB
 * 004540E5 - 56                       - push esi
 * 004540E6 - E8 45120000              - call 00455330
 * 004540EB - 5E                       - pop esi
 * 004540EC - C3                       - ret
 *
 * 将跳转语句改为：
 * 004540DA - EB 0F                    - jmp 004540EB
 * 004540E3 - EB 06                    - jmp 004540EB
 */
void Cheater::OnBgModeCheckBoxChange() {
  if (bg_mode_checkbox_->isChecked()) {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004540DA), "\xEB\x0F", 2);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004540E3), "\xEB\x06", 2);
  } else {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004540DA), "\x75\x0F", 2);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004540E3), "\x74\x06", 2);
  }
}

/**
 * 无限金币。
 *
 * 增加金币数量的汇编代码：
 * 0043478C - 01 50 54  - add [eax+54],edx
 * 0043478F - 8B 48 54  - mov ecx,[eax+54]
 *
 * 将第一行改为：
 * 0043478C - 90 90 90  - nop
 *
 * 减少金币数量的汇编代码：
 * 004416E8 - 29 78 54  - sub [eax+54],edi
 * 004416EB - 8B 48 54  - mov ecx,[eax+54]
 *
 * 将第一行改为：
 * 004416E8 - 90 90 90  - nop
 *
 * PVZ 显示的金币实际上是金币值 x 10，所以查找的时候需要除以 10。
 * 金币地址：1837BA64
 * 第一次找到的指针地址：1837BA10，偏移量：00000054
 * 第二次找到的指针地址：03B0B2A8，偏移量：0000094C
 * 最终找到金币基址：00731C50 00731CDC，这两个都可以用。
 * 可以看到金币的基址和阳光的基址是一样的。
 */
void Cheater::OnMoneyCheckBoxChange() {
  if (money_checkbox_->isChecked()) {
    auto money_address = GetRealAddress({0x00731C50, 0x0000094C, 0x00000054});
    if (money_address != 0)
      WriteGameMemory(reinterpret_cast<LPVOID>(money_address), &kMoney,
                      sizeof(kMoney));
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0043478C), "\x90\x90\x90", 3);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004416E8), "\x90\x90\x90", 3);
  } else {
    WriteGameMemory(reinterpret_cast<LPVOID>(0x0043478C), "\x01\x50\x54", 3);
    WriteGameMemory(reinterpret_cast<LPVOID>(0x004416E8), "\x29\x78\x54", 3);
  }
}

/**
 * 一键取消。
 */
void Cheater::OnCancelButtonClick() {
  sun_checkbox_->setChecked(false);
  no_plant_cd_checkbox_->setChecked(false);
  no_plant_death_checkbox_->setChecked(false);
  no_zombie_death_checkbox_->setChecked(false);
  kill_checkbox_->setChecked(false);
  bg_mode_checkbox_->setChecked(false);
  money_checkbox_->setChecked(false);
}

}  // namespace cheater
