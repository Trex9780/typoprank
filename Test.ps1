$ErrorActionPreference = 'Stop'
$assembly = [Reflection.Assembly]::LoadFile((Resolve-Path (Join-Path $PSScriptRoot 'TypoPrank.exe')).Path)
$type = $assembly.GetType('TypoPrank')
$replacement = $type.GetMethod('ReplacementFor', [Reflection.BindingFlags]'NonPublic,Static')
$rows = @('QWERTYUIOP', 'ASDFGHJKL', 'ZXCVBNM')
foreach ($row in $rows) {
    for ($index = 0; $index -lt $row.Length; $index++) {
        foreach ($right in @($false, $true)) {
            $actual = [char]$replacement.Invoke($null, @([int][char]$row[$index], $right))
            if ([Math]::Abs($row.IndexOf($actual) - $index) -ne 1) {
                throw "Unexpected replacement for $($row[$index])"
            }
        }
    }
}
if ($replacement.Invoke($null, @([int][char]'1', $true)) -ne 0) { throw 'A nonletter was replaced.' }
$inputType = $type.GetNestedType('Input', [Reflection.BindingFlags]'NonPublic')
$sizeOf = [Runtime.InteropServices.Marshal].GetMethod('SizeOf', [Type[]]@([Type]))
if ($sizeOf.Invoke($null, [object[]]@($inputType)) -ne 40) { throw 'Win32 INPUT layout is wrong.' }
$flags = [Reflection.BindingFlags]'NonPublic,Instance'
$instance = [Runtime.Serialization.FormatterServices]::GetUninitializedObject($type)
$type.GetField('heldReplacement', $flags).SetValue($instance, [byte[]]::new(256))
$type.GetField('lettersUntilTypo', $flags).SetValue($instance, 5)
$eventType = $type.GetNestedType('KeyboardEvent', [Reflection.BindingFlags]'NonPublic')
$event = [Activator]::CreateInstance($eventType)
$keyField = $eventType.GetField('vkCode', [Reflection.BindingFlags]'Public,Instance')
$onKey = $type.GetMethod('OnKey', $flags)
$pointer = [Runtime.InteropServices.Marshal]::AllocHGlobal(32)
try {
    foreach ($letter in 'RRRRRRRRRR'.ToCharArray()) {
        $keyField.SetValue($event, [uint32][char]$letter)
        [Runtime.InteropServices.Marshal]::StructureToPtr($event, $pointer, $false)
        [void]$onKey.Invoke($instance, @(0, [IntPtr]0x100, $pointer))
        [void]$onKey.Invoke($instance, @(0, [IntPtr]0x101, $pointer))
    }
    if ($type.GetField('lettersUntilTypo', $flags).GetValue($instance) -ne 5) {
        throw 'Repeating one letter advanced the typo timer.'
    }
} finally {
    [Runtime.InteropServices.Marshal]::FreeHGlobal($pointer)
}
Write-Output 'Letter replacements, repeat suppression, and Win32 input layout checks passed.'
