import AppKit
import CryptoKit

final class SessionLog {
    static let shared=SessionLog()
    let lock=NSLock();let url:URL
    init(){
        let base=FileManager.default.urls(for:.applicationSupportDirectory,in:.userDomainMask)[0].appendingPathComponent("GammaHUD/Logs")
        try? FileManager.default.createDirectory(at:base,withIntermediateDirectories:true)
        url=base.appendingPathComponent("installer-\(Int(Date().timeIntervalSince1970))-\(UUID().uuidString.prefix(8)).log")
        FileManager.default.createFile(atPath:url.path,contents:nil)
        write("Installer 1.0.20 | macOS \(ProcessInfo.processInfo.operatingSystemVersionString)")
    }
    func write(_ value:String){
        lock.lock();defer{lock.unlock()}
        guard let file=try? FileHandle(forWritingTo:url) else{return}
        defer{try? file.close()};_ = try? file.seekToEnd()
        try? file.write(contentsOf:Data("[\(ISO8601DateFormatter().string(from:Date()))] \(value)\n".utf8))
    }
}
struct InstallError: Error, CustomStringConvertible { let description: String }
final class Engine {
    let resources: URL
    var log: (String) -> Void = { _ in }
    var progress: (String) -> Void = { _ in }
    var verificationReport=""
    init(_ resources: URL) { self.resources = resources }
    func adb(_ arguments: [String], timeout: Double = 90) throws -> String {
        let p = Process()
        let temp = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        FileManager.default.createFile(atPath: temp.path, contents: nil)
        let file = try FileHandle(forWritingTo: temp)
        defer { try? file.close(); try? FileManager.default.removeItem(at: temp) }
        p.executableURL = resources.appendingPathComponent("adb")
        p.arguments = arguments; p.standardOutput = file; p.standardError = file
        log("ADB command: \(arguments.joined(separator:" "))")
        do {try p.run()}catch{log("ADB launch failed: \(error)");throw error}
        let deadline = Date().addingTimeInterval(timeout)
        while p.isRunning && Date() < deadline { Thread.sleep(forTimeInterval: 0.05) }
        if p.isRunning { p.terminate(); log("ADB timeout; partial output: \(String(decoding:(try? Data(contentsOf:temp)) ?? Data(),as:UTF8.self))");throw InstallError(description: "操作超时，请检查连接及掌机授权。") }
        p.waitUntilExit()
        let s = String(decoding: try Data(contentsOf: temp), as: UTF8.self)
        log("ADB exit code: \(p.terminationStatus)")
        if !s.isEmpty { log(s.trimmingCharacters(in: .whitespacesAndNewlines)) }
        guard p.terminationStatus == 0 else { throw InstallError(description: "ADB 操作失败：\(s)") }
        return s
    }
    func device() throws -> String {
        _ = try? adb(["version"],timeout:10)
        _ = try? adb(["server-status"],timeout:10)
        _ = try? adb(["devices","-l"],timeout:10)
        let s = try adb(["devices"])
        let rows = s.components(separatedBy: .newlines).map { $0.split(whereSeparator: { $0.isWhitespace }).map(String.init) }.filter { $0.count == 2 && ["device", "unauthorized", "offline"].contains($0[1]) }
        guard rows.count == 1 else { throw InstallError(description: rows.isEmpty ? "未找到设备。请启用 USB 调试，用数据线连接。" : "请只连接一台掌机。") }
        guard rows[0][1] == "device" else { throw InstallError(description: "请解锁掌机，允许 USB 调试授权；离线时重新连接。") }
        guard !rows[0][0].contains(":"), !rows[0][0].hasPrefix("emulator-") else { throw InstallError(description: "请通过 USB 连接实体掌机。") }
        return rows[0][0]
    }
    func script(_ serial: String, _ mode: String, _ hash: String = "") throws {
        _ = try adb(["-s", serial, "push", resources.appendingPathComponent("device.sh").path, "/data/local/tmp/gamma-desktop-device.sh"])
        if ["clock-install", "clock-remove"].contains(mode) {
            for (local, remote) in [("live.sh", "gamma-desktop-live.sh"), ("clock-original", "gamma-clock-original")] {
                _ = try adb(["-s", serial, "push", resources.appendingPathComponent(local).path, "/data/local/tmp/\(remote)"])
            }
        }
        let command = "sh /data/local/tmp/gamma-desktop-device.sh \(mode) \(hash)"
        let uid = try adb(["-s", serial, "shell", "id", "-u"]).trimmingCharacters(in: .whitespacesAndNewlines)
        let output = try adb(["-s", serial, "shell"] + (uid == "0" ? [command] : ["su", "-mm", "-c", "'\(command)'"]))
        if ["verify","clock-verify"].contains(mode) {
            verificationReport=output.components(separatedBy:.newlines).filter { mode == "verify" ? $0.hasPrefix("面板结果：") : $0.hasPrefix("时钟补丁：") }.joined(separator:"\n")
        }
    }
    func perform(_ mode: String) throws {
        progress("正在连接掌机…")
        let serial = try device()
        progress("正在检查设备与权限…")
        try script(serial, "check")
        if mode == "install" || mode == "clock-install" {
            let archive = mode == "clock-install" ? "clock" : "module"
            let zip = resources.appendingPathComponent("\(archive).zip")
            let digest = SHA256.hash(data: try Data(contentsOf: zip)).map { String(format: "%02x", $0) }.joined()
            let expected = try String(contentsOf: resources.appendingPathComponent("\(archive).sha256"), encoding: .utf8).trimmingCharacters(in: .whitespacesAndNewlines)
            guard digest == expected else { throw InstallError(description: "电脑上的安装包校验失败，请重新解压安装器。") }
            progress("正在传输安装包…")
            _ = try adb(["-s", serial, "push", zip.path, mode == "clock-install" ? "/data/local/tmp/gamma-desktop-clock.zip" : "/data/local/tmp/gamma-desktop-module.zip"])
            progress("正在安装，请稍候…")
            try script(serial, mode, digest)
        } else if mode == "reboot" {
            _ = try adb(["-s", serial, "reboot"])
            progress("正在等待掌机重启，请勿拔线…")
            Thread.sleep(forTimeInterval: 8)
            let deadline = Date().addingTimeInterval(150)
            var ready = false
            while Date() < deadline {
                if let s = try? adb(["-s", serial, "shell", "getprop", "sys.boot_completed"], timeout: 5), s.trimmingCharacters(in: .whitespacesAndNewlines) == "1" { ready = true; break }
                Thread.sleep(forTimeInterval: 3)
            }
            guard ready else { throw InstallError(description: "等待开机超时。开机后点击检测设备，再验证。") }
            Thread.sleep(forTimeInterval: 5)
            try script(serial, "verify")
        } else if ["verify", "remove", "clock-remove", "clock-verify"].contains(mode) { try script(serial, mode) }
        else { log("设备已连接，适配检查通过。可以一键安装。") }
    }
}

private func ink(_ hex: UInt32) -> NSColor {
    NSColor(calibratedRed:CGFloat((hex>>16)&255)/255,green:CGFloat((hex>>8)&255)/255,blue:CGFloat(hex&255)/255,alpha:1)
}
final class ActionButton: NSButton {
    var symbol="";var primary=false;var danger=false;var hovered=false
    override func updateTrackingAreas() {
        super.updateTrackingAreas();trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(rect:bounds,options:[.mouseEnteredAndExited,.activeInKeyWindow],owner:self,userInfo:nil))
    }
    override func mouseEntered(with event:NSEvent){hovered=true;needsDisplay=true}
    override func mouseExited(with event:NSEvent){hovered=false;needsDisplay=true}
    override var isEnabled:Bool { didSet{needsDisplay=true} }
    override func draw(_ dirtyRect:NSRect) {
        let fill=primary ? ink(hovered ? 0x6896ff:0x5685ef):ink(hovered ? 0x303c50:0x263145)
        fill.withAlphaComponent(isEnabled ? 1:0.45).setFill()
        let shape=NSBezierPath(roundedRect:bounds.insetBy(dx:0.5,dy:0.5),xRadius:10,yRadius:10);shape.fill()
        ink(0x3a4860).setStroke();shape.lineWidth=0.7;shape.stroke()
        let tint=(danger ? ink(0xeaa4ad):ink(0xf0f4ff)).withAlphaComponent(isEnabled ? 1:0.45)
        let image=NSImage(systemSymbolName:symbol,accessibilityDescription:title)?.withSymbolConfiguration(NSImage.SymbolConfiguration(pointSize:21,weight:.medium).applying(NSImage.SymbolConfiguration(paletteColors:[tint])))
        let iconY=isFlipped ? (bounds.height>70 ? 14.0:6.0):bounds.height-38
        image?.draw(in:NSRect(x:(bounds.width-24)/2,y:iconY,width:24,height:24))
        let attrs:[NSAttributedString.Key:Any]=[.font:NSFont.systemFont(ofSize:13,weight:.medium),.foregroundColor:tint]
        let size=title.size(withAttributes:attrs)
        title.draw(at:NSPoint(x:(bounds.width-size.width)/2,y:isFlipped ? bounds.height-28:12),withAttributes:attrs)
    }
}
final class App: NSObject, NSApplicationDelegate, NSWindowDelegate {
    var window: NSWindow!; var status: NSTextField!; var progressBar: NSProgressIndicator!; var buttons: [NSButton] = []; var busy = false
    func applicationDidFinishLaunching(_ notification: Notification) {
        window=NSWindow(contentRect:NSRect(x:0,y:0,width:860,height:650),styleMask:[.titled,.closable,.miniaturizable],backing:.buffered,defer:false)
        window.title="GammaOS · 掌机工具";window.delegate=self;window.appearance=NSAppearance(named:.darkAqua)
        window.backgroundColor=ink(0x101722)
        let root=window.contentView!
        func label(_ value:String,_ x:Int,_ y:Int,_ width:Int,_ height:Int,_ size:CGFloat,_ muted:Bool=false) {
            let l=NSTextField(wrappingLabelWithString:value);l.frame=NSRect(x:x,y:y,width:width,height:height)
            l.font = .systemFont(ofSize:size,weight:muted ? .regular:.semibold);l.textColor=ink(muted ? 0x96a6bd:0xeaf0fb);root.addSubview(l)
        }
        func card(_ x:Int,_ y:Int,_ w:Int,_ h:Int) {
            let v=NSView(frame:NSRect(x:x,y:y,width:w,height:h));v.wantsLayer=true;v.layer?.cornerRadius=16
            v.layer?.backgroundColor=ink(0x1a2332).cgColor;v.layer?.borderWidth=1;v.layer?.borderColor=ink(0x2b374b).cgColor;root.addSubview(v)
        }
        func action(_ tag:Int,_ name:String,_ icon:String,_ x:Int,_ y:Int,_ width:Int,_ height:Int,_ primary:Bool=false,_ danger:Bool=false) {
            let b=ActionButton(title:name,target:self,action:#selector(click(_:)));b.tag=tag;b.symbol=icon;b.primary=primary;b.danger=danger
            b.frame=NSRect(x:x,y:y,width:width,height:height);b.isBordered=false;root.addSubview(b);buttons.append(b)
        }
        label("免责声明：非官方开源工具，按现状提供；请先备份并阅读使用说明。",28,606,780,20,12,true)
        label("掌机工具中心",28,561,650,38,29)
        label("性能监测与主菜单时钟，让掌机保持最佳状态。",28,535,750,23,14,true)
        card(28,432,804,86)
        label("连接你的掌机",48,478,320,23,17)
        label("USB 数据线连接 · 开启调试并允许授权",48,450,370,22,12,true)
        action(0,"检测设备","cable.connector",490,444,150,62)
        action(2,"重启并验证","arrow.clockwise",652,444,160,62)
        card(28,220,394,194);card(438,220,394,194)
        label("性能面板",48,371,340,25,19)
        label("实时温度、频率与使用率 · BTN_MODE 开关",48,345,350,22,12,true)
        label("XMB 时钟补丁",458,371,340,25,19)
        label("优化主菜单时钟布局 · 系统升级前请移除",458,345,350,22,12,true)
        action(1,"安装面板","square.and.arrow.down",48,242,110,82,true)
        action(3,"验证状态","checkmark.shield",170,242,110,82)
        action(4,"移除面板","trash",292,242,110,82,false,true)
        action(5,"安装补丁","clock.arrow.circlepath",458,242,110,82,true)
        action(6,"检查补丁","checkmark.shield",580,242,110,82)
        action(7,"移除补丁","trash",702,242,110,82,false,true)
        card(28,38,804,164)
        label("操作状态",48,165,740,20,12,true)
        status=NSTextField(wrappingLabelWithString:"准备就绪，请连接掌机")
        status.font = .systemFont(ofSize:15,weight:.medium);status.textColor=ink(0xeaf0fb)
        status.frame=NSRect(x:48,y:95,width:764,height:61);root.addSubview(status)
        progressBar=NSProgressIndicator(frame:NSRect(x:48,y:80,width:764,height:8));progressBar.style = .bar
        progressBar.isIndeterminate=false;progressBar.minValue=0;progressBar.maxValue=100;root.addSubview(progressBar)
        label("处理期间请保持连接，重启前请保存游戏进度。",48,49,750,20,12,true)
        label("本地安装 · 无需联网     |     适用于已具备 Root / Magisk 的 RG DS",28,8,660,18,11,true)
        let export=NSButton(title:"导出日志",target:self,action:#selector(exportLog))
        export.image=NSImage(systemSymbolName:"square.and.arrow.up",accessibilityDescription:nil);export.imagePosition = .imageLeft
        export.bezelStyle = .rounded;export.frame=NSRect(x:710,y:5,width:122,height:25);root.addSubview(export)
        SessionLog.shared.write("UI ready; resource directory: \(Bundle.main.resourceURL!.path)")
        window.center();window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
    }
    @objc func exportLog(){
        let panel=NSSavePanel();panel.nameFieldStringValue="GammaHUD-diagnostic.log"
        panel.message="日志包含设备标识、ADB 状态和操作结果；分享前可检查内容。"
        panel.beginSheetModal(for:window){response in
            guard response == .OK,let target=panel.url else{return}
            do{try Data(contentsOf:SessionLog.shared.url).write(to:target,options:.atomic);self.append("日志已导出")}
            catch{self.append("导出日志失败：\(error.localizedDescription)")}
        }
    }
    func append(_ value: String) { status.stringValue=value;SessionLog.shared.write(value) }
    @objc func click(_ sender: NSButton) {
        guard !busy else { return }; busy = true; buttons.forEach { $0.isEnabled = false }
        let mode = ["check","install","reboot","verify","remove","clock-install","clock-verify","clock-remove"][sender.tag]
        let completion=["设备检测完成","性能面板安装完成","掌机重启与验证完成","性能面板状态验证完成","性能面板移除完成","时钟补丁安装完成","时钟补丁检查完成","时钟补丁移除完成"][sender.tag]
        append("正在\(sender.title)…")
        progressBar.doubleValue=0;progressBar.isIndeterminate=true;progressBar.startAnimation(nil)
        let engine = Engine(Bundle.main.resourceURL!)
        engine.log={SessionLog.shared.write($0)}
        engine.progress = { s in DispatchQueue.main.async { self.append(s) } }
        DispatchQueue.global().async {
            var result = completion;var succeeded=true
            do { try engine.perform(mode);if !engine.verificationReport.isEmpty { result=completion+"：\n"+engine.verificationReport.replacingOccurrences(of:"面板结果：",with:"") } } catch { succeeded=false;let raw=String(describing:error);SessionLog.shared.write("Operation \(mode) failed: \(raw)"); result = String((raw.components(separatedBy:"失败：").last ?? raw).prefix(220)) }
            let final = result;let success=succeeded
            DispatchQueue.main.async { self.append(final); self.progressBar.stopAnimation(nil);self.progressBar.isIndeterminate=false;self.progressBar.doubleValue=success ? 100 : 0; self.busy = false; self.buttons.forEach { $0.isEnabled = true } }
        }
    }
    func windowShouldClose(_ sender: NSWindow) -> Bool { if busy { append("操作进行中，请等待完成后关闭。") }; return !busy }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}
let app = NSApplication.shared
let delegate = App(); app.delegate = delegate; app.setActivationPolicy(.regular); app.run()
