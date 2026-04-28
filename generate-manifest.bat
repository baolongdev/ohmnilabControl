@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\generate-mcp-manifest.ps1"
