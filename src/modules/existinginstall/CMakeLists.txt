# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2026 Bliss Bass contributors
#   SPDX-License-Identifier: GPL-3.0-or-later
#
set(_bass_find_installs_dir ${CMAKE_INSTALL_LIBDIR}/calamares/modules/existinginstall)

calamares_add_plugin(existinginstall
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Install.cpp
        ExistingInstallJob.cpp
        ExistingInstallPage.cpp
        ExistingInstallViewStep.cpp
    LINK_PRIVATE_LIBRARIES
        ${qtname}::Widgets
    SHARED_LIB
    NO_CONFIG
)

target_compile_definitions(calamares_viewmodule_existinginstall
    PRIVATE BASS_FIND_INSTALLS="${CMAKE_INSTALL_PREFIX}/${_bass_find_installs_dir}/bass-find-installs"
)

install(PROGRAMS bass-find-installs DESTINATION ${_bass_find_installs_dir})

calamares_add_test(
    existinginstalltest
    SOURCES Tests.cpp Install.cpp ExistingInstallJob.cpp
)
