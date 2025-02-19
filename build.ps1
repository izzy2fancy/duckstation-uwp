name: Build DuckStation UWP

on:
  push:
    branches:
      - main

jobs:
  build:
    runs-on: windows-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v2

      - name: Install Chocolatey
        if: runner.os == 'Windows'
        run: |
          Set-ExecutionPolicy Bypass -Scope Process -Force
          iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

      - name: Install Visual Studio 2022
        if: runner.os == 'Windows'
        run: choco install visualstudio2022buildtools-preview --pre -y

      - name: Install 7-Zip
        if: runner.os == 'Windows'
        run: choco install 7zip -y

      - name: Run build script
        if: runner.os == 'Windows'
        run: |
          .\build.ps1
