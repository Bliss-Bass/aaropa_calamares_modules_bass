/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Install.h"

#include <QHash>
#include <QStringList>

static SystemVersion
versionFrom( const QHash< QString, QString >& f )
{
    SystemVersion v;
    v.sdk = f.value( QStringLiteral( "sdk" ) ).toInt();
    v.release = f.value( QStringLiteral( "release" ) );
    v.date = f.value( QStringLiteral( "date" ) ).toLongLong();
    v.display = f.value( QStringLiteral( "display" ) );
    return v;
}

ScanResult
parseScanOutput( const QString& output )
{
    ScanResult result;
    const QStringList lines = output.split( QLatin1Char( '\n' ) );
    for ( const QString& line : lines )
    {
        QStringList parts = line.split( QLatin1Char( '\t' ) );
        if ( parts.isEmpty() )
        {
            continue;
        }
        const QString kind = parts.takeFirst().trimmed();
        QHash< QString, QString > f;
        for ( const QString& part : std::as_const( parts ) )
        {
            const int eq = part.indexOf( QLatin1Char( '=' ) );
            if ( eq > 0 )
            {
                f.insert( part.left( eq ), part.mid( eq + 1 ) );
            }
        }

        if ( kind == QLatin1String( "install" ) )
        {
            InstallInfo i;
            i.device = f.value( QStringLiteral( "device" ) );
            if ( i.device.isEmpty() )
            {
                continue;
            }
            i.uuid = f.value( QStringLiteral( "uuid" ) );
            i.fs = f.value( QStringLiteral( "fs" ) );
            i.disk = f.value( QStringLiteral( "disk" ) );
            i.label = f.value( QStringLiteral( "label" ) );
            i.size = f.value( QStringLiteral( "size" ) ).toLongLong();
            i.slot = f.value( QStringLiteral( "slot" ) );
            i.version = versionFrom( f );
            i.data = f.value( QStringLiteral( "data" ) );
            i.espUuid = f.value( QStringLiteral( "esp_uuid" ) );
            i.efiId = f.value( QStringLiteral( "efi_id" ) );
            i.cmdline = f.value( QStringLiteral( "cmdline" ) );
            result.installs.append( i );
        }
        else if ( kind == QLatin1String( "mbr" ) )
        {
            MbrInfo m;
            m.disk = f.value( QStringLiteral( "disk" ) );
            m.owner = f.value( QStringLiteral( "owner" ) );
            m.live = f.value( QStringLiteral( "live" ) ) == QLatin1String( "1" );
            if ( !m.disk.isEmpty() )
            {
                result.mbrs.append( m );
            }
        }
        else if ( kind == QLatin1String( "new" ) )
        {
            result.hasNewSystem = true;
            result.newSystem = versionFrom( f );
        }
    }
    return result;
}

VersionCheck
checkVersion( const SystemVersion& installed, const SystemVersion& incoming )
{
    if ( installed.sdk > 0 && incoming.sdk > 0 && incoming.sdk < installed.sdk )
    {
        return VersionCheck::Downgrade;
    }
    if ( installed.sdk > 0 && incoming.sdk > installed.sdk )
    {
        return VersionCheck::Ok;
    }
    if ( installed.date > 0 && incoming.date > 0 && incoming.date < installed.date )
    {
        return VersionCheck::OlderBuild;
    }
    return VersionCheck::Ok;
}

QStringList
foreignMbrDisks( const ScanResult& scan )
{
    QStringList disks;
    for ( const MbrInfo& m : scan.mbrs )
    {
        if ( !m.live && m.owner != QLatin1String( "empty" ) )
        {
            disks.append( m.disk );
        }
    }
    return disks;
}
