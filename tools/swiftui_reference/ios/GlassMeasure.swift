import AppKit
import CoreGraphics
import Foundation
import ImageIO

struct Shot {
    let width: Int
    let height: Int
    let scale: CGFloat
    let bytes: [UInt8]

    static func load(_ url: URL) -> Shot? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil), let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else { return nil }
        let width = image.width, height = image.height
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        bytes.withUnsafeMutableBytes { raw in
            let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
            context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        }
        return Shot(width: width, height: height, scale: CGFloat(width) / GlassGeometry.canvas.width, bytes: bytes)
    }

    func pixel(_ x: Int, _ y: Int) -> (Double, Double, Double) {
        let cx = max(0, min(width - 1, x)), cy = max(0, min(height - 1, y))
        let at = (cy * width + cx) * 4
        return (Double(bytes[at]) / 255, Double(bytes[at + 1]) / 255, Double(bytes[at + 2]) / 255)
    }

    func colour(_ point: CGPoint) -> (Double, Double, Double) {
        let x = Int((point.x * scale).rounded(.down)), y = Int((point.y * scale).rounded(.down))
        var sum = (0.0, 0.0, 0.0)
        for dy in -1...1 {
            for dx in -1...1 {
                let p = pixel(x + dx, y + dy)
                sum = (sum.0 + p.0, sum.1 + p.1, sum.2 + p.2)
            }
        }
        return (sum.0 / 9, sum.1 / 9, sum.2 / 9)
    }

    func luma(_ point: CGPoint) -> Double {
        let c = colour(point)
        return 0.2126 * c.0 + 0.7152 * c.1 + 0.0722 * c.2
    }

    func scaled(_ factor: CGFloat) -> Data {
        let w = Int(CGFloat(width) * factor / scale), h = Int(CGFloat(height) * factor / scale)
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        var copy = bytes
        let data = NSMutableData()
        copy.withUnsafeMutableBytes { raw in
            let full = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
            let image = full.makeImage()!
            let small = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
            small.interpolationQuality = .high
            small.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
            let destination = CGImageDestinationCreateWithData(data, "public.png" as CFString, 1, nil)!
            CGImageDestinationAddImage(destination, small.makeImage()!, nil)
            CGImageDestinationFinalize(destination)
        }
        return data as Data
    }
}

struct Axis {
    let start: CGPoint
    let direction: CGPoint
    let length: CGFloat
}

final class Table {
    var keys: [String] = []
    var values: [String: [String: String]] = [:]

    static func format(_ value: Double) -> String {
        if !value.isFinite { return "nan" }
        var text = String(format: "%.3f", value)
        while text.hasSuffix("0") { text.removeLast() }
        if text.hasSuffix(".") { text.removeLast() }
        return text == "-0" ? "0" : text
    }

    func put(_ key: String, _ scheme: String, _ value: Double) { put(key, scheme, text: Self.format(value)) }

    func put(_ key: String, _ scheme: String, text: String) {
        if values[key] == nil {
            keys.append(key)
            values[key] = [:]
        }
        values[key]![scheme] = text
    }

    func write(_ url: URL, header: [String]) {
        var text = header.map { "#\t\($0)" }.joined(separator: "\n") + "\nkey\tlight\tdark\n"
        for key in keys {
            text += "\(key)\t\(values[key]!["light"] ?? "")\t\(values[key]!["dark"] ?? "")\n"
        }
        try? text.write(to: url, atomically: true, encoding: .utf8)
    }
}

enum Measure {
    static let depths: [CGFloat] = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 170]
    static let rimDepths: [CGFloat] = [0.5, 1, 1.5, 2, 3, 4, 6, 8]
    static let shadowDepths: [CGFloat] = [1, 2, 4, 8, 16]
    static let variants = ["regular", "clear", "tinted", "tinted_soft", "interactive"]

    static func axes(_ cell: GlassCell) -> [(String, Axis)] {
        let r = cell.rect
        if cell.kind == .arc {
            let mid = GlassGeometry.arcMidline
            return [("top", Axis(start: mid.outer, direction: CGPoint(x: 0, y: 1), length: GlassGeometry.arcWidth))]
        }
        return [
            ("left", Axis(start: CGPoint(x: r.minX, y: r.midY), direction: CGPoint(x: 1, y: 0), length: r.width)),
            ("right", Axis(start: CGPoint(x: r.maxX, y: r.midY), direction: CGPoint(x: -1, y: 0), length: r.width)),
            ("top", Axis(start: CGPoint(x: r.midX, y: r.minY), direction: CGPoint(x: 0, y: 1), length: r.height)),
            ("bottom", Axis(start: CGPoint(x: r.midX, y: r.maxY), direction: CGPoint(x: 0, y: -1), length: r.height)),
        ]
    }

    static func centre(_ cell: GlassCell) -> CGPoint {
        if cell.kind == .arc {
            let mid = GlassGeometry.arcMidline
            return CGPoint(x: (mid.outer.x + mid.inner.x) / 2, y: (mid.outer.y + mid.inner.y) / 2)
        }
        return CGPoint(x: cell.rect.midX, y: cell.rect.midY)
    }

    struct Ramp {
        let shot: Shot
        let black: Shot
        let grey: Shot
    }

    static func decode(_ ramp: Ramp, _ point: CGPoint, horizontal: Bool, cell: CGRect) -> Double? {
        let low = ramp.black.luma(point)
        let gain = (ramp.grey.luma(point) - low) / GlassGeometry.grey
        if gain < 0.05 { return nil }
        let t = (ramp.shot.luma(point) - low) / gain
        let ramp = cell
        return horizontal ? Double(ramp.minX) + t * Double(ramp.width) : Double(ramp.minY) + t * Double(ramp.height)
    }

    static func table_alpha(_ grey: Shot, _ black: Shot, _ point: CGPoint) -> Double {
        1 - (grey.luma(point) - black.luma(point)) / Double(GlassGeometry.grey)
    }

    static func point(_ axis: Axis, _ depth: CGFloat) -> CGPoint {
        CGPoint(x: axis.start.x + axis.direction.x * depth, y: axis.start.y + axis.direction.y * depth)
    }

    static func run(_ shots: URL, _ out: URL, _ header: [String]) {
        let table = Table()
        for scheme in ["light", "dark"] {
            func shot(_ backdrop: String, _ variant: String) -> Shot? {
                Shot.load(shots.appendingPathComponent("shapes_\(backdrop)_\(variant)_\(scheme).png"))
            }
            guard let bareGrey = shot("grey", "none"), let bareStripes = shot("stripes", "none"), let bareWide = shot("stripes_wide", "none") else { continue }
            for variant in variants {
                guard let xShot = shot("ramp_x", variant), let yShot = shot("ramp_y", variant), let grey = shot("grey", variant), let stripes = shot("stripes", variant), let wide = shot("stripes_wide", variant), let black = shot("black", variant), let white = shot("white", variant) else { continue }
                let rampX = Ramp(shot: xShot, black: black, grey: grey)
                let rampY = Ramp(shot: yShot, black: black, grey: grey)
                for cell in GlassGeometry.cells {
                    let base = "glass.\(variant).\(cell.kind.rawValue)"
                    for (side, axis) in axes(cell) {
                        let horizontal = axis.direction.x != 0
                        let ramp = horizontal ? rampX : rampY
                        for depth in depths where depth < axis.length / 2 {
                            let at = point(axis, depth)
                            if let source = decode(ramp, at, horizontal: horizontal, cell: cell.ramp) {
                                let shift = source - Double(horizontal ? at.x : at.y)
                                let inward = Double(horizontal ? axis.direction.x : axis.direction.y)
                                table.put("\(base).shift_\(side).\(Table.format(Double(depth)))", scheme, shift * inward)
                            }
                        }
                        for depth in rimDepths where depth < axis.length / 2 {
                            let at = point(axis, depth)
                            table.put("\(base).rim_\(side).\(Table.format(Double(depth)))", scheme, grey.luma(at) - bareGrey.luma(at))
                        }
                        if cell.kind != .arc {
                            for depth in shadowDepths {
                                let at = point(axis, -depth)
                                table.put("\(base).shadow_\(side).\(Table.format(Double(depth)))", scheme, grey.luma(at) - bareGrey.luma(at))
                            }
                        }
                    }
                    let middle = centre(cell)
                    let veil = grey.colour(middle)
                    table.put("\(base).veil", scheme, text: String(format: "#%02X%02X%02X", Int((veil.0 * 255).rounded()), Int((veil.1 * 255).rounded()), Int((veil.2 * 255).rounded())))
                    table.put("\(base).veil_luma", scheme, grey.luma(middle) - bareGrey.luma(middle))
                    for (label, flat) in [("black", black), ("white", white)] {
                        let c = flat.colour(middle)
                        table.put("\(base).on_\(label)", scheme, text: String(format: "#%02X%02X%02X", Int((c.0 * 255).rounded()), Int((c.1 * 255).rounded()), Int((c.2 * 255).rounded())))
                    }
                    table.put("\(base).veil_alpha", scheme, table_alpha(grey, black, middle))
                    var magnification = 1.0
                    if cell.kind != .arc, let a = decode(rampX, CGPoint(x: middle.x - 4, y: middle.y), horizontal: true, cell: cell.ramp), let b = decode(rampX, CGPoint(x: middle.x + 4, y: middle.y), horizontal: true, cell: cell.ramp), abs(b - a) > 1e-3 {
                        magnification = 8 / (b - a)
                    }
                    table.put("\(base).magnification", scheme, magnification)
                    let window: CGFloat = cell.kind == .arc ? 8 : min(16, cell.rect.width / 4)
                    func contrast(_ shot: Shot, _ bare: Shot) -> Double {
                        var low = 1.0, high = 0.0, bareLow = 1.0, bareHigh = 0.0
                        var x = middle.x - window
                        while x <= middle.x + window {
                            let p = CGPoint(x: x, y: middle.y)
                            low = min(low, shot.luma(p))
                            high = max(high, shot.luma(p))
                            bareLow = min(bareLow, bare.luma(p))
                            bareHigh = max(bareHigh, bare.luma(p))
                            x += 1 / shot.scale
                        }
                        return (high - low) / max(bareHigh - bareLow, 1e-3) / max(1 - table_alpha(grey, black, middle), 0.05)
                    }
                    let fine = contrast(stripes, bareStripes)
                    let coarse = contrast(wide, bareWide)
                    table.put("\(base).stripe_contrast", scheme, fine)
                    table.put("\(base).wide_stripe_contrast", scheme, coarse)
                    let useWide = fine < 0.08
                    let measured = min(useWide ? coarse : fine, 0.999)
                    let period = Double(useWide ? GlassGeometry.wideStripePeriod : GlassGeometry.stripePeriod) * magnification
                    let sigma = measured >= 0.98 ? 0 : period / (Double.pi * 2.0.squareRoot()) * (-log(max(measured, 1e-4))).squareRoot()
                    table.put("\(base).frost_sigma", scheme, sigma)
                }
            }
            for (name, file) in [("shapes_photo_none", "glass_backdrop"), ("shapes_photo_regular", "glass_regular"), ("shapes_photo_clear", "glass_clear"), ("shapes_photo_tinted", "glass_tinted"), ("sheet_photo_regular", "glass_sheet_regular"), ("sheet_photo_clear", "glass_sheet_clear"), ("sheet_photo_tinted", "glass_sheet_tinted"), ("sheet_photo_tinted_soft", "glass_sheet_tinted_soft"), ("shapes_photo_tinted_soft", "glass_tinted_soft"), ("system_sheet_photo_none", "glass_sheet_system")] {
                if let photo = Shot.load(shots.appendingPathComponent("\(name)_\(scheme).png")) {
                    try? FileManager.default.createDirectory(at: out.appendingPathComponent("glass"), withIntermediateDirectories: true)
                    try? photo.scaled(1.25).write(to: out.appendingPathComponent("glass/\(file)_\(scheme).png"))
                }
            }
        }
        table.write(out.appendingPathComponent("metrics_glass_ios.tsv"), header: header)
        FileHandle.standardError.write("glass rows \(table.keys.count)\n".data(using: .utf8)!)
    }
}

@main
enum GlassMeasureTool {
    static func main() {
        let arguments = CommandLine.arguments
        Measure.run(URL(fileURLWithPath: arguments[1]), URL(fileURLWithPath: arguments[2]), [
            "runtime \(arguments.count > 3 ? arguments[3] : "unknown")",
            "device \(arguments.count > 4 ? arguments[4] : "unknown")",
            "capture xcrun simctl io screenshot, headless simulator, 3x, sRGB",
            "canvas \(Int(GlassGeometry.canvas.width))x\(Int(GlassGeometry.canvas.height)) pt",
        ])
    }
}
