/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ExistingInstallPage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QLabel>
#include <QListWidget>
#include <QRadioButton>
#include <QVBoxLayout>

static QString
describe( const InstallInfo& i )
{
    QString name = i.version.display;
    if ( name.isEmpty() )
    {
        name = i.version.release.isEmpty() ? QObject::tr( "Unknown version" )
                                           : QObject::tr( "Android %1" ).arg( i.version.release );
    }
    QString when;
    if ( i.version.date > 0 )
    {
        when = QDateTime::fromSecsSinceEpoch( i.version.date ).date().toString( Qt::ISODate );
    }
    const QString where = i.label.isEmpty() ? i.device : QStringLiteral( "%1 \"%2\"" ).arg( i.device, i.label );
    return QObject::tr( "%1 (%2) on %3, %4, %5 GB" )
        .arg( name, when.isEmpty() ? QObject::tr( "unknown date" ) : when, where, i.fs )
        .arg( i.size / 1000000000 );
}

ExistingInstallPage::ExistingInstallPage( bool isEfi, QWidget* parent )
    : QWidget( parent )
    , m_isEfi( isEfi )
{
    auto* layout = new QVBoxLayout( this );

    m_status = new QLabel( this );
    m_status->setWordWrap( true );
    layout->addWidget( m_status );

    m_installBox = new QWidget( this );
    auto* box = new QVBoxLayout( m_installBox );
    box->setContentsMargins( 0, 0, 0, 0 );

    m_list = new QListWidget( m_installBox );
    m_list->setMaximumHeight( 120 );
    box->addWidget( m_list );

    m_upgrade = new QRadioButton( tr( "Upgrade the selected installation. Apps, data and settings are kept." ),
                                  m_installBox );
    box->addWidget( m_upgrade );
    m_keepBootOptions
        = new QCheckBox( tr( "Keep my boot options (the options chosen in Boot Options or at install time)" ),
                         m_installBox );
    m_keepBootOptions->setChecked( true );
    m_keepBootOptions->setContentsMargins( 24, 0, 0, 0 );
    box->addWidget( m_keepBootOptions );

    m_fresh = new QRadioButton( tr( "Install from scratch. Choose or erase partitions on the next page." ),
                                m_installBox );
    box->addWidget( m_fresh );
    m_wipeData = new QCheckBox( tr( "If the chosen partition already holds Bass OS and is not formatted, "
                                    "erase its apps and data" ),
                                m_installBox );
    m_wipeData->setContentsMargins( 24, 0, 0, 0 );
    box->addWidget( m_wipeData );

    auto* group = new QButtonGroup( this );
    group->addButton( m_upgrade );
    group->addButton( m_fresh );
    layout->addWidget( m_installBox );

    m_warning = new QLabel( this );
    m_warning->setWordWrap( true );
    m_warning->setStyleSheet( QStringLiteral( "font-weight: bold;" ) );
    layout->addWidget( m_warning );

    m_replaceMbr = new QCheckBox( this );
    m_replaceMbr->setChecked( true );
    layout->addWidget( m_replaceMbr );

    layout->addStretch();

    connect( m_list, &QListWidget::currentRowChanged, this, &ExistingInstallPage::updateState );
    connect( m_upgrade, &QRadioButton::toggled, this, &ExistingInstallPage::updateState );
    connect( m_fresh, &QRadioButton::toggled, this, &ExistingInstallPage::updateState );
    connect( m_keepBootOptions, &QCheckBox::toggled, this, &ExistingInstallPage::updateState );
    connect( m_wipeData, &QCheckBox::toggled, this, &ExistingInstallPage::updateState );
    connect( m_replaceMbr, &QCheckBox::toggled, this, &ExistingInstallPage::updateState );

    setScanning();
}

void
ExistingInstallPage::setScanning()
{
    m_status->setText( tr( "Looking for systems already installed on this computer…" ) );
    m_installBox->hide();
    m_warning->hide();
    m_replaceMbr->hide();
}

void
ExistingInstallPage::setScan( const ScanResult& scan )
{
    m_scan = scan;
    m_scanned = true;
    m_foreignMbrs = m_isEfi ? QStringList() : foreignMbrDisks( scan );

    m_list->clear();
    for ( const InstallInfo& i : std::as_const( m_scan.installs ) )
    {
        m_list->addItem( describe( i ) );
    }

    const bool any = !m_scan.installs.isEmpty();
    m_installBox->setVisible( any );
    if ( any )
    {
        m_status->setText( m_scan.installs.count() == 1
                               ? tr( "Bass OS is already installed on this computer." )
                               : tr( "Bass OS is already installed more than once on this computer. "
                                     "Select the installation to upgrade." ) );
        m_list->setCurrentRow( 0 );
        m_upgrade->setChecked( true );
    }
    else
    {
        m_status->setText( tr( "Another operating system starts this computer." ) );
    }

    m_replaceMbr->setText( tr( "Install the Bass OS boot menu at the start of the disk (%1). "
                               "It also lists the other systems. Leave unchecked to keep their boot "
                               "loader; Bass OS then has to be added to it by hand." )
                               .arg( m_foreignMbrs.join( QStringLiteral( ", " ) ) ) );
    updateState();
}

bool
ExistingInstallPage::hasContent() const
{
    return !m_scan.installs.isEmpty() || !m_foreignMbrs.isEmpty();
}

const InstallInfo*
ExistingInstallPage::selectedInstall() const
{
    const int row = m_list->currentRow();
    if ( row < 0 || row >= m_scan.installs.count() )
    {
        return nullptr;
    }
    return &m_scan.installs.at( row );
}

bool
ExistingInstallPage::isComplete() const
{
    if ( !m_scanned )
    {
        return false;
    }
    if ( m_upgrade->isChecked() )
    {
        const InstallInfo* i = selectedInstall();
        return i && checkVersion( i->version, m_scan.newSystem ) != VersionCheck::Downgrade;
    }
    return true;
}

void
ExistingInstallPage::updateState()
{
    const bool upgrade = m_upgrade->isChecked() && !m_scan.installs.isEmpty();
    m_keepBootOptions->setEnabled( upgrade );
    m_wipeData->setEnabled( m_fresh->isChecked() );
    m_list->setEnabled( upgrade );
    // An upgrade keeps the existing BIOS boot code.
    m_replaceMbr->setVisible( !m_foreignMbrs.isEmpty() && !upgrade );

    QString warning;
    const InstallInfo* i = selectedInstall();
    if ( upgrade && i )
    {
        switch ( checkVersion( i->version, m_scan.newSystem ) )
        {
        case VersionCheck::Downgrade:
            warning = tr( "This installer has an older Android version than the selected installation. "
                          "Its apps and data cannot be kept: choose \"Install from scratch\" and erase them." );
            break;
        case VersionCheck::OlderBuild:
            warning = tr( "This installer is an older build than the selected installation. "
                          "Upgrading may fail; back up your data first." );
            break;
        case VersionCheck::Ok:
            break;
        }
    }
    else if ( m_fresh->isChecked() )
    {
        for ( const InstallInfo& other : std::as_const( m_scan.installs ) )
        {
            if ( checkVersion( other.version, m_scan.newSystem ) == VersionCheck::Downgrade
                 && !m_wipeData->isChecked() )
            {
                warning = tr( "An installed Bass OS has a newer Android version. If you install over it "
                              "without formatting, also erase its apps and data." );
                break;
            }
        }
    }
    m_warning->setText( warning );
    m_warning->setVisible( !warning.isEmpty() );

    writeGlobalStorage();
    emit completeChanged();
}

void
ExistingInstallPage::writeGlobalStorage() const
{
    auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr;
    if ( !gs || !m_scanned )
    {
        return;
    }

    QVariantList installs;
    for ( const InstallInfo& i : m_scan.installs )
    {
        installs.append( QVariantMap { { QStringLiteral( "device" ), i.device },
                                       { QStringLiteral( "uuid" ), i.uuid },
                                       { QStringLiteral( "efiId" ), i.efiId } } );
    }
    gs->insert( QStringLiteral( "bassInstalls" ), installs );

    const InstallInfo* i = selectedInstall();
    QVariantMap upgrade;
    if ( m_upgrade->isChecked() && i )
    {
        upgrade.insert( QStringLiteral( "enabled" ), true );
        upgrade.insert( QStringLiteral( "device" ), i->device );
        upgrade.insert( QStringLiteral( "uuid" ), i->uuid );
        upgrade.insert( QStringLiteral( "fs" ), i->fs );
        upgrade.insert( QStringLiteral( "disk" ), i->disk );
        upgrade.insert( QStringLiteral( "keepBootOptions" ), m_keepBootOptions->isChecked() );
        upgrade.insert( QStringLiteral( "wipeData" ), false );
        upgrade.insert( QStringLiteral( "espUuid" ), i->espUuid );
        upgrade.insert( QStringLiteral( "efiId" ), i->efiId );

        // The dataimg page is skipped: keep the store the installation already uses.
        QVariantMap dataimg = gs->value( QStringLiteral( "dataimg" ) ).toMap();
        dataimg.insert( QStringLiteral( "disabled" ), i->data != QLatin1String( "img" ) );
        dataimg.insert( QStringLiteral( "useMaximum" ), true );
        dataimg.insert( QStringLiteral( "dataSize" ), 0 );
        gs->insert( QStringLiteral( "dataimg" ), dataimg );

        if ( m_isEfi && !i->efiId.isEmpty() )
        {
            gs->insert( QStringLiteral( "bassEfiBootloaderId" ), i->efiId );
        }
        else
        {
            gs->remove( QStringLiteral( "bassEfiBootloaderId" ) );
        }
    }
    else
    {
        upgrade.insert( QStringLiteral( "enabled" ), false );
        upgrade.insert( QStringLiteral( "wipeData" ), m_fresh->isChecked() && m_wipeData->isChecked() );
        gs->remove( QStringLiteral( "bassEfiBootloaderId" ) );
    }
    gs->insert( QStringLiteral( "bassUpgrade" ), upgrade );

    if ( !m_foreignMbrs.isEmpty() && !upgrade.value( QStringLiteral( "enabled" ) ).toBool() )
    {
        gs->insert( QStringLiteral( "bassAllowMbrReplace" ), m_replaceMbr->isChecked() );
    }
    else
    {
        gs->remove( QStringLiteral( "bassAllowMbrReplace" ) );
    }
}
