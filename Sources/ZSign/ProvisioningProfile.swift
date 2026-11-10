import Foundation

public struct ProvisioningProfile: Sendable {

    public enum ParseError: Error, Equatable {
        case plistNotFound
        case missingField(String)
    }

    public let data: Data
    public let uuid: String
    public let name: String
    public let teamID: String
    public let applicationIdentifier: String
    public let expirationDate: Date
    public let provisionedDeviceCount: Int?
    public let certificates: [Data]

    public var bundleID: String {
        String(applicationIdentifier.dropFirst(teamID.count + 1))
    }

    public init(data: Data) throws {
        guard
            let start = data.range(of: Data("<?xml".utf8)),
            let end = data.range(of: Data("</plist>".utf8), in: start.lowerBound..<data.endIndex)
        else {
            throw ParseError.plistNotFound
        }

        let plist = try PropertyListSerialization.propertyList(
            from: data[start.lowerBound..<end.upperBound],
            format: nil
        )
        guard let root = plist as? [String: Any] else {
            throw ParseError.plistNotFound
        }

        func require<Value>(_ key: String, in dictionary: [String: Any]) throws -> Value {
            guard let value = dictionary[key] as? Value else {
                throw ParseError.missingField(key)
            }
            return value
        }

        let entitlements: [String: Any] = try require("Entitlements", in: root)
        let teamIDs: [String] = try require("TeamIdentifier", in: root)
        guard let teamID = teamIDs.first else {
            throw ParseError.missingField("TeamIdentifier")
        }

        self.data = data
        self.uuid = try require("UUID", in: root)
        self.name = try require("Name", in: root)
        self.teamID = teamID
        self.applicationIdentifier = try require("application-identifier", in: entitlements)
        self.expirationDate = try require("ExpirationDate", in: root)
        self.provisionedDeviceCount = (root["ProvisionedDevices"] as? [Any])?.count
        self.certificates = (root["DeveloperCertificates"] as? [Data]) ?? []
    }
}
