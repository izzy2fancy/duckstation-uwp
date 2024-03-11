$PfxBase64 = '${{ secrets.YOUR_SECRET_NAME }}' # Replace 'DUCKSTATION_PFX' with the name of your GitHub secret containing the base64-encoded PFX data
$PfxBytes = [System.Convert]::FromBase64String($PfxBase64)
$PfxPath = './duckstation-uwp_TemporaryKey.pfx'
[IO.File]::WriteAllBytes($PfxPath, $PfxBytes)

$Password = ConvertTo-SecureString -String '${{ secrets.GITHUB_TOKEN }}' -AsPlainText -Force # Use GitHub token as the password
Import-PfxCertificate -FilePath $PfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $Password