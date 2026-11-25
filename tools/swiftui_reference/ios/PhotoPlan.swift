import SwiftUI

enum PhotoItem {
    case gradient(CGRect, UInt32, UInt32)
    case rect(CGRect, UInt32)
    case rounded(CGRect, CGFloat, UInt32)
    case ellipse(CGRect, UInt32)
    case stripes(CGRect, CGFloat, UInt32, UInt32)
    case checker(CGRect, CGFloat, UInt32, UInt32)
    case line(CGPoint, CGPoint, CGFloat, UInt32)
    case text(String, CGFloat, Bool, CGPoint, UInt32)

    static func colour(_ hex: UInt32) -> Color {
        Color(.sRGB, red: Double((hex >> 16) & 0xFF) / 255, green: Double((hex >> 8) & 0xFF) / 255, blue: Double(hex & 0xFF) / 255)
    }

    @ViewBuilder
    var view: some View {
        switch self {
        case let .gradient(rect, top, bottom):
            LinearGradient(colors: [Self.colour(top), Self.colour(bottom)], startPoint: .top, endPoint: .bottom)
                .frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .rect(rect, hex):
            Rectangle().fill(Self.colour(hex)).frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .rounded(rect, radius, hex):
            RoundedRectangle(cornerRadius: radius, style: .continuous).fill(Self.colour(hex))
                .frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .ellipse(rect, hex):
            Ellipse().fill(Self.colour(hex)).frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .stripes(rect, period, first, second):
            Canvas { context, size in
                var x: CGFloat = 0
                var index = 0
                while x < size.width {
                    context.fill(Path(CGRect(x: x, y: 0, width: period / 2, height: size.height)), with: .color(Self.colour(index % 2 == 0 ? first : second)))
                    x += period / 2
                    index += 1
                }
            }
            .frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .checker(rect, cell, first, second):
            Canvas { context, size in
                let columns = Int((size.width / cell).rounded(.up))
                let rows = Int((size.height / cell).rounded(.up))
                for row in 0..<rows {
                    for column in 0..<columns {
                        let hex = (row + column) % 2 == 0 ? first : second
                        context.fill(Path(CGRect(x: CGFloat(column) * cell, y: CGFloat(row) * cell, width: cell, height: cell)), with: .color(Self.colour(hex)))
                    }
                }
            }
            .frame(width: rect.width, height: rect.height).offset(x: rect.minX, y: rect.minY)
        case let .line(from, to, width, hex):
            Path { path in
                path.move(to: from)
                path.addLine(to: to)
            }
            .stroke(Self.colour(hex), style: StrokeStyle(lineWidth: width, lineCap: .round))
            .frame(width: GlassGeometry.canvas.width, height: GlassGeometry.canvas.height, alignment: .topLeading)
        case let .text(string, size, bold, origin, hex):
            Text(string).font(.system(size: size, weight: bold ? .bold : .regular)).foregroundStyle(Self.colour(hex))
                .fixedSize().offset(x: origin.x, y: origin.y)
        }
    }
}

enum PhotoPlan {
    static let items: [PhotoItem] = [
        .rect(CGRect(x: 0, y: 0, width: 402, height: 520), 0x4F9FE0),
        .gradient(CGRect(x: 0, y: 0, width: 402, height: 520), 0x4F9FE0, 0xD7ECF8),
        .ellipse(CGRect(x: 264, y: 64, width: 92, height: 92), 0xFFD24A),
        .ellipse(CGRect(x: -140, y: 360, width: 520, height: 260), 0x79C27D),
        .ellipse(CGRect(x: 90, y: 420, width: 560, height: 280), 0x3F8F50),
        .rect(CGRect(x: 0, y: 560, width: 402, height: 314), 0x2E6A3B),
        .rounded(CGRect(x: 36, y: 600, width: 330, height: 56), 28, 0x4A90D9),
        .stripes(CGRect(x: 0, y: 680, width: 402, height: 48), 16, 0xF4F4F4, 0x1C1C1E),
        .checker(CGRect(x: 214, y: 290, width: 168, height: 96), 12, 0xFFFFFF, 0x202020),
        .line(CGPoint(x: 20, y: 340), CGPoint(x: 200, y: 430), 5, 0xD93636),
        .line(CGPoint(x: 20, y: 430), CGPoint(x: 200, y: 340), 5, 0xD93636),
        .line(CGPoint(x: 110, y: 330), CGPoint(x: 110, y: 440), 5, 0xD93636),
        .rounded(CGRect(x: 30, y: 760, width: 96, height: 72), 14, 0xE5484D),
        .ellipse(CGRect(x: 170, y: 762, width: 70, height: 70), 0xF5C518),
        .rounded(CGRect(x: 262, y: 772, width: 116, height: 50), 25, 0x8E5CF7),
        .text("abcdefghijklmnopqrstuvwxyz", 15, false, CGPoint(x: 24, y: 118), 0x1C1C1E),
        .text("Liquid Glass", 34, true, CGPoint(x: 24, y: 196), 0xFFFFFF),
        .text("The quick brown fox jumps", 20, true, CGPoint(x: 24, y: 246), 0x102030),
        .text("over the lazy dog 0123456789", 17, false, CGPoint(x: 24, y: 274), 0x102030),
        .text("ARISA GLASS FIDELITY", 15, true, CGPoint(x: 64, y: 618), 0xFFFFFF),
        .text("Sharp text behind clear glass stays readable.", 13, false, CGPoint(x: 24, y: 734), 0xF4F4F4),
    ]
}
