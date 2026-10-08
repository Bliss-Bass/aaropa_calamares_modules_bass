/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ExistingInstallJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include "utils/Logger.h"

#include <QFileInfo>
#include <QProcess>

static const char espGuid[] = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b";

ExistingInstallJob::ExistingInstallJob( QObject* parent )
    : Calamares::Job( parent )
{
}

QString
ExistingInstallJob::prettyName() const
{
    return tr( "Preparing the existing installation" );
}

static QString
canonicalDevice( const QString& device )
{
    const QString c = QFileInfo( device ).canonicalFilePath();
    return c.isEmpty() ? device : c;
}

static bool
sameDevice( const QVariantMap& entry, const QString& device, const QString& uuid )
{
    const QString d = entry.value( QStringLiteral( "device" ) ).toString();
    if ( !device.isEmpty() && !d.isEmpty() && canonicalDevice( d ) == canonicalDevice( device ) )
    {
        return true;
    }
    const QString u = entry.value( QStringLiteral( "uuid" ) ).toString();
    return !uuid.isEmpty() && u.compare( uuid, Qt::CaseInsensitive ) == 0;
}

static QVariantMap
newEntry( const QString& device, const QString& uuid, const QString& fs, const QString& mountPoint )
{
    return QVariantMap { { QStringLiteral( "device" ), device },
                         { QStringLiteral( "uuid" ), uuid },
                         { QStringLiteral( "fs" ), fs },
                         { QStringLiteral( "fsName" ), fs },
                         { QStringLiteral( "mountPoint" ), mountPoint },
                         { QStringLiteral( "partlabel" ), QString() },
                         { QStringLiteral( "partuuid" ), QString() },
                         { QStringLiteral( "claimed" ), false } };
}

QVariantList
ExistingInstallJob::upgradePartitions( QVariantList partitions,
                                       const QVariantMap& upgrade,
                                       const QString& espMount,
                                       const QString& espDevice,
                                       QString* error )
{
    const QString device = upgrade.value( QStringLiteral( "device" ) ).toString();
    const QString uuid = upgrade.value( QStringLiteral( "uuid" ) ).toString();
    const QString espUuid = upgrade.value( QStringLiteral( "espUuid" ) ).toString();
    const bool efi = !espMount.isEmpty();

    bool rootFound = false;
    bool espFound = false;
    for ( QVariant& v : partitions )
    {
        QVariantMap entry = v.toMap();
        QString mountPoint;
        if ( !rootFound && sameDevice( entry, device, uuid ) )
        {
            mountPoint = QStringLiteral( "/" );
            rootFound = true;
        }
        else if ( efi && !espFound
                  && ( sameDevice( entry, QString(), espUuid ) || sameDevice( entry, espDevice, QString() ) ) )
        {
            mountPoint = espMount;
            espFound = true;
        }
        // Nothing else is mounted, whatever an earlier visit of the partition page chose.
        entry.insert( QStringLiteral( "mountPoint" ), mountPoint );
        v = entry;
    }

    if ( !rootFound )
    {
        partitions.append( newEntry(
            device, uuid, upgrade.value( QStringLiteral( "fs" ) ).toString(), QStringLiteral( "/" ) ) );
    }
    if ( efi && !espFound )
    {
        if ( espDevice.isEmpty() )
        {
            if ( error )
            {
                *error = tr( "No EFI system partition was found for %1." ).arg( device );
            }
            return QVariantList();
        }
        partitions.append( newEntry( espDevice, espUuid, QStringLiteral( "fat32" ), espMount ) );
    }
    return partitions;
}

static QString
run( const QStringList& args )
{
    QProcess p;
    p.start( args.first(), args.mid( 1 ) );
    if ( !p.waitForFinished( 30000 ) )
    {
        p.kill();
        return QString();
    }
    return QString::fromUtf8( p.readAllStandardOutput() ).trimmed();
}

/// The ESP the installation booted from, else the first ESP on its disk.
static QString
findEsp( const QString& espUuid, const QString& disk )
{
    if ( !espUuid.isEmpty() )
    {
        const QString dev = run( { QStringLiteral( "blkid" ), QStringLiteral( "-U" ), espUuid } );
        if ( !dev.isEmpty() )
        {
            return dev;
        }
    }
    if ( disk.isEmpty() )
    {
        return QString();
    }
    const QStringList lines
        = run( { QStringLiteral( "lsblk" ), QStringLiteral( "-rnpo" ), QStringLiteral( "NAME,TYPE,PARTTYPE" ), disk } )
              .split( QLatin1Char( '\n' ) );
    for ( const QString& line : lines )
    {
        const QStringList f = line.split( QLatin1Char( ' ' ) );
        if ( f.count() >= 3 && f.at( 1 ) == QLatin1String( "part" )
             && ( f.at( 2 ).compare( QLatin1String( espGuid ), Qt::CaseInsensitive ) == 0 || f.at( 2 ) == QLatin1String( "0xef" ) ) )
        {
            return f.at( 0 );
        }
    }
    return QString();
}

Calamares::JobResult
ExistingInstallJob::exec()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    const QVariantMap upgrade = gs->value( QStringLiteral( "bassUpgrade" ) ).toMap();
    const bool efi = gs->value( QStringLiteral( "firmwareType" ) ).toString() == QLatin1String( "efi" );
    QVariantList partitions = gs->value( QStringLiteral( "partitions" ) ).toList();

    if ( !upgrade.value( QStringLiteral( "enabled" ) ).toBool() )
    {
        // Installing over an existing system: keep using its EFI directory and boot entry.
        const QVariantList installs = gs->value( QStringLiteral( "bassInstalls" ) ).toList();
        for ( const QVariant& p : std::as_const( partitions ) )
        {
            const QVariantMap entry = p.toMap();
            if ( entry.value( QStringLiteral( "mountPoint" ) ).toString() != QLatin1String( "/" ) )
            {
                continue;
            }
            for ( const QVariant& i : installs )
            {
                const QVariantMap install = i.toMap();
                const QString efiId = install.value( QStringLiteral( "efiId" ) ).toString();
                if ( efi && !efiId.isEmpty()
                     && sameDevice( entry, install.value( QStringLiteral( "device" ) ).toString(), QString() ) )
                {
                    cDebug() << "Fresh install over" << entry.value( "device" ) << "reuses EFI id" << efiId;
                    gs->insert( QStringLiteral( "bassEfiBootloaderId" ), efiId );
                }
            }
        }
        return Calamares::JobResult::ok();
    }

    QString espMount;
    QString espDevice;
    if ( efi )
    {
        espMount = gs->value( QStringLiteral( "efiSystemPartition" ) ).toString();
        if ( espMount.isEmpty() )
        {
            espMount = QStringLiteral( "/boot/efi" );
        }
        espDevice = findEsp( upgrade.value( QStringLiteral( "espUuid" ) ).toString(),
                             upgrade.value( QStringLiteral( "disk" ) ).toString() );
    }

    QString error;
    partitions = upgradePartitions( partitions, upgrade, espMount, espDevice, &error );
    if ( partitions.isEmpty() )
    {
        return Calamares::JobResult::error( tr( "Cannot upgrade the existing installation." ), error );
    }
    gs->insert( QStringLiteral( "partitions" ), partitions );

    if ( !efi )
    {
        // Keep the boot code that already starts this installation (bootloader module
        // still regenerates grub.cfg).
        gs->insert( QStringLiteral( "bootLoader" ), QVariantMap { { QStringLiteral( "installPath" ), QVariant() } } );
    }
    cDebug() << "Upgrading" << upgrade.value( "device" ) << "ESP" << espDevice << "at" << espMount;
    return Calamares::JobResult::ok();
}
