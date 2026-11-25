import AppKit
import SwiftUI

enum Paths {
    static let grid: [SIMD2<Float>] = [
        [0, 0], [0.5, 0], [1, 0],
        [0, 0.5], [0.5, 0.5], [1, 0.5],
        [0, 1], [0.5, 1], [1, 1],
    ]

    static let warped: [SIMD2<Float>] = [
        [0, 0], [0.5, 0], [1, 0],
        [0, 0.5], [0.7, 0.3], [1, 0.5],
        [0, 1], [0.5, 1], [1, 1],
    ]

    static let colours: [Color] = [
        Color(red: 1, green: 0, blue: 0), Color(red: 0, green: 1, blue: 0), Color(red: 0, green: 0, blue: 1),
        Color(red: 1, green: 1, blue: 0), Color(red: 1, green: 1, blue: 1), Color(red: 0, green: 0, blue: 0),
        Color(red: 0, green: 1, blue: 1), Color(red: 1, green: 0, blue: 1), Color(red: 1, green: 0.5, blue: 0),
    ]

    static let stops: [Double] = [0.1, 0.25, 0.5, 0.75, 0.9]

    @MainActor static func mesh(_ store: Store, _ name: String, points: [SIMD2<Float>], smooth: Bool) {
        let size: CGFloat = 160
        let view = MeshGradient(width: 3, height: 3, points: points, colors: colours, smoothsColors: smooth)
            .frame(width: size, height: size)
        let renderer = ImageRenderer(content: view)
        renderer.scale = 2
        guard let image = renderer.cgImage else { return }
        let width = image.width, height = image.height
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let context = CGContext(data: &bytes, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        for (row, v) in stops.enumerated() {
            for (column, u) in stops.enumerated() {
                let x = min(width - 1, Int(Double(width) * u))
                let y = min(height - 1, Int(Double(height) * v))
                let offset = (y * width + x) * 4
                let colour = RGBA(r: Double(bytes[offset]) / 255, g: Double(bytes[offset + 1]) / 255, b: Double(bytes[offset + 2]) / 255, a: 1)
                store.put("path.mesh.\(name).\(column)_\(row)", nil, text: colour.hex, "colour")
            }
        }
        let rep = NSBitmapImageRep(cgImage: image)
        if let data = rep.representation(using: .png, properties: [:]) {
            try? data.write(to: store.imageDirectory.appendingPathComponent("path_mesh_\(name)_light.png"))
        }
    }

    @MainActor static func ink(_ store: Store, _ key: String, _ view: some View) {
        let renderer = ImageRenderer(content: view.frame(width: 130, height: 90).background(Color.white))
        renderer.scale = 2
        guard let image = renderer.cgImage else { return }
        let width = image.width, height = image.height
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let context = CGContext(data: &bytes, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        var minX = width, maxX = -1, minY = height, maxY = -1
        for y in 0..<height {
            for x in 0..<width where bytes[(y * width + x) * 4] < 128 {
                minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
            }
        }
        store.put("\(key).min_x", nil, Double(minX) / 2)
        store.put("\(key).max_x", nil, Double(maxX + 1) / 2)
        store.put("\(key).min_y", nil, Double(minY) / 2)
        store.put("\(key).max_y", nil, Double(maxY + 1) / 2)
    }

    static func run(_ store: Store) {
        MainActor.assumeIsolated {
            measureMeshes(store)
            rest(store)
        }
    }

    @MainActor static func measureMeshes(_ store: Store) {
        mesh(store, "smooth", points: grid, smooth: true)
        mesh(store, "linear", points: grid, smooth: false)
        mesh(store, "warped", points: warped, smooth: true)
    }

    @MainActor static func rest(_ store: Store) {
        let corner = Path { p in
            p.move(to: CGPoint(x: 10, y: 10))
            p.addLine(to: CGPoint(x: 110, y: 10))
            p.addLine(to: CGPoint(x: 110, y: 60))
        }
        ink(store, "path.trim.corner_half", corner.trim(from: 0, to: 0.5).stroke(Color.black, lineWidth: 4))
        ink(store, "path.trim.corner_tail", corner.trim(from: 0.5, to: 1).stroke(Color.black, lineWidth: 4))
        let curve = Path { p in
            p.move(to: CGPoint(x: 10, y: 80))
            p.addCurve(to: CGPoint(x: 120, y: 80), control1: CGPoint(x: 10, y: 0), control2: CGPoint(x: 30, y: 0))
        }
        ink(store, "path.trim.curve_half", curve.trim(from: 0, to: 0.5).stroke(Color.black, lineWidth: 2))
        let arc = Path { p in
            p.addArc(center: CGPoint(x: 60, y: 45), radius: 40, startAngle: .degrees(0), endAngle: .degrees(90), clockwise: false)
        }
        ink(store, "path.arc.counterclockwise_flag", arc.stroke(Color.black, lineWidth: 2))
        let dashed = Path { p in
            p.move(to: CGPoint(x: 10, y: 45))
            p.addLine(to: CGPoint(x: 110, y: 45))
        }
        ink(store, "path.dash.first", dashed.trim(from: 0, to: 0.2).stroke(Color.black, style: StrokeStyle(lineWidth: 4, dash: [10, 5], dashPhase: 3)))
    }
}
