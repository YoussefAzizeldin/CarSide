// VideoDecoder.swift
import VideoToolbox
import CoreMedia
import CoreVideo
import Metal
import MetalKit

class VideoDecoder {
    private var session: VTDecompressionSession?
    private var formatDesc: CMVideoFormatDescription?
    private var nalDataRetain = Data()
    // BUG FIX #19: Thread-safe access to currentPixelBuffer
    private var pixelBufferQueue = DispatchQueue(label: "com.carside.pixelbuffer", attributes: .concurrent)
    private var _currentPixelBuffer: CVPixelBuffer?
    
    var currentPixelBuffer: CVPixelBuffer? {
        get {
            pixelBufferQueue.sync { _currentPixelBuffer }
        }
        set {
            pixelBufferQueue.async(flags: .barrier) { [weak self] in
                self?._currentPixelBuffer = newValue
            }
        }
    }
    
    var onFrame: ((CVPixelBuffer) -> Void)?

    // BUG FIX #16, #17: Parse SPS/PPS with proper bounds checking and support both orders
    private func extractSPSPPS(from data: Data) -> (Data, Data) {
        var sps: Data?
        var pps: Data?
        var index = 0
        let bytes = [UInt8](data)
        
        while index + 4 <= bytes.count {  // BUG FIX #16: Prevent underflow
            if index + 3 < bytes.count && bytes[index] == 0 && bytes[index + 1] == 0 && 
               bytes[index + 2] == 0 && bytes[index + 3] == 1 && index + 4 < bytes.count {
                let nalType = bytes[index + 4] & 0x1F
                var endIndex = index + 5
                while endIndex + 4 <= bytes.count {
                    if bytes[endIndex] == 0 && bytes[endIndex + 1] == 0 && 
                       bytes[endIndex + 2] == 0 && bytes[endIndex + 3] == 1 {
                        break
                    }
                    endIndex += 1
                }
                if endIndex >= bytes.count {
                    endIndex = bytes.count
                }
                let nalData = data.subdata(in: index + 4..<endIndex)
                
                // BUG FIX #17: Find both SPS and PPS regardless of order
                if nalType == 7 && sps == nil {  // SPS
                    sps = nalData
                } else if nalType == 8 && pps == nil {  // PPS
                    pps = nalData
                }
            }
            index += 1
        }
        return (sps ?? Data(), pps ?? Data())
    }

    // BUG FIX #18: Check if NAL is an IDR/keyframe before extracting SPS/PPS
    private func isIDRFrame(_ data: Data) -> Bool {
        let bytes = [UInt8](data)
        if bytes.count >= 5 {
            for i in 0...bytes.count - 5 {
                if bytes[i] == 0 && bytes[i + 1] == 0 && 
                   bytes[i + 2] == 0 && bytes[i + 3] == 1 {
                    let nalType = bytes[i + 4] & 0x1F
                    if nalType == 5 {  // IDR/I-frame
                        return true
                    }
                }
            }
        }
        return false
    }

    func decodeNAL(_ data: Data) {
        // BUG FIX #18: Only extract SPS/PPS from IDR frames
        if formatDesc == nil && isIDRFrame(data) {
            let (sps, pps) = extractSPSPPS(from: data)
            let status = sps.withUnsafeBytes { spsBytes in
                pps.withUnsafeBytes { ppsBytes in
                    let spsPtr = spsBytes.baseAddress?.assumingMemoryBound(to: UInt8.self) ?? nil
                    let ppsPtr = ppsBytes.baseAddress?.assumingMemoryBound(to: UInt8.self) ?? nil
                    if let spsPtr = spsPtr, let ppsPtr = ppsPtr {
                        let sizes: [Int] = [sps.count, pps.count]
                        let pointers = [spsPtr, ppsPtr] as [UnsafePointer<UInt8>]
                        return CMVideoFormatDescriptionCreateFromH264ParameterSets(
                            allocator: nil,
                            parameterSetCount: 2,
                            parameterSetPointers: pointers,
                            parameterSetSizes: sizes,
                            nalUnitHeaderLength: 4,
                            formatDescriptionOut: &formatDesc)
                    }
                    return kCMFormatDescriptionError_InvalidParameter
                }
            }
            guard status == noErr else {
                print("[VideoDecoder] Failed to create format description: \(status)")
                return
            }
            createSession()
        }

        guard let session = session else {
            print("[VideoDecoder] Decoder session not ready")
            return
        }

        nalDataRetain = data

        var blockBuf: CMBlockBuffer?
        let status = nalDataRetain.withUnsafeBytes { rawBuf in
            let buf = rawBuf.baseAddress
            return CMBlockBufferCreateWithMemoryBlock(
                allocator: nil,
                memoryBlock: UnsafeMutableRawPointer(mutating: buf),
                blockLength: nalDataRetain.count,
                blockAllocator: kCFAllocatorNull,
                customBlockSource: nil, offsetToData: 0, dataLength: nalDataRetain.count,
                flags: 0, blockBufferOut: &blockBuf)
        }
        guard status == noErr, let blockBuf = blockBuf else {
            print("[VideoDecoder] Failed to create block buffer: \(status)")
            return
        }

        var sampleBuf: CMSampleBuffer?
        var timingInfo = CMSampleTimingInfo(
            duration: CMTime(value: 1, timescale: 60),
            presentationTimeStamp: CMClockGetTime(CMClockGetHostTimeClock()),
            decodeTimeStamp: .invalid)
        let sampleStatus = CMSampleBufferCreateReady(
            allocator: nil, dataBuffer: blockBuf,
            formatDescription: formatDesc,
            sampleCount: 1, sampleTimingEntryCount: 1,
            sampleTimingArray: &timingInfo,
            sampleSizeEntryCount: 0, sampleSizeArray: nil,
            sampleBufferOut: &sampleBuf)
        guard sampleStatus == noErr, let sampleBuf = sampleBuf else {
            print("[VideoDecoder] Failed to create sample buffer: \(sampleStatus)")
            return
        }

        VTDecompressionSessionDecodeFrame(
            session, sampleBuffer: sampleBuf,
            flags: ._EnableAsynchronousDecompression,
            infoFlagsOut: nil) { [weak self] result, _, pixelBuffer, _, _ in
            guard result == noErr else {
                print("[VideoDecoder] Decode error: \(result)")
                return
            }
            if let pixelBuffer = pixelBuffer {
                self?.currentPixelBuffer = pixelBuffer  // Thread-safe write
                self?.onFrame?(pixelBuffer)
            }
        }
    }

    private func createSession() {
        guard let formatDesc = formatDesc else {
            print("[VideoDecoder] Format description not available")
            return
        }
        let attrs: [String: Any] = [
            kCVPixelBufferPixelFormatTypeKey as String:
                kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            kCVPixelBufferMetalCompatibilityKey as String: true
        ]
        let status = VTDecompressionSessionCreate(
            allocator: nil,
            formatDescription: formatDesc,
            decoderSpecification: nil,
            imageBufferAttributes: attrs as CFDictionary,
            outputCallback: nil,
            outputCallbackRefCon: nil,
            decompressionSessionOut: &session)
        guard status == noErr else {
            print("[VideoDecoder] Failed to create decompression session: \(status)")
            self.session = nil
            return
        }
    }
}

// MetalRenderer.swift — zero-copy CVPixelBuffer → screen
class MetalRenderer: NSObject, MTKViewDelegate {
    private var textureCache: CVMetalTextureCache?
    private var pipelineState: MTLRenderPipelineState?
    private var commandQueue: MTLCommandQueue?
    // BUG FIX #19: Thread-safe access from metal render thread
    private var pixelBufferQueue = DispatchQueue(label: "com.carside.metalrenderer", attributes: .concurrent)
    private var _currentPixelBuffer: CVPixelBuffer?
    
    var currentPixelBuffer: CVPixelBuffer? {
        get {
            pixelBufferQueue.sync { _currentPixelBuffer }
        }
        set {
            pixelBufferQueue.async(flags: .barrier) { [weak self] in
                self?._currentPixelBuffer = newValue
            }
        }
    }

    func setup(device: MTLDevice) {
        CVMetalTextureCacheCreate(nil, nil, device, nil, &textureCache)
        self.commandQueue = device.makeCommandQueue()
        let lib = device.makeDefaultLibrary()!
        let desc = MTLRenderPipelineDescriptor()
        desc.vertexFunction   = lib.makeFunction(name: "quadVert")
        desc.fragmentFunction = lib.makeFunction(name: "yuvToRgbFrag")
        desc.colorAttachments[0].pixelFormat = .bgra8Unorm
        pipelineState = try? device.makeRenderPipelineState(descriptor: desc)
    }

    func draw(in view: MTKView) {
        guard let buf = currentPixelBuffer,
              let cache = textureCache,
              let pass = view.currentRenderPassDescriptor,
              let cmdQueue = commandQueue,
              let cmdBuf = cmdQueue.makeCommandBuffer(),
              let enc = cmdBuf.makeRenderCommandEncoder(descriptor: pass)
        else { return }

        // Bind Y and CbCr planes directly from the CVPixelBuffer — no copy
        var yTexRef: CVMetalTexture?, uvTexRef: CVMetalTexture?
        CVMetalTextureCacheCreateTextureFromImage(
            nil, cache, buf, nil, .r8Unorm,
            CVPixelBufferGetWidthOfPlane(buf, 0),
            CVPixelBufferGetHeightOfPlane(buf, 0), 0, &yTexRef)
        CVMetalTextureCacheCreateTextureFromImage(
            nil, cache, buf, nil, .rg8Unorm,
            CVPixelBufferGetWidthOfPlane(buf, 1),
            CVPixelBufferGetHeightOfPlane(buf, 1), 1, &uvTexRef)

        enc.setRenderPipelineState(pipelineState!)
        enc.setFragmentTexture(CVMetalTextureGetTexture(yTexRef!),  index: 0)
        enc.setFragmentTexture(CVMetalTextureGetTexture(uvTexRef!), index: 1)
        enc.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        enc.endEncoding()
        cmdBuf.present(view.currentDrawable!)
        cmdBuf.commit()
    }
}