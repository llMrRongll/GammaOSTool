Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$script:busy = $false
$form = New-Object Windows.Forms.Form
$form.Text = 'GammaOS RG DS · 掌机工具安装器'
$form.ClientSize = New-Object Drawing.Size(860,650)
$form.FormBorderStyle='FixedSingle';$form.MaximizeBox=$false
$form.StartPosition='CenterScreen';$form.Font=New-Object Drawing.Font('Microsoft YaHei UI',10)
$form.BackColor=[Drawing.ColorTranslator]::FromHtml('#101722')
Add-Type -TypeDefinition @'
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
public class GammaCard : Panel {
 protected override void OnPaintBackground(PaintEventArgs e) {
  e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;
  using(var path=new GraphicsPath()) {
   int r=28,w=Width-1,h=Height-1;
   path.AddArc(0,0,r,r,180,90);path.AddArc(w-r,0,r,r,270,90);
   path.AddArc(w-r,h-r,r,r,0,90);path.AddArc(0,h-r,r,r,90,90);path.CloseFigure();
   e.Graphics.Clear(Color.FromArgb(16,23,34));
   using(var brush=new SolidBrush(Color.FromArgb(26,35,50))) e.Graphics.FillPath(brush,path);
   using(var pen=new Pen(Color.FromArgb(43,55,75))) e.Graphics.DrawPath(pen,path);
  }
 }
}
'@ -ReferencedAssemblies System.Drawing,System.Windows.Forms
function UI-Label($parent,[string]$text,[int]$x,[int]$y,[int]$w,[int]$h,[float]$size,[bool]$muted=$false) {
 $l=New-Object Windows.Forms.Label;$l.Text=$text;$l.SetBounds($x,$y,$w,$h)
 $l.BackColor=[Drawing.Color]::Transparent
 $l.ForeColor=[Drawing.ColorTranslator]::FromHtml($(if($muted){'#96a6bd'}else{'#eaf0fb'}))
 $l.Font=New-Object Drawing.Font('Microsoft YaHei UI',$size,$(if($muted){[Drawing.FontStyle]::Regular}else{[Drawing.FontStyle]::Bold}))
 $parent.Controls.Add($l);return $l
}
function UI-Card([int]$x,[int]$y,[int]$w,[int]$h) {
 $c=New-Object GammaCard;$c.SetBounds($x,$y,$w,$h);$form.Controls.Add($c);return $c
}
$null=UI-Label $form '免责声明：非官方开源工具，按现状提供；请先备份并阅读使用说明。' 28 18 780 22 10 $true
$null=UI-Label $form '掌机工具中心' 28 45 700 44 25
$null=UI-Label $form '性能监测与主菜单时钟，让掌机保持最佳状态。' 28 96 750 25 11 $true
$deviceCard=UI-Card 28 132 804 86
$null=UI-Label $deviceCard '连接你的掌机' 20 16 370 27 14
$null=UI-Label $deviceCard 'USB 数据线连接 · 开启调试并允许授权' 20 48 420 23 10 $true
$hudCard=UI-Card 28 236 394 194
$clockCard=UI-Card 438 236 394 194
$null=UI-Label $hudCard '性能面板' 20 16 350 30 16
$null=UI-Label $hudCard '实时温度、频率与使用率 · BTN_MODE 开关' 20 48 355 27 9 $true
$null=UI-Label $clockCard 'XMB 时钟补丁' 20 16 350 30 16
$null=UI-Label $clockCard '优化主菜单时钟布局 · 系统升级前请移除' 20 48 355 27 9 $true
$panel=UI-Card 28 448 804 164
$null=UI-Label $panel '操作状态' 20 14 760 22 10 $true
$status=UI-Label $panel '准备就绪，请连接掌机' 20 40 764 65 11
$progress=New-Object Windows.Forms.ProgressBar;$progress.SetBounds(20,114,764,8);$panel.Controls.Add($progress)
$null=UI-Label $panel '处理期间请保持连接，重启前请保存游戏进度。' 20 134 764 23 9 $true
$null=UI-Label $form '本地安装 · 无需联网     |     适用于已具备 Root / Magisk 的 RG DS' 28 625 660 20 9 $true
$logDir=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'GammaHUD\Logs'
$script:logFile=$null
try {
 [void][IO.Directory]::CreateDirectory($logDir)
 $script:logFile=Join-Path $logDir ('installer-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.log')
 [IO.File]::WriteAllText($script:logFile,'',[Text.UTF8Encoding]::new($true))
} catch { }
function Write-Log([string]$s) {
 if($script:logFile){try{[IO.File]::AppendAllText($script:logFile,('['+(Get-Date -Format o)+'] '+$s+"`r`n"),[Text.UTF8Encoding]::new($true))}catch{}}
}
function Set-Status([string]$s) { Write-Log $s;$status.Text = $s; [Windows.Forms.Application]::DoEvents() }
Write-Log ('Installer 1.0.21 | OS '+[Environment]::OSVersion.VersionString+' | PowerShell '+$PSVersionTable.PSVersion+' | Process64 '+[Environment]::Is64BitProcess+' | OS64 '+[Environment]::Is64BitOperatingSystem)
Write-Log ('ADB path: '+(Join-Path $root 'adb.exe')+' | ADB_USB_LEGACY='+$env:ADB_USB_LEGACY+' | ADB_LIBUSB='+$env:ADB_LIBUSB)
$export=New-Object Windows.Forms.Button;$export.Text='导出日志';$export.SetBounds(710,620,122,26)
$export.FlatStyle='Flat';$export.ForeColor=[Drawing.Color]::White;$export.BackColor=[Drawing.ColorTranslator]::FromHtml('#263145')
$export.Add_Click({
 if(-not $script:logFile -or -not [IO.File]::Exists($script:logFile)){Set-Status '日志文件不可用，请检查本地用户目录权限。';return}
 $dialog=New-Object Windows.Forms.SaveFileDialog;$dialog.FileName='GammaHUD-diagnostic.log';$dialog.Filter='日志文件 (*.log)|*.log'
 $dialog.Title='导出诊断日志（包含设备标识，分享前请检查）'
 if($dialog.ShowDialog() -eq 'OK'){try{[IO.File]::Copy($script:logFile,$dialog.FileName,$true);Set-Status '日志已导出'}catch{Set-Status ('导出日志失败：'+$_.Exception.Message)}}
 $dialog.Dispose()
});$form.Controls.Add($export)
function USB-Diagnostics {
 try {
  $devices=Get-CimInstance Win32_PnPEntity -OperationTimeoutSec 5 | Where-Object { $_.PNPDeviceID -like 'USB*' -or $_.Name -match 'ADB|Android|Rockchip' }
  foreach($d in $devices){Write-Log ('PnP: '+$d.Name+' | Status='+$d.Status+' | ErrorCode='+$d.ConfigManagerErrorCode+' | Class='+$d.PNPClass+' | ID='+$d.PNPDeviceID)}
 }catch{Write-Log ('PnP diagnostics unavailable: '+$_.Exception.Message)}
 try {
  $drivers=Get-CimInstance Win32_PnPSignedDriver -OperationTimeoutSec 5 | Where-Object { $_.DeviceID -like 'USB\VID_2207*' -or $_.DeviceName -match 'GammaOS|Android.*ADB' }
  foreach($d in $drivers){Write-Log ('Driver: '+$d.DeviceName+' | Provider='+$d.DriverProviderName+' | Version='+$d.DriverVersion+' | INF='+$d.InfName+' | DeviceID='+$d.DeviceID)}
 }catch{Write-Log ('Driver diagnostics unavailable: '+$_.Exception.Message)}
}
# Windows CommandLineToArgvW-compatible quoting, including paths containing spaces.
function Quote-Arg([string]$s) {
    '"' + ([regex]::Replace(([regex]::Replace($s, '(\\*)"', '$1$1\"')), '(\\+)$', '$1$1')) + '"'
}
function Adb([string[]]$argsList, [int]$timeout = 90) {
    $p = New-Object Diagnostics.Process
    $p.StartInfo.FileName = Join-Path $root 'adb.exe'
    # Dedicated localhost server avoids reusing an older installation's default server.
    $argsList=@('-P','5039')+$argsList
    $p.StartInfo.EnvironmentVariables['ADB_USB_LEGACY']='1'
    $p.StartInfo.EnvironmentVariables['ADB_MDNS_AUTO_CONNECT']='0'
    $p.StartInfo.EnvironmentVariables.Remove('ADB_SERVER_SOCKET')
    Write-Log ('ADB command: '+($argsList -join ' ')+' | USB legacy=1')
    $p.StartInfo.Arguments = (($argsList | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $p.StartInfo.UseShellExecute = $false; $p.StartInfo.CreateNoWindow = $true
    $p.StartInfo.RedirectStandardOutput = $true; $p.StartInfo.RedirectStandardError = $true
    $p.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
    $p.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
    try{[void]$p.Start()}catch{Write-Log ('ADB launch failed: '+$_.Exception.Message);throw}
    $stdout = $p.StandardOutput.ReadToEndAsync(); $stderr = $p.StandardError.ReadToEndAsync()
    $deadline = (Get-Date).AddSeconds($timeout)
    while (-not $p.HasExited) {
        [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 50
        if ((Get-Date) -gt $deadline) { $p.Kill();Write-Log 'ADB timeout';$p.Dispose(); throw '操作超时，请检查连接及掌机授权。' }
    }
    $output = $stdout.Result + $stderr.Result; $code = $p.ExitCode;Write-Log ('ADB exit code: '+$code);$p.Dispose()
    if ($output.Trim()) { Write-Log $output.Trim() }
    if ($code -ne 0) { throw "ADB 操作失败：$output" }
    return $output
}
function Device {
    foreach($command in @('version','server-status')){try{$null=Adb @($command) 10}catch{Write-Log $_.Exception.Message}}
    try{$null=Adb @('devices','-l') 10}catch{Write-Log $_.Exception.Message}
    $output = Adb @('devices')
    $rows = @($output -split "`n" | Where-Object { $_ -match '^\S+\s+(device|unauthorized|offline)\s*$' })
    if ($rows.Count -eq 0) { USB-Diagnostics;throw '未找到设备。请开启 USB 调试、连接数据线；Windows 可能需要掌机厂家 ADB 驱动。' }
    if ($rows.Count -ne 1) { throw '请只连接一台掌机。' }
    $parts = $rows[0].Trim() -split '\s+'
    if ($parts[1] -ne 'device') { throw '请解锁掌机，允许 USB 调试授权；离线时重新连接。' }
    if ($parts[0].Contains(':') -or $parts[0].StartsWith('emulator-')) { throw '请通过 USB 连接实体掌机。' }
    return $parts[0]
}
function Device-Script([string]$serial, [string]$mode, [string]$hash = '') {
    if ($mode -in @('install','clock-install')) { Set-Status '正在安装，请稍候…' }
    elseif ($mode -in @('remove','clock-remove')) { Set-Status '正在移除，请稍候…' }
    elseif ($mode -in @('verify','clock-verify')) { Set-Status '正在验证状态…' }
    $null = Adb @('-s',$serial,'push',(Join-Path $root 'device.sh'),'/data/local/tmp/gamma-desktop-device.sh')
    if ($mode -in @('clock-install','clock-remove')) {
        $null = Adb @('-s',$serial,'push',(Join-Path $root 'live.sh'),'/data/local/tmp/gamma-desktop-live.sh')
        $null = Adb @('-s',$serial,'push',(Join-Path $root 'clock-original'),'/data/local/tmp/gamma-clock-original')
    }
    $uid = (Adb @('-s',$serial,'shell','id','-u')).Trim()
    $command = "sh /data/local/tmp/gamma-desktop-device.sh $mode $hash"
    if ($uid -eq '0') { $output = Adb @('-s',$serial,'shell',$command) }
    else { $output = Adb @('-s',$serial,'shell','su','-mm','-c',("'"+$command+"'")) }
    if ($mode -in @('verify','clock-verify')) {
        $script:verificationReport=(($output -split "`n" | Where-Object { if($mode -eq 'verify'){$_ -match '^面板结果：'}else{$_ -match '^时钟补丁：'} }) -join "`r`n").Replace('面板结果：','')
    }
}
function Pause-UI([int]$seconds) {
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 100 }
}
function Perform([string]$mode) {
    Set-Status "正在连接掌机…"
    $serial = Device
    Set-Status '正在检查设备与权限…'
    Device-Script $serial 'check'
    switch ($mode) {
        { $_ -in @('install','clock-install') } {
            $archive = if ($mode -eq 'clock-install') { 'clock' } else { 'module' }
            $zip = Join-Path $root ($archive+'.zip')
            $hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
            if ($hash -ne (Get-Content (Join-Path $root ($archive+'.sha256')) -Raw).Trim()) { throw '电脑上的安装包校验失败，请重新解压安装器。' }
            $remote = if ($mode -eq 'clock-install') { '/data/local/tmp/gamma-desktop-clock.zip' } else { '/data/local/tmp/gamma-desktop-module.zip' }
            $null = Adb @('-s',$serial,'push',$zip,$remote)
            Device-Script $serial $mode $hash
        }
        'reboot' {
            $null = Adb @('-s',$serial,'reboot')
            Set-Status '正在等待掌机重启，请勿拔线…'; Pause-UI 8
            $end = (Get-Date).AddSeconds(150); $ready = $false
            while ((Get-Date) -lt $end) {
                try { if ((Adb @('-s',$serial,'shell','getprop','sys.boot_completed') 5).Trim() -eq '1') { $ready = $true; break } } catch { }
                Pause-UI 3
            }
            if (-not $ready) { throw '等待开机超时。开机后点击验证状态。' }
            Pause-UI 5; Device-Script $serial 'verify'
        }
        'verify' { Device-Script $serial 'verify' }
        'clock-verify' { Device-Script $serial 'clock-verify' }
        'clock-remove' { Device-Script $serial 'clock-remove' }
        'remove' { Device-Script $serial 'remove' }
        default { Write-Log '设备已连接，适配检查通过。可以一键安装。' }
    }
}
$buttons = @()
$names = @('检测设备','安装面板','重启并验证','验证状态','移除面板','安装补丁','检查补丁','移除补丁')
$modes = @('check','install','reboot','verify','remove','clock-install','clock-verify','clock-remove')
for ($i=0; $i -lt 8; $i++) {
    $b = New-Object Windows.Forms.Button; $b.Text = $names[$i]; $b.Tag = $modes[$i]
    $positions=@(@(462,12,150,62),@(20,90,110,82),@(624,12,160,62),@(142,90,110,82),@(264,90,110,82),@(20,90,110,82),@(142,90,110,82),@(264,90,110,82))
    $v=$positions[$i];$b.SetBounds($v[0],$v[1],$v[2],$v[3])
    $b.FlatStyle='Flat';$b.FlatAppearance.BorderSize=0;$b.Cursor=[Windows.Forms.Cursors]::Hand
    $b.BackColor=[Drawing.ColorTranslator]::FromHtml('#263145');$b.ForeColor=[Drawing.ColorTranslator]::FromHtml('#eaf0fb')
    $b.FlatAppearance.MouseOverBackColor=[Drawing.ColorTranslator]::FromHtml('#303c50')
    if($i -eq 1 -or $i -eq 5){$b.BackColor=[Drawing.ColorTranslator]::FromHtml('#5685ef');$b.FlatAppearance.MouseOverBackColor=[Drawing.ColorTranslator]::FromHtml('#6896ff')}
    if($i -eq 4 -or $i -eq 7){$b.ForeColor=[Drawing.ColorTranslator]::FromHtml('#eaa4ad')}
    $codes=@(0xE839,0xE896,0xE72C,0xE73E,0xE74D,0xE823,0xE73E,0xE74D)
    $image=New-Object Drawing.Bitmap(24,24);$g=[Drawing.Graphics]::FromImage($image)
    $g.SmoothingMode='AntiAlias';$g.TextRenderingHint='AntiAliasGridFit'
    $iconFont=New-Object Drawing.Font('Segoe MDL2 Assets',16)
    $brush=New-Object Drawing.SolidBrush($b.ForeColor)
    $g.DrawString([string][char]$codes[$i],$iconFont,$brush,-1,-1);$g.Dispose();$brush.Dispose();$iconFont.Dispose()
    $b.Image=$image;$b.ImageAlign='TopCenter';$b.TextAlign='BottomCenter';$b.Padding=New-Object Windows.Forms.Padding(4,10,4,10)
    $b.Font=New-Object Drawing.Font('Microsoft YaHei UI',9)
    $path=New-Object Drawing.Drawing2D.GraphicsPath
    $w=$b.Width;$h=$b.Height;$r=18
    $path.AddArc(0,0,$r,$r,180,90);$path.AddArc(($w-$r),0,$r,$r,270,90)
    $path.AddArc(($w-$r),($h-$r),$r,$r,0,90);$path.AddArc(0,($h-$r),$r,$r,90,90);$path.CloseFigure()
    $b.Region=New-Object Drawing.Region($path);$path.Dispose()
    $b.Add_Click({
        param($sender,$event)
        if ($script:busy) { return }
        $script:verificationReport='';$progress.Value=0;$progress.Style='Marquee';$script:busy = $true; $buttons | ForEach-Object { $_.Enabled = $false }
        try { Set-Status ('正在'+$sender.Text+'…'); Perform $sender.Tag; $progress.Style='Continuous';$progress.Value=100;$completion=@{'check'='设备检测完成';'install'='性能面板安装完成';'reboot'='掌机重启与验证完成';'verify'='性能面板状态验证完成';'remove'='性能面板移除完成';'clock-install'='时钟补丁安装完成';'clock-verify'='时钟补丁检查完成';'clock-remove'='时钟补丁移除完成'};if($script:verificationReport){Set-Status ($completion[[string]$sender.Tag]+"：`r`n"+$script:verificationReport)}else{Set-Status $completion[[string]$sender.Tag]} }
        catch { Write-Log ('Operation '+$sender.Tag+' failed: '+$_.Exception.ToString());$progress.Style='Continuous';$progress.Value=0;$message=$_.Exception.Message -replace '(?s)^.*失败：','';if($message.Length -gt 220){$message=$message.Substring(0,220)};Set-Status ('失败：'+$message) }
        finally { $script:busy = $false; $buttons | ForEach-Object { $_.Enabled = $true } }
    })
    $buttons += $b
    if($i -eq 0 -or $i -eq 2){$deviceCard.Controls.Add($b)}elseif($i -lt 5){$hudCard.Controls.Add($b)}else{$clockCard.Controls.Add($b)}
}
$form.Add_FormClosing({ param($sender,$event) if ($script:busy) { $event.Cancel = $true; Write-Log '操作进行中，请等待完成后关闭。' } })
Write-Log '准备就绪。重启会退出当前游戏，请先保存进度。'
[void]$form.ShowDialog()
