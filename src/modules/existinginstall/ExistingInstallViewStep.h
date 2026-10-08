/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef EXISTINGINSTALLVIEWSTEP_H
#define EXISTINGINSTALLVIEWSTEP_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

class ExistingInstallPage;
class QProcess;

/**
 * Shown before the partition page. Finds installed Bass OS systems and offers to upgrade
 * one in place (the partition and dataimg pages are then skipped and nothing is
 * formatted), or a fresh install. On BIOS machines it also asks whether another
 * system's boot code may be replaced. Skips itself when there is nothing to ask.
 */
class PLUGINDLLEXPORT ExistingInstallViewStep : public Calamares::ViewStep
{
    Q_OBJECT

public:
    explicit ExistingInstallViewStep( QObject* parent = nullptr );
    ~ExistingInstallViewStep() override;

    QString prettyName() const override;
    QWidget* widget() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    Calamares::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;

private:
    void startScan();
    void scanFinished();
    void skipIfEmpty();

    ExistingInstallPage* m_widget;
    QProcess* m_scan = nullptr;
    bool m_scanDone = false;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( ExistingInstallViewStepFactory )

#endif  // EXISTINGINSTALLVIEWSTEP_H
