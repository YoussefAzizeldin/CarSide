import UIKit
import MetalKit

class ViewController: UIViewController {

    private var streamClient: StreamClient!
    private var metalView: MTKView!
    private var inputView: InputCaptureView!
    private var bonjourBrowser: BonjourBrowser!

    override func viewDidLoad() {
        super.viewDidLoad()

        // Setup Metal view for video rendering
        metalView = MTKView(frame: view.bounds)
        // BUG FIX #30: Check for nil Metal device support
        guard let metalDevice = MTLCreateSystemDefaultDevice() else {
            showError("Metal is not supported on this device")
            return
        }
        metalView.device = metalDevice
        metalView.clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        view.addSubview(metalView)

        // Setup input capture
        inputView = InputCaptureView(frame: view.bounds)
        view.addSubview(inputView)

        // Initialize components
        streamClient = StreamClient(metalView: metalView)
        bonjourBrowser = BonjourBrowser()
        // BUG FIX #31: Dispatch Bonjour callback to main thread for thread safety
        bonjourBrowser.onFound = { [weak self] host, port in
            DispatchQueue.main.async {
                self?.streamClient.connect(host: host, port: UInt16(port))
            }
        }
        bonjourBrowser.start()

        // Setup input handling
        inputView.onInput = { [weak self] event in
            self?.streamClient.sendInput(event)
        }
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        metalView.frame = view.bounds
        inputView.frame = view.bounds
    }

    private func showError(_ message: String) {
        let alert = UIAlertController(title: "Error", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
}