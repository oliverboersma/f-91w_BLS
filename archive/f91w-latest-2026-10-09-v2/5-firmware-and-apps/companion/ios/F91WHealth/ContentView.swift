import SwiftUI

struct ContentView: View {
    @EnvironmentObject var health: HealthSync
    @EnvironmentObject var watch: WatchLink

    var body: some View {
        NavigationStack {
            List {
                Section("Watch") {
                    Text(watch.state)
                    if let s = watch.steps { LabeledContent("Steps today", value: "\(s)") }
                    if let hr = watch.lastHeartRate { LabeledContent("Heart rate", value: hr) }
                    if let b = watch.battery { LabeledContent("Battery", value: "\(b) %") }
                    if let t = watch.lastSync { LabeledContent("Last sync") { Text(t, style: .time) } }
                }
                Section {
                    Button("Measure heart rate") { watch.measureHeartRate() }.disabled(!watch.isReady)
                    Button("Find watch") { watch.findWatch() }.disabled(!watch.isReady)
                }
                Section("Apple Health") {
                    if !health.available {
                        Text("Health isn't available on this device.")
                    } else if health.canWrite {
                        Text("Steps and heart rate are saved to Health on every sync.")
                    } else {
                        Button("Allow Apple Health") { health.requestAccess() }
                    }
                }
                Section {
                    Button("Forget watch", role: .destructive) { watch.forget() }
                }
            }
            .navigationTitle("F-91W")
        }
        .onAppear { if !health.canWrite { health.requestAccess() } }
    }
}
