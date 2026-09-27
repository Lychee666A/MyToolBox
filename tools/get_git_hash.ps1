# tools/get_git_hash.ps1
# 获取当前 git 短 hash，写入临时文件
# 用法: powershell -ExecutionPolicy Bypass -File get_git_hash.ps1 <输出文件>

param(
    [Parameter(Mandatory=$true)]
    [string]$OutFile
)

$ErrorActionPreference = 'SilentlyContinue'

$hash = 'nogit'

try {
    $gitOutput = & git rev-parse --short=9 HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $gitOutput) {
        $hash = $gitOutput.Trim()
    }
} catch {
    $hash = 'nogit'
}

# 写入文件（UTF-8 无 BOM）
[System.IO.File]::WriteAllText($OutFile, $hash, [System.Text.UTF8Encoding]::new($false))

exit 0