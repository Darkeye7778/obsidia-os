# Obsidia OS Commercial Parity Audit v1

Last audited: 2026-09-08
Repository/branch: `obsidia-os` / `updates`
Commit: `fc510e5` plus current working-tree milestone

Complete: 129 / 500
Partial: 56 / 500
Open: 315 / 500
Remaining to strict parity: 371
Strict commercial parity: 25.8%

Current milestone: Platform-time foundation and deterministic service readiness — completed and verified

Highest-priority blockers:

- NVMe, queued block I/O, broader AHCI hardware validation, and storage repair tooling
- Larger general-purpose storage, permissions, free-space reporting, and repair tooling
- Reboot verification and broader platform power management
- USB xHCI/HID and real-hardware input coverage
- Ethernet, IPv4/IPv6, DHCP, DNS, TCP, sockets, and firewalling
- User accounts, authentication, file permissions, secure RNG, ASLR, and secrets storage
- Service supervision, crash restart, health reporting, and structured logs
- Installer, signed transactional updates, rollback, and recovery environment
- Accelerated GPU/display-mode/multimonitor support
- Core daily-use applications and a stable native GUI toolkit

States are strict: `[x]` is implemented and meaningfully verified, `[~]` is real but not commercially sufficient, and `[ ]` is absent or only a stub.

## A. Kernel, Processes & Scheduling — 20

A001 [x] Stable ring-0/ring-3 privilege separation.
A002 [x] Isolated per-process virtual address spaces.
A003 [x] Reliable process creation.
A004 [x] Reliable process termination.
A005 [x] Zombie/process reaping without leaks.
A006 [x] Parent/child process relationships.
A007 [x] Process wait/completion semantics.
A008 [ ] Multiple threads per process.
A009 [ ] Thread creation API.
A010 [ ] Thread termination/join semantics.
A011 [x] Preemptive userspace scheduler.
A012 [~] Fair scheduler behavior under CPU contention.
A013 [ ] Scheduler priority model.
A014 [ ] SMP/multicore scheduling.
A015 [ ] CPU affinity foundation.
A016 [~] Robust kernel synchronization primitives.
A017 [ ] User synchronization/futex-equivalent primitive.
A018 [x] Stable process information/introspection API.
A019 [~] Process resource/accounting foundation.
A020 [~] Stress-tested process/thread lifecycle without resource drift.

Evidence (A001–A007, A011–A012, A016, A018–A020): `kernel/task.c`, `kernel/paging.c`, `user/tests/process-info.c`, lifecycle/free-page regression passes.

## B. Memory & Virtual Memory — 20

B001 [x] Physical memory manager.
B002 [x] Kernel heap allocator.
B003 [x] User virtual memory mapping.
B004 [x] User virtual memory unmapping.
B005 [x] Anonymous mappings.
B006 [x] Shared-memory objects.
B007 [x] Per-process memory-region tracking.
B008 [x] W^X enforcement.
B009 [x] NX support.
B010 [x] CR0.WP protection.
B011 [ ] SMEP or architecture-equivalent kernel hardening.
B012 [x] User-copy validation centralized.
B013 [ ] Guard-page support.
B014 [ ] Demand paging.
B015 [ ] Copy-on-write.
B016 [ ] Memory-mapped files.
B017 [ ] Page-cache architecture.
B018 [ ] Memory pressure handling.
B019 [ ] Out-of-memory policy/recovery.
B020 [~] Long-duration allocation/free stress with no unexplained leak.

Evidence (B001–B010, B012, B020): `kernel/memory/`, `kernel/paging.c`, `kernel/usercopy.c`, `user/tests/vm.c`, shared-memory and lifecycle regressions.

## C. Filesystems & Persistent Storage — 20

C001 [x] Read-only boot/init filesystem.
C002 [x] Hierarchical VFS.
C003 [x] Open-file objects.
C004 [x] File read.
C005 [x] File write.
C006 [x] Directory enumeration.
C007 [x] File/directory creation.
C008 [x] File deletion.
C009 [x] Directory deletion.
C010 [x] Rename/move.
C011 [x] File metadata/stat.
C012 [~] Timestamps.
C013 [x] Persistent writable filesystem mounted from disk.
C014 [~] Filesystem allocation/free-space tracking.
C015 [x] Safe filesystem synchronization/flush.
C016 [x] Atomic file replacement/rename semantics.
C017 [x] Corruption detection.
C018 [~] Filesystem recovery/fsck equivalent.
C019 [~] Mount/unmount support.
C020 [~] Power-loss persistence/recovery test in QEMU.

Evidence (C001–C020): `kernel/vfs/`, `user/include/obsidia/fs.h`, `user/tests/fs.c`; v1 migration, hierarchical create/enumerate/stat/move/delete, reboot reload, and corrupt-newest-slot fallback verified in QEMU 2026-09-07. Timestamps are durable boot-relative ticks, not wall-clock values.

## D. Block Devices, Storage Drivers & Volumes — 20

D001 [x] Generic block-device layer.
D002 [~] ATA/PIO bootstrap driver.
D003 [~] AHCI/SATA driver.
D004 [ ] NVMe driver.
D005 [x] Device geometry/capacity discovery.
D006 [ ] Block caching.
D007 [~] Asynchronous block I/O foundation.
D008 [x] Partition-table parsing.
D009 [x] GPT support.
D010 [~] MBR support where useful.
D011 [x] Volume abstraction.
D012 [ ] Multiple mounted volumes.
D013 [ ] Removable-volume detection.
D014 [ ] Safe removable-volume eject.
D015 [~] Disk identity/model/serial reporting.
D016 [~] Disk health/error reporting.
D017 [~] I/O timeout handling.
D018 [~] Faulted-device recovery behavior.
D019 [ ] Storage hotplug where hardware supports it.
D020 [ ] Large-volume stress/integrity testing.

Evidence (D001–D003, D005, D007–D011, D015–D018): `kernel/block.c`, `kernel/partition.c`, `kernel/ata.c`, `kernel/ahci.c`, `kernel/apic.c`, `tools/mkstate_disk.py`; validated MBR/GPT discovery, ATA/AHCI IDENTIFY metadata, interrupt-driven AHCI read/write/flush, timeout accounting, and bounded COMRESET/retry logic. Fresh-GPT two-boot persistence and both ATA/AHCI full regressions were verified in QEMU 2026-09-08. Asynchronous I/O remains partial because callers use a serialized synchronous block API without a request queue.

## E. Device Model, PCI & Driver Infrastructure — 20

E001 [x] Centralized device initialization.
E002 [~] Generic device representation.
E003 [x] PCI enumeration.
E004 [x] PCI configuration-space access.
E005 [x] PCI capability parsing.
E006 [~] PCIe awareness.
E007 [ ] Driver matching/binding framework.
E008 [ ] Driver lifecycle model.
E009 [~] Device ownership/resource tracking.
E010 [~] IRQ resource assignment.
E011 [~] MMIO resource mapping.
E012 [~] DMA foundation.
E013 [x] Safe DMA buffer allocation.
E014 [ ] IOMMU foundation where supported.
E015 [ ] Device hotplug notification model.
E016 [ ] Driver failure containment strategy.
E017 [ ] Device-manager userspace interface.
E018 [ ] Hardware inventory API.
E019 [x] Unknown-device safe handling.
E020 [~] Driver/device diagnostics infrastructure.

Evidence (E001–E006, E009–E013, E019–E020): `kernel/device.c`, `kernel/pci.c`, `kernel/paging.c`, `kernel/dma.c`, `kernel/ahci.c`; bounded cycle-safe PCI capability walking, MSI programming, cache-disabled PCI MMIO, and below-4-GiB contiguous DMA allocation/reclamation were exercised by AHCI production and regression QEMU 2026-09-08. DMA remains partial without scatter/gather policy or IOMMU integration.

## F. USB & Human Input — 20

F001 [x] PS/2 keyboard bootstrap.
F002 [x] PS/2 mouse bootstrap.
F003 [x] Structured keyboard events.
F004 [x] Structured pointer events.
F005 [x] Input focus routing.
F006 [x] Reliable high-rate pointer batching.
F007 [~] USB host-controller architecture.
F008 [ ] xHCI support.
F009 [ ] USB device enumeration.
F010 [ ] USB hubs.
F011 [ ] USB HID keyboard.
F012 [ ] USB HID mouse.
F013 [ ] HID generic report parsing.
F014 [ ] Gamepad/controller support.
F015 [ ] Touchscreen support.
F016 [ ] Precision touchpad foundation.
F017 [ ] Keyboard layouts.
F018 [ ] Input device hotplug.
F019 [ ] Per-device input configuration.
F020 [~] Input latency/stress validation.

Evidence (F001–F007, F020): `kernel/drivers/keyboard.c`, `kernel/drivers/mouse.c`, `kernel/resource.c`, `user/services/inputd/`; physical and high-rate synthetic input verified.

## G. Display, GPU & Graphics — 20

G001 [x] Boot framebuffer support.
G002 [x] Kernel display-resource isolation.
G003 [x] Userspace compositor.
G004 [x] Shared application surfaces.
G005 [x] Application framebuffer isolation.
G006 [x] Damage tracking.
G007 [x] Window composition/z-order.
G008 [x] Software cursor.
G009 [ ] Multiple display modes/resolution changes.
G010 [ ] EDID parsing.
G011 [ ] Multiple-monitor topology.
G012 [ ] Per-monitor work areas.
G013 [ ] Display hotplug.
G014 [~] DPI/scaling architecture.
G015 [ ] GPU driver architecture.
G016 [ ] At least one accelerated GPU backend.
G017 [ ] GPU-accelerated composition.
G018 [ ] 2D hardware acceleration API.
G019 [ ] 3D graphics API/runtime foundation.
G020 [ ] Stable high-resolution accelerated desktop performance.

Evidence (G001–G008, G014): `kernel/display.c`, `kernel/gui/surface.c`, `user/services/displayd/`, display isolation and window regression suites.

## H. Platform, ACPI, Boot & Power — 20

H001 [x] Reliable bootloader integration.
H002 [~] Boot configuration handling.
H003 [x] ACPI table discovery.
H004 [x] ACPI parsing foundation.
H005 [x] APIC/local APIC support.
H006 [x] IOAPIC support.
H007 [x] Modern interrupt routing.
H008 [x] HPET/high-resolution timer support.
H009 [x] Clean software shutdown.
H010 [~] Clean reboot.
H011 [ ] Physical power-button event.
H012 [ ] CPU idle states.
H013 [ ] CPU frequency/power-state foundation.
H014 [ ] Battery detection.
H015 [ ] Battery charge reporting.
H016 [ ] AC adapter reporting.
H017 [ ] Suspend-to-RAM.
H018 [ ] Reliable resume.
H019 [ ] Lid-close event support.
H020 [ ] Thermal/fan/platform status foundation.

Evidence (H001–H010): `kernel/acpi.c`, `kernel/apic.c`, `kernel/hpet.c`, `kernel/idt.c`, `kernel/pci.c`, `kernel/resource.c`, `user/include/obsidia/time.h`, `user/tests/system-control.c`, `user/tests/time.c`; QEMU validated MADT discovery, local-APIC EOI, IOAPIC timer/PS2 routing, dynamically allocated MSI delivery, PCI INTx fallback, legacy PIC fallback, HPET monotonic time with explicit resolution, the no-HPET PIT fallback, and synchronized ACPI shutdown after complete regressions on 2026-09-08. Reboot has not yet completed a reset-cycle integration test; x2APIC and SMP policy belong to their separate checklist items.

## I. Networking Stack — 20

I001 [ ] Generic network-device API.
I002 [ ] At least one Ethernet NIC driver.
I003 [ ] Ethernet frame handling.
I004 [ ] ARP.
I005 [ ] IPv4.
I006 [ ] ICMPv4.
I007 [ ] UDP.
I008 [ ] TCP.
I009 [ ] DHCP client.
I010 [ ] DNS resolver.
I011 [ ] IPv6.
I012 [ ] ICMPv6.
I013 [ ] Neighbor Discovery.
I014 [ ] IPv6 autoconfiguration.
I015 [ ] Socket-like native application API.
I016 [ ] Multiple network interfaces.
I017 [ ] Routing table.
I018 [ ] Network interface configuration service.
I019 [ ] Firewall foundation.
I020 [ ] Network-stack fuzz/stress testing.

## J. Wi-Fi, Bluetooth & Wireless — 20

J001 [ ] Wi-Fi driver architecture.
J002 [ ] At least one supported Wi-Fi chipset.
J003 [ ] Access-point scanning.
J004 [ ] Wi-Fi association.
J005 [ ] WPA2 authentication.
J006 [ ] WPA3 foundation.
J007 [ ] Saved Wi-Fi networks.
J008 [ ] Wireless reconnect.
J009 [ ] Airplane-mode concept.
J010 [ ] Bluetooth controller/HCI layer.
J011 [ ] Bluetooth device discovery.
J012 [ ] Pairing.
J013 [ ] Bond storage.
J014 [ ] Bluetooth HID.
J015 [ ] Bluetooth audio foundation.
J016 [ ] Bluetooth device removal/reconnect.
J017 [ ] Wireless status shell integration.
J018 [ ] Network selection UI.
J019 [ ] Wireless credential security.
J020 [ ] Hardware disconnect/reconnect stress.

## K. IPC, Services & System Daemons — 20

K001 [x] Kernel IPC object.
K002 [x] Blocking IPC.
K003 [x] Nonblocking IPC.
K004 [x] IPC handle transfer.
K005 [x] Authenticated sender identity.
K006 [x] IPC backpressure.
K007 [x] IPC malformed-message isolation.
K008 [x] Shared-memory transport.
K009 [x] Named userspace service registry.
K010 [x] Service capability isolation.
K011 [~] Service ownership/lifetime cleanup.
K012 [x] settingsd-style userspace policy service.
K013 [x] appd-style application catalog service.
K014 [x] Service dependency/start ordering.
K015 [ ] Service supervision.
K016 [ ] Automatic service restart.
K017 [ ] Service health/status API.
K018 [ ] Service crash-loop handling.
K019 [ ] Service startup timeout handling.
K020 [~] System service diagnostic/event log.

Evidence (K001–K014, K020): `kernel/object.c`, `kernel/task.c`, `user/system/serviced/`, `user/lib/service.c`, `user/system/init/main.c`, `settingsd/`, `appd/`; IPC transfer, malformed-client, subscriber-death and service tests. Registration acknowledgment plus bounded discovery barriers enforce the production serviced → appd → settingsd → displayd → inputd → desktop dependency order.

## L. Desktop Shell & Window Management — 20

L001 [x] Real userspace desktop shell.
L002 [x] Window creation.
L003 [x] Window closing.
L004 [x] Window dragging.
L005 [x] Eight-way resizing.
L006 [x] Minimize.
L007 [x] Maximize.
L008 [x] Restore.
L009 [x] Focus and raise.
L010 [x] Active/inactive window presentation.
L011 [x] Grouped taskbar by application.
L012 [x] Pinned applications.
L013 [x] Application launcher.
L014 [x] Shell overlay/pop-up infrastructure.
L015 [x] Multiple-window application switching.
L016 [ ] Alt-Tab/task switcher.
L017 [ ] Taskbar window previews.
L018 [ ] Window snapping.
L019 [ ] Multiple workspaces/virtual desktops.
L020 [ ] Shell crash/restart recovery without losing applications where practical.

Evidence (L001–L015): `user/apps/desktop/`, `user/services/displayd/`, `user/lib/window.c`, `user/lib/shell_model.c`; production screenshots and lifecycle/resize/taskbar regressions.

## M. Desktop UX, Notifications & Clipboard — 20

M001 [x] Desktop shortcuts.
M002 [ ] Wallpaper image support.
M003 [ ] Desktop context menu.
M004 [ ] Application context menus.
M005 [ ] Notification service.
M006 [ ] Notification popup UI.
M007 [ ] Notification history.
M008 [ ] System status/tray area.
M009 [ ] Clock.
M010 [ ] Calendar popup.
M011 [ ] Clipboard service.
M012 [ ] Plain-text clipboard.
M013 [ ] Rich/multiple-format clipboard.
M014 [ ] Clipboard ownership/lifetime semantics.
M015 [ ] Drag-and-drop protocol.
M016 [ ] File drag-and-drop.
M017 [ ] Application badges/progress.
M018 [ ] Global keyboard shortcut service.
M019 [~] Screen capture/screenshot API.
M020 [ ] Polished lock/session/power menu.

Evidence (M001, M019): desktop metadata-backed shortcuts; framebuffer screenshots currently rely on QEMU tooling rather than a complete OS API.

## N. GUI Toolkit & Native Application Framework — 20

N001 [x] Reusable userspace drawing primitives.
N002 [x] Reusable userspace text primitives.
N003 [x] Window application API.
N004 [~] GUI event loop abstraction.
N005 [~] Layout system.
N006 [~] Label widget.
N007 [~] Button widget.
N008 [ ] Text-input widget.
N009 [ ] Checkbox.
N010 [ ] Radio button.
N011 [ ] Slider.
N012 [ ] List view.
N013 [ ] Tree view.
N014 [ ] Scroll view.
N015 [ ] Tabs.
N016 [ ] Menu/context-menu widgets.
N017 [ ] Dialog API.
N018 [ ] File picker.
N019 [ ] Standard application command/action model.
N020 [ ] Stable documented native GUI framework suitable for real apps.

Evidence (N001–N007): `user/lib/gfx.c`, `user/lib/window.c`, public headers and demo applications; current controls/layout remain application-specific primitives.

## O. Text, Fonts, Localization & Accessibility — 20

O001 [x] Bootstrap bitmap font.
O002 [ ] TrueType/OpenType font loading.
O003 [ ] Antialiased font rendering.
O004 [x] Font metrics.
O005 [ ] Unicode text foundation.
O006 [~] UTF-8 application strings.
O007 [x] Font fallback.
O008 [ ] Text shaping.
O009 [ ] Bidirectional text.
O010 [ ] Input method/IME architecture.
O011 [ ] Localization/resource-string framework.
O012 [ ] Locale selection.
O013 [ ] Date/time/number locale formatting.
O014 [ ] High-contrast theme.
O015 [ ] Keyboard-only GUI navigation.
O016 [ ] Accessibility semantic tree.
O017 [ ] Screen-reader service/API.
O018 [~] UI scaling/text scaling.
O019 [ ] Reduced-motion/accessibility preferences.
O020 [ ] Commercial accessibility audit on core shell/apps.

Evidence (O001, O004, O006–O007, O018): `user/lib/gfx.c`, text clipping/measurement tests, runtime text-scale setting; rendering is currently bounded bitmap/ASCII-oriented.

## P. Settings, Time & System Configuration — 20

P001 [x] Typed userspace settings service.
P002 [x] Versioned settings protocol.
P003 [x] Runtime settings subscriptions.
P004 [x] Settings authority/permissions.
P005 [x] Theme configuration.
P006 [x] Panel placement configuration.
P007 [x] Panel sizing.
P008 [~] Text scaling.
P009 [x] Persistent settings storage.
P010 [x] Atomic settings persistence.
P011 [~] Settings migration/version upgrade.
P012 [~] Real user-facing Settings application.
P013 [ ] Settings search.
P014 [ ] Hardware/display settings.
P015 [ ] Keyboard/mouse settings.
P016 [ ] Date/time service.
P017 [x] RTC wall-clock support.
P018 [ ] Time-zone support.
P019 [ ] Network time synchronization.
P020 [~] Settings recovery/reset-to-defaults.

Evidence (P001–P012, P017, P020): `user/system/settingsd/main.c`, `user/lib/settings.c`, `kernel/rtc.c`, `user/include/obsidia/time.h`, `user/tests/time.c`, `settings-demo`; live settings tests plus settings reload from statefs on a second QEMU boot, and a QEMU CMOS UTC read/progression regression on 2026-09-08. Date/time policy, time zones, formatting, and synchronization are not yet services.

## Q. Application Model, Packages & Installation — 20

Q001 [x] Stable application IDs.
Q002 [x] Versioned application manifest.
Q003 [x] Userspace installed-app catalog.
Q004 [x] Manifest validation.
Q005 [x] Format-neutral app catalog records.
Q006 [x] Installed vs portable-app distinction.
Q007 [x] Trusted application capability policy.
Q008 [x] Catalog enumeration.
Q009 [~] Catalog change notifications.
Q010 [ ] Package format.
Q011 [ ] Package signatures.
Q012 [ ] Package ownership database.
Q013 [ ] Package installation.
Q014 [ ] Package uninstall.
Q015 [ ] Package upgrade.
Q016 [ ] Transactional package operations.
Q017 [ ] Dependency handling.
Q018 [ ] Graphical software/package manager.
Q019 [ ] Offline package installation.
Q020 [ ] Package rollback/recovery.

Evidence (Q001–Q009): `user/system/appd/`, `user/lib/app_metadata.c`, rootfs OAM manifests, catalog/manifest/shell tests. Generation exists; live mutation notification is not implemented.

## R. Core User Applications — 20

R001 [ ] File manager.
R002 [ ] Functional terminal emulator.
R003 [~] Command shell.
R004 [ ] Text editor.
R005 [ ] Image viewer.
R006 [ ] PDF/document viewer.
R007 [ ] Calculator.
R008 [ ] Archive/compression manager.
R009 [ ] Task Manager/process viewer.
R010 [ ] System monitor.
R011 [ ] Disk/volume utility.
R012 [ ] Device manager.
R013 [ ] Screenshot utility.
R014 [ ] Logs/event viewer.
R015 [ ] Network configuration app.
R016 [ ] Audio configuration/mixer app.
R017 [ ] Bluetooth/device configuration UI.
R018 [ ] Application/default-program settings.
R019 [ ] Core file-open/save dialogs.
R020 [ ] All essential bundled apps usable without terminal intervention.

Evidence (R003): `user/apps/shell/main.c` is a bootstrap command shell, not a commercially capable terminal/shell environment.

## S. Audio, Media & Camera — 20

S001 [ ] Audio driver architecture.
S002 [ ] At least one supported audio controller.
S003 [ ] PCM output.
S004 [ ] PCM input/microphone.
S005 [ ] Userspace audio service.
S006 [ ] Multiple application audio streams.
S007 [ ] Software mixing.
S008 [ ] Per-app volume.
S009 [ ] Output-device selection.
S010 [ ] Input-device selection.
S011 [ ] Audio hotplug.
S012 [ ] Sample-rate conversion.
S013 [ ] Basic WAV playback/recording.
S014 [ ] Media framework architecture.
S015 [ ] Common image decoders.
S016 [ ] Common audio codecs.
S017 [ ] Common video container/codec path.
S018 [ ] Webcam/camera device architecture.
S019 [ ] Camera capture API.
S020 [ ] Stable consumer audio/video playback experience.

## T. Printing, External Devices & Peripherals — 20

T001 [ ] Printer-device model.
T002 [ ] Printing service/spooler.
T003 [ ] Generic print-job API.
T004 [ ] Print dialog.
T005 [ ] At least one real printer protocol/backend.
T006 [ ] PDF print output.
T007 [ ] Scanner device model.
T008 [ ] Scanner acquisition API.
T009 [ ] USB mass-storage support.
T010 [ ] External HDD/SSD support.
T011 [ ] USB flash drive hotplug.
T012 [ ] Optical media foundation where relevant.
T013 [ ] Game controller configuration.
T014 [ ] Removable-device notifications.
T015 [ ] Camera/media-import handling.
T016 [ ] MTP/PTP foundation.
T017 [ ] Dock/hub handling.
T018 [ ] External device permission model.
T019 [ ] Graceful unsupported peripheral handling.
T020 [ ] Peripheral disconnect-under-load testing.

## U. Users, Authentication & Security — 20

U001 [ ] Explicit user account model.
U002 [ ] Login/authentication.
U003 [ ] Password hashing/storage.
U004 [ ] Lock screen authentication.
U005 [ ] Per-user home/storage separation.
U006 [ ] File ownership.
U007 [ ] File permissions/ACL foundation.
U008 [ ] User vs administrator privilege model.
U009 [ ] Privilege elevation mechanism.
U010 [x] Application sandbox foundation.
U011 [x] Capability rights enforcement.
U012 [ ] Secure secrets/keychain service.
U013 [ ] Cryptographically secure RNG.
U014 [ ] ASLR.
U015 [ ] Stack-smashing protection.
U016 [ ] Hardened userspace allocator strategy.
U017 [ ] Executable/code-signature verification.
U018 [ ] Secure boot-chain strategy.
U019 [ ] Security audit/logging.
U020 [ ] Threat-model + adversarial security regression suite.

Evidence (U010–U011): isolated address spaces, capability handles, reduced-rights IPC transfer, surface isolation and negative security tests.

## V. Windows Application Compatibility — 20

V001 [x] Genuine PE32 recognition.
V002 [x] Genuine PE32+ recognition.
V003 [x] AMD64 PE image mapping.
V004 [~] PE relocations.
V005 [~] PE import resolution.
V006 [~] Windows DLL loader model.
V007 [ ] ntdll-compatible foundation.
V008 [ ] Kernel32/KernelBase foundation.
V009 [ ] Windows thread/synchronization semantics.
V010 [ ] Windows process environment/PEB-style expectations.
V011 [ ] Windows exception/SEH handling.
V012 [ ] Windows filesystem/path semantics.
V013 [ ] Registry-compatible userspace subsystem.
V014 [ ] User32 window/input API foundation.
V015 [ ] GDI32 foundation.
V016 [ ] Winsock compatibility.
V017 [ ] COM/OLE foundation.
V018 [ ] UCRT/MSVCRT compatibility strategy.
V019 [ ] DirectX translation/native compatibility foundation.
V020 [ ] Representative real unmodified Windows desktop applications verified.

Evidence (V001–V006): `kernel/exec/pe.c`, import-free AMD64 PE fixture execution, PE32 recognition, malformed/import rejection regressions; relocation/import/DLL behavior remains foundational only.

## W. Developer Platform, Debugging & Observability — 20

W001 [~] Stable public native userspace ABI.
W002 [x] Native SDK headers.
W003 [~] Native application build documentation.
W004 [x] OBSX tooling.
W005 [ ] User debugger architecture.
W006 [ ] Process attach/debug API.
W007 [ ] Breakpoints.
W008 [ ] Register inspection.
W009 [ ] Stack traces.
W010 [ ] Symbol loading.
W011 [ ] Crash dumps.
W012 [~] Kernel panic dump.
W013 [~] Structured system logs.
W014 [~] Per-service/application logs.
W015 [~] Performance counters.
W016 [ ] CPU profiler foundation.
W017 [ ] Memory profiler foundation.
W018 [ ] IPC/resource inspection tools.
W019 [x] Automated developer test harness.
W020 [~] Public SDK sample applications and documentation.

Evidence (W001–W004, W012–W015, W019–W020): `user/include/obsidia/`, `tools/mkobs.py`, serial fault output, subsystem counters, Makefile regression image and sample applications.

## X. Reliability, Update, Recovery & Installer — 20

X001 [~] Clean boot on supported hardware.
X002 [ ] Deterministic shutdown.
X003 [ ] Installer environment.
X004 [ ] Disk partitioning during install.
X005 [ ] Bootloader installation.
X006 [ ] OS installation to persistent disk.
X007 [ ] Upgrade between Obsidia versions.
X008 [ ] Signed OS update mechanism.
X009 [ ] Transactional/atomic system update.
X010 [ ] Update rollback.
X011 [ ] Driver rollback.
X012 [ ] Recovery boot environment.
X013 [ ] Safe/recovery mode.
X014 [ ] Boot failure detection.
X015 [ ] Filesystem repair tooling.
X016 [ ] Reset/reinstall while preserving user files where possible.
X017 [ ] Backup framework.
X018 [ ] Restore framework.
X019 [ ] Crash/service watchdog infrastructure.
X020 [ ] Sudden-power-loss/update-interruption recovery testing.

Evidence (X001): deterministic QEMU production/regression boot is established; no physical supported-hardware acceptance exists.

## Y. Performance, Hardware Support & Commercial Release — 20

Y001 [ ] Defined minimum hardware requirements.
Y002 [ ] Official supported-hardware matrix.
Y003 [ ] At least one fully supported desktop configuration.
Y004 [ ] At least one fully supported laptop configuration.
Y005 [ ] Intel CPU/platform validation.
Y006 [ ] AMD CPU/platform validation.
Y007 [ ] At least one supported Intel/AMD GPU path.
Y008 [ ] Laptop battery/power validation.
Y009 [ ] USB peripheral compatibility suite.
Y010 [ ] Storage-device compatibility suite.
Y011 [ ] Network-device compatibility suite.
Y012 [ ] Boot-to-desktop performance target.
Y013 [ ] Idle CPU target.
Y014 [ ] Idle memory target.
Y015 [ ] Interactive latency target.
Y016 [ ] 24-hour stability test.
Y017 [ ] Multi-day soak test.
Y018 [ ] Release-build reproducibility.
Y019 [ ] Versioning/release-channel infrastructure.
Y020 [ ] Commercial release acceptance suite passed.

## Milestone History

### 2026-09-08 — Platform-time foundation and deterministic service readiness

Before: 126/500 complete
After: 129/500 complete
Delta: +3
Partial changed: service dependency/start ordering advanced to complete after a reproduced asynchronous-registration race was fixed with acknowledgment and bounded discovery barriers.
Verification:

- `make`
- `make test-build`
- Full AHCI/MSI regression with HPET: `TEST: all regression checks passed`
- Userspace high-resolution monotonic/resolution and CMOS UTC progression test: `TIME: monotonic clock regression passed`
- Full `-machine hpet=off` regression validated the PIT fallback
- Repeated AHCI boot reproduced and then eliminated the service-ready race; appd, settingsd, and displayd readiness now precede dependent clients
- Production AHCI/MSI boot reached the desktop and produced a 1280x720 compositor screenshot
- `git diff --check`

Notes:

- Added bounded ACPI HPET discovery, cache-disabled MMIO initialization, 32/64-bit counter handling, a generic nanosecond monotonic clock ABI, and explicit resolution reporting.
- Added stable CMOS RTC sampling, ACPI century-register use, BCD/binary and 12/24-hour decoding, checked UTC epoch conversion, and a public UTC-seconds API.
- Named service registration now acknowledges committed registry state; production and regression init use bounded service-discovery barriers instead of timing sleeps.
- PIT IRQ0 remains the scheduler clock-event source; date/time policy, calendar formatting, time zones, adjustment, and NTP remain separate open work.

### 2026-09-08 — PCI capabilities/MSI and interrupt-driven AHCI v1

Before: 124/500 complete
After: 126/500 complete
Delta: +2
Partial changed: asynchronous block I/O gained IRQ-driven controller completion but remains partial because the block API is serialized and synchronous; modern interrupt routing advanced to complete after MSI and fallback verification.
Verification:

- `make`
- `make test-build`
- Fresh GPT interrupt-driven AHCI boot: `TEST: all regression checks passed`
- Second AHCI boot: `statefs: v2 hierarchy survived reboot`
- QEMU ICH9 AHCI used dynamically allocated MSI completion
- `-no-acpi` AHCI used PCI INTx/PIC fallback and completed every test
- Legacy ATA/PIO regression: `TEST: all regression checks passed`
- `-no-acpi` AHCI completed all checks through the legacy PIC fallback
- Production AHCI boot reached appd, settingsd, displayd, inputd, and desktop
- PS/2 mouse initialization remained operational with early storage interrupts
- `git diff --check`

Notes:

- Added bounded cycle-safe PCI capability parsing, single-vector MSI programming, conventional level-low PCI INTx routing, bounded shared/dynamic vector handlers, and an interruptible AHCI wait safe inside interrupt-gate syscalls.
- Diagnosed and fixed the first-completion-only edge/level error, shared-vector overwrite risk, and IF-cleared syscall deadlock rather than masking them with longer polling.
- MSI-X, queued asynchronous requests, NCQ, multiple controllers, hotplug, and physical-hardware validation remain pending.

### 2026-09-07 — ACPI MADT and modern interrupt routing v1

Before: 122/500 complete
After: 124/500 complete
Delta: +2
Partial changed: modern interrupt routing gained a real userspace-safe platform foundation but remains partial pending PCI MSI/MSI-X and SMP destination policy.
Verification:

- `make`
- `make test-build`
- Full IOAPIC regression on ATA/PIO: `TEST: all regression checks passed`
- Full IOAPIC regression on AHCI: `TEST: all regression checks passed`
- PIT delivery preempted a ring-3 process through the IOAPIC
- QEMU monitor PS/2 movement changed the composed production framebuffer
- `-no-acpi` PIC fallback completed all regression checks
- Both ACPI runs synchronized and powered off cleanly
- `git diff --check`

Notes:

- Added bounded MADT parsing, cache-disabled xAPIC/IOAPIC mappings, ISA source overrides, explicit driver IRQ routing, local-APIC EOI, and 256 correctly numbered IDT stubs.
- The legacy PIC remains a tested fallback; x2APIC, MSI/MSI-X, SMP routing, balancing, and hotplug remain pending.

### 2026-09-07 — AHCI/SATA DMA storage v1

Before: 120/500 complete
After: 122/500 complete
Delta: +2
Partial changed: AHCI, generic DMA, disk identity/health diagnostics, and bounded fault recovery gained real implementations; device capacity discovery and safe DMA allocation advanced to complete.
Verification:

- `make`
- `make test-build`
- Production QEMU reached the desktop using an explicit PCI AHCI SATA disk
- Fresh AHCI GPT regression boot: `TEST: all regression checks passed`
- Second AHCI regression boot: `statefs: v2 hierarchy survived reboot`
- AHCI regression synchronized and powered off cleanly
- Legacy ATA/PIO full regression still passed
- DMA allocation/free baseline self-test passed on every boot
- Header dependency generation verified across 91 kernel/userspace dependency files
- `git diff --check`

Notes:

- Added PCI class lookup, BAR handling, cache-disabled MMIO mapping, coherent contiguous DMA allocation, serialized AHCI IDENTIFY/read/write/flush, generic identity/error counters, and one bounded COMRESET/retry path.
- Added compiler-generated header dependencies after a stale block-device layout exposed the prior build-system gap; `make test-run` is now reliably headless.
- Interrupt-driven completion, NCQ, hotplug, ATAPI, fault-injected recovery validation, multiple-controller support, and IOMMU isolation remain pending.

### 2026-09-07 — ACPI discovery and capability-gated clean shutdown

Before: 117/500 complete
After: 120/500 complete
Delta: +3
Partial changed: clean reboot gained a real FADT-reset/i8042 implementation but remains partial pending reset-cycle verification.
Verification:

- `make`
- `make test-build`
- Production QEMU: validated ACPI tables/S5 and reached the desktop
- Full regression QEMU: `TEST: all regression checks passed`
- Unprivileged system-control calls rejected
- Regression QEMU exited itself through synchronized ACPI S5 shutdown
- `git diff --check`

Notes:

- Added bounded firmware-table validation, S5 extraction, and generic capability-gated userspace power control.
- Physical power-button events, full AML, suspend/resume, battery/thermal policy, and a user-facing power menu remain pending.

### 2026-09-07 — GPT recovery, MBR discovery, and bounded volume devices

Before: 114/500 complete
After: 117/500 complete
Delta: +3
Partial changed: primary-MBR support gained a real bounded implementation; partition parsing, GPT, and volume abstraction advanced to complete.
Verification:

- `make`
- `make test-build`
- Kernel MBR/GPT CRC, overlap, translation, and range self-tests
- Production QEMU reached the desktop from a generated GPT disk
- Full regression QEMU twice: `TEST: all regression checks passed`
- Second GPT boot: `statefs: v2 hierarchy survived reboot`
- Corrupted primary GPT recovered through the validated backup header
- Corrupted primary and backup headers refused raw-disk formatting and booted safely without persistence
- `git diff --check`

Notes:

- Added format-neutral child block devices and a deterministic primary/backup GPT development image.
- Preserved legacy raw statefs disks only when no partition table is present.
- Extended MBR chains, multiple filesystem mounts, AHCI, and NVMe remain pending.

### 2026-09-07 — Hierarchical statefs and public filesystem API v2

Before: 108/500 complete
After: 114/500 complete
Delta: +6
Partial changed: C012 gained boot-relative created/modified metadata; C007 and C010 advanced from partial to complete.
Verification:

- `make`
- `make test-build`
- Full regression QEMU: `TEST: all regression checks passed`
- Existing v1 volume loaded and upgraded to v2
- Hierarchical state survived a second QEMU boot
- Corrupted newest v2 snapshot recovered from the prior generation
- Read-only, open-file deletion, nonempty-directory, and invalid-user-pointer negative tests

Notes:

- Added bounded stat, directory enumeration, mkdir, unlink/rmdir, and cross-directory rename APIs.
- Statefs remains intentionally small; partitions, large files, permissions, and repair tooling remain open.

### 2026-09-07 — Persistent statefs and durable settings v1

Before: 102/500 complete
After: 108/500 complete
Delta: +6
Partial changed: C010, C014, C018–C020 gained real bounded statefs foundations.
Verification:

- `make`
- `make test-build`
- Full regression QEMU: `TEST: all regression checks passed`
- Blank disk formatted and committed through dual snapshots
- Second QEMU boot verified prior-boot file and settings state
- Corrupted newest snapshot header fell back to the prior valid generation

Notes:

- Added `/state`, generic file sync and same-filesystem atomic replacement.
- Settingsd now persists versioned, checksummed theme/layout/pin state.
- Statefs remains deliberately bounded and is not yet a general consumer filesystem.
