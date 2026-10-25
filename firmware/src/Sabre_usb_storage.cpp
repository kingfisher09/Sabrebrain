// Exposes onboard storage as a USB drive while normal robot operation is stopped.
#include "sabre_usb_storage.h"

#include <FatFS.h>
#include <FatFSUSB.h>
#include <atomic>

namespace {
std::atomic<bool> ejected{false};
void on_eject(uint32_t) { ejected = true; }
bool drive_ready(uint32_t) { return !ejected.load(); }
}  // namespace

bool setup_usb_storage() {
  if (!FatFS.begin()) {
    Serial.println("USB storage: could not mount FatFS");
    return false;
  }

  // Prepare the directory before handing the entire filesystem to the host.
  const bool directory_ready =
      (FatFS.exists("/SABREBRAIN") || FatFS.mkdir("/SABREBRAIN")) &&
      (FatFS.exists("/SABREBRAIN/config") || FatFS.mkdir("/SABREBRAIN/config"));
  FatFS.end();
  if (!directory_ready) {
    Serial.println("USB storage: could not create /SABREBRAIN/config");
    return false;
  }

  // Keep FatFS unmounted for this entire boot, including after host eject.
  // FatFSUSB flushes its pending writes when the computer ejects the drive.
  FatFSUSB.onUnplug(on_eject);
  FatFSUSB.driveReady(drive_ready);
  return FatFSUSB.begin();
}
