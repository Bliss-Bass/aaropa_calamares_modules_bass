/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ExistingInstallViewStep.h"

#include "ExistingInstallJob.h"
#include "ExistingInstallPage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "ViewManager.h"

#include "utils/Logger.h"

#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#ifndef BASS_FIND_INSTALLS
#define BASS_FIND_INSTALLS "/usr/lib/calamares/modules/existinginstall/bass-find-installs"
#endif

// The system image the ota job copies (calamares ota.conf).
#define NEW_SYSTEM_IMAGE "/source/system.img"
#define SKIPPED_FLAG "_existinginstall_skipped"

CALAMARES_PLUGIN_FACTORY_DEFINITION( ExistingInstallViewStepFactory, registerPlugin< ExistingInstallViewStep >(); )

ExistingInstallViewStep::ExistingInstallViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_widget( new ExistingInstallPage( QFileInfo::exists( QStringLiteral( "/sys/firmware/efi" ) ) ) )
{
    connect( m_widget, &ExistingInstallPage::completeChanged, this, [ this ] { emit nextStatusChanged( isNextEnabled() ); } );
    // Start early so the result is usually ready when the page is reached.
    QTimer::singleShot( 0, this, &ExistingInstallViewStep::startScan );
}

ExistingInstallViewStep::~ExistingInstallViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}

QString
ExistingInstallViewStep::prettyName() const
{
    return tr( "Existing installation" );
}

QString
ExistingInstallViewStep::prettyStatus() const
{
    return m_widget->summary();
}

QWidget*
ExistingInstallViewStep::widget()
{
    return m_widget;
}

bool
ExistingInstallViewStep::isNextEnabled() const
{
    return m_scanDone && m_widget->isComplete();
}

bool
ExistingInstallViewStep::isBackEnabled() const
{
    return true;
}

bool
ExistingInstallViewStep::isAtBeginning() const
{
    return true;
}

bool
ExistingInstallViewStep::isAtEnd() const
{
    return true;
}

Calamares::JobList
ExistingInstallViewStep::jobs() const
{
    return Calamares::JobList() << Calamares::job_ptr( new ExistingInstallJob() );
}

void
ExistingInstallViewStep::startScan()
{
    if ( m_scan || m_scanDone )
    {
        return;
    }
    m_scan = new QProcess( this );
    m_scan->setProcessChannelMode( QProcess::SeparateChannels );
    connect( m_scan,
             QOverload< int, QProcess::ExitStatus >::of( &QProcess::finished ),
             this,
             &ExistingInstallViewStep::scanFinished );
    connect( m_scan,
             &QProcess::errorOccurred,
             this,
             [ this ]( QProcess::ProcessError e )
             {
                 if ( e == QProcess::FailedToStart )
                 {
                     cWarning() << "Could not start" << BASS_FIND_INSTALLS;
                     scanFinished();
                 }
             } );
    // Mounting every partition read-only can take a while; never block the installer.
    QTimer::singleShot( 90000,
                        m_scan,
                        [ this ]
                        {
                            if ( m_scan && m_scan->state() != QProcess::NotRunning )
                            {
                                cWarning() << "bass-find-installs timed out";
                                m_scan->kill();
                            }
                        } );
    cDebug() << "Scanning for existing installations";
    m_scan->start( QStringLiteral( BASS_FIND_INSTALLS ), { QStringLiteral( "--new" ), QStringLiteral( NEW_SYSTEM_IMAGE ) } );
}

void
ExistingInstallViewStep::scanFinished()
{
    if ( m_scanDone )
    {
        return;
    }
    m_scanDone = true;
    const QString output = m_scan ? QString::fromUtf8( m_scan->readAllStandardOutput() ) : QString();
    cDebug() << "bass-find-installs:" << output;

    m_widget->setScan( parseScanOutput( output ) );
    emit nextStatusChanged( isNextEnabled() );

    if ( Calamares::ViewManager::instance()->currentStep() == this )
    {
        skipIfEmpty();
    }
}

void
ExistingInstallViewStep::skipIfEmpty()
{
    if ( m_widget->hasContent() )
    {
        return;
    }
    // Nothing to ask: behave as if the page did not exist, in both directions.
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    auto* vm = Calamares::ViewManager::instance();
    m_widget->writeGlobalStorage();
    if ( gs->contains( QStringLiteral( SKIPPED_FLAG ) ) )
    {
        gs->remove( QStringLiteral( SKIPPED_FLAG ) );
        vm->back();
    }
    else
    {
        gs->insert( QStringLiteral( SKIPPED_FLAG ), true );
        vm->next();
    }
}

void
ExistingInstallViewStep::onActivate()
{
    if ( !m_scanDone )
    {
        m_widget->setScanning();
        startScan();
        return;
    }
    skipIfEmpty();
}

void
ExistingInstallViewStep::onLeave()
{
    m_widget->writeGlobalStorage();
}
