// StreamClient.swift
import Network
import MetalKit

class StreamClient: NSObject {
    private var connection: NWConnection?
    private var videoDecoder: VideoDecoder?
    private var metalRenderer: MetalRenderer?
    private let metalView: MTKView

    init(metalView: MTKView) {
        self.metalView = metalView
        super.init()
        setupVideoPipeline()
    }

    private func setupVideoPipeline() {
        guard let device = metalView.device else { return }
        metalRenderer = MetalRenderer()
        metalRenderer?.setup(device: device)
        metalView.delegate = metalRenderer

        videoDecoder = VideoDecoder()
        videoDecoder?.onFrame = { [weak self] pixelBuffer in
            self?.metalRenderer?.currentPixelBuffer = pixelBuffer
            DispatchQueue.main.async {
                self?.metalView.setNeedsDisplay()
            }
        }
    }

    private var receiveBuffer = Data()
    private var reconnectAttempts = 0
    private let maxReconnectAttempts = 5
    private let initialReconnectDelay = 1.0
    private let sendQueue = DispatchQueue(label: "com.carside.streamclient.send")

    func connect(host: String, port: UInt16 = 7878) {
        // BUG FIX #20: Cancel existing connection before creating new one
        if let oldConnection = connection {
            oldConnection.cancel()
        }
        
        let endpoint = NWEndpoint.hostPort(
            host: NWEndpoint.Host(host),
            port: NWEndpoint.Port(rawValue: port)!)
        let params = NWParameters.tcp
        if let tcpOptions = params.defaultProtocolStack.transportProtocol as? NWProtocolTCP.Options {
            tcpOptions.noDelay = true
        }

        connection = NWConnection(to: endpoint, using: params)
        connection?.stateUpdateHandler = { [weak self] state in
            switch state {
            case .ready:
                self?.reconnectAttempts = 0
                self?.receiveBuffer.removeAll()
                self?.startReceiving()
            case .failed, .cancelled:
                self?.handleConnectionFailure(host: host, port: port)
            default:
                break
            }
        }
        connection?.start(queue: .global(qos: .userInteractive))
    }

    private func handleConnectionFailure(host: String, port: UInt16) {
        guard reconnectAttempts < maxReconnectAttempts else {
            print("[StreamClient] Max reconnection attempts reached")
            return
        }
        reconnectAttempts += 1
        let delay = initialReconnectDelay * pow(2.0, Double(reconnectAttempts - 1))
        DispatchQueue.global(qos: .userInteractive).asyncAfter(deadline: .now() + delay) { [weak self] in
            print("[StreamClient] Reconnecting (attempt \(self?.reconnectAttempts ?? 0))...")
            self?.connect(host: host, port: port)
        }
    }

    private func startReceiving() {
        connection?.receive(minimumIncompleteLength: 4,
                            maximumLength: 4) { [weak self] data, _, _, _ in
            guard let self, let data else { return }
            self.receiveBuffer.append(data)
            if self.receiveBuffer.count >= 4 {
                let len = self.receiveBuffer.prefix(4).withUnsafeBytes { $0.load(as: UInt32.self).bigEndian }
                self.receiveBuffer.removeFirst(4)
                self.receiveFrameData(remainingLength: Int(len))
            } else {
                self.startReceiving()
            }
        }
    }

    private func receiveFrameData(remainingLength: Int) {
        connection?.receive(minimumIncompleteLength: 1,
                            maximumLength: remainingLength) { [weak self] data, _, _, _ in
            guard let self, let data else { return }
            self.receiveBuffer.append(data)
            let dataNeeded = remainingLength - data.count
            if self.receiveBuffer.count < remainingLength {
                self.receiveFrameData(remainingLength: dataNeeded)
            } else if self.receiveBuffer.count == remainingLength {
                let nalFrame = Data(self.receiveBuffer)
                self.receiveBuffer.removeAll()
                self.videoDecoder?.decodeNAL(nalFrame)
                self.startReceiving()
            }
        }
    }

    func sendInput(_ event: InputEvent) {
        guard let data = try? JSONEncoder().encode(event) else { return }
        var len = UInt32(data.count).bigEndian
        let lenData = Data(bytes: &len, count: 4)
        // BUG FIX #21: Dispatch sendInput to the proper queue for thread safety
        sendQueue.async { [weak self] in
            self?.connection?.send(content: lenData + data, completion: .contentProcessed { _ in })
        }
    }
}