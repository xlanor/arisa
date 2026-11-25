import AppKit

struct RGBA: Equatable {
    var r: Double
    var g: Double
    var b: Double
    var a: Double

    var hex: String {
        let h = String(format: "#%02X%02X%02X", Self.byte(r), Self.byte(g), Self.byte(b))
        let percent = Int((a * 100).rounded())
        return percent >= 100 ? h : "\(h)@\(percent)%"
    }

    static func byte(_ v: Double) -> Int { max(0, min(255, Int((v * 255).rounded()))) }

    func over(_ base: RGBA) -> RGBA {
        RGBA(r: r * a + base.r * (1 - a), g: g * a + base.g * (1 - a), b: b * a + base.b * (1 - a), a: 1)
    }

    func distance(_ other: RGBA) -> Double {
        let dr = r - other.r, dg = g - other.g, db = b - other.b
        return (dr * dr + dg * dg + db * db).squareRoot()
    }

    var luminance: Double { 0.2126 * r + 0.7152 * g + 0.0722 * b }

    static func of(_ colour: NSColor, appearance: NSAppearance) -> RGBA {
        var out = RGBA(r: 0, g: 0, b: 0, a: 0)
        appearance.performAsCurrentDrawingAppearance {
            if let c = colour.usingColorSpace(.sRGB) {
                out = RGBA(r: c.redComponent, g: c.greenComponent, b: c.blueComponent, a: c.alphaComponent)
            }
        }
        return out
    }
}

struct Raster {
    let width: Int
    let height: Int
    let scale: CGFloat
    let pixels: [UInt8]
    let background: RGBA

    static func render(_ view: NSView, size: CGSize, scale: CGFloat, scheme: Scheme) -> Raster {
        let w = Int(size.width * scale), h = Int(size.height * scale)
        let base = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .calibratedRGB, bitmapFormat: [], bytesPerRow: w * 4, bitsPerPixel: 32)!
        let rep = base.retagging(with: .sRGB)!
        rep.size = size
        view.cacheDisplay(in: NSRect(origin: .zero, size: size), to: rep)
        let count = rep.bytesPerRow * h
        var bytes = [UInt8](repeating: 0, count: w * h * 4)
        let data = rep.bitmapData!
        for y in 0..<h {
            for x in 0..<(w * 4) {
                bytes[y * w * 4 + x] = data[y * rep.bytesPerRow + x]
            }
        }
        _ = count
        let background = RGBA.of(.windowBackgroundColor, appearance: scheme.appearance)
        return Raster(width: w, height: h, scale: scale, pixels: bytes, background: background)
    }

    func straight(_ x: Int, _ y: Int) -> RGBA {
        guard x >= 0, y >= 0, x < width, y < height else { return RGBA(r: 0, g: 0, b: 0, a: 0) }
        let i = (y * width + x) * 4
        let a = Double(pixels[i + 3]) / 255
        if a <= 0 { return RGBA(r: 0, g: 0, b: 0, a: 0) }
        return RGBA(r: min(1, Double(pixels[i]) / 255 / a), g: min(1, Double(pixels[i + 1]) / 255 / a), b: min(1, Double(pixels[i + 2]) / 255 / a), a: a)
    }

    func flat(_ x: Int, _ y: Int) -> RGBA { straight(x, y).over(background) }

    func alpha(_ x: Int, _ y: Int) -> Double {
        guard x >= 0, y >= 0, x < width, y < height else { return 0 }
        return Double(pixels[(y * width + x) * 4 + 3]) / 255
    }

    func px(_ v: CGFloat) -> Int { Int((v * scale).rounded(.down)) }

    func sample(_ point: CGPoint) -> RGBA {
        let x = px(point.x), y = px(point.y)
        var acc = RGBA(r: 0, g: 0, b: 0, a: 0)
        var n = 0.0
        for dy in 0...1 {
            for dx in 0...1 {
                let s = straight(x + dx, y + dy)
                acc.r += s.r * s.a; acc.g += s.g * s.a; acc.b += s.b * s.a; acc.a += s.a
                n += 1
            }
        }
        if acc.a <= 0 { return RGBA(r: 0, g: 0, b: 0, a: 0) }
        return RGBA(r: acc.r / acc.a, g: acc.g / acc.a, b: acc.b / acc.a, a: acc.a / n)
    }

    func sampleFlat(_ point: CGPoint) -> RGBA { sample(point).over(background) }

    func opaqueBounds(in rect: CGRect, threshold: Double = 0.5) -> CGRect? {
        let x0 = max(0, px(rect.minX)), x1 = min(width - 1, px(rect.maxX))
        let y0 = max(0, px(rect.minY)), y1 = min(height - 1, px(rect.maxY))
        guard x0 <= x1, y0 <= y1 else { return nil }
        var minX = Int.max, minY = Int.max, maxX = Int.min, maxY = Int.min
        for y in y0...y1 {
            for x in x0...x1 where alpha(x, y) >= threshold {
                minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
            }
        }
        guard minX <= maxX else { return nil }
        return CGRect(x: CGFloat(minX) / scale, y: CGFloat(minY) / scale, width: CGFloat(maxX - minX + 1) / scale, height: CGFloat(maxY - minY + 1) / scale)
    }

    func differingBounds(in rect: CGRect, from reference: RGBA, tolerance: Double = 0.02) -> CGRect? {
        let x0 = max(0, px(rect.minX)), x1 = min(width - 1, px(rect.maxX))
        let y0 = max(0, px(rect.minY)), y1 = min(height - 1, px(rect.maxY))
        guard x0 <= x1, y0 <= y1 else { return nil }
        var minX = Int.max, minY = Int.max, maxX = Int.min, maxY = Int.min
        for y in y0...y1 {
            for x in x0...x1 where flat(x, y).distance(reference) > tolerance {
                minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
            }
        }
        guard minX <= maxX else { return nil }
        return CGRect(x: CGFloat(minX) / scale, y: CGFloat(minY) / scale, width: CGFloat(maxX - minX + 1) / scale, height: CGFloat(maxY - minY + 1) / scale)
    }

    func cornerRadius(of box: CGRect, coverage: (Int, Int) -> Double) -> Double {
        let x0 = px(box.minX), y0 = px(box.minY)
        let limit = min(px(box.width), px(box.height)) / 2
        var previous = coverage(x0, y0)
        if previous >= 0.5 { return 0 }
        for k in 1..<max(2, limit) {
            let c = coverage(x0 + k, y0 + k)
            if c >= 0.5 {
                let t = Double(k - 1) + (0.5 - previous) / max(c - previous, 1e-6)
                let inset = (t + 0.5) / Double(scale)
                return inset / (1 - 1 / 2.0.squareRoot())
            }
            previous = c
        }
        return Double(limit) / Double(scale)
    }

    func alphaRadius(of box: CGRect) -> Double {
        cornerRadius(of: box) { alpha($0, $1) }
    }

    func contrastRadius(of box: CGRect, fill: RGBA) -> Double {
        let span = max(fill.distance(background), 1e-6)
        return cornerRadius(of: box) { x, y in
            let d = flat(x, y).distance(background) / span
            return min(1, d)
        }
    }

    func span(from point: CGPoint, tolerance: Double = 0.03, limit: CGFloat = 400) -> CGRect {
        let seed = sampleFlat(point)
        let y = px(point.y), x = px(point.x)
        var left = x, right = x, top = y, bottom = y
        while left > 0, x - left < Int(limit * scale), flat(left - 1, y).distance(seed) <= tolerance { left -= 1 }
        while right < width - 1, right - x < Int(limit * scale), flat(right + 1, y).distance(seed) <= tolerance { right += 1 }
        while top > 0, y - top < Int(limit * scale), flat(x, top - 1).distance(seed) <= tolerance { top -= 1 }
        while bottom < height - 1, bottom - y < Int(limit * scale), flat(x, bottom + 1).distance(seed) <= tolerance { bottom += 1 }
        return CGRect(x: CGFloat(left) / scale, y: CGFloat(top) / scale, width: CGFloat(right - left + 1) / scale, height: CGFloat(bottom - top + 1) / scale)
    }

    func inkLines(in rect: CGRect, contrast: Double = 0.3, reference: RGBA? = nil) -> [CGRect] {
        let x0 = max(0, px(rect.minX)), x1 = min(width - 1, px(rect.maxX))
        let y0 = max(0, px(rect.minY)), y1 = min(height - 1, px(rect.maxY))
        guard x0 < x1, y0 < y1 else { return [] }
        var lines: [CGRect] = []
        var current: (Int, Int, Int, Int)? = nil
        for y in y0...y1 {
            let base = reference ?? flat(x1, y)
            var minX = Int.max, maxX = Int.min
            for x in x0...x1 where flat(x, y).distance(base) > contrast {
                minX = min(minX, x); maxX = max(maxX, x)
            }
            if minX <= maxX {
                if let c = current { current = (min(c.0, minX), c.1, max(c.2, maxX), y) } else { current = (minX, y, maxX, y) }
            } else if let c = current {
                lines.append(CGRect(x: CGFloat(c.0) / scale, y: CGFloat(c.1) / scale, width: CGFloat(c.2 - c.0 + 1) / scale, height: CGFloat(c.3 - c.1 + 1) / scale))
                current = nil
            }
        }
        if let c = current {
            lines.append(CGRect(x: CGFloat(c.0) / scale, y: CGFloat(c.1) / scale, width: CGFloat(c.2 - c.0 + 1) / scale, height: CGFloat(c.3 - c.1 + 1) / scale))
        }
        return lines
    }

    func regionRadius(of box: CGRect, inside: RGBA, outside: RGBA) -> Double {
        let span = max(inside.distance(outside), 1e-6)
        return cornerRadius(of: box) { x, y in min(1, flat(x, y).distance(outside) / span) }
    }

    func relativeRun(column x: CGFloat, from y0: CGFloat, to y1: CGFloat) -> Double {
        let cx = px(x)
        var peak = 0.0
        for y in px(y0)...max(px(y0), px(y1)) { peak = max(peak, alpha(cx, y)) }
        return runLength(column: x, from: y0, to: y1, threshold: max(peak * 0.5, 0.01))
    }

    func runLength(column x: CGFloat, from y0: CGFloat, to y1: CGFloat, threshold: Double = 0.4) -> Double {
        let cx = px(x)
        var best = 0, run = 0
        for y in px(y0)...max(px(y0), px(y1)) {
            if alpha(cx, y) >= threshold { run += 1; best = max(best, run) } else { run = 0 }
        }
        return Double(best) / Double(scale)
    }

    func runLength(row y: CGFloat, from x0: CGFloat, to x1: CGFloat, threshold: Double = 0.4) -> Double {
        let cy = px(y)
        var best = 0, run = 0
        for x in px(x0)...max(px(x0), px(x1)) {
            if alpha(x, cy) >= threshold { run += 1; best = max(best, run) } else { run = 0 }
        }
        return Double(best) / Double(scale)
    }

    func extreme(in rect: CGRect, from reference: RGBA) -> RGBA {
        var best = reference, distance = -1.0
        let y0 = max(0, px(rect.minY)), y1 = min(height - 1, px(rect.maxY))
        let x0 = max(0, px(rect.minX)), x1 = min(width - 1, px(rect.maxX))
        guard y0 <= y1, x0 <= x1 else { return reference }
        for y in y0...y1 {
            for x in x0...x1 {
                let c = flat(x, y)
                let d = c.distance(reference)
                if d > distance { distance = d; best = c }
            }
        }
        return best
    }

    func png(crop: CGRect) -> Data {
        let x0 = max(0, px(crop.minX)), y0 = max(0, px(crop.minY))
        let x1 = min(width, px(crop.maxX)), y1 = min(height, px(crop.maxY))
        let w = max(1, x1 - x0), h = max(1, y1 - y0)
        let base = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h, bitsPerSample: 8, samplesPerPixel: 3, hasAlpha: false, isPlanar: false, colorSpaceName: .calibratedRGB, bytesPerRow: w * 3, bitsPerPixel: 24)!
        let rep = base.retagging(with: .sRGB)!
        let data = rep.bitmapData!
        for y in 0..<h {
            for x in 0..<w {
                let c = flat(x0 + x, y0 + y)
                let i = y * rep.bytesPerRow + x * 3
                data[i] = UInt8(RGBA.byte(c.r)); data[i + 1] = UInt8(RGBA.byte(c.g)); data[i + 2] = UInt8(RGBA.byte(c.b))
            }
        }
        return rep.representation(using: .png, properties: [:])!
    }
}
