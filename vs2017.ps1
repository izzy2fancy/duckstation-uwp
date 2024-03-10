Start-Process `
  -FilePath "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" `
  -Wait `
  -ArgumentList @( `
    "modify",
    "--installPath `"C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools`"",
    "--add Microsoft.VisualStudio.Component.Windows10SDK",
    "--add Microsoft.VisualStudio.Component.Windows10SDK.15063.Desktop",
    "--add Microsoft.VisualStudio.Component.Windows10SDK.15063.UWP",
    "--add Microsoft.VisualStudio.Workload.WebBuildTools",
    "--add Microsoft.VisualStudio.Workload.UniversalBuildTools",
    "--norestart",
    "--quiet"
  )