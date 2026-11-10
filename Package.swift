// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "zsign",
    platforms: [.macOS(.v13)],
    products: [
        .library(name: "ZSign", targets: ["ZSign"])
    ],
    targets: [
        .systemLibrary(
            name: "COpenSSL",
            pkgConfig: "openssl",
            providers: [
                .brew(["openssl@3"]),
                .yum(["openssl-devel"]),
                .apt(["libssl-dev"]),
            ]
        ),
        .target(
            name: "CZSign",
            dependencies: ["COpenSSL"],
            path: "src",
            exclude: [
                "zsign.cpp",
                "certcheck.cpp",
                "certcheck.h",
                "metadata.cpp",
                "metadata.h",
                "third-party/zlib",
                "third-party/minizip/iowin32.c",
                "third-party/minizip/iowin32.h",
                "third-party/minizip/mztools.c",
                "third-party/minizip/mztools.h",
            ],
            publicHeadersPath: "swiftpm/include",
            cSettings: [
                .headerSearchPath("."),
                .headerSearchPath("common"),
            ],
            cxxSettings: [
                .headerSearchPath("."),
                .headerSearchPath("common"),
            ],
            linkerSettings: [
                .linkedLibrary("z"),
                .linkedLibrary("crypto"),
            ]
        ),
        .target(
            name: "ZSign",
            dependencies: ["CZSign"]
        ),
    ],
    cxxLanguageStandard: .gnucxx14
)
