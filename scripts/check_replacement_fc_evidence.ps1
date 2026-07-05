param(
    [string]$EvidenceDir = "",
    [switch]$RequireFcReady
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $RepoRoot "docs\evidence\replacement_fc"
}

if (-not (Test-Path -LiteralPath $EvidenceDir -PathType Container)) {
    throw "Evidence directory not found: $EvidenceDir"
}

$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceDir).Path

function Test-EvidenceFile {
    param([string]$Name)

    $path = Join-Path $EvidenceRoot $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $false
    }

    $item = Get-Item -LiteralPath $path
    return ($item.Length -gt 0)
}

function Get-EvidenceText {
    param([string]$Name)

    return Get-Content -Encoding utf8 -Raw -LiteralPath (Join-Path $EvidenceRoot $Name)
}

function Convert-HexToUtf8String {
    param([string]$Hex)

    if (($Hex.Length % 2) -ne 0) {
        throw "Invalid UTF-8 hex literal length"
    }

    $bytes = New-Object byte[] ($Hex.Length / 2)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [Convert]::ToByte($Hex.Substring($i * 2, 2), 16)
    }

    return [Text.Encoding]::UTF8.GetString($bytes)
}

function Assert-JsonEvidence {
    param(
        [string]$Name,
        [System.Collections.Generic.List[string]]$Problems
    )

    if (-not (Test-EvidenceFile $Name)) {
        return
    }

    try {
        $null = Get-EvidenceText $Name | ConvertFrom-Json
    } catch {
        $Problems.Add("$Name exists but is not valid JSON: $($_.Exception.Message)")
    }
}

function Assert-TextEvidenceTerms {
    param(
        [string]$Name,
        [array]$TermGroups,
        [System.Collections.Generic.List[string]]$Problems
    )

    if (-not (Test-EvidenceFile $Name)) {
        return
    }

    $text = Get-EvidenceText $Name
    foreach ($group in $TermGroups) {
        $matched = $false
        foreach ($pattern in $group.Patterns) {
            if ($text -match $pattern) {
                $matched = $true
                break
            }
        }

        if ($matched) {
            Write-Host "OK       $Name records $($group.Label)"
        } else {
            Write-Host "MISSING  $Name does not record $($group.Label)"
            $Problems.Add("$Name missing required note: $($group.Label)")
        }
    }
}

Write-Host "== Replacement-FC evidence package check =="
Write-Host "Evidence: $EvidenceRoot"
Write-Host ""
Write-Host "Safety: read-only file completeness check; no firmware flashing, no port access, no motors."
Write-Host "Scope: this checks whether the evidence package is complete enough to review."
Write-Host "Scope: passing this script is not proof that the aircraft flies correctly."
Write-Host ""

$required = @(
    [pscustomobject]@{ File = "software_checks.txt"; Stage = "software"; Why = "host checks and build output saved" },
    [pscustomobject]@{ File = "setup_model_preview.png"; Stage = "Betaflight"; Why = "model orientation preview" },
    [pscustomobject]@{ File = "ports_uart3_msp.png"; Stage = "Betaflight"; Why = "UART3 MSP port setting" },
    [pscustomobject]@{ File = "ports_receiver_serial_rx.png"; Stage = "Betaflight"; Why = "receiver input port setting" },
    [pscustomobject]@{ File = "receiver_protocol_map.png"; Stage = "Betaflight"; Why = "receiver protocol and channel map" },
    [pscustomobject]@{ File = "receiver_ch1_ch6.png"; Stage = "Betaflight"; Why = "CH1-CH6 receiver movement proof" },
    [pscustomobject]@{ File = "modes_arm_aux1.png"; Stage = "Betaflight"; Why = "AUX1/ARM or mode assignment" },
    [pscustomobject]@{ File = "motors_mixer_protocol.png"; Stage = "Betaflight"; Why = "mixer, motor protocol, and no-prop state" },
    [pscustomobject]@{ File = "betaflight_cli_snapshot.txt"; Stage = "Betaflight"; Why = "CLI version/status/serial/resource/map/aux/diff snapshot" },
    [pscustomobject]@{ File = "receiver_page.png"; Stage = "Receiver"; Why = "final Receiver page proof" },
    [pscustomobject]@{ File = "receiver_channel_video.mp4"; Stage = "Receiver"; Why = "MC6C six-channel movement video" },
    [pscustomobject]@{ File = "uart3_usb_ttl.log"; Stage = "UART3"; Why = "USB-TTL read-only UART3 MSP proof" },
    [pscustomobject]@{ File = "fc_uart_probe.log"; Stage = "UART3"; Why = "ESP32 read-only UART3 probe proof" },
    [pscustomobject]@{ File = "fc_diag.log"; Stage = "UART3"; Why = "ESP32 FC diagnostic proof" },
    [pscustomobject]@{ File = "motor_order_video.mp4"; Stage = "Motors"; Why = "no-prop M1-M4 order and rotation proof" },
    [pscustomobject]@{ File = "failsafe_video.mp4"; Stage = "Failsafe"; Why = "MC6C failsafe proof" },
    [pscustomobject]@{ File = "unified_web_page.png"; Stage = "Default Web"; Why = "default Web noninterference proof" },
    [pscustomobject]@{ File = "unified_web_status.json"; Stage = "Default Web"; Why = "default Web JSON status proof" },
    [pscustomobject]@{ File = "manual_hover_direction_video.mp4"; Stage = "Manual flight"; Why = "low-altitude MC6C direction proof" },
    [pscustomobject]@{ File = "manual_flight_notes.txt"; Stage = "Manual flight"; Why = "pilot notes and final channel-map result" }
)

$requiredAny = @(
    [pscustomobject]@{
        Label = "motor/prop direction record"
        Stage = "Motors"
        Why = "prop direction, motor direction, and correction record"
        Files = @(
            "replacement_fc_motor_direction_card_filled.md",
            "replacement_fc_motor_direction_card_filled.png",
            "replacement_fc_motor_direction_card_filled.jpg",
            "replacement_fc_motor_direction_card_filled.pdf",
            "motor_direction_card.png",
            "motor_direction_card.jpg",
            "motor_direction_card.pdf"
        )
    }
)

$fcReady = @(
    [pscustomobject]@{ File = "modes_msp_override_aux2.png"; Stage = "FC-ready later"; Why = "MSP Override gated by AUX2" },
    [pscustomobject]@{ File = "fc_ready_gate_page.png"; Stage = "FC-ready later"; Why = "FC-ready Web gate proof" },
    [pscustomobject]@{ File = "fc_ready_output_diag.json"; Stage = "FC-ready later"; Why = "FC-ready output diagnostic JSON" },
    [pscustomobject]@{ File = "fc_ready_no_prop_test_video.mp4"; Stage = "FC-ready later"; Why = "no-prop FC-ready output test video" }
)

$missing = New-Object System.Collections.Generic.List[string]
$contentProblems = New-Object System.Collections.Generic.List[string]

foreach ($item in $required) {
    if (Test-EvidenceFile $item.File) {
        Write-Host ("OK       {0,-18} {1}" -f $item.Stage, $item.File)
    } else {
        Write-Host ("MISSING  {0,-18} {1} - {2}" -f $item.Stage, $item.File, $item.Why)
        $missing.Add($item.File)
    }
}

foreach ($group in $requiredAny) {
    $found = @($group.Files | Where-Object { Test-EvidenceFile $_ })
    if ($found.Count -gt 0) {
        Write-Host ("OK       {0,-18} {1}" -f $group.Stage, ($found -join ", "))
    } else {
        Write-Host ("MISSING  {0,-18} {1} - {2}" -f $group.Stage, $group.Label, $group.Why)
        $missing.Add($group.Label)
    }
}

Write-Host ""
Write-Host "== Betaflight CLI snapshot content =="
if (Test-EvidenceFile "betaflight_cli_snapshot.txt") {
    $cliText = Get-EvidenceText "betaflight_cli_snapshot.txt"
    $terms = @("version", "status", "serial", "resource", "map", "aux", "diff all")
    foreach ($term in $terms) {
        if ($cliText.IndexOf($term, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            Write-Host "OK       betaflight_cli_snapshot.txt contains '$term'"
        } else {
            Write-Host "MISSING  betaflight_cli_snapshot.txt does not contain '$term'"
            $contentProblems.Add("betaflight_cli_snapshot.txt missing required term: $term")
        }
    }
} else {
    Write-Host "SKIP     betaflight_cli_snapshot.txt content check; file is missing"
}

Write-Host ""
Write-Host "== Software and manual-flight note content =="
$cnChannelMap = Convert-HexToUtf8String "E9809AE98193E698A0E5B084"
$cnLow = Convert-HexToUtf8String "E4BD8EE4BD8D"
$cnNotParticipate = Convert-HexToUtf8String "E4B88DE58F82E4B88E"
$cnDidNotParticipate = Convert-HexToUtf8String "E69CAAE58F82E4B88E"
$cnNotEnable = Convert-HexToUtf8String "E4B88DE590AFE794A8"
$cnDefault = Convert-HexToUtf8String "E9BB98E8AEA4"
$cnNotAffect = Convert-HexToUtf8String "E4B88DE5BDB1E5938D"
$cnThrottle = Convert-HexToUtf8String "E6B2B9E997A8"
$cnAil = Convert-HexToUtf8String "E589AFE7BFBC"
$cnRoll = Convert-HexToUtf8String "E6A8AAE6BB9A"
$cnEle = Convert-HexToUtf8String "E58D87E9998D"
$cnPitch = Convert-HexToUtf8String "E4BFAFE4BBB0"
$cnRud = Convert-HexToUtf8String "E696B9E59091E888B5"
$cnYaw = Convert-HexToUtf8String "E5818FE888AA"
$cnPass = Convert-HexToUtf8String "E9809AE8BF87"
$cnFailPhrase = Convert-HexToUtf8String "E4B88DE9809AE8BF87"
$cnFailed = Convert-HexToUtf8String "E5A4B1E8B4A5"
$cnAbnormal = Convert-HexToUtf8String "E5BC82E5B8B8"

Assert-TextEvidenceTerms "software_checks.txt" @(
    [pscustomobject]@{ Label = "final software check pass line"; Patterns = @("Replacement-FC software checks passed") },
    [pscustomobject]@{ Label = "host test pass line"; Patterns = @("All host tests passed") },
    [pscustomobject]@{ Label = "default Web build"; Patterns = @("esp32-s3-unified-web") },
    [pscustomobject]@{ Label = "FC-ready build"; Patterns = @("esp32-s3-unified-web-fc-ready") },
    [pscustomobject]@{ Label = "UART probe build"; Patterns = @("esp32-s3-fc-uart-probe") },
    [pscustomobject]@{ Label = "FC diag build"; Patterns = @("esp32-s3-fc-diag") }
) $contentProblems
if (-not (Test-EvidenceFile "software_checks.txt")) {
    Write-Host "SKIP     software_checks.txt content check; file is missing"
}

Assert-TextEvidenceTerms "manual_flight_notes.txt" @(
    [pscustomobject]@{ Label = "final Channel Map"; Patterns = @("(?i)Channel\s*Map", "(?i)\bmap\b", $cnChannelMap) },
    [pscustomobject]@{ Label = "CH6/AUX2 low for first manual flight"; Patterns = @("CH6.*$cnLow", "AUX2.*$cnLow", "$cnLow.*CH6", "$cnLow.*AUX2", "(?i)CH6.*low", "(?i)AUX2.*low", "(?i)low.*CH6", "(?i)low.*AUX2") },
    [pscustomobject]@{ Label = "ESP32 did not participate in first manual flight"; Patterns = @("ESP32.*$cnNotParticipate", "ESP32.*$cnDidNotParticipate", "$cnNotEnable.*ESP32", "$cnDefault.*Web.*$cnNotAffect", "(?i)ESP32.*did not participate", "(?i)ESP32.*not participate", "(?i)ESP32.*disabled", "(?i)ESP32.*off") },
    [pscustomobject]@{ Label = "throttle result"; Patterns = @($cnThrottle, "(?i)Throttle", "(?i)\bTHR\b") },
    [pscustomobject]@{ Label = "AIL/roll result"; Patterns = @("(?i)\bAIL\b", $cnAil, "(?i)\bRoll\b", $cnRoll) },
    [pscustomobject]@{ Label = "ELE/pitch result"; Patterns = @("(?i)\bELE\b", $cnEle, "(?i)\bPitch\b", $cnPitch) },
    [pscustomobject]@{ Label = "RUD/yaw result"; Patterns = @("(?i)\bRUD\b", $cnRud, "(?i)\bYaw\b", $cnYaw) },
    [pscustomobject]@{ Label = "pass/fail conclusion"; Patterns = @($cnPass, $cnFailPhrase, $cnFailed, $cnAbnormal, "(?i)\bpass\b", "(?i)\bfail\b") }
) $contentProblems
if (-not (Test-EvidenceFile "manual_flight_notes.txt")) {
    Write-Host "SKIP     manual_flight_notes.txt content check; file is missing"
}

Write-Host ""
Write-Host "== JSON sanity =="
Assert-JsonEvidence "unified_web_status.json" $contentProblems
if (Test-EvidenceFile "unified_web_status.json") {
    Write-Host "OK       unified_web_status.json is parseable JSON"
} else {
    Write-Host "SKIP     unified_web_status.json JSON check; file is missing"
}

if ($RequireFcReady -or (Test-EvidenceFile "fc_ready_output_diag.json")) {
    Assert-JsonEvidence "fc_ready_output_diag.json" $contentProblems
    if (Test-EvidenceFile "fc_ready_output_diag.json") {
        Write-Host "OK       fc_ready_output_diag.json is parseable JSON"
    } else {
        Write-Host "SKIP     fc_ready_output_diag.json JSON check; file is missing"
    }
}

Write-Host ""
Write-Host "== FC-ready later-stage evidence =="
foreach ($item in $fcReady) {
    if (Test-EvidenceFile $item.File) {
        Write-Host ("OK       {0,-18} {1}" -f $item.Stage, $item.File)
    } elseif ($RequireFcReady) {
        Write-Host ("MISSING  {0,-18} {1} - {2}" -f $item.Stage, $item.File, $item.Why)
        $missing.Add($item.File)
    } else {
        Write-Host ("LATER    {0,-18} {1}" -f $item.Stage, $item.File)
    }
}

if ($missing.Count -gt 0 -or $contentProblems.Count -gt 0) {
    Write-Host ""
    Write-Host "Evidence package is incomplete."
    if ($missing.Count -gt 0) {
        Write-Host "Missing files or groups:"
        foreach ($item in $missing) {
            Write-Host "  - $item"
        }
    }
    if ($contentProblems.Count -gt 0) {
        Write-Host "Content problems:"
        foreach ($item in $contentProblems) {
            Write-Host "  - $item"
        }
    }
    throw "Replacement-FC evidence package check failed"
}

Write-Host ""
Write-Host "Replacement-FC evidence package check passed."
Write-Host "Review reminder: this proves evidence completeness only; final flight success still requires the acceptance log and audit."
