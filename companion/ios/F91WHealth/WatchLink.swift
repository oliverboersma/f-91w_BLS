import Foundation
import CoreBluetooth

/// GATT layout from firmware/docs/BLE.md.
enum F91W {
    static func uuid(_ id: UInt16) -> CBUUID {
        CBUUID(string: String(format: "6F91%04X-C2A6-4B3E-8F1D-0A5E3B7C9D10", id))
    }
    static let service = uuid(1)
    static let time = uuid(2), steps = uuid(3), history = uuid(4), heartRate = uuid(5), control = uuid(6)
    static let hrService = CBUUID(string: "180D")
    static let batteryService = CBUUID(string: "180F"), batteryLevel = CBUUID(string: "2A19")
}

extension Data {
    func u32(at o: Int) -> UInt32? {
        guard count >= o + 4 else { return nil }
        return (0..<4).reduce(UInt32(0)) { $0 | UInt32(self[startIndex + o + $1]) << (8 * $1) }
    }
    mutating func append<T: FixedWidthInteger>(le v: T) {
        Swift.withUnsafeBytes(of: v.littleEndian) { append(contentsOf: $0) }
    }
}

/// Finds the watch, keeps a pending connection to it, and pushes everything it reads to Health.
/// iOS completes a pending connection whenever the watch advertises (hold LIGHT 1.5 s), even with
/// the app in the background.
final class WatchLink: NSObject, ObservableObject {
    @Published var state = "Starting Bluetooth…"
    @Published var isReady = false
    @Published var steps: UInt32?
    @Published var battery: Int?
    @Published var lastHeartRate: String?
    @Published var lastSync: Date?

    private var central: CBCentralManager!
    private var watch: CBPeripheral?
    private var chars: [CBUUID: CBCharacteristic] = [:]
    private let health: HealthSync
    private let savedKey = "watchID"

    init(health: HealthSync) {
        self.health = health
        super.init()
        central = CBCentralManager(delegate: self, queue: nil,
                                   options: [CBCentralManagerOptionRestoreIdentifierKey: "f91w-central"])
    }

    func measureHeartRate() { send([0x01]) }
    func findWatch() { send([0x02]) }

    func forget() {
        let w = watch
        watch = nil
        chars = [:]
        isReady = false
        UserDefaults.standard.removeObject(forKey: savedKey)
        if let w { central.cancelPeripheralConnection(w) }
        scan()
        state = "Forgotten. Also remove F-91W under Settings > Bluetooth."
    }

    private func send(_ bytes: [UInt8]) {
        guard let w = watch, let c = chars[F91W.control] else { return }
        w.writeValue(Data(bytes), for: c, type: .withResponse)
    }

    private func use(_ p: CBPeripheral) {
        watch = p
        p.delegate = self
    }

    private func connectOrScan() {
        guard central.state == .poweredOn else { return }
        if watch == nil, let s = UserDefaults.standard.string(forKey: savedKey), let id = UUID(uuidString: s),
           let p = central.retrievePeripherals(withIdentifiers: [id]).first {
            use(p)
        }
        guard let w = watch else { return scan() }
        if w.state == .connected {
            w.discoverServices([F91W.service, F91W.batteryService])
        } else {
            central.connect(w)
            state = "Waiting for the watch. Hold LIGHT for 1.5 s to sync."
        }
    }

    private func scan() {
        guard central.state == .poweredOn else { return }
        central.scanForPeripherals(withServices: [F91W.hrService])
        state = "Searching. Hold LIGHT on the watch for 1.5 s."
    }

    private func handleHeartRate(_ v: Data) {
        guard v.count >= 8 else { return }
        let status = v[v.startIndex], bpm = v[v.startIndex + 1], conf = v[v.startIndex + 2]
        let t = v.u32(at: 4) ?? 0
        guard status == 0 else { lastHeartRate = "Last check failed (code \(status))"; return }
        guard bpm > 0, t > 1_600_000_000 else { return }
        lastHeartRate = "\(bpm) bpm, \(conf) % confidence"
        health.saveHeartRate(Int(bpm), at: Date(timeIntervalSince1970: TimeInterval(t)))
    }
}

extension WatchLink: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        switch c.state {
        case .poweredOn: connectOrScan()
        case .unauthorized: state = "Bluetooth permission is off for this app (Settings > F-91W)."
        default: state = "Bluetooth is off."
        }
    }

    func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let p = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.first { use(p) }
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral,
                        advertisementData ad: [String: Any], rssi: NSNumber) {
        guard (ad[CBAdvertisementDataLocalNameKey] as? String ?? p.name) == "F-91W" else { return }
        c.stopScan()
        use(p)
        c.connect(p)
        state = "Connecting…"
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
        UserDefaults.standard.set(p.identifier.uuidString, forKey: savedKey)
        state = "Connected. Enter the passkey shown on the watch if iOS asks."
        p.discoverServices([F91W.service, F91W.batteryService])
    }

    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        guard p == watch else { return }
        chars = [:]
        isReady = false
        state = "Disconnected. Hold LIGHT for 1.5 s to sync again."
        c.connect(p)   // pending: completes the next time the watch advertises
    }

    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        if p == watch { c.connect(p) }
    }
}

extension WatchLink: CBPeripheralDelegate {
    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        for s in p.services ?? [] { p.discoverCharacteristics(nil, for: s) }
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        for c in s.characteristics ?? [] { chars[c.uuid] = c }
        if s.uuid == F91W.batteryService, let b = chars[F91W.batteryLevel] {
            p.readValue(for: b)
            p.setNotifyValue(true, for: b)
        }
        if s.uuid == F91W.service, let t = chars[F91W.time] {
            // Setting the clock is the first access that needs a bond, so iOS asks for the passkey here.
            var d = Data()
            d.append(le: UInt32(Date().timeIntervalSince1970))
            d.append(le: Int16(TimeZone.current.secondsFromGMT() / 60))
            p.writeValue(d, for: t, type: .withResponse)
        }
    }

    func peripheral(_ p: CBPeripheral, didWriteValueFor c: CBCharacteristic, error: Error?) {
        guard c.uuid == F91W.time else { return }
        if let error { state = "Pairing failed: \(error.localizedDescription)"; return }
        for u in [F91W.steps, F91W.heartRate] { if let ch = chars[u] { p.setNotifyValue(true, for: ch) } }
        for u in [F91W.history, F91W.steps, F91W.heartRate] { if let ch = chars[u] { p.readValue(for: ch) } }
        isReady = true
        state = "Connected"
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
        guard error == nil, let v = c.value else { return }
        switch c.uuid {
        case F91W.batteryLevel:
            battery = v.first.map(Int.init)
        case F91W.steps:
            guard let n = v.u32(at: 0) else { return }
            steps = n
            health.saveSteps(n, daysAgo: 0)
        case F91W.history:
            for i in 0..<7 { if let n = v.u32(at: i * 4) { health.saveSteps(n, daysAgo: i + 1) } }
        case F91W.heartRate:
            handleHeartRate(v)
        default:
            return
        }
        lastSync = Date()
    }
}
