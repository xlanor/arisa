import CoreGraphics
import Foundation

enum GlassShapeKind: String, CaseIterable {
    case capsule
    case circle
    case pill
    case rounded
    case arc
    case panel
}

struct GlassCell {
    let kind: GlassShapeKind
    let rect: CGRect
    let radius: CGFloat

    var ramp: CGRect { rect.insetBy(dx: -GlassGeometry.rampMargin, dy: -GlassGeometry.rampMargin) }
}

enum GlassGeometry {
    static let canvas = CGSize(width: 402, height: 874)
    static let rampMargin: CGFloat = 16
    static let arcCentre = CGPoint(x: 201, y: 534)
    static let arcRadius: CGFloat = 200
    static let arcWidth: CGFloat = 44
    static let arcStart: CGFloat = 215
    static let arcEnd: CGFloat = 325
    static let stripePeriod: CGFloat = 4
    static let wideStripePeriod: CGFloat = 16
    static let grey: CGFloat = 0.5
    static let softTint: Double = 0.3

    static let cells: [GlassCell] = [
        GlassCell(kind: .capsule, rect: CGRect(x: 24, y: 48, width: 200, height: 56), radius: 28),
        GlassCell(kind: .circle, rect: CGRect(x: 266, y: 36, width: 88, height: 88), radius: 44),
        GlassCell(kind: .pill, rect: CGRect(x: 250, y: 160, width: 120, height: 28), radius: 14),
        GlassCell(kind: .rounded, rect: CGRect(x: 24, y: 140, width: 200, height: 140), radius: 28),
        GlassCell(kind: .arc, rect: CGRect(x: 15, y: 312, width: 372, height: 130), radius: 22),
        GlassCell(kind: .panel, rect: CGRect(x: 24, y: 500, width: 354, height: 340), radius: 34),
    ]

    static func cell(_ kind: GlassShapeKind) -> GlassCell { cells.first { $0.kind == kind }! }

    static func arcPoint(_ degrees: CGFloat, radius: CGFloat) -> CGPoint {
        let angle = degrees * .pi / 180
        return CGPoint(x: arcCentre.x + radius * cos(angle), y: arcCentre.y + radius * sin(angle))
    }

    static var arcMidline: (outer: CGPoint, inner: CGPoint) {
        let middle = (arcStart + arcEnd) / 2
        return (arcPoint(middle, radius: arcRadius + arcWidth / 2), arcPoint(middle, radius: arcRadius - arcWidth / 2))
    }

    static func arcPath() -> CGPath {
        let path = CGMutablePath()
        path.addArc(center: arcCentre, radius: arcRadius, startAngle: arcStart * .pi / 180, endAngle: arcEnd * .pi / 180, clockwise: false)
        return path.copy(strokingWithWidth: arcWidth, lineCap: .round, lineJoin: .round, miterLimit: 10)
    }
}
