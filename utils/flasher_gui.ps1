Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FullBinPath = Join-Path $ScriptDir "firmware\ESP32_CYD_APRS_FULL.bin"
$UpdateBinPath = Join-Path $ScriptDir "firmware\update.bin"
$LocalEsptoolDir = Join-Path $ScriptDir "bin"
$LocalEsptoolExe = Join-Path $LocalEsptoolDir "esptool.exe"

# Colores estilo Radioaficionado / Dark UI
$ColBg     = [System.Drawing.Color]::FromArgb(14, 22, 33)
$ColCard   = [System.Drawing.Color]::FromArgb(24, 37, 51)
$ColAccent = [System.Drawing.Color]::FromArgb(0, 188, 212)
$ColGreen  = [System.Drawing.Color]::FromArgb(46, 204, 113)
$ColOrange = [System.Drawing.Color]::FromArgb(243, 156, 18)
$ColWhite  = [System.Drawing.Color]::FromArgb(240, 245, 250)
$ColMuted  = [System.Drawing.Color]::FromArgb(160, 175, 190)

$Form = New-Object System.Windows.Forms.Form
$Form.Text = "Grabador de Firmware ESP32 CYD APRS (LU4HNE) - Para Principiantes"
$Form.Size = New-Object System.Drawing.Size(660, 590)
$Form.StartPosition = "CenterScreen"
$Form.FormBorderStyle = "FixedDialog"
$Form.MaximizeBox = $false
$Form.BackColor = $ColBg
$Form.ForeColor = $ColWhite
$Form.Font = New-Object System.Drawing.Font("Segoe UI", 9.5)

# Encabezado
$HeaderPanel = New-Object System.Windows.Forms.Panel
$HeaderPanel.Size = New-Object System.Drawing.Size(660, 64)
$HeaderPanel.Location = New-Object System.Drawing.Point(0, 0)
$HeaderPanel.BackColor = $ColCard
$Form.Controls.Add($HeaderPanel)

$TitleLbl = New-Object System.Windows.Forms.Label
$TitleLbl.Text = "ESP32 CYD APRS 1200 bps - Instalador Facil de Firmware"
$TitleLbl.Font = New-Object System.Drawing.Font("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
$TitleLbl.ForeColor = $ColAccent
$TitleLbl.AutoSize = $true
$TitleLbl.Location = New-Object System.Drawing.Point(18, 10)
$HeaderPanel.Controls.Add($TitleLbl)

$SubTitleLbl = New-Object System.Windows.Forms.Label
$SubTitleLbl.Text = "Graba tu placa ESP32-2432S024C / 2432S028R sin necesidad de saber programar (LU4HNE)"
$SubTitleLbl.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$SubTitleLbl.ForeColor = $ColMuted
$SubTitleLbl.AutoSize = $true
$SubTitleLbl.Location = New-Object System.Drawing.Point(20, 36)
$HeaderPanel.Controls.Add($SubTitleLbl)

# Paso 1: Puerto USB (COM)
$Step1Group = New-Object System.Windows.Forms.GroupBox
$Step1Group.Text = " PASO 1: Conecta tu placa CYD por USB y elige el Puerto COM "
$Step1Group.ForeColor = $ColAccent
$Step1Group.Size = New-Object System.Drawing.Size(616, 76)
$Step1Group.Location = New-Object System.Drawing.Point(16, 76)
$Form.Controls.Add($Step1Group)

$PortCombo = New-Object System.Windows.Forms.ComboBox
$PortCombo.DropDownStyle = "DropDownList"
$PortCombo.Size = New-Object System.Drawing.Size(430, 28)
$PortCombo.Location = New-Object System.Drawing.Point(16, 30)
$PortCombo.BackColor = $ColCard
$PortCombo.ForeColor = $ColWhite
$PortCombo.Font = New-Object System.Drawing.Font("Segoe UI", 10)
$Step1Group.Controls.Add($PortCombo)

$RefreshBtn = New-Object System.Windows.Forms.Button
$RefreshBtn.Text = "Buscar Puertos"
$RefreshBtn.Size = New-Object System.Drawing.Size(150, 30)
$RefreshBtn.Location = New-Object System.Drawing.Point(454, 28)
$RefreshBtn.BackColor = $ColAccent
$RefreshBtn.ForeColor = $ColBg
$RefreshBtn.FlatStyle = "Flat"
$RefreshBtn.Font = New-Object System.Drawing.Font("Segoe UI", 9.5, [System.Drawing.FontStyle]::Bold)
$Step1Group.Controls.Add($RefreshBtn)

# Paso 2: Modo de Grabacion
$Step2Group = New-Object System.Windows.Forms.GroupBox
$Step2Group.Text = " PASO 2: Elige que tipo de instalacion deseas realizar "
$Step2Group.ForeColor = $ColAccent
$Step2Group.Size = New-Object System.Drawing.Size(616, 134)
$Step2Group.Location = New-Object System.Drawing.Point(16, 162)
$Form.Controls.Add($Step2Group)

$RadioFull = New-Object System.Windows.Forms.RadioButton
$RadioFull.Text = "Instalacion Completa desde Cero (Recomendado para placas nuevas - Offset 0x0000)"
$RadioFull.ForeColor = $ColWhite
$RadioFull.AutoSize = $true
$RadioFull.Checked = $true
$RadioFull.Location = New-Object System.Drawing.Point(16, 26)
$Step2Group.Controls.Add($RadioFull)

$RadioUpdate = New-Object System.Windows.Forms.RadioButton
$RadioUpdate.Text = "Solo Actualizar Firmware (Conserva tu Indicativo, WiFi y Config guardada - Offset 0x10000)"
$RadioUpdate.ForeColor = $ColWhite
$RadioUpdate.AutoSize = $true
$RadioUpdate.Location = New-Object System.Drawing.Point(16, 56)
$Step2Group.Controls.Add($RadioUpdate)

$ChkErase = New-Object System.Windows.Forms.CheckBox
$ChkErase.Text = "Borrar completamente la memoria Flash antes de grabar (Limpia configuraciones viejas)"
$ChkErase.ForeColor = $ColMuted
$ChkErase.AutoSize = $true
$ChkErase.Location = New-Object System.Drawing.Point(18, 92)
$Step2Group.Controls.Add($ChkErase)

# Paso 3: Botones de Accion
$FlashBtn = New-Object System.Windows.Forms.Button
$FlashBtn.Text = "GRABAR FIRMWARE EN LA PLACA (USB)"
$FlashBtn.Size = New-Object System.Drawing.Size(375, 46)
$FlashBtn.Location = New-Object System.Drawing.Point(16, 308)
$FlashBtn.BackColor = $ColGreen
$FlashBtn.ForeColor = $ColBg
$FlashBtn.FlatStyle = "Flat"
$FlashBtn.Font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
$Form.Controls.Add($FlashBtn)

$SdCopyBtn = New-Object System.Windows.Forms.Button
$SdCopyBtn.Text = "Copiar update.bin a MicroSD"
$SdCopyBtn.Size = New-Object System.Drawing.Size(230, 46)
$SdCopyBtn.Location = New-Object System.Drawing.Point(402, 308)
$SdCopyBtn.BackColor = $ColOrange
$SdCopyBtn.ForeColor = $ColBg
$SdCopyBtn.FlatStyle = "Flat"
$SdCopyBtn.Font = New-Object System.Drawing.Font("Segoe UI", 9.5, [System.Drawing.FontStyle]::Bold)
$Form.Controls.Add($SdCopyBtn)

# Barra de Progreso y Consola de Salida
$ProgressBar = New-Object System.Windows.Forms.ProgressBar
$ProgressBar.Size = New-Object System.Drawing.Size(616, 18)
$ProgressBar.Location = New-Object System.Drawing.Point(16, 364)
$ProgressBar.Style = "Continuous"
$Form.Controls.Add($ProgressBar)

$LogBox = New-Object System.Windows.Forms.TextBox
$LogBox.Multiline = $true
$LogBox.ScrollBars = "Vertical"
$LogBox.ReadOnly = $true
$LogBox.BackColor = [System.Drawing.Color]::FromArgb(8, 12, 18)
$LogBox.ForeColor = [System.Drawing.Color]::FromArgb(120, 255, 160)
$LogBox.Font = New-Object System.Drawing.Font("Consolas", 9)
$LogBox.Size = New-Object System.Drawing.Size(616, 150)
$LogBox.Location = New-Object System.Drawing.Point(16, 390)
$Form.Controls.Add($LogBox)

function Write-Log([string]$msg) {
    $timestamp = (Get-Date).ToString("HH:mm:ss")
    $LogBox.AppendText("[$timestamp] $msg`r`n")
    $LogBox.SelectionStart = $LogBox.Text.Length
    $LogBox.ScrollToCaret()
    [System.Windows.Forms.Application]::DoEvents()
}

function Update-ComPorts {
    $PortCombo.Items.Clear()
    $ports = @()
    try {
        $wmiPorts = Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "\(COM\d+\)" }
        foreach ($p in $wmiPorts) {
            if ($p.Name -match "\((COM\d+)\)") {
                $comName = $Matches[1]
                $ports += "$comName - $($p.Name)"
            }
        }
    } catch {}

    if ($ports.Count -eq 0) {
        $rawPorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
        foreach ($r in $rawPorts) {
            $ports += "$r - Puerto Serie USB"
        }
    }

    foreach ($item in $ports) {
        [void]$PortCombo.Items.Add($item)
    }

    if ($PortCombo.Items.Count -gt 0) {
        # Priorizar CH340 / CP210x si existe
        $bestIdx = 0
        for ($i = 0; $i -lt $PortCombo.Items.Count; $i++) {
            if ($PortCombo.Items[$i] -match "CH340|CP210|USB-SERIAL|Silicon|wch") {
                $bestIdx = $i
                break
            }
        }
        $PortCombo.SelectedIndex = $bestIdx
        Write-Log "Se detectaron $($PortCombo.Items.Count) puerto(s) COM. Seleccionado: $($PortCombo.SelectedItem)"
    } else {
        Write-Log "No se detecto ningun puerto COM. Conecta la placa CYD por USB y pulsa 'Buscar Puertos'."
    }
}

function Find-Or-Download-Esptool {
    # 1. Revisar en utils\bin\esptool.exe
    if (Test-Path $LocalEsptoolExe) {
        return $LocalEsptoolExe
    }
    # 2. Revisar en PlatformIO del usuario actual
    $pioEsptool = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptool\esptool.exe"
    if (Test-Path $pioEsptool) {
        return $pioEsptool
    }
    # 3. Revisar en PATH global
    $cmd = Get-Command "esptool.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    # 4. Descargar esptool.exe oficial de Espressif automáticamente para usuarios nuevos sin nada instalado
    Write-Log "Primera ejecucion en esta PC: Descargando herramienta oficial esptool.exe de Espressif..."
    try {
        if (!(Test-Path $LocalEsptoolDir)) {
            New-Item -ItemType Directory -Force -Path $LocalEsptoolDir | Out-Null
        }
        $zipUrl = "https://github.com/espressif/esptool/releases/download/v4.7.0/esptool-v4.7.0-win64.zip"
        $zipPath = Join-Path $LocalEsptoolDir "esptool.zip"
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
        Expand-Archive -Path $zipPath -DestinationPath $LocalEsptoolDir -Force
        $extracted = Get-ChildItem -Path $LocalEsptoolDir -Recurse -Filter "esptool.exe" | Select-Object -First 1
        if ($extracted -and ($extracted.FullName -ne $LocalEsptoolExe)) {
            Copy-Item $extracted.FullName $LocalEsptoolExe -Force
        }
        Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
        if (Test-Path $LocalEsptoolExe) {
            Write-Log "esptool.exe descargado y listo en utils\bin\esptool.exe."
            return $LocalEsptoolExe
        }
    } catch {
        Write-Log "Error al descargar esptool.exe: $($_.Exception.Message)"
    }
    return $null
}

$RefreshBtn.Add_Click({
    Update-ComPorts
})

$SdCopyBtn.Add_Click({
    if (!(Test-Path $UpdateBinPath)) {
        [System.Windows.Forms.MessageBox]::Show("No se encontro el archivo firmware\update.bin", "Archivo faltante", "OK", "Error")
        return
    }
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = "Selecciona la unidad de tu tarjeta MicroSD (FAT32) para copiar update.bin:"
    if ($dlg.ShowDialog() -eq "OK") {
        $destFile = Join-Path $dlg.SelectedPath "update.bin"
        try {
            Copy-Item $UpdateBinPath $destFile -Force
            Write-Log "Archivo copiado con exito a: $destFile"
            Write-Log "Inserta la MicroSD en la placa CYD y reiniciala: se actualizara sola al encender!"
            [System.Windows.Forms.MessageBox]::Show(
                "Se copio 'update.bin' correctamente a:`n$destFile`n`nAhora solo inserta la tarjeta MicroSD en tu placa CYD y enciendela. La placa detectara el archivo y se actualizara automaticamente mostrando una barra en su pantalla.",
                "MicroSD Lista", "OK", "Information")
        } catch {
            Write-Log "Error al copiar a MicroSD: $($_.Exception.Message)"
        }
    }
})

$FlashBtn.Add_Click({
    if ($PortCombo.SelectedIndex -lt 0) {
        [System.Windows.Forms.MessageBox]::Show("Por favor conecta la placa por USB y selecciona un puerto COM en el Paso 1.", "Selecciona Puerto COM", "OK", "Warning")
        return
    }

    $selText = $PortCombo.SelectedItem.ToString()
    $comPort = ($selText -split " ")[0].Trim()

    $targetBin = if ($RadioFull.Checked) { $FullBinPath } else { $UpdateBinPath }
    $targetOffset = if ($RadioFull.Checked) { "0x0" } else { "0x10000" }

    if (!(Test-Path $targetBin)) {
        [System.Windows.Forms.MessageBox]::Show("No se encontro el archivo de firmware:`n$targetBin", "Error", "OK", "Error")
        return
    }

    $FlashBtn.Enabled = $false
    $RefreshBtn.Enabled = $false
    $SdCopyBtn.Enabled = $false
    $ProgressBar.Value = 10

    $esptool = Find-Or-Download-Esptool
    if (!$esptool) {
        Write-Log "ERROR: No se pudo obtener esptool.exe. Puedes usar Grabador_Web_Chrome_Edge.html desde tu navegador."
        $FlashBtn.Enabled = $true
        $RefreshBtn.Enabled = $true
        $SdCopyBtn.Enabled = $true
        $ProgressBar.Value = 0
        return
    }

    try {
        if ($ChkErase.Checked) {
            Write-Log "Borrando memoria Flash completa en $comPort..."
            $ProgressBar.Value = 25
            $eraseOut = & $esptool --chip esp32 --port $comPort erase_flash 2>&1
            foreach ($line in $eraseOut) { Write-Log $line.ToString() }
        }

        Write-Log "Iniciando grabacion en $comPort (Offset $targetOffset)..."
        Write-Log "Archivo: $(Split-Path -Leaf $targetBin)"
        Write-Log "NOTA: Si dice 'Connecting...', manten presionado el boton BOOT de la placa 2 segundos."
        $ProgressBar.Value = 45

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $esptool
        $psi.Arguments = "--chip esp32 --port $comPort --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB $targetOffset `"$targetBin`""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($psi)
        while (!$proc.StandardOutput.EndOfStream) {
            $line = $proc.StandardOutput.ReadLine()
            if ($line) {
                Write-Log $line
                if ($line -match "\((\d+)\s*%\)") {
                    $pct = [int]$Matches[1]
                    $ProgressBar.Value = [Math]::Min(100, 45 + [int]($pct * 0.55))
                }
            }
        }
        $errText = $proc.StandardError.ReadToEnd()
        if ($errText) { Write-Log $errText }
        $proc.WaitForExit()

        if ($proc.ExitCode -eq 0) {
            $ProgressBar.Value = 100
            Write-Log "=================================================================="
            Write-Log "EXITO TOTAL! El firmware APRS se grabo correctamente en tu placa."
            Write-Log "Nota: Si tu pantalla se ve blanca o invertida, manten presionado"
            Write-Log "el boton BOOT de la placa durante 2 segundos para alternar ILI9341/ST7789."
            Write-Log "=================================================================="
            [System.Windows.Forms.MessageBox]::Show(
                "El firmware se grabo con exito en tu placa ESP32 CYD!`n`nTIP IMPORTANTE:`nSi tu placa es la variante ST7789 y al encender no da imagen o los colores se ven distintos, manten presionado el boton fisico BOOT de la placa durante 2 segundos y cambiara automaticamente el driver de pantalla.",
                "Grabacion Exitosa - 73 de LU4HNE!", "OK", "Information")
        } else {
            $ProgressBar.Value = 0
            Write-Log "ERROR durante la grabacion (Codigo $($proc.ExitCode)). Verifica el puerto COM o manten apretado BOOT."
            [System.Windows.Forms.MessageBox]::Show(
                "No se pudo completar la grabacion.`n`nVerifica que:`n1. Elegiste el puerto COM correcto.`n2. Ningun otro programa (como Monitor Serie) este usando el puerto.`n3. Intenta mantener presionado el boton BOOT de la placa mientras pulsas GRABAR.",
                "Error de Grabacion", "OK", "Error")
        }
    } catch {
        Write-Log "Excepcion: $($_.Exception.Message)"
    } finally {
        $FlashBtn.Enabled = $true
        $RefreshBtn.Enabled = $true
        $SdCopyBtn.Enabled = $true
    }
})

Write-Log "Bienvenido al Grabador de Firmware ESP32 CYD APRS."
if (Test-Path $FullBinPath) {
    $sz = [Math]::Round((Get-Item $FullBinPath).Length / 1MB, 2)
    Write-Log "Binario completo listo: ESP32_CYD_APRS_FULL.bin ($sz MB)"
}
Update-ComPorts

[void]$Form.ShowDialog()
