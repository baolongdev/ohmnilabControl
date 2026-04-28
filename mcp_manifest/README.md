# MCP Manifest Layout

This folder splits `ohmni_robot_mcp.yaml` into smaller sections for easier editing.

Current structure:

1. `00-meta.yaml`
2. `10-hardware.yaml`
3. `20-motor.yaml`
4. `30-connection.yaml`
5. `40-tools-motion.yaml`
6. `41-tools-config.yaml`
7. `42-tools-actions.yaml`
8. `50-http-endpoints.yaml`

Notes:

- Files in this folder are the editable source layout.
- `ohmni_robot_mcp.yaml` at the repo root is now a generated file.
- Regenerate the root manifest with:
  `powershell -ExecutionPolicy Bypass -File scripts/generate-mcp-manifest.ps1`
- Short command from repo root:
  `generate-manifest.bat`
