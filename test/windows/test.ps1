$PACKAGES = "..\ipa"
$PRIVATE_KEY = "..\assets\test.p12"
$MOBILE_PROVISION = "..\assets\test.mobileprovision"

Get-ChildItem -Path $PACKAGES -Filter "*.ipa" | ForEach-Object {
    $file = $_.FullName
    Write-Host "$file : " -NoNewline
    
    $zsignOutput = & ..\..\bin\zsign.exe -q -i -k $PRIVATE_KEY -m $MOBILE_PROVISION $file 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        Write-Host -ForegroundColor Green "OK."
    } else {
        Write-Host -ForegroundColor Red "!!!FAILED!!!"
    }
}
