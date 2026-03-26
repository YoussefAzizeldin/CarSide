// BonjourDiscovery.swift — find the Windows host on Wi-Fi
import Network

class BonjourBrowser: NSObject, NetServiceBrowserDelegate, NetServiceDelegate {
    private let browser = NetServiceBrowser()
    private var resolvingServices: Set<NetService> = []
    var onFound: ((String, Int) -> Void)?

    func start() {
        browser.delegate = self
        // BUG FIX #29: Explicitly schedule on main RunLoop for reliable callbacks
        browser.schedule(in: .main, forMode: .default)
        browser.searchForServices(ofType: "_winextend._tcp.", inDomain: "local.")
    }

    func netServiceBrowser(_ browser: NetServiceBrowser,
                           didFind service: NetService, moreComing: Bool) {
        resolvingServices.insert(service)
        service.delegate = self
        service.resolve(withTimeout: 5)
    }

    func netServiceDidResolveAddress(_ sender: NetService) {
        defer {
            resolvingServices.remove(sender)
        }
        if let hostName = sender.hostName {
            onFound?(hostName, sender.port)
        }
    }

    func netService(_ sender: NetService, didNotResolve errorDict: [String : NSNumber]) {
        resolvingServices.remove(sender)
    }

    // BUG FIX #28: Handle discovery failures
    func netServiceBrowser(_ browser: NetServiceBrowser,
                           didNotFind serviceType: String,
                           in errorDict: [String : NSNumber]) {
        if let error = errorDict[NetServiceBrowser.errorCode] {
            let errorCode = NetServiceBrowser.ErrorCode(rawValue: error.intValue) ?? .unknown
            print("[BonjourBrowser] Service discovery failed for \(serviceType): \(errorCode)")
            onFound?("Error: service not found", -1)
        }
    }

    func netServiceBrowserDidStopSearch(_ browser: NetServiceBrowser) {
        print("[BonjourBrowser] Search stopped")
    }
}