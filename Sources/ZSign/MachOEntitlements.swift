import Foundation

/// Reads the XML entitlements embedded in a Mach-O code signature.
public enum MachOEntitlements {

    public enum ReadError: Error, Equatable {
        case notMachO
        case truncated
    }

    private static let fatMagic: UInt32 = 0xCAFE_BABE
    private static let thinMagic64: UInt32 = 0xFEED_FACF
    private static let codeSignatureCommand: UInt32 = 0x1D
    private static let embeddedSignatureMagic: UInt32 = 0xFADE_0CC0
    private static let entitlementsMagic: UInt32 = 0xFADE_7171
    private static let entitlementsSlot: UInt32 = 5

    /// Returns nil when the binary is unsigned or carries no entitlements.
    public static func read(from url: URL) throws -> Data? {
        let data = try Data(contentsOf: url, options: .mappedIfSafe)
        let reader = Reader(data: data)

        var sliceOffset = 0
        if try reader.bigEndian(at: 0) == fatMagic {
            guard try reader.bigEndian(at: 4) > 0 else {
                throw ReadError.notMachO
            }
            sliceOffset = Int(try reader.bigEndian(at: 16))
        }

        guard try reader.littleEndian(at: sliceOffset) == thinMagic64 else {
            throw ReadError.notMachO
        }

        let commandCount = try reader.littleEndian(at: sliceOffset + 16)
        var commandOffset = sliceOffset + 32
        for _ in 0..<commandCount {
            let command = try reader.littleEndian(at: commandOffset)
            let commandSize = Int(try reader.littleEndian(at: commandOffset + 4))
            if command == codeSignatureCommand {
                let signatureOffset = sliceOffset + Int(try reader.littleEndian(at: commandOffset + 8))
                return try entitlements(in: reader, signatureOffset: signatureOffset)
            }
            guard commandSize > 0 else {
                throw ReadError.truncated
            }
            commandOffset += commandSize
        }
        return nil
    }

    private static func entitlements(in reader: Reader, signatureOffset: Int) throws -> Data? {
        guard try reader.bigEndian(at: signatureOffset) == embeddedSignatureMagic else {
            return nil
        }
        let blobCount = try reader.bigEndian(at: signatureOffset + 8)
        for index in 0..<Int(blobCount) {
            let entryOffset = signatureOffset + 12 + index * 8
            guard try reader.bigEndian(at: entryOffset) == entitlementsSlot else {
                continue
            }
            let blobOffset = signatureOffset + Int(try reader.bigEndian(at: entryOffset + 4))
            guard try reader.bigEndian(at: blobOffset) == entitlementsMagic else {
                return nil
            }
            let blobLength = Int(try reader.bigEndian(at: blobOffset + 4))
            guard blobLength >= 8 else {
                throw ReadError.truncated
            }
            return try reader.bytes(at: blobOffset + 8, count: blobLength - 8)
        }
        return nil
    }

    private struct Reader {
        let data: Data

        func littleEndian(at offset: Int) throws -> UInt32 {
            guard offset >= 0, offset + 4 <= data.count else {
                throw ReadError.truncated
            }
            return data.withUnsafeBytes { buffer in
                UInt32(littleEndian: buffer.loadUnaligned(fromByteOffset: offset, as: UInt32.self))
            }
        }

        func bigEndian(at offset: Int) throws -> UInt32 {
            try littleEndian(at: offset).byteSwapped
        }

        func bytes(at offset: Int, count: Int) throws -> Data {
            guard offset >= 0, count >= 0, offset + count <= data.count else {
                throw ReadError.truncated
            }
            let start = data.startIndex + offset
            return Data(data[start..<start + count])
        }
    }
}
