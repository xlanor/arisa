import SwiftUI
import UIKit

enum Backdrop: String {
    case photo
    case ramp_x
    case ramp_y
    case grey
    case black
    case white
    case stripes
    case stripes_wide
}

enum Variant: String {
    case none
    case regular
    case clear
    case tinted
    case tinted_soft
    case interactive
}

enum Layout: String {
    case shapes
    case sheet
    case system_sheet
}

struct Configuration {
    var backdrop = Backdrop.photo
    var variant = Variant.regular
    var layout = Layout.shapes
    var dark = false

    static var current: Configuration {
        var configuration = Configuration()
        let defaults = UserDefaults.standard
        if let value = defaults.string(forKey: "backdrop"), let parsed = Backdrop(rawValue: value) { configuration.backdrop = parsed }
        if let value = defaults.string(forKey: "variant"), let parsed = Variant(rawValue: value) { configuration.variant = parsed }
        if let value = defaults.string(forKey: "layout"), let parsed = Layout(rawValue: value) { configuration.layout = parsed }
        configuration.dark = defaults.string(forKey: "scheme") == "dark"
        return configuration
    }
}

@main
struct GlassReferenceApp: App {
    let configuration = Configuration.current

    var body: some Scene {
        WindowGroup {
            GlassScene(configuration: configuration)
                .preferredColorScheme(configuration.dark ? .dark : .light)
                .statusBarHidden(true)
                .persistentSystemOverlays(.hidden)
        }
    }
}

struct ArcShape: Shape {
    func path(in rect: CGRect) -> Path { Path(GlassGeometry.arcPath()) }
}

struct GlassScene: View {
    let configuration: Configuration
    @State private var presented = false

    var body: some View {
        ZStack(alignment: .topLeading) {
            BackdropView(backdrop: configuration.backdrop)
            switch configuration.layout {
            case .shapes:
                if configuration.variant != .none {
                    ForEach(GlassGeometry.cells, id: \.kind) { cell in
                        GlassCellView(cell: cell, variant: configuration.variant)
                    }
                }
            case .sheet:
                SheetPanel(variant: configuration.variant)
            case .system_sheet:
                Color.clear
            }
        }
        .frame(width: GlassGeometry.canvas.width, height: GlassGeometry.canvas.height, alignment: .topLeading)
        .ignoresSafeArea()
        .sheet(isPresented: $presented) {
            SheetContent().presentationDetents([.height(520)])
        }
        .onAppear { presented = configuration.layout == .system_sheet }
    }
}

extension View {
    @ViewBuilder
    func glass<S: Shape>(_ variant: Variant, in shape: S) -> some View {
        switch variant {
        case .none:
            self
        case .regular:
            glassEffect(.regular, in: shape)
        case .clear:
            glassEffect(.clear, in: shape)
        case .tinted:
            glassEffect(.regular.tint(.blue), in: shape)
        case .tinted_soft:
            glassEffect(.regular.tint(.blue.opacity(GlassGeometry.softTint)), in: shape)
        case .interactive:
            glassEffect(.regular.interactive(), in: shape)
        }
    }
}

struct GlassCellView: View {
    let cell: GlassCell
    let variant: Variant

    var body: some View {
        Group {
            switch cell.kind {
            case .capsule, .pill:
                Color.clear.frame(width: cell.rect.width, height: cell.rect.height).glass(variant, in: Capsule())
            case .circle:
                Color.clear.frame(width: cell.rect.width, height: cell.rect.height).glass(variant, in: Circle())
            case .rounded, .panel:
                Color.clear.frame(width: cell.rect.width, height: cell.rect.height).glass(variant, in: RoundedRectangle(cornerRadius: cell.radius, style: .continuous))
            case .arc:
                Color.clear.frame(width: GlassGeometry.canvas.width, height: GlassGeometry.canvas.height).glass(variant, in: ArcShape())
            }
        }
        .offset(x: cell.kind == .arc ? 0 : cell.rect.minX, y: cell.kind == .arc ? 0 : cell.rect.minY)
    }
}

struct SheetContent: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Sheet").font(.title2.bold())
            Text("Content slides up over a dimmed page.").font(.body)
            HStack(spacing: 10) {
                Button("Done") {}.buttonStyle(.glass)
                Button("Present alert") {}.buttonStyle(.glass)
                Button("Block B") {}.buttonStyle(.glass)
            }
            Spacer()
        }
        .padding(24)
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct SheetPanel: View {
    let variant: Variant

    var body: some View {
        SheetContent()
            .frame(width: 386, height: 520, alignment: .topLeading)
            .glass(variant, in: RoundedRectangle(cornerRadius: 34, style: .continuous))
            .offset(x: 8, y: 346)
    }
}

struct BackdropView: View {
    let backdrop: Backdrop

    var body: some View {
        switch backdrop {
        case .photo:
            PhotoScene()
        default:
            Image(uiImage: PatternImage.make(backdrop))
                .resizable()
                .interpolation(.none)
                .frame(width: GlassGeometry.canvas.width, height: GlassGeometry.canvas.height)
        }
    }
}

enum PatternImage {
    static let scale = 3

    static func make(_ backdrop: Backdrop) -> UIImage {
        let width = Int(GlassGeometry.canvas.width) * scale
        let height = Int(GlassGeometry.canvas.height) * scale
        var bytes = [UInt8](repeating: 255, count: width * height * 4)
        let grey = UInt8(GlassGeometry.grey * 255)
        for y in 0..<height {
            for x in 0..<width {
                let point = CGPoint(x: (CGFloat(x) + 0.5) / CGFloat(scale), y: (CGFloat(y) + 0.5) / CGFloat(scale))
                let at = (y * width + x) * 4
                var colour: (UInt8, UInt8, UInt8) = (grey, grey, grey)
                switch backdrop {
                case .ramp_x, .ramp_y:
                    for cell in GlassGeometry.cells where cell.ramp.contains(point) {
                        let span = backdrop == .ramp_x ? (point.x - cell.ramp.minX) / cell.ramp.width : (point.y - cell.ramp.minY) / cell.ramp.height
                        let level = UInt8(max(0, min(255, (span * 255).rounded())))
                        colour = (level, level, level)
                    }
                case .black:
                    colour = (0, 0, 0)
                case .white:
                    colour = (255, 255, 255)
                case .stripes, .stripes_wide:
                    let period = backdrop == .stripes ? GlassGeometry.stripePeriod : GlassGeometry.wideStripePeriod
                    let phase = point.x.truncatingRemainder(dividingBy: period)
                    let level: UInt8 = phase < period / 2 ? 0 : 255
                    colour = (level, level, level)
                default:
                    break
                }
                bytes[at] = colour.0
                bytes[at + 1] = colour.1
                bytes[at + 2] = colour.2
            }
        }
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let provider = CGDataProvider(data: Data(bytes) as CFData)!
        let image = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width * 4, space: space, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue), provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent)!
        return UIImage(cgImage: image, scale: CGFloat(scale), orientation: .up)
    }
}

struct PhotoScene: View {
    var body: some View {
        ZStack(alignment: .topLeading) {
            ForEach(Array(PhotoPlan.items.enumerated()), id: \.offset) { _, item in
                item.view
            }
        }
        .frame(width: GlassGeometry.canvas.width, height: GlassGeometry.canvas.height, alignment: .topLeading)
        .clipped()
    }
}
