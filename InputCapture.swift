// InputCapture.swift
import UIKit

// MARK: - Input Event Model

struct InputEvent: Codable {
    enum EventType: String, Codable {
        case touch       // Regular finger touch → mouse
        case pencil      // Apple Pencil → stylus/digitizer
        case drawing     // DrawingPad mode: pencil strokes only, fingers ignored
    }

    var type: EventType
    var x: Float          // Normalised 0–1
    var y: Float          // Normalised 0–1
    var phase: Int        // UITouch.Phase rawValue (0=began,1=moved,2=stationary,3=ended,4=cancelled)
    var pressure: Float   // 0–1 (Pencil force)
    var altitude: Float   // Pencil altitude angle in radians
    var azimuth: Float    // Pencil azimuth angle in radians
    var isDrawingPad: Bool // True when sent from DrawingPad mode
}

// MARK: - Mode

enum InputMode {
    case standard   // Touch + Pencil both forwarded as-is
    case drawingPad // Only Pencil strokes forwarded; touch gestures are local pan/zoom
}

// MARK: - InputCaptureView

class InputCaptureView: UIView {

    // Called for every event that should be forwarded to the Windows host
    var onInput: ((InputEvent) -> Void)?

    // Switch between standard and drawing-pad mode
    var inputMode: InputMode = .standard {
        didSet { configurePanGesture() }
    }

    // DrawingPad local canvas (visible only in drawingPad mode)
    private let canvasView = DrawingCanvas()

    // Pan/pinch for DrawingPad navigation
    private var panGesture: UIPanGestureRecognizer?
    private var pinchGesture: UIPinchGestureRecognizer?
    private var canvasOffset: CGPoint = .zero
    private var canvasScale: CGFloat = 1.0

    // ── Toolbar ──────────────────────────────────────────────────────────────
    private let toolbar = UIStackView()
    private let modeToggleButton = UIButton(type: .system)
    private let clearButton = UIButton(type: .system)
    private let colorWell = UIButton(type: .system)  // simple colour picker
    private var strokeColor: UIColor = .black

    // MARK: Init

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        isUserInteractionEnabled = true
        buildToolbar()
        buildCanvas()
        configurePanGesture()
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) not implemented") }

    // MARK: Toolbar

    private func buildToolbar() {
        toolbar.axis = .horizontal
        toolbar.spacing = 12
        toolbar.alignment = .center
        toolbar.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.85)
        toolbar.layer.cornerRadius = 12
        toolbar.layoutMargins = UIEdgeInsets(top: 6, left: 12, bottom: 6, right: 12)
        toolbar.isLayoutMarginsRelativeArrangement = true
        toolbar.translatesAutoresizingMaskIntoConstraints = false

        modeToggleButton.setTitle("🖱 Standard", for: .normal)
        modeToggleButton.addTarget(self, action: #selector(toggleMode), for: .touchUpInside)

        clearButton.setTitle("🗑 Clear", for: .normal)
        clearButton.addTarget(self, action: #selector(clearCanvas), for: .touchUpInside)
        clearButton.isHidden = true

        colorWell.setTitle("🎨", for: .normal)
        colorWell.addTarget(self, action: #selector(pickColor), for: .touchUpInside)
        colorWell.isHidden = true

        [modeToggleButton, clearButton, colorWell].forEach { toolbar.addArrangedSubview($0) }
        addSubview(toolbar)

        NSLayoutConstraint.activate([
            toolbar.topAnchor.constraint(equalTo: safeAreaLayoutGuide.topAnchor, constant: 8),
            toolbar.centerXAnchor.constraint(equalTo: centerXAnchor)
        ])
    }

    @objc private func toggleMode() {
        inputMode = (inputMode == .standard) ? .drawingPad : .standard
        let isDrawing = inputMode == .drawingPad
        modeToggleButton.setTitle(isDrawing ? "✏️ DrawingPad" : "🖱 Standard", for: .normal)
        clearButton.isHidden = !isDrawing
        colorWell.isHidden   = !isDrawing
        canvasView.isHidden  = !isDrawing
    }

    @objc private func clearCanvas() {
        canvasView.clear()
    }

    @objc private func pickColor() {
        // Cycle through a small palette for simplicity
        let palette: [UIColor] = [.black, .systemRed, .systemBlue, .systemGreen, .systemOrange]
        if let idx = palette.firstIndex(where: { $0 == strokeColor }) {
            strokeColor = palette[(idx + 1) % palette.count]
        } else {
            strokeColor = palette[0]
        }
        canvasView.strokeColor = strokeColor
        colorWell.setTitle("🎨", for: .normal)
        colorWell.tintColor = strokeColor
    }

    // MARK: Canvas

    private func buildCanvas() {
        canvasView.frame = bounds
        canvasView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        canvasView.isHidden = true          // Hidden until DrawingPad mode activated
        canvasView.isUserInteractionEnabled = false  // We forward events manually
        insertSubview(canvasView, at: 0)
    }

    // MARK: Pan / Pinch (DrawingPad navigation with fingers)

    private func configurePanGesture() {
        panGesture.map { removeGestureRecognizer($0) }
        pinchGesture.map { removeGestureRecognizer($0) }

        guard inputMode == .drawingPad else { return }

        let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        pan.minimumNumberOfTouches = 2   // Two-finger pan to scroll the remote desktop
        addGestureRecognizer(pan)
        panGesture = pan

        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
        addGestureRecognizer(pinch)
        pinchGesture = pinch
    }

    @objc private func handlePan(_ gr: UIPanGestureRecognizer) {
        let delta = gr.translation(in: self)
        gr.setTranslation(.zero, in: self)
        // Send as a scroll/pan event to Windows host
        let event = InputEvent(
            type: .touch,
            x: Float(delta.x / bounds.width),
            y: Float(delta.y / bounds.height),
            phase: Int(gr.state.rawValue),
            pressure: 0, altitude: 0, azimuth: 0,
            isDrawingPad: true)
        onInput?(event)
    }

    @objc private func handlePinch(_ gr: UIPinchGestureRecognizer) {
        // You can forward pinch scale as a scroll/zoom hint if the Windows app supports it
        canvasScale *= gr.scale
        gr.scale = 1
    }

    // MARK: UITouch Handling

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        process(touches, phase: 0)
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        // Use predicted touches for lower-latency pencil strokes
        if let touch = touches.first, touch.type == .pencil,
           let predicted = event?.predictedTouches(for: touch) {
            process(Set(predicted), phase: 1, isPredicted: true)
        }
        process(touches, phase: 1)
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        process(touches, phase: 3)
    }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        process(touches, phase: 4)
    }

    private func process(_ touches: Set<UITouch>,
                         phase: Int,
                         isPredicted: Bool = false) {
        for touch in touches {
            let isPencil = touch.type == .pencil

            // In DrawingPad mode: skip finger touches (handled by pan gesture instead)
            if inputMode == .drawingPad && !isPencil { continue }

            let loc = touch.location(in: self)
            let normX = Float(loc.x / bounds.width)
            let normY = Float(loc.y / bounds.height)

            // DrawingPad: render stroke locally on the overlay canvas
            if inputMode == .drawingPad && isPencil && !isPredicted {
                canvasView.addPoint(loc,
                                    phase: phase,
                                    pressure: touch.force / max(touch.maximumPossibleForce, 1),
                                    color: strokeColor)
            }

            let evt = InputEvent(
                type: isPencil ? (inputMode == .drawingPad ? .drawing : .pencil) : .touch,
                x: normX,
                y: normY,
                phase: phase,
                pressure: Float(touch.force / max(touch.maximumPossibleForce, 1)),
                altitude: Float(touch.altitudeAngle),
                azimuth: Float(touch.azimuthAngle(in: self)),
                isDrawingPad: inputMode == .drawingPad)

            onInput?(evt)
        }
    }
}

// MARK: - DrawingCanvas (local ink overlay)

/// A lightweight UIView that draws pencil strokes locally using Core Graphics.
/// In DrawingPad mode this gives zero-latency ink feedback independent of network round-trips.
class DrawingCanvas: UIView {

    var strokeColor: UIColor = .black
    private var strokes: [Stroke] = []
    private var currentStroke: Stroke?

    private struct Stroke {
        var points: [(CGPoint, CGFloat)] = []  // (location, pressure 0-1)
        var color: UIColor
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
    }
    required init?(coder: NSCoder) { fatalError() }

    func addPoint(_ point: CGPoint, phase: Int, pressure: CGFloat, color: UIColor) {
        switch phase {
        case 0: // began
            currentStroke = Stroke(points: [(point, pressure)], color: color)
        case 1: // moved
            currentStroke?.points.append((point, pressure))
        case 3, 4: // ended / cancelled
            if let stroke = currentStroke { strokes.append(stroke) }
            currentStroke = nil
        default: break
        }
        setNeedsDisplay()
    }

    func clear() {
        strokes.removeAll()
        currentStroke = nil
        setNeedsDisplay()
    }

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        var all = strokes
        if let cur = currentStroke { all.append(cur) }

        for stroke in all {
            guard stroke.points.count > 1 else { continue }
            ctx.setStrokeColor(stroke.color.cgColor)
            ctx.setLineCap(.round)
            ctx.setLineJoin(.round)

            for i in 1..<stroke.points.count {
                let (prev, prevP) = stroke.points[i - 1]
                let (curr, currP) = stroke.points[i]
                let width = max(0.5, CGFloat(lerp(Float(prevP), Float(currP), 0.5)) * 8)
                ctx.setLineWidth(width)
                ctx.move(to: prev)
                ctx.addLine(to: curr)
                ctx.strokePath()
            }
        }
    }

    private func lerp(_ a: Float, _ b: Float, _ t: Float) -> Float {
        return a + (b - a) * t
    }
}