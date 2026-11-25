import AppKit

final class Store {
    struct Row {
        var key: String
        var unit: String
        var values: [Scheme: String] = [:]
    }

    private(set) var rows: [Row] = []
    private var index: [String: Int] = [:]
    private(set) var images: [(String, Int)] = []
    let imageDirectory: URL

    init(imageDirectory: URL) {
        self.imageDirectory = imageDirectory
        try? FileManager.default.createDirectory(at: imageDirectory, withIntermediateDirectories: true)
    }

    static func format(_ value: Double) -> String {
        if !value.isFinite { return "inf" }
        var text = String(format: "%.3f", value)
        while text.hasSuffix("0") { text.removeLast() }
        if text.hasSuffix(".") { text.removeLast() }
        return text == "-0" ? "0" : text
    }

    private func slot(_ key: String, _ unit: String) -> Int {
        if let i = index[key] { return i }
        rows.append(Row(key: key, unit: unit))
        index[key] = rows.count - 1
        return rows.count - 1
    }

    func put(_ key: String, _ scheme: Scheme?, _ value: Double, _ unit: String = "pt") {
        put(key, scheme, text: Self.format(value), unit)
    }

    func put(_ key: String, _ scheme: Scheme?, text: String, _ unit: String) {
        let i = slot(key, unit)
        for s in scheme.map({ [$0] }) ?? Scheme.allCases {
            rows[i].values[s] = text
        }
    }

    func colour(_ key: String, _ scheme: Scheme, _ value: RGBA) {
        put(key, scheme, text: value.hex, "colour")
    }

    func image(_ name: String, _ shot: Shot, _ rect: CGRect, pad: CGFloat = 6) {
        let crop = rect.insetBy(dx: -pad, dy: -pad).intersection(CGRect(origin: .zero, size: shot.size))
        let data = shot.raster.png(crop: crop)
        let file = imageDirectory.appendingPathComponent("\(name)_\(shot.scheme.rawValue).png")
        try? data.write(to: file)
        images.append((file.lastPathComponent, data.count))
    }

    func write(_ url: URL, header: [String]) {
        var text = header.map { "#\t\($0)" }.joined(separator: "\n") + "\n"
        text += "key\tlight\tdark\tunit\n"
        for row in rows {
            text += "\(row.key)\t\(row.values[.light] ?? "")\t\(row.values[.dark] ?? "")\t\(row.unit)\n"
        }
        try? text.write(to: url, atomically: true, encoding: .utf8)
    }
}

enum Measure {
    static func record(_ store: Store, _ key: String, _ shot: Shot, control: String = "control", label: String? = "label", text: String = "Button", fill: CGPoint? = nil, radius: Bool = true, image: String? = nil) {
        let s = shot.scheme
        let box = shot.frame(control)
        store.put("\(key).width", s, box.width)
        store.put("\(key).height", s, box.height)
        if let label, let inner = shot.frames[label] {
            store.put("\(key).padding_leading", s, inner.minX - box.minX)
            store.put("\(key).padding_top", s, inner.minY - box.minY)
            store.put("\(key).label_height", s, inner.height)
            store.put("\(key).font_size", s, fontSize(width: inner.width, text: text, height: inner.height))
        }
        let padded = (shot.frames[label ?? ""].map { $0.minX - box.minX } ?? 3) >= 2
        let point = fill ?? (padded ? CGPoint(x: box.midX, y: box.minY + min(2.5, box.height / 4)) : CGPoint(x: box.maxX + 3, y: box.midY))
        let raw = shot.raster.sample(point)
        let fillColour = raw.over(shot.raster.background)
        let search = box.insetBy(dx: -4, dy: -4)
        let level = max(raw.a, 0.02)
        if radius, raw.a > 0.02, let opaque = shot.raster.opaqueBounds(in: search, threshold: level * 0.5) {
            store.put("\(key).visual_width", s, opaque.width)
            store.put("\(key).visual_height", s, opaque.height)
            store.put("\(key).radius", s, shot.raster.cornerRadius(of: opaque) { min(1, shot.raster.alpha($0, $1) / level) })
        }
        store.colour("\(key).fill", s, fillColour)
        store.colour("\(key).fill_alpha", s, raw)
        if let label, let inner = shot.frames[label] {
            store.colour("\(key).text", s, shot.raster.extreme(in: inner.insetBy(dx: 1, dy: 1), from: fillColour))
        }
        if let image {
            store.image(image, shot, box)
        }
    }

    static func fontSize(width: CGFloat, text: String, height: CGFloat) -> Double {
        var best = 13.0, error = Double.infinity
        var size = 8.0
        while size <= 30 {
            for weight in [NSFont.Weight.regular, .medium] {
                let font = NSFont.systemFont(ofSize: size, weight: weight)
                let w = (text as NSString).size(withAttributes: [.font: font]).width
                let e = abs(Double(w - width))
                if e < error { error = e; best = size }
            }
            size += 0.5
        }
        return best
    }
}
