param()

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Read-RepoText {
    param([string]$RelativePath)
    return Get-Content -Encoding utf8 -Raw -LiteralPath (Join-Path $RepoRoot $RelativePath)
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )
    if ($Text -notmatch $Pattern) {
        throw $Message
    }
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

function Assert-ContainsUtf8Hex {
    param(
        [string]$Text,
        [string]$HexPattern,
        [string]$Message
    )

    $pattern = Convert-HexToUtf8String $HexPattern
    if (-not $Text.Contains($pattern)) {
        throw $Message
    }
}

function Assert-NotContains {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )
    if ($Text -match $Pattern) {
        throw $Message
    }
}

function Assert-PrivateAfterPublic {
    param(
        [string]$Text,
        [string]$Needle,
        [string]$Message
    )

    $privateIndex = $Text.IndexOf("private:")
    $needleIndex = $Text.IndexOf($Needle)
    if ($privateIndex -lt 0 -or $needleIndex -lt 0 -or $needleIndex -lt $privateIndex) {
        throw $Message
    }
}

function Assert-NoHardwareActionCommands {
    param(
        [string]$Text,
        [string]$Name
    )

    Assert-NotContains $Text "(?i)(-\s*t\s+upload|--target\s+upload|--upload-port|\bdevice\s+monitor\b|\bmonitor\s+--port\b)" `
        "$Name must not upload firmware or open serial monitors"
}

function Assert-RealFcOutputOnlyInFcReady {
    param([string]$PlatformioText)

    $currentEnv = ""
    $fcReadyOccurrences = 0
    $violations = @()

    foreach ($line in ($PlatformioText -split "`r?`n")) {
        if ($line -match "^\s*\[env:([^\]]+)\]") {
            $currentEnv = $Matches[1]
            continue
        }
        if ($line -match "^\s*\[") {
            $currentEnv = ""
            continue
        }

        if ($line -match "-D\s*ENABLE_REAL_FC_OUTPUT\s*=\s*1") {
            if ($currentEnv -eq "esp32-s3-unified-web-fc-ready") {
                $fcReadyOccurrences++
            } else {
                $violations += if ([string]::IsNullOrWhiteSpace($currentEnv)) {
                    "<global platformio section>"
                } else {
                    $currentEnv
                }
            }
        }
    }

    if ($violations.Count -gt 0) {
        throw "ENABLE_REAL_FC_OUTPUT=1 is only allowed in esp32-s3-unified-web-fc-ready; found in: $($violations -join ', ')"
    }
    if ($fcReadyOccurrences -ne 1) {
        throw "esp32-s3-unified-web-fc-ready must define ENABLE_REAL_FC_OUTPUT=1 exactly once"
    }
}

function Assert-BuildFlagNotEnabled {
    param(
        [string]$PlatformioText,
        [string]$MacroName,
        [string]$Message
    )

    $currentEnv = ""
    $violations = @()
    $escapedMacro = [regex]::Escape($MacroName)

    foreach ($line in ($PlatformioText -split "`r?`n")) {
        if ($line -match "^\s*\[env:([^\]]+)\]") {
            $currentEnv = $Matches[1]
            continue
        }
        if ($line -match "^\s*\[") {
            $currentEnv = ""
            continue
        }

        if ($line -match "-D\s*$escapedMacro\s*=\s*1") {
            $violations += if ([string]::IsNullOrWhiteSpace($currentEnv)) {
                "<global platformio section>"
            } else {
                $currentEnv
            }
        }
    }

    if ($violations.Count -gt 0) {
        throw "$Message Found in: $($violations -join ', ')"
    }
}

function Get-PlatformioEnvSection {
    param(
        [string]$PlatformioText,
        [string]$EnvName
    )

    $collecting = $false
    $found = $false
    $lines = New-Object System.Collections.Generic.List[string]

    foreach ($line in ($PlatformioText -split "`r?`n")) {
        if ($line -match "^\s*\[env:([^\]]+)\]") {
            if ($collecting) {
                break
            }
            if ($Matches[1] -eq $EnvName) {
                $collecting = $true
                $found = $true
                $lines.Add($line)
            }
            continue
        }

        if ($collecting -and $line -match "^\s*\[") {
            break
        }

        if ($collecting) {
            $lines.Add($line)
        }
    }

    if (-not $found) {
        throw "Missing PlatformIO environment: $EnvName"
    }

    return ($lines -join "`n")
}

function Assert-UnifiedWebEnvSeparation {
    param([string]$PlatformioText)

    $defaultWeb = Get-PlatformioEnvSection $PlatformioText "esp32-s3-unified-web"
    Assert-Contains $defaultWeb "comm[\\/]fc_bridge\.cpp" `
        "esp32-s3-unified-web must compile the read-only FC bridge"
    Assert-Contains $defaultWeb "comm[\\/]msp\.cpp" `
        "esp32-s3-unified-web must compile the read-only MSP transport"

    $demo = Get-PlatformioEnvSection $PlatformioText "esp32-s3-unified-web-demo"
    Assert-Contains $demo "extends\s*=\s*env:esp32-s3-unified-web" `
        "esp32-s3-unified-web-demo must extend the default unified Web environment"
    Assert-Contains $demo "-DFC_PRESENTATION_MODE=1" `
        "esp32-s3-unified-web-demo must explicitly enable FC presentation mode"
    Assert-Contains $demo "-DENABLE_FC_READONLY_TELEMETRY=0" `
        "esp32-s3-unified-web-demo must explicitly disable real FC polling"
    Assert-Contains $demo "-DUNIFIED_DEMO_MODE=1" `
        "esp32-s3-unified-web-demo must use the demo identity and AP name"

    $fcReady = Get-PlatformioEnvSection $PlatformioText "esp32-s3-unified-web-fc-ready"
    Assert-Contains $fcReady "comm[\\/]fc_bridge\.cpp" `
        "esp32-s3-unified-web-fc-ready must compile FC bridge source"
    Assert-Contains $fcReady "comm[\\/]msp\.cpp" `
        "esp32-s3-unified-web-fc-ready must compile MSP source"
}

Write-Host "== Replacement-FC static safety checks =="
Write-Host "Repo: $RepoRoot"
Write-Host ""
Write-Host "Safety: static checks only; no upload, no serial, no MSP writes, no motors."

$platformio = Read-RepoText "platformio.ini"
Assert-Contains $platformio "(?ms)\[platformio\]\s*default_envs\s*=\s*esp32-s3-unified-web" `
    "platformio.ini must keep default_envs = esp32-s3-unified-web"
Assert-RealFcOutputOnlyInFcReady $platformio
Assert-BuildFlagNotEnabled $platformio "ENABLE_ESP32_ARM_DISARM" `
    "No PlatformIO environment may enable ESP32 arm/disarm during replacement-FC readiness."
Assert-BuildFlagNotEnabled $platformio "ENABLE_LEGACY_FOLLOW_FC_OUTPUT" `
    "No PlatformIO environment may enable legacy FollowController real FC output during MC6C bring-up."
Assert-UnifiedWebEnvSeparation $platformio

$config = Read-RepoText "include\config.h"
Assert-Contains $config "#define\s+ENABLE_REAL_FC_OUTPUT\s+0" `
    "ENABLE_REAL_FC_OUTPUT must default to 0"
Assert-Contains $config "#define\s+ENABLE_ESP32_ARM_DISARM\s+0" `
    "ENABLE_ESP32_ARM_DISARM must default to 0"
Assert-Contains $config "#define\s+ENABLE_LEGACY_FOLLOW_FC_OUTPUT\s+0" `
    "ENABLE_LEGACY_FOLLOW_FC_OUTPUT must default to 0"
Assert-Contains $config "#define\s+REAL_FC_ASSIST_AUX_CHANNEL\s+6" `
    "REAL_FC_ASSIST_AUX_CHANNEL must remain CH6/AUX2"

$fcBridge = Read-RepoText "src\comm\fc_bridge.h"
Assert-NotContains $fcBridge "MSP\s*&\s*getMSP\s*\(" `
    "FCBridge must not expose mutable MSP& getMSP()"
Assert-Contains $fcBridge "const\s+MSPDiag\s*&?\s*getMSPDiag\s*\(" `
    "FCBridge must expose read-only MSP diagnostics through getMSPDiag()"

$fcBridgeSource = Read-RepoText "src\comm\fc_bridge.cpp"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastRoll\s*=\s*ch\[FC_RAW_RC_ROLL\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC roll"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastPitch\s*=\s*ch\[FC_RAW_RC_PITCH\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC pitch"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastYaw\s*=\s*ch\[FC_RAW_RC_YAW\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC yaw"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastThrottle\s*=\s*ch\[FC_RAW_RC_THROTTLE\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC throttle"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastAux1\s*=\s*ch\[FC_RAW_RC_AUX1\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC AUX1"
Assert-Contains $fcBridgeSource "m_outputDiag\.lastAux2\s*=\s*ch\[FC_RAW_RC_AUX2\]" `
    "FCBridge output diagnostics must report packed/clamped Betaflight RAW_RC AUX2"

$mspHeader = Read-RepoText "src\comm\msp.h"
Assert-PrivateAfterPublic $mspHeader "bool sendCommand" `
    "MSP::sendCommand() must stay private"

$mspSource = Read-RepoText "src\comm\msp.cpp"
Assert-Contains $mspSource "#if\s+ENABLE_REAL_FC_OUTPUT" `
    "MSP::setRawRC() must be guarded by ENABLE_REAL_FC_OUTPUT"
Assert-Contains $mspSource "#if\s+ENABLE_ESP32_ARM_DISARM" `
    "MSP::sendArmCommand() must be guarded by ENABLE_ESP32_ARM_DISARM"

$unifiedWebMain = Read-RepoText "src\unified_web_main.cpp"
Assert-Contains $unifiedWebMain "Default safe build: display only; no MSP writes" `
    "unified web serial banner must describe the default build as no-MSP-write"
Assert-Contains $unifiedWebMain "FC-ready: gated MSP_SET_RAW_RC roll/pitch/yaw assist" `
    "unified web serial banner must describe FC-ready as gated MSP_SET_RAW_RC only"
Assert-Contains $unifiedWebMain "Never arms/disarms or drives motors directly" `
    "unified web serial banner must preserve no arm/disarm/direct-motor boundary"

$indexHtml = Read-RepoText "src\web\index_html.h"
Assert-Contains $indexHtml "fcRealOutputCompiled" `
    "web page must distinguish output-capable builds from read-only telemetry"
Assert-ContainsUtf8Hex $indexHtml "E9A39EE68EA7E69CAAE8BF9EE68EA5" `
    "web page must explicitly show flight controller offline"
Assert-Contains $indexHtml "UART3 MSP" `
    "web page must identify the UART3 MSP wait state"
Assert-ContainsUtf8Hex $indexHtml "E58FAAE8AFBBE981A5E6B58B" `
    "web page must label the default link as read-only telemetry"

$srcFiles = Get-ChildItem -LiteralPath (Join-Path $RepoRoot "src") -Recurse -Include *.cpp,*.h | Where-Object { -not $_.PSIsContainer }
$externalMspWrites = @()
foreach ($file in $srcFiles) {
    $relative = Resolve-Path -Relative -LiteralPath $file.FullName
    $text = Get-Content -Encoding utf8 -Raw -LiteralPath $file.FullName
    if ($relative -notmatch "src[\\/]+comm[\\/]+msp\.(cpp|h)$" -and $text -match "sendCommand\s*\(") {
        $externalMspWrites += $relative
    }
}
if ($externalMspWrites.Count -gt 0) {
    throw "External code must not call MSP::sendCommand(): $($externalMspWrites -join ', ')"
}

$taskPlan = Read-RepoText "task_plan.md"
Assert-Contains $taskPlan "verified a transparent-heatshrink ESC" `
    "task_plan.md must record that the user-provided ESC photo was verified"
Assert-Contains $taskPlan "red wire/BEC subject to meter confirmation" `
    "task_plan.md must keep ESC red-wire/BEC evidence conditional on meter confirmation"
Assert-NotContains $taskPlan "referenced local image is unavailable" `
    "task_plan.md must not keep the stale ESC-photo-unavailable statement after the photo was provided"

$acceptance = Read-RepoText "replacement_fc_acceptance_log_template.md"
Assert-ContainsUtf8Hex $acceptance "E69CBAE5A4B4E69C9DE7A9BAE697B7E696B9E59091" `
    "acceptance log must record nose direction toward open space"
Assert-ContainsUtf8Hex $acceptance "E79FADE4BF83E8BDBBE5BEAEE58AA8E4BD9C" `
    "acceptance log must require short/light direction inputs"
Assert-ContainsUtf8Hex $acceptance "E5BC82E5B8B8E7AB8BE588BBE694B6E6B2B92FE696ADE8A7A3E99481" `
    "acceptance log must require immediate throttle cut/disarm on abnormal behavior"

$evidenceReadme = Read-RepoText "docs\evidence\replacement_fc\README.md"
Assert-ContainsUtf8Hex $evidenceReadme "E69CBAE5A4B4E69C9DE7A9BAE697B7E696B9E59091" `
    "evidence README must record nose direction toward open space"
Assert-ContainsUtf8Hex $evidenceReadme "E79FADE4BF83E8BDBBE5BEAEE58AA8E4BD9C" `
    "evidence README must require short/light direction inputs"

$goalAudit = Read-RepoText "replacement_fc_goal_completion_audit.md"
Assert-ContainsUtf8Hex $goalAudit "E79C9FE5AE9EE9A39EE8A18CE79BAEE6A0873A20E69CAAE5AE8CE68890" `
    "goal audit must say the real flight goal is not complete without hardware evidence"
Assert-ContainsUtf8Hex $goalAudit "E5BF85E9A1BBE7AD89E5BE85E696B0E9A39EE68EA7E5AE9EE789A9E8AF81E68DAE" `
    "goal audit must require replacement-FC physical evidence"
Assert-Contains $goalAudit "web-output-path:\s+20 passed,\s+0 failed" `
    "goal audit must reflect the current web-output-path host test count"

$arrivalChecklist = Read-RepoText "replacement_fc_arrival_checklist.md"
Assert-ContainsUtf8Hex $arrivalChecklist "45535033322D533320E4B88DE68EA5E4BBBBE4BD95E794B5E8B083E4BFA1E58FB7E7BABF" `
    "arrival checklist must forbid ESP32-S3 from connecting to ESC signal wires"
Assert-ContainsUtf8Hex $arrivalChecklist "43483620E4BF9DE68C81E4BD8EE4BD8DEFBC8CE4B88DE590AFE794A820455350333220E8BE85E58AA9E8BE93E587BA" `
    "arrival checklist must keep CH6 low for first manual flight"

$fieldOnePage = Read-RepoText "replacement_fc_field_one_page.md"
Assert-ContainsUtf8Hex $fieldOnePage "E7ACACE4B880E6ACA1E6898BE58AA8E9A39EE8A18CE4B88DE590AFE794A820455350333220E79C9FE69CBAE8BE85E58AA9E8BE93E587BA" `
    "field one-page must say first manual flight does not use ESP32 real assist output"

$softwareChecks = Read-RepoText "scripts\run_replacement_fc_software_checks.ps1"
Assert-Contains $softwareChecks "run_replacement_fc_static_checks\.ps1" `
    "replacement-FC software checks must run static safety checks first"

$evidenceCheckerPath = Join-Path $RepoRoot "scripts\check_replacement_fc_evidence.ps1"
if (-not (Test-Path -LiteralPath $evidenceCheckerPath -PathType Leaf)) {
    throw "Missing read-only evidence package checker: scripts\check_replacement_fc_evidence.ps1"
}

$manualNotesTemplatePath = Join-Path $RepoRoot "docs\evidence\replacement_fc\manual_flight_notes_template.txt"
if (-not (Test-Path -LiteralPath $manualNotesTemplatePath -PathType Leaf)) {
    throw "Missing manual flight notes template: docs\evidence\replacement_fc\manual_flight_notes_template.txt"
}

$repoReadme = Read-RepoText "README.md"
Assert-Contains $repoReadme "check_replacement_fc_evidence\.ps1" `
    "README.md must document the replacement-FC evidence package checker"
Assert-Contains $evidenceReadme "check_replacement_fc_evidence\.ps1" `
    "evidence README must document the replacement-FC evidence package checker"

$commandCard = Read-RepoText "replacement_fc_command_record_card.md"
Assert-Contains $commandCard "fcOutRoll/fcOutPitch/fcOutYaw/fcOutThrottle/fcOutAux1/fcOutAux2" `
    "command card must document FC-ready fcOut* JSON diagnostics"
Assert-Contains $commandCard "packed/clamped Betaflight RAW_RC output diagnostics" `
    "command card must define fcOut* as packed/clamped Betaflight RAW_RC output diagnostics"
Assert-Contains $commandCard "fcOutThrottle preserves live MC6C throttle" `
    "command card must state FC-ready preserves live MC6C throttle"
Assert-Contains $commandCard "check_replacement_fc_evidence\.ps1" `
    "command card must document the replacement-FC evidence package checker"

$manualNotesTemplate = Get-Content -Encoding utf8 -Raw -LiteralPath $manualNotesTemplatePath
Assert-Contains $manualNotesTemplate "Final Channel Map" `
    "manual flight notes template must include Final Channel Map"
Assert-Contains $manualNotesTemplate "CH6/AUX2 low for first manual flight" `
    "manual flight notes template must record CH6/AUX2 low for first manual flight"
Assert-Contains $manualNotesTemplate "ESP32 did not participate in first manual flight" `
    "manual flight notes template must record ESP32 nonparticipation"
Assert-Contains $manualNotesTemplate "Throttle result" `
    "manual flight notes template must include throttle result"
Assert-Contains $manualNotesTemplate "AIL right result" `
    "manual flight notes template must include AIL right result"
Assert-Contains $manualNotesTemplate "AIL left result" `
    "manual flight notes template must include AIL left result"
Assert-Contains $manualNotesTemplate "ELE forward result" `
    "manual flight notes template must include ELE forward result"
Assert-Contains $manualNotesTemplate "ELE backward result" `
    "manual flight notes template must include ELE backward result"
Assert-Contains $manualNotesTemplate "RUD right result" `
    "manual flight notes template must include RUD right result"
Assert-Contains $manualNotesTemplate "RUD left result" `
    "manual flight notes template must include RUD left result"
Assert-Contains $manualNotesTemplate "pass/fail" `
    "manual flight notes template must include pass/fail fields"
Assert-Contains $evidenceReadme "manual_flight_notes_template\.txt" `
    "evidence README must document the manual flight notes template"
Assert-Contains $commandCard "manual_flight_notes_template\.txt" `
    "command card must document the manual flight notes template"

$scriptFiles = @(
    (Join-Path $RepoRoot "scripts\run_host_tests.ps1"),
    (Join-Path $RepoRoot "scripts\run_replacement_fc_software_checks.ps1"),
    (Join-Path $RepoRoot "scripts\check_replacement_fc_evidence.ps1")
)
foreach ($script in $scriptFiles) {
    $scriptItem = Get-Item -LiteralPath $script
    $scriptText = Get-Content -Encoding utf8 -Raw -LiteralPath $scriptItem.FullName
    Assert-NoHardwareActionCommands $scriptText $scriptItem.Name
}

$evidenceCheckerText = Get-Content -Encoding utf8 -Raw -LiteralPath $evidenceCheckerPath
Assert-NotContains $evidenceCheckerText "(?i)\bplatformio(\.exe)?\b" `
    "Evidence package checker must not call PlatformIO"
Assert-Contains $evidenceCheckerText "Scope: passing this script is not proof that the aircraft flies correctly" `
    "Evidence package checker must not overclaim flight success"
Assert-Contains $evidenceCheckerText "manual_flight_notes\.txt" `
    "Evidence package checker must inspect manual_flight_notes.txt"
Assert-Contains $evidenceCheckerText "Channel Map" `
    "Evidence package checker must require manual flight notes to record the final Channel Map"
Assert-Contains $evidenceCheckerText "software_checks\.txt" `
    "Evidence package checker must inspect software_checks.txt"
Assert-Contains $evidenceCheckerText "Replacement-FC software checks passed" `
    "Evidence package checker must require the final software-check pass line"
Assert-Contains $evidenceReadme "manual_flight_notes\.txt" `
    "evidence README must document manual flight notes content checks"
Assert-Contains $commandCard "manual_flight_notes\.txt" `
    "command card must document manual flight notes content checks"

Write-Host ""
Write-Host "Replacement-FC static safety checks passed."
