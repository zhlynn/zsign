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
            name: "zsign",
            targets: ["zsign"]
        ),
        .library(
            name: "Zupersign",
            targets: ["Zupersign"]
        ),
    ],
    dependencies: [
        .package(url: "https://github.com/xtool-org/xtool-core", .upToNextMinor(from: "1.3.0")),
    ],
    targets: [
        .target(
            name: "zsign",
            dependencies: [
                .product(name: "OpenSSL", package: "xtool-core"),
            ],
            path: "src",
            exclude: [
                "third-party/minizip/iowin32.c",
            ],
            cSettings: [
                .headerSearchPath("include/common"),
            ]
        ),
        .target(
            name: "Zupersign",
            dependencies: [
                "zsign",
                .product(name: "OpenSSL", package: "xtool-core"),
                .product(name: "SignerSupport", package: "xtool-core")
            ]
        ),
    ]
)
