//
//  Zsign.swift
//  Feather
//
//  Created by samara on 17.04.2025.
//

import ZsignC

public enum Zsign {
	/// Checks if the MachO-file is properly signed
	/// - Parameter appExecutable: Executable
	/// - Returns: True if its signed
	static public func checkSigned(appExecutable: String) -> Bool {
		CheckIfSigned(appExecutable)
	}
	/// Injects a load command to an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - path: Load command (i.e. `@rpath/CydiaSubstrate.framework`)
	///   - weak: Weak inject
	/// - Returns: True if its successful
	static public func injectDyLib(appExecutable: String, with path: String, weak: Bool = true) -> Bool {
		InjectDyLib(appExecutable, path, weak)
	}
	/// Removes load commands from an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - dylibs: Load commands (i.e. `@rpath/CydiaSubstrate.framework...`)
	/// - Returns: True if its successful
	static public func removeDylibs(appExecutable: String, using dylibs: [String]) -> Bool  {
		UninstallDylibs(appExecutable, dylibs)
	}
	/// List load commands from an executable
	/// - Parameter appExecutable: Executable
	/// - Returns: String array with load commands if any
	static public func listDylibs(appExecutable: String) -> [String] {
		ListDylibs(appExecutable)
	}
	/// Matches and replaces load commands to an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - old: Old load command (i.e. `/Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate`)
	///   - new: New load command (i.e. `@rpath/CydiaSubstrate.framework/CydiaSubstrate`)
	/// - Returns: True if its successful
	static public func changeDylibPath(appExecutable: String, for old: String, with new: String) -> Bool {
		ChangeDylibPath(appExecutable, old, new)
	}
	/// Signs a folder (application bundle) using Zsign
	/// - Parameters:
	///   - appPath: Relative path to app bundle
	///   - provisionPath: Relative path to a provisioning file (i.e. `samara.mobileprovision`)
	///   - p12Path: Relative path to a key file (i.e. `samara.p12`)
	///   - p12Password: Password to the key file
	///   - entitlementsPath: Relative path to an entitlements file
	///   - customIdentifier: Custom indentifier for the app bundle
	///   - customName: Custom display name for the app bundle
	///   - customVersion: Custom version for the app bundle
	///   - adhoc: If the app bundle should be signed using Adhoc (no signing identity)
	///   - removeProvision: If `embedded.mobileprovision` should be excluded when signing
	/// - Returns: True if its successful
	static public func sign(
		appPath: String = "",
		provisionPath: String = "",
		p12Path: String = "",
		p12Password: String = "",
		entitlementsPath: String = "",
		customIdentifier: String = "",
		customName: String = "",
		customVersion: String = "",
		adhoc: Bool = false,
		removeProvision: Bool = false,
		completion: ((Bool, Error?) -> Void)? = nil
	) -> Bool {
		if zsign(
			appPath,
			provisionPath,
			p12Path,
			p12Password,
			entitlementsPath,
			customIdentifier,
			customName,
			customVersion,
			adhoc,
			removeProvision,
			completion.map { callback in
				{ success, error in
					callback(success, error)
				}
			}
		) != 0 {
			return false
		}
		return true
	}
	/// Check revokage
	/// - Parameters:
	///   - provisionPath: Relative path to a provisioning file (i.e. `samara.mobileprovision`)
	///   - p12Path: Relative path to a key file (i.e. `samara.p12`)
	///   - p12Password: Password to the key file
	///   - completionHandler: Handler
	static public func checkRevokage(
		provisionPath: String = "",
		p12Path: String = "",
		p12Password: String = "",
		completionHandler: @escaping (Int32, Date?, String?) -> Void
	) {
		checkCert(
			provisionPath,
			p12Path,
			p12Password
		) { status, expirationDate, error in
			completionHandler(status, expirationDate, error)
		}
	}
}
