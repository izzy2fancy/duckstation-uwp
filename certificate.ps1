$PfxPath = './duckstation-uwp_TemporaryKey.pfx'
$PfxBytes = [System.Convert]::FromBase64String('${{ secrets.DUCKSTATION_PFX }}') # Replace 'DUCKSTATION_PFX' with the name of your GitHub secret containing the Base64 encoded PFX file
[IO.File]::WriteAllBytes($PfxPath, $PfxBytes)

$Password = ConvertTo-SecureString -String '${{ secrets.GITHUB_TOKEN }}' -AsPlainText -Force # Use GitHub token as the password
Import-PfxCertificate -FilePath $PfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $Password
