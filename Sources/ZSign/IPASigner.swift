import CZSign
import Foundation

public struct SigningIdentity: Sendable {
    /// PEM or DER private key, or a PKCS#12 bundle.
    public let privateKey: Data
    /// Only needed when the key is not PKCS#12 and the certificate should not be taken from the profile.
    public let certificate: Data?
    public let password: String

    public init(privateKey: Data, certificate: Data? = nil, password: String = "") {
        self.privateKey = privateKey
        self.certificate = certificate
        self.password = password
    }
}

public struct IPASigner: Sendable {

    public enum SigningError: Error, CustomStringConvertible {
        case appBundleNotFound
        case invalidBundle(String)
        case missingProfile(bundleID: String)
        case entitlementsMismatch(bundleID: String, binary: String, profile: String)
        case zsign(step: String, message: String)

        public var description: String {
            switch self {
            case .appBundleNotFound:
                "No .app bundle found under Payload"
            case .invalidBundle(let path):
                "Bundle has no readable Info.plist identifier or executable: \(path)"
            case .missingProfile(let bundleID):
                "No provisioning profile for \(bundleID)"
            case .entitlementsMismatch(let bundleID, let binary, let profile):
                "\(bundleID) was built as \(binary) but the profile grants \(profile)"
            case .zsign(let step, let message):
                "zsign \(step) failed: \(message)"
            }
        }
    }

    public struct Report: Sendable {
        public struct SignedBundle: Sendable {
            public let bundleID: String
            public let profileUUID: String
            public let reusedEntitlements: Bool
        }

        public let bundles: [SignedBundle]
        public let extract: Duration
        public let sign: Duration
        public let archive: Duration
    }

    /// 0 stores without compression, which is fastest; 1 is a good size trade.
    public var compressionLevel: Int

    public init(compressionLevel: Int = 1) {
        self.compressionLevel = compressionLevel
    }

    /// Re-signs `ipa` into `output`. `workDirectory` must be writable and is left empty.
    public func sign(
        ipa: URL,
        output: URL,
        identity: SigningIdentity,
        profiles: [ProvisioningProfile],
        workDirectory: URL
    ) throws -> Report {
        let fileManager = FileManager.default
        let scratch = workDirectory.appendingPathComponent("zsign-\(UUID().uuidString)", isDirectory: true)
        let payloadRoot = scratch.appendingPathComponent("ipa", isDirectory: true)
        let assetDirectory = scratch.appendingPathComponent("assets", isDirectory: true)
        try fileManager.createDirectory(at: assetDirectory, withIntermediateDirectories: true)
        defer {
            try? fileManager.removeItem(at: scratch)
        }

        let clock = ContinuousClock()

        let extractDuration = try clock.measure {
            try run("extract") { error in
                zsign_extract(ipa.path, payloadRoot.path, error)
            }
        }

        var report: [Report.SignedBundle] = []
        let signDuration = try clock.measure {
            let bundles = try Self.signableBundles(in: payloadRoot)
            let assets = try bundles.enumerated().map { index, bundle in
                try Self.asset(for: bundle, index: index, profiles: profiles, directory: assetDirectory)
            }
            report = assets.map(\.report)

            let keyURL = assetDirectory.appendingPathComponent("identity.key")
            try identity.privateKey.write(to: keyURL)
            var certificatePath: String?
            if let certificate = identity.certificate {
                let certificateURL = assetDirectory.appendingPathComponent("identity.cer")
                try certificate.write(to: certificateURL)
                certificatePath = certificateURL.path
            }

            let profilePaths = assets.map { strdup($0.profilePath) }
            let entitlementPaths = assets.map { $0.entitlementsPath.flatMap { strdup($0) } }
            defer {
                profilePaths.forEach { free($0) }
                entitlementPaths.forEach { free($0) }
            }
            let cAssets = zip(profilePaths, entitlementPaths).map { profilePath, entitlementsPath in
                zsign_asset_t(profile_path: profilePath, entitlements_path: entitlementsPath)
            }

            try run("sign") { error in
                zsign_sign_folder(
                    payloadRoot.path,
                    keyURL.path,
                    certificatePath,
                    identity.password,
                    cAssets,
                    cAssets.count,
                    error
                )
            }
        }

        let archiveDuration = try clock.measure {
            try? fileManager.removeItem(at: output)
            try run("archive") { error in
                zsign_archive(payloadRoot.path, output.path, Int32(compressionLevel), error)
            }
        }

        return Report(bundles: report, extract: extractDuration, sign: signDuration, archive: archiveDuration)
    }

    private func run(_ step: String, _ body: (UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>) -> Bool) throws {
        var error: UnsafeMutablePointer<CChar>?
        defer {
            zsign_free(error)
        }
        guard body(&error) else {
            let message = error.map { String(cString: $0) } ?? "unknown error"
            throw SigningError.zsign(step: step, message: message.trimmingCharacters(in: .whitespacesAndNewlines))
        }
    }
}

extension IPASigner {

    struct SignableBundle {
        let url: URL
        let bundleID: String
        let executable: URL
    }

    struct Asset {
        let profilePath: String
        let entitlementsPath: String?
        let report: Report.SignedBundle
    }

    /// The main app first, then every app extension: the bundles that carry their own profile.
    static func signableBundles(in payloadRoot: URL) throws -> [SignableBundle] {
        let fileManager = FileManager.default
        let payload = payloadRoot.appendingPathComponent("Payload", isDirectory: true)
        let contents = (try? fileManager.contentsOfDirectory(at: payload, includingPropertiesForKeys: nil)) ?? []
        guard let app = contents.first(where: { $0.pathExtension == "app" }) else {
            throw SigningError.appBundleNotFound
        }

        var bundles = [try bundle(at: app)]
        for folder in ["PlugIns", "Extensions"] {
            let directory = app.appendingPathComponent(folder, isDirectory: true)
            let extensions = (try? fileManager.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
            for appex in extensions.sorted(by: { $0.path < $1.path }) where appex.pathExtension == "appex" {
                bundles.append(try bundle(at: appex))
            }
        }
        return bundles
    }

    private static func bundle(at url: URL) throws -> SignableBundle {
        guard
            let data = try? Data(contentsOf: url.appendingPathComponent("Info.plist")),
            let plist = try? PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any],
            let bundleID = plist["CFBundleIdentifier"] as? String,
            let executable = plist["CFBundleExecutable"] as? String
        else {
            throw SigningError.invalidBundle(url.path)
        }
        return SignableBundle(url: url, bundleID: bundleID, executable: url.appendingPathComponent(executable))
    }

    /// Keeps the entitlements the binary was built with. A profile's own entitlements hold
    /// wildcards and every environment it allows, which is not what a signed app should claim.
    private static func asset(
        for bundle: SignableBundle,
        index: Int,
        profiles: [ProvisioningProfile],
        directory: URL
    ) throws -> Asset {
        guard let profile = profiles.first(where: { $0.bundleID == bundle.bundleID }) else {
            throw SigningError.missingProfile(bundleID: bundle.bundleID)
        }

        let profileURL = directory.appendingPathComponent("\(index).mobileprovision")
        try profile.data.write(to: profileURL)

        var entitlementsPath: String?
        if let entitlements = try MachOEntitlements.read(from: bundle.executable) {
            let plist = try PropertyListSerialization.propertyList(from: entitlements, format: nil) as? [String: Any]
            let applicationIdentifier = plist?["application-identifier"] as? String ?? ""
            guard applicationIdentifier == profile.applicationIdentifier else {
                throw SigningError.entitlementsMismatch(
                    bundleID: bundle.bundleID,
                    binary: applicationIdentifier,
                    profile: profile.applicationIdentifier
                )
            }
            let entitlementsURL = directory.appendingPathComponent("\(index).entitlements")
            try entitlements.write(to: entitlementsURL)
            entitlementsPath = entitlementsURL.path
        }

        return Asset(
            profilePath: profileURL.path,
            entitlementsPath: entitlementsPath,
            report: Report.SignedBundle(
                bundleID: bundle.bundleID,
                profileUUID: profile.uuid,
                reusedEntitlements: entitlementsPath != nil
            )
        )
    }
}
