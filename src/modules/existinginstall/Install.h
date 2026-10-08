/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef EXISTINGINSTALL_INSTALL_H
#define EXISTINGINSTALL_INSTALL_H

#include <QList>
#include <QString>

/// Version of an Android system image (from its build.prop).
struct SystemVersion
{
    int sdk = 0;
    QString release;
    qint64 date = 0;  ///< ro.build.date.utc
    QString display;
};

/// An installed Bass OS / BlissOS found by bass-find-installs.
struct InstallInfo
{
    QString device;
    QString uuid;
    QString fs;
    QString disk;
    QString label;
    qint64 size = 0;
    QString slot;
    SystemVersion version;
    QString data;  ///< img, dir or none
    QString espUuid;
    QString efiId;
    QString cmdline;  ///< kernel options in its android.cfg (empty when not GRUB-safe)
};

/// Boot code found at the start of a disk (relevant on BIOS machines).
struct MbrInfo
{
    QString disk;
    QString owner;  ///< empty, grub, windows or other
    bool live = false;  ///< the disk the installer was started from
};

struct ScanResult
{
    QList< InstallInfo > installs;
    QList< MbrInfo > mbrs;
    bool hasNewSystem = false;
    SystemVersion newSystem;
};

/// Parses bass-find-installs output (one tab-separated key=value record per line).
ScanResult parseScanOutput( const QString& output );

enum class VersionCheck
{
    Ok,
    OlderBuild,  ///< same or newer Android, but an older build: allowed with a warning
    Downgrade  ///< older Android than installed: upgrading would break apps and data
};

VersionCheck checkVersion( const SystemVersion& installed, const SystemVersion& incoming );

/// Disks whose boot code belongs to another system (not empty, not the installer USB).
QStringList foreignMbrDisks( const ScanResult& scan );

#endif  // EXISTINGINSTALL_INSTALL_H
