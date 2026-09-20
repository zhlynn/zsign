// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "zsign",
    platforms: [
        .iOS("14.0"),
        .macOS("11.0"),
    ],
    products: [
        .library(
            name: "Zupersign",
            targets: ["Zupersign"]
        ),
    ],
    dependencies: [
        .package(url: "https://github.com/xtool-org/xtool-core", .upToNextMinor(from: "1.5.0")),
    ],
    targets: [
        .target(
            name: "Zupersign",
            dependencies: [
                .product(name: "OpenSSL", package: "xtool-core"),
                .product(name: "SignerSupport", package: "xtool-core")
            ],
            path: "src",
            exclude: [
                "third-party/minizip/iowin32.c",
            ],
            cSettings: [
                .headerSearchPath("common"),
                .headerSearchPath("."),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx11
)
