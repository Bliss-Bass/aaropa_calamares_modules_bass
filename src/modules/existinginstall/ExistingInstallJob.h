/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef EXISTINGINSTALLJOB_H
#define EXISTINGINSTALLJOB_H

#include "Job.h"

#include <QVariantList>

/**
 * Runs right after the partition job. For an upgrade it mounts the existing system as "/"
 * (and its ESP on EFI) through the "partitions" list, and keeps the BIOS boot code. For a
 * fresh install over an existing system it reuses that system's EFI directory name.
 */
class ExistingInstallJob : public Calamares::Job
{
    Q_OBJECT

public:
    explicit ExistingInstallJob( QObject* parent = nullptr );

    QString prettyName() const override;
    Calamares::JobResult exec() override;

    /// Applies an upgrade to the "partitions" list (pure, for tests). @p espDevice is the
    /// ESP to mount at @p espMount when no entry matches @p espUuid; empty on BIOS.
    static QVariantList upgradePartitions( QVariantList partitions,
                                           const QVariantMap& upgrade,
                                           const QString& espMount,
                                           const QString& espDevice,
                                           QString* error );
};

#endif  // EXISTINGINSTALLJOB_H
