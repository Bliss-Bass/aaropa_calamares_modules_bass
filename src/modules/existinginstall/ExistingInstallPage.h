/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef EXISTINGINSTALLPAGE_H
#define EXISTINGINSTALLPAGE_H

#include "Install.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QListWidget;
class QRadioButton;

class ExistingInstallPage : public QWidget
{
    Q_OBJECT

public:
    explicit ExistingInstallPage( bool isEfi, QWidget* parent = nullptr );

    void setScanning();
    void setScan( const ScanResult& scan );

    /// Something to ask: an install to upgrade, or (BIOS) another system's boot code.
    bool hasContent() const;
    bool isComplete() const;

    /// Publishes the choice: bassUpgrade, dataimg, bassEfiBootloaderId, bassInstalls,
    /// bassAllowMbrReplace.
    void writeGlobalStorage() const;

signals:
    void completeChanged();

private:
    void updateState();
    const InstallInfo* selectedInstall() const;

    bool m_isEfi;
    ScanResult m_scan;
    QStringList m_foreignMbrs;
    bool m_scanned = false;

    QLabel* m_status;
    QWidget* m_installBox;
    QListWidget* m_list;
    QRadioButton* m_upgrade;
    QCheckBox* m_keepBootOptions;
    QRadioButton* m_fresh;
    QCheckBox* m_wipeData;
    QLabel* m_warning;
    QCheckBox* m_replaceMbr;
};

#endif  // EXISTINGINSTALLPAGE_H
