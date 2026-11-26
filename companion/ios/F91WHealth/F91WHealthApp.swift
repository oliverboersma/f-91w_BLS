import SwiftUI

@main
struct F91WHealthApp: App {
    @StateObject private var health: HealthSync
    @StateObject private var watch: WatchLink

    init() {
        let h = HealthSync()
        _health = StateObject(wrappedValue: h)
        _watch = StateObject(wrappedValue: WatchLink(health: h))
    }

    var body: some Scene {
        WindowGroup {
            ContentView().environmentObject(health).environmentObject(watch)
        }
    }
}
