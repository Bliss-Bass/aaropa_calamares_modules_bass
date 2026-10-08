/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ExistingInstallJob.h"
#include "Install.h"

#include <QtTest/QtTest>

class ExistingInstallTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testParse();
    void testVersion();
    void testForeignMbr();
    void testUpgradeEfi();
    void testUpgradeBiosAppendsRoot();
    void testUpgradeEfiMissingEsp();
};

static const char scanOutput[]
    = "new\tsdk=36\trelease=16\tdate=1760000000\tdisplay=Bass 16 new\n"
      "mbr\tdisk=/dev/sda\towner=windows\tlive=0\n"
      "mbr\tdisk=/dev/sdb\towner=grub\tlive=1\n"
      "mbr\tdisk=/dev/sdc\towner=empty\tlive=0\n"
      "install\tdevice=/dev/sda3\tuuid=1111\tfs=ext4\tdisk=/dev/sda\tlabel=Bass OS\tsize=64000000000\t"
      "slot=_b\tsdk=36\trelease=16\tdate=1759000000\tdisplay=Bass 16 old\tdata=img\tesp_uuid=AB-CD\tefi_id=BassOS\t"
      "cmdline=quiet SET_RMB=true DATA=data.img\n"
      "garbage line\n"
      "install\tuuid=no-device\n";

void
ExistingInstallTests::testParse()
{
    const ScanResult r = parseScanOutput( QString::fromUtf8( scanOutput ) );
    QVERIFY( r.hasNewSystem );
    QCOMPARE( r.newSystem.sdk, 36 );
    QCOMPARE( r.newSystem.date, qint64( 1760000000 ) );
    QCOMPARE( r.installs.count(), 1 );
    const InstallInfo& i = r.installs.first();
    QCOMPARE( i.device, QStringLiteral( "/dev/sda3" ) );
    QCOMPARE( i.label, QStringLiteral( "Bass OS" ) );
    QCOMPARE( i.slot, QStringLiteral( "_b" ) );
    QCOMPARE( i.version.display, QStringLiteral( "Bass 16 old" ) );
    QCOMPARE( i.data, QStringLiteral( "img" ) );
    QCOMPARE( i.espUuid, QStringLiteral( "AB-CD" ) );
    QCOMPARE( i.efiId, QStringLiteral( "BassOS" ) );
    QCOMPARE( i.cmdline, QStringLiteral( "quiet SET_RMB=true DATA=data.img" ) );
    QCOMPARE( i.size, qint64( 64000000000 ) );
    QCOMPARE( r.mbrs.count(), 3 );
}

void
ExistingInstallTests::testVersion()
{
    SystemVersion installed;
    installed.sdk = 36;
    installed.date = 2000;
    SystemVersion incoming = installed;
    QCOMPARE( checkVersion( installed, incoming ), VersionCheck::Ok );
    incoming.date = 1000;
    QCOMPARE( checkVersion( installed, incoming ), VersionCheck::OlderBuild );
    incoming.sdk = 37;
    QCOMPARE( checkVersion( installed, incoming ), VersionCheck::Ok );
    incoming.sdk = 35;
    incoming.date = 3000;
    QCOMPARE( checkVersion( installed, incoming ), VersionCheck::Downgrade );
    // Unknown versions never block.
    QCOMPARE( checkVersion( SystemVersion(), incoming ), VersionCheck::Ok );
    QCOMPARE( checkVersion( installed, SystemVersion() ), VersionCheck::Ok );
}

void
ExistingInstallTests::testForeignMbr()
{
    const ScanResult r = parseScanOutput( QString::fromUtf8( scanOutput ) );
    QCOMPARE( foreignMbrDisks( r ), QStringList { QStringLiteral( "/dev/sda" ) } );
}

static QVariantMap
part( const QString& device, const QString& uuid, const QString& fs, const QString& mountPoint = QString() )
{
    return QVariantMap { { QStringLiteral( "device" ), device },
                         { QStringLiteral( "uuid" ), uuid },
                         { QStringLiteral( "fs" ), fs },
                         { QStringLiteral( "mountPoint" ), mountPoint } };
}

static QString
mountOf( const QVariantList& l, const QString& device )
{
    for ( const QVariant& v : l )
    {
        if ( v.toMap().value( QStringLiteral( "device" ) ).toString() == device )
        {
            return v.toMap().value( QStringLiteral( "mountPoint" ) ).toString();
        }
    }
    return QStringLiteral( "<missing>" );
}

void
ExistingInstallTests::testUpgradeEfi()
{
    // A stale "/" from an earlier visit of the partition page must be cleared.
    QVariantList l { part( QStringLiteral( "/dev/sda1" ), QStringLiteral( "AB-CD" ), QStringLiteral( "fat32" ) ),
                     part( QStringLiteral( "/dev/sda2" ), QStringLiteral( "2222" ), QStringLiteral( "ntfs" ) ),
                     part( QStringLiteral( "/dev/sda3" ), QStringLiteral( "1111" ), QStringLiteral( "ext4" ) ),
                     part( QStringLiteral( "/dev/sdb1" ), QStringLiteral( "3333" ), QStringLiteral( "ext4" ), QStringLiteral( "/" ) ) };
    const QVariantMap upgrade { { QStringLiteral( "device" ), QStringLiteral( "/dev/sda3" ) },
                                { QStringLiteral( "uuid" ), QStringLiteral( "1111" ) },
                                { QStringLiteral( "espUuid" ), QStringLiteral( "ab-cd" ) } };
    QString error;
    const QVariantList r = ExistingInstallJob::upgradePartitions(
        l, upgrade, QStringLiteral( "/boot/efi" ), QStringLiteral( "/dev/sda1" ), &error );
    QVERIFY( error.isEmpty() );
    QCOMPARE( r.count(), 4 );
    QCOMPARE( mountOf( r, QStringLiteral( "/dev/sda3" ) ), QStringLiteral( "/" ) );
    QCOMPARE( mountOf( r, QStringLiteral( "/dev/sda1" ) ), QStringLiteral( "/boot/efi" ) );
    QCOMPARE( mountOf( r, QStringLiteral( "/dev/sda2" ) ), QString() );
    QCOMPARE( mountOf( r, QStringLiteral( "/dev/sdb1" ) ), QString() );
}

void
ExistingInstallTests::testUpgradeBiosAppendsRoot()
{
    const QVariantMap upgrade { { QStringLiteral( "device" ), QStringLiteral( "/dev/sda3" ) },
                                { QStringLiteral( "uuid" ), QStringLiteral( "1111" ) },
                                { QStringLiteral( "fs" ), QStringLiteral( "ext4" ) } };
    QString error;
    const QVariantList r = ExistingInstallJob::upgradePartitions( QVariantList(), upgrade, QString(), QString(), &error );
    QCOMPARE( r.count(), 1 );
    QCOMPARE( mountOf( r, QStringLiteral( "/dev/sda3" ) ), QStringLiteral( "/" ) );
    QCOMPARE( r.first().toMap().value( QStringLiteral( "fs" ) ).toString(), QStringLiteral( "ext4" ) );
}

void
ExistingInstallTests::testUpgradeEfiMissingEsp()
{
    const QVariantMap upgrade { { QStringLiteral( "device" ), QStringLiteral( "/dev/sda3" ) } };
    QString error;
    const QVariantList r = ExistingInstallJob::upgradePartitions(
        QVariantList(), upgrade, QStringLiteral( "/boot/efi" ), QString(), &error );
    QVERIFY( r.isEmpty() );
    QVERIFY( !error.isEmpty() );
}

QTEST_GUILESS_MAIN( ExistingInstallTests )

#include "utils/moc-warnings.h"

#include "Tests.moc"
