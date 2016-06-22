import qbs 1.0

QtcPlugin {
    name: "OpenMV"

    Depends { name: "Core" }
    Depends { name: "Qt"; submodules: ["widgets"] }

    files: [
        "openmvplugin.cpp",
        "openmvplugin.h"
    ]
}
