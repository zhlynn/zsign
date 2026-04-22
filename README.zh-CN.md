# zsign — 快速跨平台 iOS 代码签名工具

[![Build](https://github.com/zhlynn/zsign/actions/workflows/build.yml/badge.svg)](https://github.com/zhlynn/zsign/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20Android%20%7C%20FreeBSD-lightgrey)](#构建)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C.svg)](#构建)
[![Stars](https://img.shields.io/github/stars/zhlynn/zsign?style=social)](https://github.com/zhlynn/zsign)

**语言:** [English](README.md) | 简体中文

**zsign** 是一个快速、开源、跨平台的 iOS 代码签名工具，兼容 iOS 12+，可作为 Apple `codesign` 的替代品。它可以在无需 Xcode、无需 macOS 的环境下，使用自定义证书和描述文件对 `.ipa` 安装包、Mach-O 二进制文件以及 `.app` Bundle 进行重签名。适用于 iOS 重签名、dylib 注入、CI/CD 流水线，以及在 Linux / Windows 服务器上分发 IPA 的工作流。

**关键词:** iOS 签名 · iOS 重签名 · codesign 替代品 · IPA 重签 · Mach-O 签名 · dylib 注入 · iOS 自动化打包 · Linux 签名 iOS · Windows 签名 iOS · p12 签名 · 描述文件 · OCSP 检查 · 临时签名 · ad-hoc 签名

如果这个工具对你有帮助，请点一个 ⭐ **Star** — [zhlynn](https://github.com/zhlynn)

## 目录

- [功能特性](#功能特性)
- [支持平台](#支持平台)
- [构建](#构建)
- [使用方法](#使用方法)
- [示例](#示例)
- [证书检查 (-C)](#证书检查--c)
- [快速重签名](#快速重签名)
- [常见问题](#常见问题)
- [开源协议](#开源协议)

## 功能特性

- **快速 IPA 重签名** — 通过 `.zsign_cache` 持久化缓存，后续签名可跳过未变更的 Mach-O
- **跨平台** — 支持 macOS、Linux、Windows、Android、FreeBSD（无需 Xcode）
- **dylib 注入 / 移除** — 支持 `LC_LOAD_DYLIB` 与 `LC_LOAD_WEAK_DYLIB`
- **多描述文件签名** — 支持 App Extensions 的独立描述文件
- **Bundle 信息修改** — 修改 Bundle ID、显示名称、版本号、最低 OS 版本、entitlements
- **Ad-hoc 临时签名** — 无需开发者证书即可签名
- **证书 / OCSP 检查** — 解析 `.ipa`、`.p12`、`.mobileprovision`、`.cer`、`.pem`、Mach-O 中的证书
- **元数据提取** — 导出 `Info.plist` 字段和应用图标为 `metadata.json` + PNG
- **Bundle 清理** — 移除 App Extensions、Watch App、`UISupportedDevices`
- **Files App 集成开关** — 一键开启 `UISupportsDocumentBrowser` 和 `UIFileSharingEnabled`

## 支持平台

macOS · Linux · Windows · Android · FreeBSD

## 构建

### macOS

```bash
brew install pkg-config openssl
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/macos
make clean && make
```

### Linux

#### Ubuntu / Debian

```bash
sudo apt-get install -y git g++ pkg-config libssl-dev
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/linux
make clean && make
```

#### RHEL / CentOS / Alma / Rocky

先安装 `epel-release`：

```bash
sudo yum -y install epel-release
```

再构建：

```bash
sudo yum install -y git gcc-c++ pkg-config openssl-devel
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/linux
make clean && make
```

### Windows

使用 Visual Studio 2022 打开 `build/windows/vs2022/zsign.sln` 进行构建。

## 使用方法

```
Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder

Options:
  -k, --pkey              私钥或 p12 文件路径（PEM 或 DER 格式）
  -m, --prov              描述文件路径（Extensions 场景可多次使用 -m）
  -c, --cert              证书文件路径（PEM 或 DER 格式）
  -a, --adhoc             仅进行 ad-hoc 临时签名
  -d, --debug             输出调试信息到 .zsign_debug 目录
  -f, --force             强制签名，忽略缓存
  -o, --output            输出的 ipa 路径
  -p, --password          私钥或 p12 文件的密码
  -b, --bundle_id         新的 Bundle ID
  -n, --bundle_name       新的 Bundle 显示名称
  -r, --bundle_version    新的 Bundle 版本号
  -e, --entitlements      新的 entitlements 文件
  -z, --zip_level         输出 ipa 的压缩等级（0-9）
  -l, --dylib             注入 dylib（可多次使用）
  -D, --rm_dylib          移除 dylib（可多次使用）
  -w, --weak              以 LC_LOAD_WEAK_DYLIB 方式注入
  -i, --install           签名后通过 ideviceinstaller 安装
  -t, --temp_folder       中间文件的临时目录
  -2, --sha256_only       仅使用 SHA256 Code Directory
  -C, --check             检查证书有效期及 OCSP 状态
  -x, --metadata          导出元数据与图标到指定目录
  -R, --rm_provision      签名后移除描述文件
  -S, --enable_docs       开启 UISupportsDocumentBrowser 与 UIFileSharingEnabled
  -M, --min_version       修改 Info.plist 中的 MinimumOSVersion
  -E, --rm_extensions     移除所有 App Extensions（PlugIns/Extensions）
  -W, --rm_watch          移除 Bundle 中的 Watch App
  -U, --rm_uisd           移除 Info.plist 中的 UISupportedDevices
  -q, --quiet             安静模式
  -v, --version           显示版本
  -h, --help              显示帮助
```

## 示例

**查看 Mach-O 与代码签名信息：**
```bash
zsign demo.app/demo
```

**签名一个 IPA：**
```bash
zsign -k privkey.pem -m dev.prov -o output.ipa -z 9 demo.ipa
```

**使用 p12 签名（带缓存）：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.app
```

**使用 p12 强制签名（不使用缓存）：**
```bash
zsign -f -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.app
```

**Ad-hoc 临时签名：**
```bash
zsign -a -o output.ipa demo.ipa
```

**注入 dylib 并重签：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -l demo.dylib -o output.ipa demo.ipa
```

**修改 Bundle ID 与名称：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -b 'com.new.bundle.id' -n 'NewName' -o output.ipa demo.ipa
```

**向 Mach-O 注入 dylib（LC_LOAD_DYLIB）：**
```bash
zsign -a -l "@executable_path/demo1.dylib" -l "@executable_path/demo2.dylib" demo.app/execute
```

**注入 weak dylib（LC_LOAD_WEAK_DYLIB）：**
```bash
zsign -w -l "@executable_path/demo.dylib" demo.app/execute
```

**导出元数据与图标：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -x ./metadata -o output.ipa demo.ipa
# 输出 ./metadata/metadata.json 与 ./metadata/<hash>.png
```

**开启 Files App 集成：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -S -o output.ipa demo.ipa
```

**设置最低 iOS 版本：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -M 14.0 -o output.ipa demo.ipa
```

**移除所有 App Extensions：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -E -o output.ipa demo.ipa
```

**移除 Watch App：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -W -o output.ipa demo.ipa
```

**移除 UISupportedDevices：**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -U -o output.ipa demo.ipa
```

## 证书检查 (-C)

检查任意支持类型文件中的签名证书，并向 Apple OCSP 服务器查询吊销状态。对 IPA 内部的 Mach-O 可直接读取，无需解压到磁盘。

**支持的文件类型:** `.ipa`、`.mobileprovision`、`.p12`/`.pfx`、`.cer`/`.pem`、Mach-O 二进制

```bash
# 检查 IPA
zsign -C demo.ipa

# 检查描述文件
zsign -C dev.mobileprovision

# 检查 P12/PFX 证书
zsign -C dev.p12 -p 123

# 检查 Mach-O
zsign -C demo.app/demo

# 签名前验证证书
zsign -C -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.ipa
```

**输出示例：**
```
>>> Check:      demo.ipa (IPA)
>>> Signed:     Yes
>>> Name:       Apple Distribution: Company Name (TEAMID)
>>> Type:       Apple Distribution
>>> Org:        Company Name
>>> Team:       TEAMID
>>> Serial:     XX:XX:XX:XX:XX:XX:XX:XX
>>> Issued:     2025-01-01T00:00:00Z
>>> Expires:    2026-01-01T00:00:00Z (365 days remaining)
>>> Algorithm:  RSA 2048-bit
>>> Issuer:     Apple Worldwide Developer Relations Certification Authority
>>> OCSP:       Valid (ocsp.apple.com)
```

## 快速重签名

先将 IPA 解压，再对解压后的目录签名。首次签名时 zsign 会在 `.zsign_cache` 中缓存签名数据；后续更换证书/描述文件再次签名时会复用缓存，比每次都重新跑 `codesign` 快得多 — 这是 zsign 的核心优势之一。

## 常见问题

**Q: zsign 是否必须在 macOS 或 Xcode 环境下运行？**
不需要。zsign 可直接在 Linux、Windows、macOS、Android、FreeBSD 上运行，不依赖 Xcode 或 Apple 的 `codesign`。

**Q: 可以用于 CI/CD 流水线吗？**
可以。zsign 是单一可执行文件，天然适合自动化场景，可在 Linux Runner、Docker 镜像、Windows 构建机上运行。

**Q: 支持 SHA256-only Code Directory 吗？**
支持，使用 `-2` / `--sha256_only` 参数即可。

**Q: 相比 Apple 的 `codesign` 有什么优势？**
跨平台运行、支持非 Apple 主机脚本化调用、原生支持 dylib 注入，以及基于缓存的快速重签名。

## 开源协议

zsign 基于 MIT 协议开源。详见 [LICENSE](LICENSE) 文件。
