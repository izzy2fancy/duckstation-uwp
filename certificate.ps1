$Pfxpath = './codesigning.pfx'
$Password = ConvertTo-SecureString -String 'secret' -AsPlainText -Force
Import-PfxCertificate -FilePath $Pfxpath -CertStoreLocation Cert:\CurrentUser\My -Password $Password