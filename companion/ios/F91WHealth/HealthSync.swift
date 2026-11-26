import Foundation
import HealthKit

/// Writes watch data to Apple Health. Every sample carries a sync identifier, so re-sending the
/// same day or reading replaces the earlier sample instead of adding a duplicate.
final class HealthSync: ObservableObject {
    @Published private(set) var canWrite = false

    private let store = HKHealthStore()
    private let stepType = HKQuantityType.quantityType(forIdentifier: .stepCount)!
    private let hrType = HKQuantityType.quantityType(forIdentifier: .heartRate)!
    private let device = HKDevice(name: "F-91W", manufacturer: "Casio (modded)", model: "F-91W BLE",
                                  hardwareVersion: "rev A", firmwareVersion: nil, softwareVersion: nil,
                                  localIdentifier: nil, udiDeviceIdentifier: nil)
    private static let dayFormat: DateFormatter = {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.calendar = Calendar(identifier: .gregorian)
        f.dateFormat = "yyyy-MM-dd"
        return f
    }()

    var available: Bool { HKHealthStore.isHealthDataAvailable() }

    init() { refresh() }

    func requestAccess() {
        guard available else { return }
        store.requestAuthorization(toShare: [stepType, hrType], read: []) { _, _ in
            DispatchQueue.main.async { self.refresh() }
        }
    }

    private func refresh() {
        canWrite = available && store.authorizationStatus(for: stepType) == .sharingAuthorized
    }

    /// One sample per local day. The watch's count only grows during a day, so the count doubles
    /// as the sync version: a later, larger count replaces the earlier sample.
    func saveSteps(_ count: UInt32, daysAgo: Int) {
        guard count > 0 else { return }
        let cal = Calendar.current
        guard let start = cal.date(byAdding: .day, value: -daysAgo, to: cal.startOfDay(for: Date())),
              let next = cal.date(byAdding: .day, value: 1, to: start) else { return }
        let sample = HKQuantitySample(
            type: stepType, quantity: HKQuantity(unit: .count(), doubleValue: Double(count)),
            start: start, end: min(next, Date()), device: device,
            metadata: [HKMetadataKeySyncIdentifier: "f91w-steps-" + Self.dayFormat.string(from: start),
                       HKMetadataKeySyncVersion: Int(count)])
        save(sample)
    }

    func saveHeartRate(_ bpm: Int, at date: Date) {
        let unit = HKUnit.count().unitDivided(by: .minute())
        let sample = HKQuantitySample(
            type: hrType, quantity: HKQuantity(unit: unit, doubleValue: Double(bpm)),
            start: date, end: date, device: device,
            metadata: [HKMetadataKeySyncIdentifier: "f91w-hr-\(Int(date.timeIntervalSince1970))",
                       HKMetadataKeySyncVersion: 1,
                       HKMetadataKeyHeartRateSensorLocation: HKHeartRateSensorLocation.wrist.rawValue])
        save(sample)
    }

    private func save(_ sample: HKSample) {
        guard available else { return }
        store.save(sample) { _, error in
            if let error { print("HealthKit save failed:", error.localizedDescription) }
        }
    }
}
