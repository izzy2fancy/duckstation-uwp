Start-Process `
  -FilePath "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" `
  -Wait `
  -ArgumentList @( `
    "modify",
    "--installPath `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`"",
    "--add Microsoft.VisualStudio.Component.Windows10SDK",
    "--add Microsoft.VisualStudio.Component.Windows10SDK.15063.Desktop",
    "--add Microsoft.VisualStudio.Component.Windows10SDK.15063.UWP",
    "--add Microsoft.VisualStudio.Workload.WebBuildTools",
    "--add Microsoft.VisualStudio.Workload.UniversalBuildTools",
    "--norestart",
    "--quiet"
  )

wget -O deps-x64.7z https://github.com/stenzek/duckstation-ext-qt-minimal/releases/download/latest/deps-x64.7z
7z x deps-x64.7z -o"dep/msvc"
