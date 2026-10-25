#pragma once

// Exposes FatFS to the computer. Call only before starting robot hardware,
// with all firmware files closed and neither core accessing the filesystem.
bool setup_usb_storage();
