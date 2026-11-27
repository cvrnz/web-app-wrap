import AppKit
import WebAppWrapKit

enum IconSource {
    /// A colored gradient tile (hue derived from `name`) showing an SF Symbol if given,
    /// otherwise the name's first letter.
    case generated(name: String, symbol: String? = nil)
    /// An SVG on a solid `background`. At `zoom` 1 the whole SVG fits in the icon shape;
    /// below 1 it shrinks, above 1 it grows and is cropped, staying centered. With `color`,
    /// every visible part of the SVG is filled with that one color; nil keeps its own colors.
    case svg(NSImage, zoom: CGFloat = 1, background: HexColor = .defaultIconBackground, color: HexColor? = nil)

    /// How far the user can zoom an SVG out and in.
    static let svgZoomRange: ClosedRange<CGFloat> = 0.5...3

    static func isSVG(_ url: URL) -> Bool {
        url.pathExtension.lowercased() == "svg"
    }

    /// Reads an SVG the way the window draws it: on `background` (default #1C1C1C), in `color`
    /// if given, at `zoom`. Icons are SVG only: nil for any other file, or if macOS can't read it.
    static func load(_ url: URL, zoom: Double = AppSettings.defaultSVGZoom, background: HexColor? = nil, color: HexColor? = nil) -> IconSource? {
        guard isSVG(url), let image = NSImage(contentsOf: url), image.isValid,
              image.size.width > 0, image.size.height > 0
        else { return nil }
        let zoom = min(max(CGFloat(zoom), svgZoomRange.lowerBound), svgZoomRange.upperBound)
        return .svg(image, zoom: zoom, background: background ?? .defaultIconBackground, color: color)
    }
}

/// Draws icons on Apple's macOS grid: an 824pt rounded square centered on a 1024pt canvas.
enum IconRenderer {
    private static let canvas: CGFloat = 1024
    private static let tileRect = CGRect(x: 100, y: 100, width: 824, height: 824)
    private static let sRGB = CGColorSpace(name: CGColorSpace.sRGB)!

    /// Every file an .iconset needs.
    static let iconsetEntries: [(name: String, pixels: Int)] = [
        ("icon_16x16", 16), ("icon_16x16@2x", 32),
        ("icon_32x32", 32), ("icon_32x32@2x", 64),
        ("icon_128x128", 128), ("icon_128x128@2x", 256),
        ("icon_256x256", 256), ("icon_256x256@2x", 512),
        ("icon_512x512", 512), ("icon_512x512@2x", 1024),
    ]

    // MARK: - Output

    static func cgImage(_ source: IconSource, pixels: Int) -> CGImage? {
        guard let context = CGContext(
            data: nil, width: pixels, height: pixels, bitsPerComponent: 8, bytesPerRow: 0,
            space: sRGB, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else { return nil }

        context.interpolationQuality = .high
        let scale = CGFloat(pixels) / canvas
        context.scaleBy(x: scale, y: scale)

        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(cgContext: context, flipped: false)
        draw(source, in: context, scale: scale)
        NSGraphicsContext.restoreGraphicsState()

        return context.makeImage()
    }

    static func nsImage(_ source: IconSource, pixels: Int) -> NSImage {
        guard let image = cgImage(source, pixels: pixels) else { return NSImage() }
        return NSImage(cgImage: image, size: NSSize(width: pixels, height: pixels))
    }

    static func pngData(_ source: IconSource, pixels: Int) -> Data? {
        guard let image = cgImage(source, pixels: pixels) else { return nil }
        return NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:])
    }

    /// Writes AppIcon.icns with every size macOS uses (16 through 1024, 1x and 2x).
    static func writeICNS(_ source: IconSource, to destination: URL) throws {
        var pngs: [Int: Data] = [:]
        for pixels in Set(iconsetEntries.map(\.pixels)) {
            guard let data = pngData(source, pixels: pixels) else { throw BuildError.iconRenderingFailed }
            pngs[pixels] = data
        }

        let fileManager = FileManager.default
        let workDir = fileManager.temporaryDirectory.appendingPathComponent("WebAppWrapIcon-\(UUID().uuidString)")
        let iconset = workDir.appendingPathComponent("AppIcon.iconset")
        try fileManager.createDirectory(at: iconset, withIntermediateDirectories: true)
        defer { try? fileManager.removeItem(at: workDir) }

        for entry in iconsetEntries {
            try pngs[entry.pixels]!.write(to: iconset.appendingPathComponent(entry.name + ".png"))
        }

        do {
            try Shell.run("/usr/bin/iconutil", ["-c", "icns", iconset.path, "-o", destination.path])
        } catch {
            // iconutil ships with macOS, but if it ever balks, assemble the file directly.
            try icnsData(pngs: pngs).write(to: destination)
        }
    }

    /// Minimal .icns writer: a header followed by PNG-payload chunks.
    static func icnsData(pngs: [Int: Data]) -> Data {
        let chunkTypes: [(type: String, pixels: Int)] = [
            ("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128), ("ic08", 256), ("ic09", 512),
            ("ic10", 1024), ("ic11", 32), ("ic12", 64), ("ic13", 256), ("ic14", 512),
        ]
        func bigEndian(_ value: Int) -> Data {
            withUnsafeBytes(of: UInt32(value).bigEndian) { Data($0) }
        }

        var body = Data()
        for chunk in chunkTypes {
            guard let png = pngs[chunk.pixels] else { continue }
            body.append(Data(chunk.type.utf8))
            body.append(bigEndian(8 + png.count))
            body.append(png)
        }
        var file = Data("icns".utf8)
        file.append(bigEndian(8 + body.count))
        file.append(body)
        return file
    }

    // MARK: - Drawing (in 1024pt canvas coordinates)

    private static func draw(_ source: IconSource, in context: CGContext, scale: CGFloat) {
        switch source {
        case let .generated(name, symbol):
            drawTile(in: context, scale: scale) {
                drawGradient(seed: name, in: context)
                if let symbol, drawSymbol(symbol) { return }
                drawLetter(of: name)
            }

        case let .svg(image, zoom, background, color):
            let zoom = max(zoom, 0.01)
            let imageRect = tileRect.insetBy(dx: -tileRect.width * (zoom - 1) / 2, dy: -tileRect.height * (zoom - 1) / 2)
            drawTile(in: context, scale: scale) {
                context.setFillColor(cgColor(background))
                context.fill(tileRect)
                guard let color else {
                    drawImage(image, in: imageRect, fill: false)
                    return
                }
                // Draw the SVG on its own layer, then paint the color over just its pixels,
                // keeping its edges' transparency.
                context.saveGState()
                context.beginTransparencyLayer(auxiliaryInfo: nil)
                drawImage(image, in: imageRect, fill: false)
                context.setBlendMode(.sourceIn)
                context.setFillColor(cgColor(color))
                context.fill(imageRect.union(tileRect))
                context.endTransparencyLayer()
                context.restoreGState()
            }

        }
    }

    private static func cgColor(_ hex: HexColor) -> CGColor {
        CGColor(srgbRed: hex.red, green: hex.green, blue: hex.blue, alpha: 1)
    }

    /// White rounded square with Apple's soft drop shadow; `content` draws clipped inside it.
    private static func drawTile(in context: CGContext, scale: CGFloat, content: () -> Void) {
        let shape = squircle(in: tileRect)

        // Shadow offsets are in device pixels, so scale them with the canvas.
        context.saveGState()
        context.setShadow(
            offset: CGSize(width: 0, height: -10 * scale),
            blur: 24 * scale,
            color: CGColor(gray: 0, alpha: 0.3)
        )
        context.addPath(shape)
        context.setFillColor(CGColor(gray: 1, alpha: 1))
        context.fillPath()
        context.restoreGState()

        context.saveGState()
        context.addPath(shape)
        context.clip()
        content()
        context.restoreGState()
    }

    /// Superellipse (|x|^5 + |y|^5 = 1), a close match for the macOS icon outline.
    private static func squircle(in rect: CGRect) -> CGPath {
        let path = CGMutablePath()
        let exponent: CGFloat = 2 / 5
        let steps = 360
        for step in 0..<steps {
            let angle = CGFloat(step) / CGFloat(steps) * 2 * .pi
            let c = cos(angle), s = sin(angle)
            let point = CGPoint(
                x: rect.midX + rect.width / 2 * copysign(pow(abs(c), exponent), c),
                y: rect.midY + rect.height / 2 * copysign(pow(abs(s), exponent), s)
            )
            if step == 0 { path.move(to: point) } else { path.addLine(to: point) }
        }
        path.closeSubpath()
        return path
    }

    private static func drawGradient(seed: String, in context: CGContext) {
        // djb2: stable across launches, unlike Swift's hashValue.
        var hash: UInt64 = 5381
        for byte in seed.lowercased().utf8 { hash = (hash &* 33) &+ UInt64(byte) }
        let hue = CGFloat(hash % 360) / 360

        let top = NSColor(calibratedHue: hue, saturation: 0.55, brightness: 0.98, alpha: 1)
        let bottom = NSColor(
            calibratedHue: (hue + 0.06).truncatingRemainder(dividingBy: 1),
            saturation: 0.8, brightness: 0.74, alpha: 1
        )
        let gradient = CGGradient(
            colorsSpace: sRGB,
            colors: [top.cgColor, bottom.cgColor] as CFArray,
            locations: [0, 1]
        )!
        context.drawLinearGradient(
            gradient,
            start: CGPoint(x: tileRect.midX, y: tileRect.maxY),
            end: CGPoint(x: tileRect.midX, y: tileRect.minY),
            options: []
        )
    }

    private static func drawLetter(of name: String) {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let letter = trimmed.first.map { String($0).uppercased() } ?? "?"

        let size: CGFloat = 500
        let base = NSFont.systemFont(ofSize: size, weight: .semibold)
        let font = base.fontDescriptor.withDesign(.rounded).flatMap { NSFont(descriptor: $0, size: size) } ?? base
        let text = NSAttributedString(string: letter, attributes: [.font: font, .foregroundColor: NSColor.white])

        let width = text.size().width
        let baseline = tileRect.midY - font.capHeight / 2
        text.draw(at: CGPoint(x: tileRect.midX - width / 2, y: baseline + font.descender))
    }

    private static func drawSymbol(_ name: String) -> Bool {
        let configuration = NSImage.SymbolConfiguration(pointSize: 420, weight: .medium)
            .applying(NSImage.SymbolConfiguration(paletteColors: [.white]))
        guard let symbol = NSImage(systemSymbolName: name, accessibilityDescription: nil)?
            .withSymbolConfiguration(configuration)
        else { return false }
        drawImage(symbol, in: tileRect.insetBy(dx: 170, dy: 170), fill: false)
        return true
    }

    private static func drawImage(_ image: NSImage, in rect: CGRect, fill: Bool) {
        let size = image.size
        guard size.width > 0, size.height > 0 else { return }
        let ratio = fill
            ? max(rect.width / size.width, rect.height / size.height)
            : min(rect.width / size.width, rect.height / size.height)
        let drawn = CGSize(width: size.width * ratio, height: size.height * ratio)
        let target = CGRect(
            x: rect.midX - drawn.width / 2, y: rect.midY - drawn.height / 2,
            width: drawn.width, height: drawn.height
        )
        image.draw(
            in: target, from: .zero, operation: .sourceOver, fraction: 1,
            respectFlipped: true, hints: [.interpolation: NSImageInterpolation.high.rawValue]
        )
    }
}
