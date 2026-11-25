import AppKit
import SwiftUI

enum Pointer {
    static func run(_ store: Store) {
        var cursors: [(String, NSCursor)] = [
            ("arrow", .arrow),
            ("i_beam", .iBeam),
            ("i_beam_vertical", .iBeamCursorForVerticalLayout),
            ("crosshair", .crosshair),
            ("open_hand", .openHand),
            ("closed_hand", .closedHand),
            ("pointing_hand", .pointingHand),
            ("resize_left_right", .resizeLeftRight),
            ("resize_up_down", .resizeUpDown),
        ]
        if #available(macOS 15.0, *) {
            cursors += [
                ("zoom_in", .zoomIn),
                ("zoom_out", .zoomOut),
                ("column_resize", .columnResize),
                ("row_resize", .rowResize),
                ("frame_resize_bottom_trailing", .frameResize(position: .bottomRight, directions: .all)),
                ("frame_resize_top", .frameResize(position: .top, directions: .all)),
            ]
        }
        for (name, cursor) in cursors {
            store.put("pointer.\(name).width", nil, Double(cursor.image.size.width))
            store.put("pointer.\(name).height", nil, Double(cursor.image.size.height))
            store.put("pointer.\(name).hot_x", nil, Double(cursor.hotSpot.x))
            store.put("pointer.\(name).hot_y", nil, Double(cursor.hotSpot.y))
        }
    }
}
